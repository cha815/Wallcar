/* 作者：Codex；文件：can_frame.h；说明：平台无关CAN帧与有界队列契约。 */
#ifndef CAN_FRAME_H
#define CAN_FRAME_H
#include <stdint.h>
#include <stddef.h>

/* Hardware-independent CAN data frame. Remote frames are not supported. */
typedef struct {
    uint32_t id;
    uint32_t timestamp_ms;
    uint8_t len;
    uint8_t extended, fd, brs;
    uint8_t data[64];
} can_frame_t;

#define CAN_QUEUE_CAPACITY 32u
typedef struct {
    can_frame_t frames[CAN_QUEUE_CAPACITY];
    uint8_t read, write, count;
} can_queue_t;

int can_frame_valid(const can_frame_t *frame);
int can_length_to_dlc(uint8_t len, uint8_t *dlc);
int can_dlc_to_length(uint32_t dlc, uint8_t *len);
/* Caller provides synchronization when producer and consumer differ. */
void can_queue_reset(can_queue_t *queue);
int can_queue_push(can_queue_t *queue, const can_frame_t *frame);
int can_queue_pop(can_queue_t *queue, can_frame_t *frame);
#endif /* CAN_FRAME_H */
