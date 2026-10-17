/* 作者：Codex；文件：motor_port.h；说明：电机能力与SI单位快照。 */
#ifndef MOTOR_PORT_H
#define MOTOR_PORT_H
#include <stdint.h>

typedef struct { float position,velocity,kp,kd,torque; } motor_command_t;
/* 驱动实际读回的协议映射范围；不是额外的软件测试限幅。 */
typedef struct {
    float position_min,position_max,velocity_min,velocity_max;
    float kp_min,kp_max,kd_min,kd_max,torque_min,torque_max;
} motor_limits_t;
typedef struct {
    float position,velocity,torque; /* rad / rad/s / N m，沿驱动正方向。 */
    int16_t temperature;
    uint16_t status,fault;
    uint8_t enabled;
    uint32_t time_ms,count; /* 接收单调毫秒；count=0表示从未收到。 */
} motor_feedback_t;
typedef struct {
    motor_feedback_t feedback;
    motor_limits_t limits;
    uint32_t feedback_id;
    uint32_t parameter_raw, parameter_count;
    uint8_t ready,failed,parameter_index,id;
} motor_snapshot_t;
/* read为同一主循环一致快照；输出1入队、0忙、-1拒绝/错误。
 * ctx由注册方持有，应用不可直接访问；失能无需配置通过。 */
typedef struct {
    void *ctx;
    void (*read)(void *ctx,motor_snapshot_t *out);
    int (*enable)(void *ctx,uint8_t enabled);
    int (*command)(void *ctx,const motor_command_t *command);
} motor_port_t;
#endif /* MOTOR_PORT_H */
