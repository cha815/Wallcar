/*
 * 作者：Codex
 * 文件：control_pid.c
 * 说明：平台无关PID，条件积分抗饱和、测量值微分及一阶滤波。
 * 日期：2026-10-14
 */
#include <math.h>
#include <string.h>

#include "./control_pid.h"

/* 校验并复制配置；pid和config由调用方持有，不分配动态内存。 */
int control_pid_init(control_pid_t *pid,const control_pid_config_t *config) {
    if(!pid||!config||!isfinite(config->kp)||!isfinite(config->ki)||!isfinite(config->kd)||
       !isfinite(config->minimum)||!isfinite(config->maximum)||!isfinite(config->derivative_filter_s)||
       config->kp<0||config->ki<0||config->kd<0||config->minimum>=config->maximum||config->derivative_filter_s<0) return 0;
    memset(pid,0,sizeof(*pid));pid->config=*config;return 1;
}
/* 开始新试验或停车时清除历史积分和微分，保留增益及限幅配置。 */
void control_pid_reset(control_pid_t *pid) { if(pid) { pid->integral=pid->derivative=0;pid->initialized=0; } }
/* 限制积分和最终输出，避免不可达目标导致持续累积。 */
static float clamp(float value,float minimum,float maximum) {
    return value<minimum ? minimum : (value>maximum ? maximum : value);
}
/* 消费一组新测量；dt单位为秒，按实际测量时间戳间隔计算。 */
float control_pid_step(control_pid_t *pid,float target,float measurement,float dt) {
    if(!pid||!isfinite(target)||!isfinite(measurement)||!isfinite(dt)||dt<=0||dt>0.2f) return NAN;
    float error=target-measurement;
    /* 测量值微分避免目标阶跃造成微分冲击；首次测量不求导。 */
    float derivative=pid->initialized ? -(measurement-pid->previous_measurement)/dt : 0;
    float alpha=dt/(pid->config.derivative_filter_s+dt);
    pid->derivative+=alpha*(derivative-pid->derivative);
    pid->previous_measurement=measurement;pid->initialized=1;
    float integral=clamp(pid->integral+pid->config.ki*error*dt,pid->config.minimum,pid->config.maximum);
    float pd=pid->config.kp*error+pid->config.kd*pid->derivative;
    float proposed=pd+integral;
    /* Conditional integration: don't accumulate into an unreachable limit.
     * Error pointing back out of saturation is allowed to unwind the integral. */
    if(!((proposed>pid->config.maximum && error>0) || (proposed<pid->config.minimum && error<0)))
        pid->integral=integral;
    return clamp(pd+pid->integral,pid->config.minimum,pid->config.maximum);
}
