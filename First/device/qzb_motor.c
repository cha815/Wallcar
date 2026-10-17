/* 作者：Codex；文件：qzb_motor.c；说明：节点独立配置事务、反馈与MIT接口。 */
#include <math.h>
#include <string.h>
#include "qzb_motor.h"
#include "../component/motor_codec.h"

static const uint8_t parameters[]={10,11,21,24,25,26,27,28,29,30,31,34,35,36,37,56,67,69};

/* 实际读回必须与编码量程一致；不以JSON替代现场。 */
static int parameter_valid(qzb_motor_t *m,uint8_t index,uint32_t raw) {
    float value;memcpy(&value,&raw,4);
    switch(index) {
    case 11:return raw==2;
    case 21:return isfinite(value)&&value>0;
    case 36:return raw==m->snapshot.id;
    case 67: {
        uint32_t id=(raw&0x80000000u)?m->snapshot.id:raw;
        if(!can_service_claim_feedback(m->bus,m,id)) return 0;
        m->snapshot.feedback_id=id;return 1;
    }
    case 69:return raw==0;
    default:break;
    }
    float *endpoint;
    switch(index) {
    case 24:endpoint=&m->snapshot.limits.position_min;break;case 25:endpoint=&m->snapshot.limits.position_max;break;
    case 26:endpoint=&m->snapshot.limits.velocity_min;break;case 27:endpoint=&m->snapshot.limits.velocity_max;break;
    case 28:endpoint=&m->snapshot.limits.kp_min;break;case 29:endpoint=&m->snapshot.limits.kp_max;break;
    case 30:endpoint=&m->snapshot.limits.kd_min;break;case 31:endpoint=&m->snapshot.limits.kd_max;break;
    case 34:endpoint=&m->snapshot.limits.torque_min;break;case 35:endpoint=&m->snapshot.limits.torque_max;break;
    default:return 1;
    }
    if(!isfinite(value)) return 0;
    *endpoint=value;return 1;
}
static int send(qzb_motor_t *m,uint32_t base,const uint8_t data[8]) {
    can_frame_t f={.id=base+m->snapshot.id,.len=8,.fd=1};
    memcpy(f.data,data,8);
    can_result_t r=can_service_send(m->bus,&f);
    return r==CAN_OK ? 1 : (r==CAN_BUSY ? 0 : -1);
}
static int state_command(qzb_motor_t *m,uint8_t command) {
    uint8_t data[8];memset(data,0xff,8);data[7]=command;return send(m,0x100,data);
}
int qzb_motor_init(qzb_motor_t *m,can_service_t *bus,uint8_t id,uint32_t now) {
    if(!m||!bus||!id) return 0;
    memset(m,0,sizeof(*m));m->bus=bus;m->snapshot.id=id;m->query_ms=now-20;
    m->snapshot.feedback_id=id;
    return can_service_subscribe(bus,m,qzb_motor_receive);
}
/* 只消费本节点反馈/参数，时间戳是BSP收包时刻，不是处理时刻。 */
void qzb_motor_receive(void *ctx,const can_frame_t *f) {
    qzb_motor_t *m=ctx;
    if(!f||!can_frame_valid(f)||f->extended||f->len!=8) return;
    if(m->snapshot.ready&&f->id==m->snapshot.feedback_id) {
        motor_feedback_t *b=&m->snapshot.feedback;
        if(motor_codec_feedback_limits(f->data,&m->snapshot.limits,b)) { b->time_ms=f->timestamp_ms;++b->count; }
    }
    if(!m->waiting||f->id!=0x600u+m->snapshot.id||f->data[0]!=m->snapshot.id||
       f->data[1]!=parameters[m->cursor]||f->data[6]!=0||f->data[7]!=0xff||
       (int32_t)(f->timestamp_ms-m->sent_ms)<0) return;
    uint32_t raw=(uint32_t)f->data[2]|((uint32_t)f->data[3]<<8)|
        ((uint32_t)f->data[4]<<16)|((uint32_t)f->data[5]<<24);
    m->snapshot.parameter_raw=raw;++m->snapshot.parameter_count;
    m->parameter_values[m->cursor]=raw;
    m->waiting=0;m->retries=0;
    if(!parameter_valid(m,parameters[m->cursor],raw)) { m->snapshot.failed=1;return; }
    if(++m->cursor==sizeof(parameters)) {
        if(motor_codec_limits_valid(&m->snapshot.limits)) m->snapshot.ready=1;
        else m->snapshot.failed=1;
    }
}
/* 参数读回有界重试；配置失败仍查询状态，供上层读取设备事实。 */
void qzb_motor_tick(qzb_motor_t *m,uint32_t now) {
    m->now_ms=now;
    if(m->snapshot.ready&&!can_service_claim_feedback(m->bus,m,m->snapshot.feedback_id)) {
        /* 没有节点ID字段，重复反馈ID无法区分来源；两个实例都报告配置失败。 */
        m->snapshot.ready=0;m->snapshot.failed=1;m->streaming=0;
    }
    if(!m->snapshot.ready&&!m->snapshot.failed) {
        if(m->waiting&&(uint32_t)(now-m->sent_ms)>=100) {
            m->waiting=0;if(++m->retries>=3) m->snapshot.failed=1;
        }
        if(!m->waiting&&!m->snapshot.failed) {
            uint8_t d[8]={0x67,parameters[m->cursor],0,0,0,0,0x04,0x76};
            m->snapshot.parameter_index=parameters[m->cursor];
            int r=send(m,0x600,d);
            if(r>0) { m->waiting=1;m->sent_ms=now; }
            else if(r<0) m->snapshot.failed=1;
        }
    }
    if((uint32_t)(now-m->query_ms)>=20) {
        if(state_command(m,0xfa)>0) m->query_ms=now;
    }
    if(m->streaming&&m->snapshot.feedback.enabled&&!m->snapshot.feedback.fault&&
       (uint32_t)(now-m->refresh_ms)>=2) {
        uint8_t data[8];
        if(motor_codec_pack_limits(&m->command,&m->snapshot.limits,data)&&send(m,0x200,data)>0) m->refresh_ms=now;
    }
}
static void read_snapshot(void *ctx,motor_snapshot_t *out) { if(out) *out=((qzb_motor_t *)ctx)->snapshot; }
static int enable(void *ctx,uint8_t value) {
    qzb_motor_t *m=ctx;
    if(value>1||(value&&(!m->snapshot.ready||m->snapshot.failed))) return -1;
    if(value) {
        /* 使能前覆盖驱动上次缓存目标，新一轮只执行上层重新设定的命令。 */
        const motor_command_t neutral={0};uint8_t data[8];
        if(!motor_codec_pack_limits(&neutral,&m->snapshot.limits,data)) return -1;
        int queued=send(m,0x200,data);
        if(queued<=0) return queued;
        m->streaming=0;
    }
    int r=state_command(m,value?0xfc:0xfd);
    if(r>0&&!value) m->streaming=0;
    return r;
}
/* 仅协议范围和设备状态校验；上层直接设MIT目标，闭环在电机驱动执行。 */
static int command(void *ctx,const motor_command_t *requested) {
    qzb_motor_t *m=ctx;
    motor_feedback_t *f=&m->snapshot.feedback;
    if(!m->snapshot.ready||m->snapshot.failed||!f->count||!f->enabled||f->fault) return -1;
    uint8_t d[8];
    if(!motor_codec_pack_limits(requested,&m->snapshot.limits,d)) return -1;
    int r=send(m,0x200,d);
    if(r>0) { m->command=*requested;m->streaming=1;m->refresh_ms=m->now_ms; }
    return r;
}
motor_port_t qzb_motor_port(qzb_motor_t *m) {
    return (motor_port_t){.ctx=m,.read=read_snapshot,.enable=enable,.command=command};
}
