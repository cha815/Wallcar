/*
 * 作者：Codex
 * 文件：motor.c
 * 说明：ID1协议适配、实际参数校验和反馈快照。
 * 日期：2026-10-14
 */
#include <math.h>
#include <string.h>

#include "./motor.h"
#include "./motor_ro.h"
#include "./component/mw_canQZB.h"

static const uint8_t indices[]={10,11,21,24,25,26,27,28,29,30,31,34,35,36,37,56,67,69};
static motor_feedback_t feedback;
static uint32_t sent,last_query;
static uint32_t last_raw;
static uint32_t diagnostic_raw;
static uint8_t diagnostic_index;
static uint8_t index_no,waiting,failures,configured,failed;
/* Adapt the reference Motor Wire framework to CAN1/ID1. All protocol work
 * runs in main, after the BSP queue; no HAL or ISR parsing leaks upwards. */
static uint32_t received_ms;
static bsp_fdcan_result_t tx_result;
static int qzb_tx(uint8_t bus,const mwQZB_frame_t *wire) {
    if(bus!=0 || !wire) { tx_result=BSP_FDCAN_INVALID;return -1; }
    can_frame_t frame={.id=wire->id,.len=wire->len,.extended=wire->extended,
        .fd=wire->fd,.brs=wire->brs};
    memcpy(frame.data,wire->data,sizeof(frame.data));
    tx_result=bsp_fdcan_send(&frame);
    return tx_result==BSP_FDCAN_OK ? 1 : (tx_result==BSP_FDCAN_BUSY ? 0 : -1);
}
static uint32_t qzb_millis(void) { return received_ms; }
static bsp_fdcan_result_t qzb_send(uint16_t base,const uint8_t data[8]) {
    (void)mwQZB_send(0,base,1,data);
    return tx_result;
}
static void qzb_receive(const can_frame_t *frame) {
    mwQZB_frame_t wire={.id=frame->id,.len=frame->len,.extended=frame->extended,
        .fd=frame->fd,.brs=frame->brs};
    memcpy(wire.data,frame->data,sizeof(wire.data));
    received_ms=frame->timestamp_ms;
    mwQZB_recv(0,&wire);
    mwQZB_feedback_t value;
    if(frame->id==1 && mwQZB_get_feedback(0,1,&value)) {
        feedback=(motor_feedback_t){.position=value.pos,.velocity=value.vel,.torque=value.torque,
            .temperature=value.temperature,.status=value.status,.fault=(uint16_t)(value.status&0xfffeu),
            .enabled=value.enabled,.count=value.count,.time_ms=value.time};
    }
}
static int parameter_valid(uint8_t index,uint32_t raw) {
    float value; memcpy(&value,&raw,4);
    /* 本轮MC02发送范围为6Nm；驱动必须实际允许该范围，不能只信JSON。
     * 不满足时拒绝配置，不自动改写驱动力矩或保护参数。 */
    if(index==21) return isfinite(value) && value>=6.0f;
    switch(index) {
    case 11:return raw==2;
    case 36:return raw==1;
    case 37:return raw>0 && raw<=1000;
    case 56:return (raw&128u)!=0;
    case 67:return raw==0xffffffffu;
    case 69:return raw==0;
    default:break;
    }
    float expected;
    switch(index) {
    case 24:expected=-12.5f;break;case 25:expected=12.5f;break;
    case 26:expected=-65;break;case 27:expected=65;break;
    case 28:case 30:expected=0;break;case 29:expected=500;break;case 31:expected=5;break;
    case 34:expected=-18;break;case 35:expected=18;break;
    default:return 1; /* Firmware identity is recorded; FA/individual support is observed. */
    }
    return isfinite(value) && fabsf(value-expected)<0.0001f;
}
/* main启动期调用，初始化协议状态并开始逐项只读校验。 */
void motor_init(uint32_t now) {
    const mwQZB_ops_t ops={.tx=qzb_tx,.millis=qzb_millis};
    mwQZB_init(&ops);mwQZB_bus_register(0);
    received_ms=now;tx_result=BSP_FDCAN_NOT_READY;
    memset(&feedback,0,sizeof(feedback));index_no=waiting=failures=configured=failed=0;
    sent=now;last_query=now-20u;last_raw=0;diagnostic_index=0;diagnostic_raw=0;
}
/* 主循环消费BSP队列并推进只读事务，ISR不解析电机协议。 */
void motor_poll(uint32_t now) {
    can_frame_t f;
    for(unsigned n=0;n<32 && bsp_fdcan_receive(&f)==BSP_FDCAN_OK;++n) {
        if(f.extended || f.len!=8 || !can_frame_valid(&f)) continue;
        qzb_receive(&f);
        uint32_t raw;
        if(diagnostic_index && motor_ro_parameter_response(&f,diagnostic_index,&raw)) diagnostic_raw=raw;
        if(waiting && !configured && !failed &&
           (int32_t)(f.timestamp_ms-sent)>=0 && motor_ro_parameter_response(&f,indices[index_no],&raw)) {
            last_raw=raw;
            if(!parameter_valid(indices[index_no],raw)) { failed=1;waiting=0;continue; }
            waiting=0;failures=0;
            if(++index_no==sizeof(indices)) configured=1;
        }
    }
    if(failed) return;
    if(!configured) {
        if(waiting && (uint32_t)(now-sent)>=100) { waiting=0;if(++failures>=3) { failed=1;return; } }
        if(!waiting) {
            bsp_fdcan_result_t r=motor_ro_read_parameter(indices[index_no],0);
            if(r==BSP_FDCAN_OK) { waiting=1;sent=now; }
            else if(r!=BSP_FDCAN_BUSY) failed=1;
        }
    } else if(!feedback.enabled && (uint32_t)(now-last_query)>=20) {
        if(motor_ro_query_status(0,0)==BSP_FDCAN_OK) last_query=now;
    }
}
int motor_configured(void) { return configured; }
int motor_config_failed(void) { return failed; }
uint8_t motor_config_index(void) { return diagnostic_index ? diagnostic_index : (index_no<sizeof(indices)?indices[index_no]:0); }
uint32_t motor_config_raw(void) { return diagnostic_index ? diagnostic_raw : last_raw; }
float motor_diagnostic_value(void) { float value;memcpy(&value,&diagnostic_raw,4);return value; }
bsp_fdcan_result_t motor_retry_config(uint32_t now) {
    bsp_fdcan_stats_t s;
    bsp_fdcan_result_t r=bsp_fdcan_get_stats(&s);
    if(r!=BSP_FDCAN_OK) return r;
    if(s.bus_off) {
        r=bsp_fdcan_recover();
        if(r!=BSP_FDCAN_OK) return r;
    }
    motor_init(now);
    return BSP_FDCAN_OK;
}
bsp_fdcan_result_t motor_read_diagnostic(uint8_t index) {
    if(!configured || feedback.enabled || (index!=49 && index!=50 && index!=66 && index!=71)) return BSP_FDCAN_INVALID;
    diagnostic_index=index;diagnostic_raw=0;
    return motor_ro_read_parameter(index,0);
}
void motor_get_feedback(motor_feedback_t *out) { if(out) *out=feedback; }
/* 仅实际参数校验通过后发送；返回OK代表入队，不代表电机已执行。 */
bsp_fdcan_result_t motor_send(const motor_command_t *command) {
    if(!configured || failed) return BSP_FDCAN_NOT_READY;
    uint8_t data[8];
    if(!motor_codec_pack(command,data)) return BSP_FDCAN_INVALID;
    return qzb_send(MWQZB_CONTROL_ID_BASE,data);
}
bsp_fdcan_result_t motor_set_enabled(uint8_t enabled) {
    if(enabled>1 || (enabled && !configured)) return BSP_FDCAN_INVALID;
    uint8_t data[8];
    mwQZB_pack_state(data,enabled ? MWQZB_STATE_ENABLE : MWQZB_STATE_DISABLE);
    return qzb_send(MWQZB_STATE_ID_BASE,data);
}
bsp_fdcan_result_t motor_clear_fault(void) {
    uint16_t fault=(uint16_t)(feedback.status&0xfffeu);
    if(!configured || feedback.enabled || !fault || (fault & ~MWQZB_FAULT_MASK)) return BSP_FDCAN_INVALID;
    uint8_t data[8];
    mwQZB_pack_state(data,MWQZB_STATE_CLEAR);
    return qzb_send(MWQZB_STATE_ID_BASE,data);
}
bsp_fdcan_result_t motor_cancel(void) { return bsp_fdcan_cancel(); }
bsp_fdcan_result_t motor_tx_pending(uint8_t *p) { return bsp_fdcan_tx_pending(p); }
bsp_fdcan_result_t motor_bus_stats(bsp_fdcan_stats_t *s) { return bsp_fdcan_get_stats(s); }
