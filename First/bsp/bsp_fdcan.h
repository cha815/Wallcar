#ifndef BSP_FDCAN_H
#define BSP_FDCAN_H
#include "can_frame.h"

typedef enum {
    BSP_FDCAN_OK=0, BSP_FDCAN_EMPTY, BSP_FDCAN_BUSY, BSP_FDCAN_INVALID,
    BSP_FDCAN_NOT_READY, BSP_FDCAN_HAL_ERROR, BSP_FDCAN_BUS_OFF
} bsp_fdcan_result_t;
typedef enum { BSP_FDCAN_NORMAL=0, BSP_FDCAN_LOOPBACK=1 } bsp_fdcan_mode_t;
typedef struct {
    uint32_t tx_enqueued, tx_complete, tx_busy, tx_error;
    uint32_t rx, rx_reject, rx_drop, rx_hw_lost, tx_event_lost;
    uint32_t bus_off_count, protocol_error, hal_error, recoveries;
    uint32_t last_hal_error, last_error_flags;
    uint8_t bus_off, error_passive, warning;
    uint8_t tx_error_counter, rx_error_counter, last_error_code, data_error_code;
} bsp_fdcan_stats_t;

/* MX_FDCAN1_Init must have completed. Call all public APIs from main only. */
bsp_fdcan_result_t bsp_fdcan_init(bsp_fdcan_mode_t mode);
/* OK means submitted; TX event counter confirms completion. */
bsp_fdcan_result_t bsp_fdcan_send(const can_frame_t *frame);
bsp_fdcan_result_t bsp_fdcan_receive(can_frame_t *frame);
bsp_fdcan_result_t bsp_fdcan_get_stats(bsp_fdcan_stats_t *out);
/* One attempt per initialization; cancels pending frames and clears RX queue. */
bsp_fdcan_result_t bsp_fdcan_recover(void);
/* Cancellation is asynchronous: poll until pending=0 before submitting stop frames. */
bsp_fdcan_result_t bsp_fdcan_cancel(void);
bsp_fdcan_result_t bsp_fdcan_tx_pending(uint8_t *pending);
#endif
