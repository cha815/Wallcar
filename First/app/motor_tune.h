/*
 * 作者：Codex
 * 文件：motor_tune.h
 * 说明：主循环电机调参、启停及自主PID试验接口。
 * 日期：2026-10-14
 */
#ifndef MOTOR_TUNE_H
#define MOTOR_TUNE_H
#include <stdint.h>
/* State numbers are the VOFA channel-9 values. */
typedef enum { MOTOR_TUNE_BOOT=0,MOTOR_TUNE_OFF=1,MOTOR_TUNE_ENABLING=2,
    MOTOR_TUNE_READY=3,MOTOR_TUNE_VELOCITY=4,MOTOR_TUNE_POSITION=5,
    MOTOR_TUNE_STOPPING=6,MOTOR_TUNE_FAULT=7 } motor_tune_state_t;
/* 初始化交互模式；now_ms为平台单调毫秒时钟。 */
void motor_tune_init(uint32_t now_ms);
/* Dedicated bench image: start ID1 once after valid feedback, ramp to
 * 10 rad/s with Kd=1, publish two JustFloat channels, no host heartbeat.
 * STOP/fault latches the automatic start until the next board reset. */
void motor_tune_speed_demo_init(uint32_t now_ms);
/* Main-controller speed PID -> torque only; fixed 5 rad/s target,
 * UART7 target/feedback, one autonomous start per reset. */
void motor_tune_pid_demo_init(uint32_t now_ms);
/* 周期更新；入参为毫秒时钟和UART累计错误计数。 */
void motor_tune_tick(uint32_t now_ms,uint32_t uart_errors);
/* API for another application: same main context, units rad/rad/s. */
/* 请求使能；1表示等待反馈，0表示门禁不满足。 */
int motor_tune_arm(uint32_t now_ms);
/* 设置速度目标和驱动阻尼；成功1，非法参数或状态0。 */
int motor_tune_velocity(float velocity,float kd,uint32_t now_ms);
/* 设置相对ARM锚点的位置及驱动增益；成功1，拒绝0。 */
int motor_tune_position(float relative_position,float kp,float kd,uint32_t now_ms);
/* 请求异步停车，提交不能视为停机确认。 */
void motor_tune_stop(uint32_t now_ms);
/* 与tick在同一主循环上下文读取状态快照。 */
motor_tune_state_t motor_tune_state(void);
#endif /* MOTOR_TUNE_H */
