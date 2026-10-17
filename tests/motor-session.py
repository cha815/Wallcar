"""COM12 MIT bench runner. Always STOP in finally. Never writes motor Flash."""
import argparse, csv, math, struct, time
from pathlib import Path
try:
    import serial
except ImportError:
    import win_serial as serial

FIELDS = ['time_s','target_pos','actual_pos','target_vel','actual_vel','actual_torque',
          'kp','kd','temperature','state','fault','command_result','enabled','feedback_age_ms','stop_confirmed']
TAIL = b'\x00\x00\x80\x7f'
parser = argparse.ArgumentParser()
parser.add_argument('--port',default='COM12')
parser.add_argument('--seconds',type=float,default=5)
parser.add_argument('--output',default='docs/evidence/tuning-idle.csv')
parser.add_argument('--command',default='',help='VEL ... or POS ... after confirmed ARM')
parser.add_argument('--info',action='store_true')
parser.add_argument('--clear',action='store_true',help='Explicit one-shot clear of a known protection while disabled; never changes protection settings')
parser.add_argument('--read',type=int,default=0,choices=[0,49,50,66,71])
args = parser.parse_args()
rows=[]; buffer=bytearray(); start=time.monotonic(); ping=0; stage=0; arm_time=0
clear_sent=False;clear_time=0;failure=''
info_sent=False

def send(port,text):
    port.write((text+'\n').encode('ascii')); port.flush()

with serial.Serial(args.port,115200,timeout=0.02,write_timeout=1) as port:
    try:
        if args.read: send(port,f'READ {args.read}')
        elif args.info: send(port,'INFO')
        while time.monotonic()-start < args.seconds:
            current=time.monotonic()
            if args.read and not info_sent and current-start>0.2:
                send(port,'INFO');info_sent=True
            if current-ping>=0.1: send(port,'PING');ping=current
            buffer.extend(port.read(max(1,port.in_waiting)))
            while True:
                at=buffer.find(TAIL)
                if at<0: break
                if at>=60:
                    values=struct.unpack('<15f',buffer[at-60:at])
                    if all(math.isfinite(v) for v in values) and 0<=values[9]<=7 and values[9]==int(values[9]):
                        if values[0]<0:
                            print('INFO index/raw/TX/done/RX/protocol/TEC/REC/state/fault/LEC+16DLEC/motor_status/age/stop=',values[1:15])
                            del buffer[:at+4]
                            continue
                        rows.append(values)
                        state=int(values[9]);fault=int(values[10])
                        if args.clear and not clear_sent and state==7 and not values[12]:
                            send(port,'CLEAR');clear_sent=True;clear_time=current
                        if args.command and stage==0 and state==1 and not fault:
                            if values[8]>=80:
                                failure=f'Motor reports {values[8]:.1f} C; no enable attempted'
                            else:
                                send(port,'ARM'); stage=1;arm_time=current
                        elif args.command and stage==1 and state==3 and not fault:
                            send(port,args.command);stage=2
                        if args.command and state==7 and (not args.clear or (clear_sent and current-clear_time>1)):
                            failure=f'Motor FAULT={fault}'
                del buffer[:at+4]
            if len(buffer)>4096: del buffer[:-64]
            if args.command and stage==1 and current-arm_time>1.0:
                failure='ARM was not confirmed within 1 second'
            if failure: break
    finally:
        send(port,'STOP')
        until=time.monotonic()+0.7
        while time.monotonic()<until:
            buffer.extend(port.read(max(1,port.in_waiting)))
            at=buffer.find(TAIL)
            while at>=0:
                if at>=60:
                    values=struct.unpack('<15f',buffer[at-60:at])
                    if all(math.isfinite(v) for v in values): rows.append(values)
                del buffer[:at+4];at=buffer.find(TAIL)

output=Path(args.output);output.parent.mkdir(parents=True,exist_ok=True)
with output.open('w',newline='',encoding='utf-8') as f:
    writer=csv.writer(f);writer.writerow(FIELDS);writer.writerows(rows)
if not rows: raise SystemExit('No JustFloat frames received')
last=rows[-1]
print(f'frames={len(rows)} state={int(last[9])} fault={int(last[10])} result={int(last[11])} '
      f'pos={last[2]:.4f} vel={last[4]:.4f} torque={last[5]:.4f} csv={output}')
if args.command and stage<2: raise SystemExit('No motion command was accepted')
if failure: raise SystemExit(failure)
if not last[14] or last[12] or last[13]>=200:
    raise SystemExit('STOP not confirmed by fresh disabled feedback: inspect telemetry')
