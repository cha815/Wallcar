/*
 * 作者：Codex
 * 文件：test_motor_device.c
 * 说明：参数读回及驱动力矩能力门禁的主机回归。
 * 日期：2026-10-14
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "./device/motor.h"
//only forDEBUG：替代外部总线，保留真实设备参数校验。
static can_frame_t pending,sent;
static uint8_t available;
static uint32_t now;
static bsp_fdcan_result_t send_result;
static float drive_torque_limit=18;
static unsigned torque_limit_reads;
static float parameter_float(uint8_t i) {
    switch(i) { case 24:return -12.5f;case 25:return 12.5f;case 26:return -65;case 27:return 65;
    case 29:return 500;case 31:return 5;case 34:return -18;case 35:return 18;default:return 0; }
}
bsp_fdcan_result_t bsp_fdcan_send(const can_frame_t *f) {
    if(send_result!=BSP_FDCAN_OK) return send_result;
    sent=*f;pending=(can_frame_t){.id=0x601,.len=8,.fd=1,.timestamp_ms=now};
    if(f->id==0x601) {
        uint8_t i=f->data[1];uint32_t raw;
        float v=parameter_float(i);
        if(i==21) { v=drive_torque_limit;++torque_limit_reads; }
        memcpy(&raw,&v,4);
        if(i==11)raw=2;
        if(i==36)raw=1;
        if(i==37)raw=1000;
        if(i==56)raw=128;
        if(i==67)raw=0xffffffff;
        if(i==69)raw=0;
        pending.data[0]=1;pending.data[1]=i;memcpy(pending.data+2,&raw,4);pending.data[7]=0xff;
    } else { pending.id=1;pending.data[6]=0x20; }
    available=1;return BSP_FDCAN_OK;
}
bsp_fdcan_result_t bsp_fdcan_receive(can_frame_t *f) { if(!available)return BSP_FDCAN_EMPTY;*f=pending;available=0;return BSP_FDCAN_OK; }
bsp_fdcan_result_t bsp_fdcan_cancel(void) { return BSP_FDCAN_OK; }
bsp_fdcan_result_t bsp_fdcan_tx_pending(uint8_t *p) { *p=0;return BSP_FDCAN_OK; }
bsp_fdcan_result_t bsp_fdcan_get_stats(bsp_fdcan_stats_t *s) { memset(s,0,sizeof(*s));return BSP_FDCAN_OK; }
bsp_fdcan_result_t bsp_fdcan_recover(void) { return BSP_FDCAN_OK; }
int main(void) {
    motor_init(0);
    for(now=0;now<100;now+=2) motor_poll(now);
    assert(motor_configured()&&!motor_config_failed() && torque_limit_reads==1);
    motor_feedback_t f;motor_get_feedback(&f);assert(f.fault==0x2000&&!f.enabled);
    assert(motor_clear_fault()==BSP_FDCAN_OK && sent.id==0x101&&sent.data[7]==0xfb);
    /* A retry may explicitly clear a reported protection, never disable it. */
    pending=(can_frame_t){.id=1,.len=8,.fd=1,.timestamp_ms=now};
    pending.data[7]=2;available=1;motor_poll(now);
    motor_get_feedback(&f);assert(f.status==2 && !f.enabled);
    assert(motor_clear_fault()==BSP_FDCAN_OK && sent.fd && !sent.brs && sent.data[7]==0xfb);
    pending.data[7]=3;pending.data[6]=0;available=1;motor_poll(now);
    assert(motor_clear_fault()==BSP_FDCAN_INVALID); /* Enabled motor. */
    pending.data[7]=0;pending.data[6]=0x80;available=1;motor_poll(now);
    assert(motor_clear_fault()==BSP_FDCAN_INVALID); /* Unknown status bit. */
    motor_command_t command={.velocity=1,.kd=0.2f};
    assert(motor_send(&command)==BSP_FDCAN_OK && sent.id==0x201 && sent.fd && !sent.brs);
    const uint8_t expected[8]={0x7f,0xff,0x81,0xf0,0,0x0a,0x37,0xff};
    assert(memcmp(sent.data,expected,8)==0);
    send_result=BSP_FDCAN_BUSY;assert(motor_send(&command)==BSP_FDCAN_BUSY);
    send_result=BSP_FDCAN_BUS_OFF;assert(motor_send(&command)==BSP_FDCAN_BUS_OFF);
    send_result=BSP_FDCAN_OK;
    pending=(can_frame_t){.id=1,.len=8,.fd=1,.timestamp_ms=7};
    pending.data[5]=74;available=1;now=1000;motor_poll(now);
    motor_get_feedback(&f);assert(f.time_ms==7 && f.temperature==34);
    /* 驱动限制低于6Nm或非法时，配置不得通过；不自动改写驱动参数。 */
    drive_torque_limit=5.9f;motor_init(0);
    for(now=0;now<100;now+=2) motor_poll(now);
    assert(motor_config_failed() && !motor_configured() && motor_config_index()==21);
    drive_torque_limit=6;motor_init(0);
    for(now=0;now<100;now+=2) motor_poll(now);
    assert(motor_configured() && !motor_config_failed());
    drive_torque_limit=NAN;motor_init(0);
    for(now=0;now<100;now+=2) motor_poll(now);
    assert(motor_config_failed() && !motor_configured());
    puts("PASS motor device: driver-torque-limit6/parameter gates/explicit protection clear");
}
