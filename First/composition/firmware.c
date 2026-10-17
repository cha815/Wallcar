/* 作者：Codex；文件：firmware.c；说明：四电机实例注册、总线和测试接口装配。 */
#include <stddef.h>
#include "firmware.h"
#include "../bsp/bsp_fdcan.h"
#include "../bsp/bsp_uart_stream.h"
#include "../device/qzb_motor.h"
#include "../service/bench_link.h"

static can_service_t service;
static qzb_motor_t nodes[MOTOR_REGISTRY_CAPACITY];
static motor_registry_t registry;
static bench_link_t link;
static uint32_t wave_ms,info_ms;
static uint8_t wave_node,info_node,info_parameter;
static int command_results[MOTOR_REGISTRY_CAPACITY];
static const uint8_t indices[]={10,11,21,24,25,26,27,28,29,30,31,34,35,36,37,56,67,69};

/* 显式适配BSP与通信契约，不将HAL类型传给上层。 */
static can_result_t result(bsp_fdcan_result_t r) {
    switch(r) {
    case BSP_FDCAN_OK:return CAN_OK;case BSP_FDCAN_EMPTY:return CAN_EMPTY;
    case BSP_FDCAN_BUSY:return CAN_BUSY;case BSP_FDCAN_INVALID:return CAN_INVALID;
    case BSP_FDCAN_NOT_READY:return CAN_NOT_READY;case BSP_FDCAN_BUS_OFF:return CAN_BUS_OFF;
    default:return CAN_IO_ERROR;
    }
}
static can_result_t send(void *ctx,const can_frame_t *f) { (void)ctx;return result(bsp_fdcan_send(f)); }
static can_result_t receive(void *ctx,can_frame_t *f) { (void)ctx;return result(bsp_fdcan_receive(f)); }
static can_result_t health(void *ctx,can_health_t *out) {
    (void)ctx;bsp_fdcan_stats_t s;can_result_t r=result(bsp_fdcan_get_stats(&s));
    if(r==CAN_OK) *out=(can_health_t){.completed=s.tx_complete,
        .errors=s.protocol_error+s.hal_error+s.tx_error+s.tx_event_lost,
        .dropped=s.rx_drop+s.rx_hw_lost,.bus_off=s.bus_off,.tec=s.tx_error_counter,.rec=s.rx_error_counter,
        .lec=s.last_error_code,.dlec=s.data_error_code};
    return r;
}
const motor_registry_t *firmware_motors(void) { return &registry; }
int firmware_init(uint32_t now) {
    const can_port_t bus={.send=send,.receive=receive,.health=health};
    if(!bsp_uart_stream_init()||!can_service_init(&service,&bus)) return 0;
    motor_registry_init(&registry);
    for(unsigned i=0;i<MOTOR_REGISTRY_CAPACITY;++i) {
        if(!qzb_motor_init(&nodes[i],&service,(uint8_t)(i+1),now)) return 0;
        motor_port_t port=qzb_motor_port(&nodes[i]);
        if(!motor_registry_register(&registry,(uint8_t)(i+1),&port)) return 0;
        command_results[i]=0;
    }
    bench_link_init(&link,bsp_uart_stream_read,bsp_uart_stream_write);
    wave_ms=info_ms=now;wave_node=info_node=info_parameter=0;return 1;
}
/* 主循环先路由反馈，再刷新设备事务，测试入口调用与产品上层相同的公开接口。 */
void firmware_tick(uint32_t now) {
    can_service_poll(&service);
    for(unsigned i=0;i<MOTOR_REGISTRY_CAPACITY;++i) qzb_motor_tick(&nodes[i],now);
    bench_command_t c;
    if(bench_link_read(&link,&c)&&c.kind!=BENCH_INVALID) {
        const motor_port_t *motor=motor_registry_find(&registry,c.id);int r=-1;
        if(c.kind==BENCH_ENABLE) r=motor_port_enable(motor,1);
        else if(c.kind==BENCH_DISABLE) r=motor_port_enable(motor,0);
        else if(c.kind==BENCH_VEL) r=motor_port_velocity(motor,c.velocity,c.kd);
        if(c.id>=1&&c.id<=MOTOR_REGISTRY_CAPACITY) command_results[c.id-1]=r;
    }
    if((uint32_t)(now-wave_ms)<10) return;
    float v[15]={0};
    if((uint32_t)(now-info_ms)>=100) {
        qzb_motor_t *m=&nodes[info_node];can_health_t h={0};(void)health(NULL,&h);
        uint32_t raw=m->parameter_values[info_parameter];
        v[0]=-1;v[1]=m->snapshot.id;v[2]=indices[info_parameter];v[3]=(float)(raw&0xffffu);v[4]=(float)(raw>>16);
        v[5]=m->snapshot.parameter_count;v[6]=h.completed;v[7]=h.errors;v[8]=h.tec;v[9]=h.rec;
        v[10]=h.dropped;v[11]=h.bus_off;v[12]=m->snapshot.parameter_index;v[13]=m->snapshot.failed;v[14]=h.lec+16*h.dlec;
        if(bench_link_write(&link,v)>0) {
            info_ms=wave_ms=now;
            if(++info_node==MOTOR_REGISTRY_CAPACITY) { info_node=0;if(++info_parameter==18) info_parameter=0; }
        }
        return;
    }
    unsigned i=wave_node;motor_snapshot_t s;
    (void)motor_port_read(motor_registry_find(&registry,(uint8_t)(i+1)),&s);
    motor_feedback_t *f=&s.feedback;
    v[0]=(float)now*0.001f;v[1]=s.id;v[2]=s.ready;v[3]=f->enabled;
    v[4]=nodes[i].streaming?nodes[i].command.velocity:0;v[5]=f->velocity;v[6]=f->position;
    v[7]=f->torque;v[8]=f->temperature;v[9]=f->fault;
    v[10]=f->count?(float)(uint32_t)(now-f->time_ms):1000000;
    v[11]=command_results[i];v[12]=nodes[i].streaming;v[13]=(float)f->count;v[14]=s.failed;
    if(bench_link_write(&link,v)>0) { wave_node=(uint8_t)((wave_node+1u)%MOTOR_REGISTRY_CAPACITY);wave_ms=now; }
}
