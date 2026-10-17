/* 作者：Codex；文件：can_service.c；说明：主循环有界收包、分发与完整性检查。 */
#include <string.h>
#include "can_service.h"

/* 保存端口副本，不拥有调用方资源，不访问HAL。 */
int can_service_init(can_service_t *s,const can_port_t *p) {
    if(!s||!p||!p->send||!p->receive||!p->health) return 0;
    memset(s,0,sizeof(*s));s->port=*p;return 1;
}
int can_service_subscribe(can_service_t *s,void *ctx,can_receiver_t receive) {
    if(!s||!receive) return 0;
    for(unsigned i=0;i<CAN_SERVICE_NODE_CAPACITY;++i) if(!s->nodes[i].receive) {
        s->nodes[i].ctx=ctx;s->nodes[i].receive=receive;return 1;
    }
    return 0;
}
int can_service_claim_feedback(can_service_t *s,void *ctx,uint32_t id) {
    if(!s||id>0x7ff) return 0;
    int unique=1;
    for(unsigned i=0;i<CAN_SERVICE_NODE_CAPACITY;++i) if(s->nodes[i].claimed&&s->nodes[i].ctx!=ctx&&s->nodes[i].feedback_id==id) unique=0;
    for(unsigned i=0;i<CAN_SERVICE_NODE_CAPACITY;++i) if(s->nodes[i].receive&&s->nodes[i].ctx==ctx) {
        s->nodes[i].feedback_id=id;s->nodes[i].claimed=1;return unique;
    }
    return 0;
}
/* 每轮最多32帧，避免接收洪泛饿死应用周期。 */
void can_service_poll(can_service_t *s) {
    can_frame_t f;
    for(unsigned n=0;n<32 && s->port.receive(s->port.ctx,&f)==CAN_OK;++n) {
        if(!can_frame_valid(&f)||f.extended||f.len!=8) { ++s->rejected;continue; }
        for(unsigned i=0;i<CAN_SERVICE_NODE_CAPACITY;++i) if(s->nodes[i].receive)
            s->nodes[i].receive(s->nodes[i].ctx,&f);
    }
}
can_result_t can_service_send(can_service_t *s,const can_frame_t *f) {
    if(!s||!can_frame_valid(f)) return CAN_INVALID;
    return s->port.send(s->port.ctx,f);
}
