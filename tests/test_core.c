#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "can_frame.h"
#include "motor_ro.h"

static can_frame_t sent;
static bsp_fdcan_result_t send_result = BSP_FDCAN_OK;
bsp_fdcan_result_t bsp_fdcan_send(const can_frame_t *frame) {
    sent = *frame;
    return send_result;
}

int main(void) {
    const uint8_t lengths[] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
    can_frame_t f = {.id=0x7ff, .fd=1, .brs=1};
    uint8_t dlc, len;
    for (unsigned i=0; i<16; ++i) {
        f.len=lengths[i];
        assert(can_frame_valid(&f));
        assert(can_length_to_dlc(f.len,&dlc) && dlc==i);
        assert(can_dlc_to_length(dlc,&len) && len==f.len);
    }
    f.len=9; assert(!can_frame_valid(&f));
    f.len=8; f.id=0x800; assert(!can_frame_valid(&f));
    f.extended=1; f.id=0x1fffffff; assert(can_frame_valid(&f));
    f.id=0x20000000; assert(!can_frame_valid(&f));
    f.id=1; f.fd=0; assert(!can_frame_valid(&f));
    f.brs=0; assert(can_frame_valid(&f));
    f.len=12; assert(!can_frame_valid(&f));
    assert(!can_frame_valid(NULL));
    assert(!can_dlc_to_length(16,&len));
    can_queue_t q; can_queue_reset(&q);
    f.len=8;
    for (unsigned i=0; i<CAN_QUEUE_CAPACITY; ++i) {
        f.id=i; assert(can_queue_push(&q,&f));
    }
    assert(!can_queue_push(&q,&f));
    for (unsigned i=0; i<CAN_QUEUE_CAPACITY; ++i) {
        assert(can_queue_pop(&q,&f) && f.id==i);
    }
    assert(!can_queue_pop(&q,&f));
    for (unsigned i=0; i<100; ++i) {
        f.id=i; assert(can_queue_push(&q,&f));
        assert(can_queue_pop(&q,&f) && f.id==i);
    }
    assert(motor_ro_read_parameter(0,0)==BSP_FDCAN_INVALID);
    assert(motor_ro_read_parameter(4,0)==BSP_FDCAN_INVALID);
    assert(motor_ro_read_parameter(10,0)==BSP_FDCAN_OK);
    assert(sent.id==0x601 && sent.len==8 && sent.fd && !sent.brs);
    assert(sent.data[0]==0x67 && sent.data[1]==10 && sent.data[6]==4 && sent.data[7]==0x76);
    assert(motor_ro_read_parameter(56,1)==BSP_FDCAN_OK && sent.brs);
    send_result=BSP_FDCAN_BUSY;
    assert(motor_ro_read_parameter(36,0)==BSP_FDCAN_BUSY);
    send_result=BSP_FDCAN_OK;
    assert(motor_ro_query_status(0,1)==BSP_FDCAN_OK);
    assert(sent.id==0x101 && sent.len==8 && sent.data[7]==0xfa);
    assert(motor_ro_query_status(1,1)==BSP_FDCAN_OK);
    assert(sent.id==0x10 && sent.len==64);
    for(unsigned i=0;i<7;++i) assert(sent.data[i]==0xff);
    for(unsigned i=8;i<64;++i) assert(sent.data[i]==0);
    f=sent; f.id=0x601; f.len=8; memset(f.data,0,64);
    f.data[0]=1; f.data[1]=36; f.data[2]=1; f.data[7]=0xff;
    uint32_t raw=0;
    assert(motor_ro_parameter_response(&f,36,&raw) && raw==1);
    assert(!motor_ro_parameter_response(&f,37,&raw));
    f.data[7]=0x76; assert(!motor_ro_parameter_response(&f,36,&raw));
    f.id=1; f.data[6]=0; f.data[7]=1;
    uint16_t status;
    assert(motor_ro_status_response(&f,&status) && status==1);
    f.extended=1; assert(!motor_ro_status_response(&f,&status));
    puts("PASS core: DLC/flags/queue/readonly-whitelist/frames/responses");
    return 0;
}
