/*
 * 作者：Codex
 * 文件：test_motor_tune.c
 * 说明：主机验证调参状态机、PID定速与启停接管。
 * 日期：2026-10-14
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "./app/motor_tune.h"
#include "./device/motor.h"
#include "./service/tuning_link.h"
//only forDEBUG：仅替代外部设备收发，PID和应用状态机执行真实实现。
static motor_feedback_t f={.count=1};
static motor_command_t last;
static int enable_requests,disable_requests,cancel_requests;
static bsp_fdcan_result_t cancel_result;
static bsp_fdcan_result_t send_result;
static uint32_t complete;
static int pending_cancel,stale_feedback;
static int clear_requests,command_queued;
static tune_command_t queued_command;
static float demo_target,demo_feedback;
void motor_poll(uint32_t now) { if(!stale_feedback) { f.time_ms=now;++f.count; } }
int motor_configured(void) { return 1; }
int motor_config_failed(void) { return 0; }
void motor_init(uint32_t now) { (void)now; }
bsp_fdcan_result_t motor_retry_config(uint32_t now) { (void)now;return BSP_FDCAN_OK; }
uint8_t motor_config_index(void) { return 0; }
uint32_t motor_config_raw(void) { return 0; }
float motor_diagnostic_value(void) { return 0; }
bsp_fdcan_result_t motor_read_diagnostic(uint8_t i) { (void)i;return BSP_FDCAN_OK; }
bsp_fdcan_result_t motor_clear_fault(void) { ++clear_requests;return BSP_FDCAN_OK; }
void motor_get_feedback(motor_feedback_t *out) { *out=f; }
bsp_fdcan_result_t motor_send(const motor_command_t *c) { if(send_result!=BSP_FDCAN_OK) return send_result;last=*c;++complete;return BSP_FDCAN_OK; }
bsp_fdcan_result_t motor_set_enabled(uint8_t v) { if(v) ++enable_requests;else ++disable_requests;++complete;return BSP_FDCAN_OK; }
bsp_fdcan_result_t motor_cancel(void) { ++cancel_requests;return cancel_result; }
bsp_fdcan_result_t motor_tx_pending(uint8_t *p) { *p=(uint8_t)pending_cancel;return BSP_FDCAN_OK; }
bsp_fdcan_result_t motor_bus_stats(bsp_fdcan_stats_t *s) { memset(s,0,sizeof(*s));s->tx_complete=complete;return BSP_FDCAN_OK; }
int tuning_link_command(tune_command_t *c) { if(!command_queued) return 0;*c=queued_command;command_queued=0;return 1; }
int tuning_link_waveform(const float *v) { (void)v;return 1; }
int tuning_link_speed_waveform(float target,float measured) { demo_target=target;demo_feedback=measured;return 1; }
int main(void) {
    motor_tune_init(0);motor_tune_tick(0,0);
    assert(motor_tune_state()==MOTOR_TUNE_OFF);
    assert(!motor_tune_velocity(1,1,0));
    assert(motor_tune_arm(1));motor_tune_tick(2,0);
    assert(enable_requests==1&&last.kp==0&&last.velocity==0);
    assert(motor_tune_state()==MOTOR_TUNE_ENABLING);
    f.enabled=1;motor_tune_tick(4,0);assert(motor_tune_state()==MOTOR_TUNE_READY);
    assert(motor_tune_velocity(1,1,5));motor_tune_tick(6,0);
    assert(last.velocity<=0.021f&&last.kp==0&&last.kd==1);
    assert(!motor_tune_velocity(6,1,6));
    assert(!motor_tune_position(0.6f,5,1,6));
    motor_tune_tick(506,0);motor_tune_tick(508,0);
    assert(cancel_requests>0&&disable_requests>0);
    f.enabled=0;motor_tune_tick(510,0);assert(motor_tune_state()==MOTOR_TUNE_FAULT);
    assert(!motor_tune_arm(511));
    f.enabled=0;f.velocity=0;motor_tune_init(0);motor_tune_tick(0,0);
    assert(motor_tune_arm(1));motor_tune_tick(2,0);f.enabled=1;motor_tune_tick(4,0);
    assert(motor_tune_velocity(0,2,5));f.velocity=7;motor_tune_tick(6,0);
    assert(last.velocity<=5&&last.velocity>=-5);
    f.velocity=0;motor_tune_stop(7);pending_cancel=1;
    motor_tune_tick(8,0);f.enabled=0;motor_tune_tick(10,0);
    stale_feedback=1;pending_cancel=0;motor_tune_tick(12,0);
    assert(motor_tune_state()==MOTOR_TUNE_STOPPING);
    stale_feedback=0;motor_tune_tick(14,0);assert(motor_tune_state()==MOTOR_TUNE_OFF);
    f.fault=f.status=2;pending_cancel=1;motor_tune_init(100);motor_tune_tick(100,0);
    queued_command=(tune_command_t){.kind=TUNE_CLEAR};command_queued=1;
    motor_tune_tick(102,0);assert(!clear_requests); /* Stop still pending. */
    pending_cancel=0;motor_tune_tick(104,0);motor_tune_tick(106,0);
    assert(motor_tune_state()==MOTOR_TUNE_FAULT);
    queued_command=(tune_command_t){.kind=TUNE_CLEAR};command_queued=1;
    int previous_enables=enable_requests;
    motor_tune_tick(108,0);assert(clear_requests==1 && motor_tune_state()==MOTOR_TUNE_BOOT);
    motor_tune_tick(110,0);assert(motor_tune_state()==MOTOR_TUNE_BOOT);
    f.fault=f.status=0;motor_tune_tick(112,0);
    assert(motor_tune_state()==MOTOR_TUNE_OFF && enable_requests==previous_enables);
    f=(motor_feedback_t){.count=1,.temperature=34,.position=12.5f};
    motor_tune_pid_demo_init(6000);motor_tune_tick(6000,0);
    /* 使能前的控制报文同样必须禁用驱动增益、置零力矩。 */
    assert(last.kp==0 && last.kd==0 && last.torque==0);
    f.enabled=1;motor_tune_tick(6002,0);
    for(uint32_t pid_ms=6004;pid_ms<=30004;pid_ms+=2) {
        f.velocity=-2;motor_tune_tick(pid_ms,0);
        assert(motor_tune_state()==MOTOR_TUNE_VELOCITY);
        assert(last.kp==0 && last.kd==0 && last.velocity==0);
        assert(last.torque<=5.99f && last.torque>=-5.99f);
        motor_command_t quantized;
        assert(motor_codec_quantize(&last,&quantized) && fabsf(quantized.torque)<=6.0f);
        /* 连续运行超过原来的两个周期，目标仍固定5rad/s。 */
        if((pid_ms-6000)%10==0) assert(demo_target==5 && demo_feedback==-2);
    }
    assert(last.torque==5.99f);
    /* 速度明显超过目标时应产生负纠正力矩。 */
    f.velocity=40;motor_tune_tick(30006,0);assert(last.torque<0);
    motor_tune_stop(30007);motor_tune_tick(30008,0);f.enabled=0;motor_tune_tick(30010,0);
    previous_enables=enable_requests;motor_tune_tick(30020,0);
    assert(motor_tune_state()==MOTOR_TUNE_OFF && enable_requests==previous_enables && demo_target==0);
    f.velocity=0;
    f.temperature=142;motor_tune_init(200);motor_tune_tick(200,0);
    assert(!motor_tune_arm(201)); /* Fault=0 does not prove a hot motor safe. */
    f.temperature=34;motor_tune_init(210);motor_tune_tick(210,0);
    assert(motor_tune_arm(211));motor_tune_tick(212,0);
    f.enabled=1;motor_tune_tick(214,0);assert(motor_tune_state()==MOTOR_TUNE_READY);
    f.temperature=80;motor_tune_tick(216,0);
    assert(motor_tune_state()==MOTOR_TUNE_STOPPING);
    /* Standalone VOFA demo: one automatic start, no host heartbeat needed. */
    f=(motor_feedback_t){.count=1,.temperature=34,.position=12.5f};
    previous_enables=enable_requests;
    motor_tune_speed_demo_init(1000);motor_tune_tick(1000,0);
    assert(motor_tune_state()==MOTOR_TUNE_ENABLING && enable_requests==previous_enables+1);
    f.enabled=1;motor_tune_tick(1002,0);
    for(uint32_t demo_ms=1004;demo_ms<=2600;demo_ms+=2) {
        f.velocity=last.velocity;motor_tune_tick(demo_ms,0);
    }
    assert(motor_tune_state()==MOTOR_TUNE_VELOCITY);
    assert(last.velocity==10 && last.kd==1 && last.kp==0 && last.torque==0);
    assert(demo_target>9.98f && demo_target<=10 && demo_feedback>9.9f);
    motor_tune_stop(2601);motor_tune_tick(2602,0);f.enabled=0;motor_tune_tick(2604,0);
    previous_enables=enable_requests;
    motor_tune_tick(2704,0);
    assert(motor_tune_state()==MOTOR_TUNE_OFF && enable_requests==previous_enables && demo_target==0);
    motor_tune_speed_demo_init(3000);motor_tune_tick(3000,0);
    f.enabled=1;motor_tune_tick(3002,0);previous_enables=enable_requests;
    f.fault=f.status=2;motor_tune_tick(3004,0);f.enabled=0;motor_tune_tick(3006,0);
    assert(motor_tune_state()==MOTOR_TUNE_FAULT);
    queued_command=(tune_command_t){.kind=TUNE_CLEAR};command_queued=1;motor_tune_tick(3008,0);
    f.fault=f.status=0;motor_tune_tick(3010,0);motor_tune_tick(3020,0);
    assert(motor_tune_state()==MOTOR_TUNE_OFF && enable_requests==previous_enables);
    f=(motor_feedback_t){.count=1,.temperature=34,.enabled=1,.velocity=9};
    motor_tune_pid_demo_init(40000);motor_tune_tick(40000,0);
    assert(motor_tune_state()==MOTOR_TUNE_STOPPING);
    f.enabled=0;motor_tune_tick(40002,0);previous_enables=enable_requests;
    motor_tune_tick(40004,0);assert(enable_requests==previous_enables);
    f.velocity=0;motor_tune_tick(40006,0);
    assert(motor_tune_state()==MOTOR_TUNE_ENABLING && enable_requests==previous_enables+1);
    f=(motor_feedback_t){.count=1,.temperature=34,.fault=0x2000,.status=0x2000};
    int previous_clears=clear_requests;
    motor_tune_pid_demo_init(50000);motor_tune_tick(50000,0);
    assert(clear_requests==previous_clears+1 && motor_tune_state()==MOTOR_TUNE_BOOT);
    f.fault=f.status=0;motor_tune_tick(50002,0);
    assert(motor_tune_state()==MOTOR_TUNE_ENABLING);
    /* 启动接管中取消失败，随后停车确认并CLEAR，仍不得自动重启。 */
    f=(motor_feedback_t){.count=1,.temperature=34,.enabled=1};
    cancel_result=BSP_FDCAN_HAL_ERROR;
    motor_tune_pid_demo_init(60000);motor_tune_tick(60000,0);
    cancel_result=BSP_FDCAN_OK;f.enabled=0;
    motor_tune_tick(60002,0);motor_tune_tick(60004,0);
    assert(motor_tune_state()==MOTOR_TUNE_FAULT);
    queued_command=(tune_command_t){.kind=TUNE_CLEAR};command_queued=1;
    previous_enables=enable_requests;
    motor_tune_tick(60006,0);motor_tune_tick(60008,0);
    assert(motor_tune_state()==MOTOR_TUNE_OFF && enable_requests==previous_enables);
    /* 主控PID模式不得经公开MIT接口引入驱动增益。 */
    f=(motor_feedback_t){.count=1,.temperature=34};
    motor_tune_pid_demo_init(70000);motor_tune_tick(70000,0);
    f.enabled=1;motor_tune_tick(70002,0);
    assert(!motor_tune_velocity(1,1,70002));
    assert(!motor_tune_position(0.1f,1,1,70002));
    /* BUSY期间测量改变，未提交的积分不得污染下一次成功输出。 */
    f=(motor_feedback_t){.count=1,.temperature=34};
    motor_tune_pid_demo_init(80000);motor_tune_tick(80000,0);
    f.enabled=1;motor_tune_tick(80002,0);
    f.velocity=4.9f;motor_tune_tick(80004,0);
    send_result=BSP_FDCAN_BUSY;f.velocity=4;motor_tune_tick(80006,0);
    f.velocity=6;motor_tune_tick(80008,0);
    send_result=BSP_FDCAN_OK;f.velocity=5;motor_tune_tick(80010,0);
    assert(fabsf(last.torque-0.00002f)<0.0000001f);
    /* 重复反馈只重发原控制，不得再次积分。 */
    f.velocity=4;motor_tune_tick(80012,0);float previous_torque=last.torque;
    stale_feedback=1;motor_tune_tick(80014,0);motor_tune_tick(80016,0);
    assert(last.torque==previous_torque);
    /* 连续发送忙100ms必须退出定速控制并锁存停车。 */
    stale_feedback=0;send_result=BSP_FDCAN_BUSY;
    for(uint32_t busy_ms=80018;busy_ms<=80118;busy_ms+=2) motor_tune_tick(busy_ms,0);
    assert(motor_tune_state()==MOTOR_TUNE_STOPPING);
    send_result=BSP_FDCAN_OK;
    /* 反馈力矩允许新发送范围的正常波动；超过关联保护阈值必须停车。 */
    f=(motor_feedback_t){.count=1,.temperature=34};
    motor_tune_pid_demo_init(90000);motor_tune_tick(90000,0);
    f.enabled=1;f.torque=8;motor_tune_tick(90002,0);motor_tune_tick(90004,0);
    assert(motor_tune_state()==MOTOR_TUNE_VELOCITY);
    f.torque=12.1f;motor_tune_tick(90006,0);
    assert(motor_tune_state()==MOTOR_TUNE_STOPPING);
    puts("PASS motor app: fixed5/PID6Nm/quantized-budget/feedback-protection/lifecycle/BUSY-samples");
}
