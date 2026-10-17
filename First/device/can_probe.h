#ifndef CAN_PROBE_H
#define CAN_PROBE_H
#include "bsp_fdcan.h"
/* Raw test sender is only enabled by main for the internal-loopback image. */
void can_probe_init(uint8_t loopback);
bsp_fdcan_result_t can_probe_send(const can_frame_t *frame);
bsp_fdcan_result_t can_probe_receive(can_frame_t *frame);
bsp_fdcan_result_t can_probe_stats(bsp_fdcan_stats_t *stats);
bsp_fdcan_result_t can_probe_recover(void);
#endif
