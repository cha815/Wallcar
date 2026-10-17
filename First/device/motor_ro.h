#ifndef MOTOR_RO_H
#define MOTOR_RO_H
#include "bsp_fdcan.h"

/* Deliberately ID1-only. No write, enable, motion, zero or OTA API exists. */
bsp_fdcan_result_t motor_ro_read_parameter(uint8_t index, uint8_t brs);
bsp_fdcan_result_t motor_ro_query_status(uint8_t broadcast, uint8_t brs);
int motor_ro_parameter_response(const can_frame_t *frame, uint8_t index, uint32_t *raw);
int motor_ro_status_response(const can_frame_t *frame, uint16_t *status);
/* Diagnostic transport facade: service does not access BSP directly. */
bsp_fdcan_result_t motor_ro_receive(can_frame_t *frame);
bsp_fdcan_result_t motor_ro_stats(bsp_fdcan_stats_t *out);
bsp_fdcan_result_t motor_ro_recover(void);
#endif
