#ifndef BSP_UART_LOG_H
#define BSP_UART_LOG_H
#include <stdint.h>
/* UART7 polling logger, main context only. Does not use DTCM-inaccessible DMA. */
void bsp_uart_log_write(const char *text);
void bsp_uart_log_test_tick(void);
#endif
