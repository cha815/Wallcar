/* 作者：Codex；文件：can_port.h；说明：主循环CAN端口，不依赖HAL。 */
#ifndef CAN_PORT_H
#define CAN_PORT_H
#include "can_frame.h"

typedef enum { CAN_OK=0, CAN_EMPTY, CAN_BUSY, CAN_INVALID, CAN_NOT_READY,
    CAN_IO_ERROR, CAN_BUS_OFF } can_result_t;
typedef struct {
    uint32_t completed, errors, dropped;
    uint8_t bus_off, tec, rec, lec, dlec;
} can_health_t;
/* ctx由装配层持有；OK仅表示入队；所有函数仅主循环调用。 */
typedef struct {
    void *ctx;
    can_result_t (*send)(void *ctx,const can_frame_t *frame);
    can_result_t (*receive)(void *ctx,can_frame_t *frame);
    can_result_t (*health)(void *ctx,can_health_t *health);
} can_port_t;
#endif /* CAN_PORT_H */
