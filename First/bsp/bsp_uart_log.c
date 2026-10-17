#include "bsp_uart_log.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

void bsp_uart_log_write(const char *text) {
    if (text) {
        size_t len=strlen(text);
        if (len<=256u) (void)HAL_UART_Transmit(&huart7,(const uint8_t *)text,(uint16_t)len,50);
    }
}
void bsp_uart_log_test_tick(void) {
    static uint32_t last;
    static uint8_t started;
    uint32_t now=HAL_GetTick();
    if (!started || (uint32_t)(now-last)>=1000u) {
        char line[80];
        started=1; last=now;
        snprintf(line,sizeof(line),"UART7_TEST H723 115200 8N1 tick=%lu\r\n",(unsigned long)now);
        bsp_uart_log_write(line);
    }
    uint8_t byte;
    if (HAL_UART_Receive(&huart7,&byte,1,0)==HAL_OK) {
        (void)HAL_UART_Transmit(&huart7,&byte,1,10);
    }
}
