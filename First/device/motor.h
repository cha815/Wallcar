#ifndef MOTOR_H
#define MOTOR_H
#include "motor_codec.h"
#include "bsp_fdcan.h"
void motor_init(uint32_t now_ms);
void motor_poll(uint32_t now_ms);
int motor_configured(void);
int motor_config_failed(void);
uint8_t motor_config_index(void);
uint32_t motor_config_raw(void);
bsp_fdcan_result_t motor_read_diagnostic(uint8_t index);
float motor_diagnostic_value(void);
bsp_fdcan_result_t motor_retry_config(uint32_t now_ms);
void motor_get_feedback(motor_feedback_t *out);
bsp_fdcan_result_t motor_send(const motor_command_t *command);
bsp_fdcan_result_t motor_set_enabled(uint8_t enabled);
/* One explicit clear request for a known protection on a disabled motor.
 * Does not change protection thresholds/switches or re-enable the motor. */
bsp_fdcan_result_t motor_clear_fault(void);
bsp_fdcan_result_t motor_cancel(void);
bsp_fdcan_result_t motor_tx_pending(uint8_t *pending);
bsp_fdcan_result_t motor_bus_stats(bsp_fdcan_stats_t *out);
#endif
