/*
 * 作者：Codex
 * 文件：motor_tune.c
 * 说明：ID1调参状态机、自主速度演示及主控PID力矩定速试验。
 * 日期：2026-10-14
 */
#include <math.h>
#include <string.h>

#include "./motor_tune.h"
#include "./device/motor.h"
#include "./service/tuning_link.h"
#include "./component/control_pid.h"

#define MOTOR_TUNE_MAX_TEMP_C 80
#define MOTOR_TUNE_PID_TARGET_RAD_S 5.0f
/* 本轮PID台架发送上限；0.01Nm余量覆盖协议量化误差。 */
#define MOTOR_TUNE_PID_TORQUE_LIMIT_NM 6.0f
#define MOTOR_TUNE_PID_OUTPUT_LIMIT_NM (MOTOR_TUNE_PID_TORQUE_LIMIT_NM - 0.01f)
#define MOTOR_TUNE_PID_FEEDBACK_LIMIT_NM (MOTOR_TUNE_PID_TORQUE_LIMIT_NM * 2.0f)
enum { F_CONFIG=1,F_BUS=2,F_FEEDBACK=4,F_MOTOR=8,F_HOST=16,F_TX=32,F_UART=64,F_LIMIT=128 };
static motor_tune_state_t state;
static motor_command_t command;
static motor_command_t applied;
static motor_feedback_t feedback;
static uint32_t fault,last_tick,last_wave,last_host,transition_ms,last_state_tx,last_uart_errors;
static uint8_t stop_phase,stop_fault,disable_sent,enable_sent;
static uint8_t stop_confirmed,tx_busy;
static uint8_t info_requested;
static uint8_t clear_waiting;
static uint8_t speed_demo,demo_phase;
static uint8_t pid_demo;
static uint8_t pid_boot_resume,pid_boot_clear_attempted,pid_waiting;
static uint32_t pid_wait_ms;
static control_pid_t speed_pid;
static uint32_t pid_feedback_count,pid_feedback_ms;
static float pid_target;
/* 力矩量化约0.0088Nm，PID输出留余量，使正负发送值都不超过6Nm。
 * 驱动报文Kp/Kd始终为0。 */
//TODO：根据VOFA+实测波形整定增益；当前为保守初值。
static const control_pid_config_t pid_config={.kp=1.0f,.ki=0.1f,.kd=0,
    .minimum=-MOTOR_TUNE_PID_OUTPUT_LIMIT_NM,.maximum=MOTOR_TUNE_PID_OUTPUT_LIMIT_NM,.derivative_filter_s=0.02f};
static uint32_t clear_ms,clear_count;
static uint32_t disable_ms,disable_count,disable_tx_goal,busy_since;
static float anchor,desired_velocity,desired_position;
static int result;

/* 接受取时钟期间产生的2ms未来偏差，防止无符号时间差下溢。 */
static int fresh(uint32_t now) { return feedback.count && (int32_t)(now-feedback.time_ms)<200 && (int32_t)(now-feedback.time_ms)>=-2; }
static int active(void) { return state>=MOTOR_TUNE_ENABLING && state<=MOTOR_TUNE_POSITION; }
/* 锁存停车请求；先取消旧帧，再发送失能，等待新失能反馈确认。 */
static void stop_request(uint32_t now,uint32_t reason) {
    /* STOP或故障后禁止自主重启，防止旧目标重新生效。 */
    if(speed_demo) demo_phase=3;
    if(pid_demo) { control_pid_reset(&speed_pid);pid_target=0; }
    if(reason) pid_boot_resume=0;
    fault|=reason;
    if(state==MOTOR_TUNE_STOPPING) { if(reason) stop_fault=1;return; }
    memset(&command,0,sizeof(command));desired_velocity=0;
    state=MOTOR_TUNE_STOPPING;transition_ms=now;stop_phase=disable_sent=0;stop_fault=fault!=0;
    stop_confirmed=0;
}
/* 按实际量化值校验力矩预算；仅成功入队的命令更新遥测快照。 */
static bsp_fdcan_result_t send_control(const motor_command_t *requested,uint32_t now) {
    motor_command_t safe=*requested,quantized;
    float torque_limit=pid_demo ? MOTOR_TUNE_PID_TORQUE_LIMIT_NM : 3.0f;
    int bounded=0;
    for(unsigned attempt=0;attempt<4;++attempt) {
        if(!motor_codec_quantize(&safe,&quantized)) break;
        float budget=fabsf(quantized.kp*(quantized.position-feedback.position))+
            fabsf(quantized.kd*(quantized.velocity-feedback.velocity))+fabsf(quantized.torque);
        if(budget<=torque_limit) { bounded=1;break; }
        float scale=(torque_limit-0.10f)/budget;safe.kp*=scale;safe.kd*=scale;
    }
    if(!bounded) return BSP_FDCAN_INVALID;
    bsp_fdcan_result_t r=motor_send(&safe);
    if(r==BSP_FDCAN_OK) { applied=quantized;tx_busy=0; }
    else if(r==BSP_FDCAN_BUSY) {
        if(!tx_busy) { tx_busy=1;busy_since=now; }
        if((uint32_t)(now-busy_since)>=100) stop_request(now,F_TX);
    }
    return r;
}
/* 初始化本层状态为失能；设备和BSP必须由main提前初始化。 */
void motor_tune_init(uint32_t now) {
    speed_demo=demo_phase=pid_demo=0;pid_target=0;
    pid_boot_resume=pid_boot_clear_attempted=pid_waiting=0;
    state=MOTOR_TUNE_BOOT;fault=0;result=0;
    memset(&command,0,sizeof(command));memset(&feedback,0,sizeof(feedback));
    memset(&applied,0,sizeof(applied));stop_confirmed=tx_busy=info_requested=clear_waiting=0;
    last_tick=now-2u;last_wave=now;last_host=now;last_uart_errors=0;
    enable_sent=disable_sent=stop_phase=stop_fault=0;
}
/* 主循环读取状态，不访问HAL或设备寄存器。 */
motor_tune_state_t motor_tune_state(void) { return state; }
/* 单次启动10rad/s驱动速度阻尼演示，沿用共同停车流程。 */
void motor_tune_speed_demo_init(uint32_t now) { motor_tune_init(now);speed_demo=1; }
/* 主控速度PID演示：报文增益清零，力矩闭环，固定5rad/s。 */
//only forDEBUG：独立台架演示模式，由main选择后自动启动一次。
void motor_tune_pid_demo_init(uint32_t now) {
    motor_tune_init(now);speed_demo=pid_demo=1;
    if(!control_pid_init(&speed_pid,&pid_config)) stop_request(now,F_CONFIG);
}
/* 使能前检查新鲜失能反馈、故障、温度和静止条件。 */
int motor_tune_arm(uint32_t now) {
    if(state!=MOTOR_TUNE_OFF || !motor_configured() || !fresh(now) || feedback.enabled || feedback.fault || fault ||
       feedback.temperature>=MOTOR_TUNE_MAX_TEMP_C || fabsf(feedback.velocity)>0.5f) return 0;
    memset(&command,0,sizeof(command));anchor=feedback.position;command.position=anchor;command.kd=pid_demo ? 0 : 0.2f;
    desired_position=anchor;desired_velocity=0;last_host=now;transition_ms=now;
    stop_confirmed=0;tx_busy=0;
    last_state_tx=now-20u;enable_sent=0;state=MOTOR_TUNE_ENABLING;return 1;
}
/* 交互MIT速度接口；velocity为rad/s，kd为驱动速度阻尼增益。 */
int motor_tune_velocity(float velocity,float kd,uint32_t now) {
    /* PID模式独占力矩输出，公开MIT接口也不得修改驱动增益。 */
    if(pid_demo) return 0;
    if(state<MOTOR_TUNE_READY || state>MOTOR_TUNE_POSITION || !isfinite(velocity)||!isfinite(kd)||
       fabsf(velocity)>(speed_demo ? 10.0f : 5.0f)||kd<0||kd>2) return 0;
    desired_velocity=velocity;command.kp=0;command.kd=kd;state=MOTOR_TUNE_VELOCITY;last_host=now;return 1;
}
/* 设置相对本次ARM锚点的位置；relative单位为rad。 */
int motor_tune_position(float relative,float kp,float kd,uint32_t now) {
    if(pid_demo) return 0;
    if(state<MOTOR_TUNE_READY || state>MOTOR_TUNE_POSITION || !isfinite(relative)||!isfinite(kp)||!isfinite(kd)||fabsf(relative)>0.5f||kp<0||kp>5||kd<0||kd>2) return 0;
    float absolute=anchor+relative;
    if(absolute < -12.0f || absolute > 12.0f) return 0;
    desired_position=absolute;desired_velocity=0;command.kp=kp;command.kd=kd;
    state=MOTOR_TUNE_POSITION;last_host=now;return 1;
}
/* 显式STOP取消启动接管意图，不能因为清错而自动重启。 */
void motor_tune_stop(uint32_t now) { pid_boot_resume=0;stop_request(now,0); }
static void handle_command(const tune_command_t *c,uint32_t now) {
    result=-1;
    if(pid_demo && (c->kind==TUNE_ARM || c->kind==TUNE_VEL || c->kind==TUNE_POS)) { result=-2;return; }
    switch(c->kind) {
    case TUNE_PING:last_host=now;result=1;break;
    case TUNE_STOP:motor_tune_stop(now);last_host=now;result=1;break;
    case TUNE_ARM:result=motor_tune_arm(now)?1:-2;break;
    case TUNE_VEL:result=motor_tune_velocity(c->target,c->kd,now)?1:-2;break;
    case TUNE_POS:result=motor_tune_position(c->target,c->kp,c->kd,now)?1:-2;break;
    case TUNE_CLEAR:
        if(state==MOTOR_TUNE_FAULT && stop_confirmed && motor_configured() && fresh(now) && !feedback.enabled && feedback.fault) {
            if(motor_clear_fault()==BSP_FDCAN_OK) {
                clear_ms=now;clear_count=feedback.count;clear_waiting=1;
                state=MOTOR_TUNE_BOOT;fault=0;result=1;last_host=now;
            }
            break;
        }
        if(state==MOTOR_TUNE_FAULT && !enable_sent && (fault&F_CONFIG)) {
            if(motor_retry_config(now)==BSP_FDCAN_OK) { fault=0;state=MOTOR_TUNE_BOOT;result=1;last_host=now; }
            break;
        }
        if(state==MOTOR_TUNE_FAULT && motor_configured() && fresh(now) && !feedback.enabled && !feedback.fault) {
            fault=0;state=MOTOR_TUNE_OFF;result=1;last_host=now;
        }
        break;
    default:break;
    case TUNE_INFO:info_requested=1;result=1;break;
    case TUNE_READ:result=motor_read_diagnostic((uint8_t)c->target)==BSP_FDCAN_OK?1:-2;break;
    }
}
/* 异步取消/失能：TX完成、无pending、新失能反馈三者共同确认停车。 */
static void stop_tick(uint32_t now) {
    bsp_fdcan_result_t r;
    if(!stop_phase) {
        r=motor_cancel();
        if(r==BSP_FDCAN_OK) stop_phase=1;
        else if(r!=BSP_FDCAN_BUSY) fault|=F_TX;
    }
    if(stop_phase==1) {
        uint8_t pending;
        r=motor_tx_pending(&pending);
        if(r==BSP_FDCAN_OK && !pending) { stop_phase=2;last_state_tx=now-20u; }
    }
    if(stop_phase==2 && (uint32_t)(now-last_state_tx)>=20) {
        bsp_fdcan_stats_t s;
        if(motor_bus_stats(&s)!=BSP_FDCAN_OK) { fault|=F_BUS;return; }
        r=motor_set_enabled(0);
        if(r==BSP_FDCAN_OK) {
            last_state_tx=now;
            if(!disable_sent) { disable_ms=now;disable_count=feedback.count;disable_tx_goal=s.tx_complete+1; }
            disable_sent=1;
        }
        else if(r!=BSP_FDCAN_BUSY) fault|=F_TX;
    }
    bsp_fdcan_stats_t final_bus;
    uint8_t pending=1;
    int tx_done=motor_bus_stats(&final_bus)==BSP_FDCAN_OK && final_bus.tx_complete>=disable_tx_goal &&
        motor_tx_pending(&pending)==BSP_FDCAN_OK && !pending;
    if(disable_sent && tx_done && fresh(now) && !feedback.enabled && feedback.count>disable_count &&
       (int32_t)(feedback.time_ms-disable_ms)>=0) {
        stop_confirmed=1;
        state=(stop_fault||fault)?MOTOR_TUNE_FAULT:MOTOR_TUNE_OFF;
    } else if((uint32_t)(now-transition_ms)>=500) { fault|=F_FEEDBACK;state=MOTOR_TUNE_FAULT; }
}
/* main周期调用；2ms控制、100Hz波形，uart_errors为UART累计错误快照。 */
void motor_tune_tick(uint32_t now,uint32_t uart_errors) {
    uint32_t elapsed=now-last_tick;
    if(elapsed<2) return;
    last_tick=now;
    motor_poll(now);motor_get_feedback(&feedback);
    bsp_fdcan_stats_t bus;
    bsp_fdcan_result_t bus_result=motor_bus_stats(&bus);
    if(state==MOTOR_TUNE_BOOT) {
        if(clear_waiting) {
            if(feedback.count>clear_count && !feedback.enabled && !feedback.fault && fresh(now)) {
                clear_waiting=0;state=MOTOR_TUNE_OFF;
            } else if((uint32_t)(now-clear_ms)>=500) { clear_waiting=0;stop_request(now,F_MOTOR); }
        }
        else if(motor_config_failed()) stop_request(now,F_CONFIG);
        else if(motor_configured() && fresh(now)) {
            /* 仅启动时清除烧录通信中断遗留的纯CAN超时一次。 */
            if(pid_demo && !feedback.enabled && feedback.fault==0x2000u && !pid_boot_clear_attempted) {
                bsp_fdcan_result_t r=motor_clear_fault();
                if(r==BSP_FDCAN_OK) {
                    pid_boot_clear_attempted=1;clear_waiting=1;clear_ms=now;clear_count=feedback.count;
                } else if(r!=BSP_FDCAN_BUSY) stop_request(now,F_TX);
            }
            else if(feedback.enabled || feedback.fault) {
                /* A previous autonomous image can leave the motor enabled.
                 * Confirm disable first; don't silently continue its command. */
                if(pid_demo && feedback.enabled && !feedback.fault) pid_boot_resume=1;
                stop_request(now,feedback.fault?F_MOTOR:0);
            }
            else state=MOTOR_TUNE_OFF;
        }
    }
    if(active()) {
        if(bus_result!=BSP_FDCAN_OK || bus.bus_off) stop_request(now,F_BUS);
        else if(!fresh(now)) stop_request(now,F_FEEDBACK);
        else if(feedback.fault || (enable_sent && state!=MOTOR_TUNE_ENABLING && !feedback.enabled)) stop_request(now,F_MOTOR);
        else if(feedback.temperature>=MOTOR_TUNE_MAX_TEMP_C) stop_request(now,F_LIMIT);
        else if(!speed_demo && (uint32_t)(now-last_host)>=500) stop_request(now,F_HOST);
        else if(uart_errors!=last_uart_errors) stop_request(now,F_UART);
        else if(elapsed>10 || fabsf(feedback.velocity)>(pid_demo ? 62.0f : (speed_demo ? 12.0f : 8.0f)) ||
                fabsf(feedback.torque)>(pid_demo ? MOTOR_TUNE_PID_FEEDBACK_LIMIT_NM : 6.0f)) stop_request(now,F_LIMIT);
    }
    last_uart_errors=uart_errors;
    if(pid_demo && pid_boot_resume && state==MOTOR_TUNE_OFF && stop_confirmed) {
        pid_boot_resume=0;demo_phase=0;
    }
    if(speed_demo && demo_phase==0 && state==MOTOR_TUNE_OFF) {
        if(pid_demo && !pid_waiting) { pid_waiting=1;pid_wait_ms=now; }
        if(pid_demo && fresh(now) && !feedback.enabled && !feedback.fault &&
           feedback.temperature<MOTOR_TUNE_MAX_TEMP_C && fabsf(feedback.velocity)>0.5f) {
            if((uint32_t)(now-pid_wait_ms)>=5000u) stop_request(now,F_LIMIT);
        }
        else if(motor_tune_arm(now)) demo_phase=1;
        else stop_request(now,F_LIMIT);
    }
    if(speed_demo && demo_phase==1 && state==MOTOR_TUNE_READY) {
        if(pid_demo) {
            memset(&command,0,sizeof(command));control_pid_reset(&speed_pid);
            pid_feedback_count=0;pid_feedback_ms=feedback.time_ms-2u;
            state=MOTOR_TUNE_VELOCITY;demo_phase=2;
        }
        else if(motor_tune_velocity(10.0f,1.0f,now)) demo_phase=2;
        else stop_request(now,F_LIMIT);
    }
    tune_command_t input;
    for(unsigned n=0;n<4 && tuning_link_command(&input);++n) handle_command(&input,now);
    if(state==MOTOR_TUNE_STOPPING) {
        stop_tick(now);
        /* 取消/失能过程发生过任何故障后，CLEAR不能恢复旧接管意图。 */
        if(fault) pid_boot_resume=0;
    }
    else if(state==MOTOR_TUNE_ENABLING) {
        if(!enable_sent && (uint32_t)(now-last_state_tx)>=20) {
            bsp_fdcan_result_t r=send_control(&command,now);
            if(r==BSP_FDCAN_OK) r=motor_set_enabled(1);
            if(r==BSP_FDCAN_OK) { enable_sent=1;last_state_tx=now; }
            else if(r!=BSP_FDCAN_BUSY) stop_request(now,F_TX);
        }
        if(enable_sent && feedback.enabled && (int32_t)(feedback.time_ms-transition_ms)>=0) state=MOTOR_TUNE_READY;
        else if((uint32_t)(now-transition_ms)>=500) stop_request(now,F_FEEDBACK);
    } else if(pid_demo && state==MOTOR_TUNE_VELOCITY) {
        /* 使能确认后持续使用固定速度参考，不再切换周期目标。 */
        pid_target=MOTOR_TUNE_PID_TARGET_RAD_S;
        /* 在副本上计算，发送BUSY或失败时不提交PID历史。 */
        control_pid_t candidate=speed_pid;
        motor_command_t requested=command;
        uint32_t sample_ms=feedback.time_ms-pid_feedback_ms;
        /* 同一反馈只积分一次，dt来自接收时间戳间隔。 */
        int new_sample=feedback.count!=pid_feedback_count && sample_ms>0;
        if(new_sample) requested.torque=control_pid_step(&candidate,pid_target,feedback.velocity,(float)sample_ms/1000.0f);
        bsp_fdcan_result_t r=send_control(&requested,now);
        if(r==BSP_FDCAN_OK) {
            command=requested;
            if(new_sample) { speed_pid=candidate;pid_feedback_count=feedback.count;pid_feedback_ms=feedback.time_ms; }
        } else if(r!=BSP_FDCAN_BUSY) stop_request(now,F_TX);
    } else if(state>=MOTOR_TUNE_READY && state<=MOTOR_TUNE_POSITION) {
        float dv=desired_velocity-command.velocity,limit=10.0f*elapsed/1000.0f;
        if(dv>limit) dv=limit;
        if(dv<-limit) dv=-limit;
        command.velocity+=dv;
        command.position=state==MOTOR_TUNE_POSITION ? desired_position : feedback.position;
        if(state==MOTOR_TUNE_POSITION && fabsf(feedback.position-anchor)>0.75f) stop_request(now,F_LIMIT);
        else {
            bsp_fdcan_result_t r=send_control(&command,now);
            if(r!=BSP_FDCAN_OK && r!=BSP_FDCAN_BUSY) stop_request(now,F_TX);
        }
    }
    if((uint32_t)(now-last_wave)>=10) {
        last_wave=now;
        if(speed_demo) {
            (void)tuning_link_speed_waveform(active() ? (pid_demo ? pid_target : applied.velocity) : 0.0f,feedback.velocity);
            return;
        }
        float age=feedback.count ? (float)(int32_t)(now-feedback.time_ms) : 1000000.0f;
        if(age<0) age=0;
        float values[15]={(float)now/1000.0f,applied.position,feedback.position,applied.velocity,
            feedback.velocity,feedback.torque,applied.kp,applied.kd,(float)feedback.temperature,
            (float)state,(float)fault,(float)result,(float)feedback.enabled,age,(float)stop_confirmed};
        if(info_requested) {
            values[0]=-1;values[1]=(float)motor_config_index();values[2]=(float)motor_config_raw();
            values[3]=(float)bus.tx_enqueued;values[4]=(float)bus.tx_complete;values[5]=(float)bus.rx;
            values[6]=(float)bus.protocol_error;values[7]=(float)bus.tx_error_counter;values[8]=(float)bus.rx_error_counter;
            values[11]=(float)(bus.last_error_code+16u*bus.data_error_code);
            values[12]=(float)feedback.status;
            if(motor_config_index()==49 || motor_config_index()==50 || motor_config_index()==66 || motor_config_index()==71)
                values[2]=motor_diagnostic_value();
            info_requested=0;
        }
        (void)tuning_link_waveform(values);
    }
}
