/*
 * 作者：Codex
 * 文件：test_control_pid.c
 * 说明：主机验证PID计算、抗饱和及微分行为。
 * 日期：2026-10-14
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "./component/control_pid.h"
//only forDEBUG：主机回归，不能代替电机实测。
int main(void) {
    control_pid_t pid;
    control_pid_config_t cfg={.kp=2,.ki=0.5f,.minimum=-3,.maximum=3,.derivative_filter_s=0.02f};
    assert(control_pid_init(&pid,&cfg));
    assert(fabsf(control_pid_step(&pid,1,0,0.1f)-2.05f)<0.0001f);
    assert(fabsf(control_pid_step(&pid,1,0,0.1f)-2.10f)<0.0001f);
    cfg.kp=cfg.ki=1;assert(control_pid_init(&pid,&cfg));
    for(unsigned i=0;i<1000;++i) assert(control_pid_step(&pid,60,0,0.002f)==3);
    /* 不可达目标持续饱和后，积分不能阻碍回到零输出。 */
    assert(control_pid_step(&pid,0,0,0.002f)==0);
    for(unsigned i=0;i<1000;++i) assert(control_pid_step(&pid,-60,0,0.002f)==-3);
    assert(control_pid_step(&pid,0,0,0.002f)==0);
    cfg.kp=cfg.ki=0;cfg.kd=0.1f;cfg.derivative_filter_s=0;
    assert(control_pid_init(&pid,&cfg));
    assert(control_pid_step(&pid,0,0,0.1f)==0);
    /* 目标阶跃时测量未变，不应产生微分冲击。 */
    assert(control_pid_step(&pid,60,0,0.1f)==0);
    assert(fabsf(control_pid_step(&pid,60,1,0.1f)+1)<0.0001f);
    assert(isnan(control_pid_step(&pid,1,0,0)));
    assert(isnan(control_pid_step(&pid,NAN,0,0.1f)));
    cfg.minimum=3;assert(!control_pid_init(&pid,&cfg));
    puts("PASS PID: proportional/integral/derivative-on-measurement/antiwindup/invalid-input");
}
