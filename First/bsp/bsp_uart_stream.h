#ifndef BSP_UART_STREAM_H
#define BSP_UART_STREAM_H
#include <stdint.h>
int bsp_uart_stream_init(void);
int bsp_uart_stream_read(uint8_t *byte);
/* Copies <=64 bytes into a persistent buffer, returns 1 accepted/0 busy/-1 error. */
int bsp_uart_stream_write(const uint8_t *data,uint16_t len);
uint32_t bsp_uart_stream_errors(void);
#endif
