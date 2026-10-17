#include "can_frame.h"

static const uint8_t lengths[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
int can_length_to_dlc(uint8_t len, uint8_t *dlc) {
    if (!dlc) return 0;
    for (uint8_t i=0; i<16; ++i) {
        if (lengths[i]==len) { *dlc=i; return 1; }
    }
    return 0;
}
int can_dlc_to_length(uint32_t dlc, uint8_t *len) {
    if (!len || dlc>=16) return 0;
    *len=lengths[dlc]; return 1;
}
int can_frame_valid(const can_frame_t *f) {
    uint8_t dlc;
    return f && f->extended<=1 && f->fd<=1 && f->brs<=1 &&
        f->id <= (f->extended ? 0x1fffffffu : 0x7ffu) &&
        (!f->brs || f->fd) && (f->fd || f->len<=8) &&
        can_length_to_dlc(f->len,&dlc);
}
void can_queue_reset(can_queue_t *q) { q->read=q->write=q->count=0; }
int can_queue_push(can_queue_t *q, const can_frame_t *f) {
    if (q->count==CAN_QUEUE_CAPACITY) return 0;
    q->frames[q->write]=*f;
    q->write=(uint8_t)((q->write+1u)%CAN_QUEUE_CAPACITY);
    ++q->count; return 1;
}
int can_queue_pop(can_queue_t *q, can_frame_t *f) {
    if (!q->count) return 0;
    *f=q->frames[q->read];
    q->read=(uint8_t)((q->read+1u)%CAN_QUEUE_CAPACITY);
    --q->count; return 1;
}
