#include "can_probe.h"
static uint8_t raw_enabled;
void can_probe_init(uint8_t loopback) { raw_enabled=loopback==1; }
bsp_fdcan_result_t can_probe_send(const can_frame_t *frame) {
    return raw_enabled ? bsp_fdcan_send(frame) : BSP_FDCAN_INVALID;
}
bsp_fdcan_result_t can_probe_receive(can_frame_t *frame) { return bsp_fdcan_receive(frame); }
bsp_fdcan_result_t can_probe_stats(bsp_fdcan_stats_t *stats) { return bsp_fdcan_get_stats(stats); }
bsp_fdcan_result_t can_probe_recover(void) { return bsp_fdcan_recover(); }
