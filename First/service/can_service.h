/* 作者：Codex；文件：can_service.h；说明：总线单一消费者与节点回调路由。 */
#ifndef CAN_SERVICE_H
#define CAN_SERVICE_H
#include "../interfaces/can_port.h"
typedef void (*can_receiver_t)(void *ctx,const can_frame_t *frame);
#define CAN_SERVICE_NODE_CAPACITY 4u
typedef struct {
    can_port_t port;
    struct { void *ctx;can_receiver_t receive;uint32_t feedback_id;uint8_t claimed; } nodes[CAN_SERVICE_NODE_CAPACITY];
    uint32_t rejected;
} can_service_t;
int can_service_init(can_service_t *service,const can_port_t *port);
int can_service_subscribe(can_service_t *service,void *ctx,can_receiver_t receive);
/* 反馈载荷不含节点ID，禁止同总线两个实例认领同一反馈标识符。 */
int can_service_claim_feedback(can_service_t *service,void *ctx,uint32_t feedback_id);
void can_service_poll(can_service_t *service);
can_result_t can_service_send(can_service_t *service,const can_frame_t *frame);
#endif /* CAN_SERVICE_H */
