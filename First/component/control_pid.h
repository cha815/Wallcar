/*
 * 作者：Codex
 * 文件：control_pid.h
 * 说明：PID配置、调用方持有的状态和计算接口。
 * 日期：2026-10-14
 */
#ifndef CONTROL_PID_H
#define CONTROL_PID_H
typedef struct {
    float kp,ki,kd;
    float minimum,maximum;
    float derivative_filter_s;
} control_pid_config_t;
typedef struct {
    control_pid_config_t config;
    float integral,previous_measurement,derivative;
    unsigned initialized;
} control_pid_t;
/* 初始化并复制配置；成功返回1，非法配置返回0。无动态内存，
 * pid及config由调用方管理；速度环输入rad/s、输出Nm。 */
int control_pid_init(control_pid_t *pid,const control_pid_config_t *config);
/* 清除动态状态，保留配置；NULL为空操作。 */
void control_pid_reset(control_pid_t *pid);
/* 计算并更新状态；dt为秒，合法范围(0,0.2]；非法输入返回NaN。
 * 需要回滚未提交的控制时，调用方应在状态副本上计算。 */
float control_pid_step(control_pid_t *pid,float target,float measurement,float dt);
#endif /* CONTROL_PID_H */
