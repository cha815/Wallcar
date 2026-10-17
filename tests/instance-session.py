"""Exercise the registered public motor API over the default UART bench bridge.

Default: read both nodes. --motion: selected nodes, each 5rad/s for --duration seconds.
Generic DISABLE calls release each instance after measurement; no chassis code.
"""
import argparse
import csv
import json
import math
import struct
import time
from pathlib import Path
try:
    import serial
except ImportError:
    import win_serial as serial

FIELDS = ['time_s','id','ready','enabled','target_velocity','velocity','position',
          'torque','temperature','fault','age_ms','command_result','streaming','count','config_failed']
TAIL = b'\x00\x00\x80\x7f'
parser = argparse.ArgumentParser()
parser.add_argument('--port', default='COM12')
parser.add_argument('--seconds', type=float, default=6)
parser.add_argument('--motion', action='store_true')
parser.add_argument('--simultaneous', action='store_true', help='Run all selected instances together for one shared duration')
parser.add_argument('--duration', type=float, default=2, help='Motion capture duration per selected motor, seconds')
parser.add_argument('--id', type=int, choices=[1,2], help='Validate one instance only; no dependency on the other node')
parser.add_argument('--output', default='docs/evidence/dual-instances-idle.csv')
args = parser.parse_args()
required = (args.id,) if args.id else (1,2)
rows, diagnostics, latest, arrivals, buffer, summary = [], [], {}, {}, bytearray(), []
failure = ''

with serial.Serial(args.port,115200,timeout=0.02,write_timeout=1) as port:
    def send(text):
        port.write((text+'\n').encode('ascii'));port.flush()

    def pump():
        buffer.extend(port.read(max(1,port.in_waiting)))
        while True:
            at=buffer.find(TAIL)
            if at<0:
                break
            if at>=60:
                v=struct.unpack('<15f',buffer[at-60:at])
                if all(math.isfinite(x) for x in v) and v[1] in (1,2):
                    if v[0]==-1:
                        diagnostics.append(v)
                    elif v[0]>=0 and v[2] in (0,1) and v[3] in (0,1):
                        rows.append(v);latest[int(v[1])]=v;arrivals[int(v[1])]=time.monotonic()
            del buffer[:at+4]
        if len(buffer)>4096:
            del buffer[:-64]

    def fresh(ident):
        return ident in latest and time.monotonic()-arrivals[ident]<0.3 and latest[ident][10]<100

    def wait(predicate,seconds,description):
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            pump()
            if predicate():
                return
        raise RuntimeError(description+': '+repr(latest))

    try:
        until=time.monotonic()+args.seconds
        while time.monotonic()<until:
            pump()
        print('Nodes:',json.dumps(latest),flush=True)
        print('Bus:',diagnostics[-2:],flush=True)
        wait(lambda: all(fresh(i) and latest[i][2] and not latest[i][14] and
             not latest[i][9] for i in required),2,'Selected instances must have valid config and feedback')
        if args.motion:
            groups = [required] if args.simultaneous else [(i,) for i in required]
            for group in groups:
                for ident in group:
                    send(f'ENABLE {ident}')
                wait(lambda: all(fresh(i) and latest[i][3] for i in group),1,'Enable feedback')
                for ident in group:
                    send(f'VEL {ident} 5 1')
                wait(lambda: all(fresh(i) and latest[i][12] and abs(latest[i][4]-5)<0.01 for i in group),
                     1,'Public velocity interface accepted')
                first=len(rows);start=time.monotonic()
                while time.monotonic()-start<args.duration:
                    pump()
                    # USB/wireless UART delivery is not a real-time motor watchdog.
                    # Keep the requested duration; inspect received device facts.
                    if any(latest[i][9] or not latest[i][3] for i in group):
                        raise RuntimeError('Motor feedback invalid during API test: '+repr(latest))
                for ident in group:
                    samples=[v for v in rows[first:] if v[1]==ident and v[12] and v[3]]
                    end=samples[-1][0]
                    steady=[v for v in samples if v[0]>=end-0.5]
                    result=dict(id=ident,target=5,mean=sum(v[5] for v in steady)/len(steady),
                        min=min(v[5] for v in steady),max=max(v[5] for v in steady),
                        max_feedback_torque=max(abs(v[7]) for v in samples),samples=len(samples))
                    summary.append(result);print(json.dumps(result),flush=True)
                for ident in group:
                    send(f'DISABLE {ident}')
                wait(lambda: all(fresh(i) and not latest[i][3] for i in group),1,'Disable interface feedback')
    except Exception as exc:
        failure=str(exc)
    finally:
        if args.motion:
            for ident in required:
                send(f'DISABLE {ident}')
            try:
                wait(lambda: all(fresh(i) and not latest[i][3] for i in required),1.5,'Final disabled feedback')
            except Exception as exc:
                failure+='\n'+str(exc)

output=Path(args.output);output.parent.mkdir(parents=True,exist_ok=True)
with output.open('w',newline='',encoding='utf-8') as stream:
    writer=csv.writer(stream);writer.writerow(FIELDS);writer.writerows(rows)
output.with_suffix('.json').write_text(json.dumps(dict(summary=summary,latest=latest,
    diagnostics=diagnostics,failure=failure),indent=2),encoding='utf-8')
print(f'frames={len(rows)} csv={output}')
if failure:
    raise SystemExit(failure)
