#include "bsp_uart_stream.h"
#include "usart.h"
#include <string.h>
static uint8_t rx_byte,rx_data[256],tx_data[64];
static volatile uint16_t read_index,write_index;
static volatile uint32_t errors;
static volatile uint8_t tx_busy;
int bsp_uart_stream_init(void) {
    read_index=write_index=0;errors=0;tx_busy=0;
    return HAL_UART_Receive_IT(&huart7,&rx_byte,1)==HAL_OK;
}
int bsp_uart_stream_read(uint8_t *byte) {
    if(!byte||read_index==write_index) return 0;
    *byte=rx_data[read_index];read_index=(read_index+1u)&255u;return 1;
}
int bsp_uart_stream_write(const uint8_t *data,uint16_t len) {
    if(!data||!len||len>sizeof(tx_data)) return -1;
    if(tx_busy) return 0;
    memcpy(tx_data,data,len);tx_busy=1;
    if(HAL_UART_Transmit_IT(&huart7,tx_data,len)!=HAL_OK) { tx_busy=0;++errors;return -1; }
    return 1;
}
uint32_t bsp_uart_stream_errors(void) { return errors; }
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h) {
    if(h!=&huart7) return;
    uint16_t next=(write_index+1u)&255u;
    if(next==read_index) ++errors;
    else { rx_data[write_index]=rx_byte;write_index=next; }
    if(HAL_UART_Receive_IT(h,&rx_byte,1)!=HAL_OK) ++errors;
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h) { if(h==&huart7) tx_busy=0; }
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h) {
    if(h==&huart7) { ++errors; (void)HAL_UART_Receive_IT(h,&rx_byte,1); }
}
