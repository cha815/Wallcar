/*
 * 作者：Codex
 * 文件：motor_ro.c
 * 说明：ID1只读参数白名单与状态查询，不写驱动参数或Flash。
 * 日期：2026-10-14
 */
#include <string.h>

#include "./motor_ro.h"

/* Index=0会触发Flash保存，禁止列入只读白名单。 */
static int allowed(uint8_t index) {
    return index==10 || index==11 || index==21 || (index>=24 && index<=37) ||
        index==45 || index==49 || index==50 || index==56 || index==66 || index==67 || index==69 || index==71;
}
/* 按白名单读取；Index21用于核对驱动实际力矩能力。 */
bsp_fdcan_result_t motor_ro_read_parameter(uint8_t index, uint8_t brs) {
    if (!allowed(index) || brs>1) return BSP_FDCAN_INVALID;
    can_frame_t f={.id=0x601,.len=8,.fd=1,.brs=brs};
    f.data[0]=0x67; f.data[1]=index; f.data[6]=0x04; f.data[7]=0x76;
    return bsp_fdcan_send(&f);
}
bsp_fdcan_result_t motor_ro_query_status(uint8_t broadcast, uint8_t brs) {
    if (broadcast>1 || brs>1) return BSP_FDCAN_INVALID;
    can_frame_t f={.id=broadcast ? 0x10u : 0x101u,
                   .len=broadcast ? 64u : 8u,.fd=1,.brs=brs};
    memset(f.data,0xff,7); f.data[7]=0xfa;
    return bsp_fdcan_send(&f);
}
int motor_ro_parameter_response(const can_frame_t *f, uint8_t index, uint32_t *raw) {
    if (!f || !raw || !allowed(index) || !can_frame_valid(f) || f->extended ||
        f->id!=0x601 || f->len!=8 || f->data[0]!=1 || f->data[1]!=index ||
        f->data[6]!=0 || f->data[7]!=0xff) return 0;
    *raw=(uint32_t)f->data[2] | ((uint32_t)f->data[3]<<8) |
         ((uint32_t)f->data[4]<<16) | ((uint32_t)f->data[5]<<24);
    return 1;
}
int motor_ro_status_response(const can_frame_t *f, uint16_t *status) {
    if (!f || !status || !can_frame_valid(f) || f->extended || f->id!=1 || f->len!=8) return 0;
    *status=(uint16_t)(((uint16_t)f->data[6]<<8)|f->data[7]);
    return 1;
}
#ifndef MOTOR_RO_PACKET_TEST
bsp_fdcan_result_t motor_ro_receive(can_frame_t *f) { return bsp_fdcan_receive(f); }
bsp_fdcan_result_t motor_ro_stats(bsp_fdcan_stats_t *s) { return bsp_fdcan_get_stats(s); }
bsp_fdcan_result_t motor_ro_recover(void) { return bsp_fdcan_recover(); }
#endif /* MOTOR_RO_PACKET_TEST */
