/* 作者：Codex；文件：qzb_motor.h；说明：泉智博独立节点上下文及能力适配。 */
#ifndef QZB_MOTOR_H
#define QZB_MOTOR_H
#include "../interfaces/motor_port.h"
#include "../service/can_service.h"
typedef struct {
    can_service_t *bus;
    motor_snapshot_t snapshot;
    uint32_t sent_ms,query_ms;
    uint32_t parameter_values[18];
    uint32_t refresh_ms,now_ms;
    motor_command_t command;
    uint8_t cursor,waiting,retries,streaming;
} qzb_motor_t;
/* 只读参数校验，不修改模式/保护/Flash；id为1..255。 */
int qzb_motor_init(qzb_motor_t *motor,can_service_t *bus,uint8_t id,uint32_t now);
void qzb_motor_tick(qzb_motor_t *motor,uint32_t now);
void qzb_motor_receive(void *motor,const can_frame_t *frame);
motor_port_t qzb_motor_port(qzb_motor_t *motor);
#endif /* QZB_MOTOR_H */
