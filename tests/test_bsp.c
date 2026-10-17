#include <assert.h>
#include <stdio.h>
#include "bsp_fdcan.h"
#include "fdcan.h"
static MockRegs regs;
FDCAN_HandleTypeDef hfdcan1={.Instance=&regs,.Init={.FrameFormat=FDCAN_FRAME_FD_BRS,
    .DataPrescaler=1,.DataSyncJumpWidth=3,.DataTimeSeg1=17,.DataTimeSeg2=6}};
static uint32_t free_level=8,rx_count;
static int start_error,send_error,sequence,tdc_order,start_order;
static uint32_t pending_mask;
HAL_StatusTypeDef HAL_FDCAN_AbortTxRequest(FDCAN_HandleTypeDef *h,uint32_t mask) { (void)h;assert(mask==255);return HAL_OK; }
uint32_t HAL_FDCAN_IsTxBufferMessagePending(const FDCAN_HandleTypeDef *h,uint32_t mask) { (void)h;return pending_mask&mask; }
static FDCAN_TxHeaderTypeDef tx;
static FDCAN_RxHeaderTypeDef rx={.Identifier=1,.DataLength=15,.FDFormat=1,.BitRateSwitch=1};
static FDCAN_ProtocolStatusTypeDef protocol;
uint32_t HAL_GetTick(void) { return 123; }
HAL_StatusTypeDef HAL_FDCAN_Init(FDCAN_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_DeInit(FDCAN_HandleTypeDef *h) { (void)h; regs.TXEFS=0; return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(FDCAN_HandleTypeDef *h,uint32_t a,uint32_t b,uint32_t c,uint32_t d) { (void)h;(void)a;(void)b;assert(c==1&&d==1);return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_ConfigTxDelayCompensation(FDCAN_HandleTypeDef *h,uint32_t a,uint32_t b) { (void)h;assert(a==18&&b==0);tdc_order=++sequence;return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_EnableTxDelayCompensation(FDCAN_HandleTypeDef *h) { (void)h;return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(FDCAN_HandleTypeDef *h,uint32_t a,uint32_t b) { (void)h;(void)b;assert(a&FDCAN_IT_TX_EVT_FIFO_ELT_LOST);return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *h) { (void)h;start_order=++sequence;return start_error?HAL_ERROR:HAL_OK; }
uint32_t HAL_FDCAN_GetTxFifoFreeLevel(const FDCAN_HandleTypeDef *h) { (void)h;return free_level; }
HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef *h,const FDCAN_TxHeaderTypeDef *t,const uint8_t *d) { (void)h;(void)d;tx=*t;return send_error?HAL_ERROR:HAL_OK; }
uint32_t HAL_FDCAN_GetRxFifoFillLevel(const FDCAN_HandleTypeDef *h,uint32_t fifo) { (void)h;(void)fifo;return rx_count; }
HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(FDCAN_HandleTypeDef *h,uint32_t fifo,FDCAN_RxHeaderTypeDef *r,uint8_t *d) { (void)h;(void)fifo;*r=rx;memset(d,0xaa,64);--rx_count;return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_GetTxEvent(FDCAN_HandleTypeDef *h,FDCAN_TxEventFifoTypeDef *event) { --h->Instance->TXEFS;event->MessageMarker=0;return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_GetProtocolStatus(const FDCAN_HandleTypeDef *h,FDCAN_ProtocolStatusTypeDef *p) { (void)h;*p=protocol;return HAL_OK; }
HAL_StatusTypeDef HAL_FDCAN_GetErrorCounters(const FDCAN_HandleTypeDef *h,FDCAN_ErrorCountersTypeDef *p) { (void)h;memset(p,0,sizeof(*p));return HAL_OK; }
int main(void) {
    can_frame_t f={.id=1,.len=64,.fd=1,.brs=1};
    bsp_fdcan_stats_t s;
    assert(bsp_fdcan_send(&f)==BSP_FDCAN_NOT_READY);
    assert(bsp_fdcan_init(BSP_FDCAN_LOOPBACK)==BSP_FDCAN_OK);
    assert(tdc_order<start_order);
    assert(hfdcan1.Init.Mode==FDCAN_MODE_INTERNAL_LOOPBACK);
    pending_mask=1;uint8_t pending;
    assert(bsp_fdcan_cancel()==BSP_FDCAN_OK);
    assert(bsp_fdcan_tx_pending(&pending)==BSP_FDCAN_OK && pending);
    pending_mask=0;assert(bsp_fdcan_tx_pending(&pending)==BSP_FDCAN_OK && !pending);
    f.len=9;assert(bsp_fdcan_send(&f)==BSP_FDCAN_INVALID);
    f.len=64;free_level=0;assert(bsp_fdcan_send(&f)==BSP_FDCAN_BUSY);
    free_level=8;send_error=1;assert(bsp_fdcan_send(&f)==BSP_FDCAN_HAL_ERROR);
    send_error=0;assert(bsp_fdcan_send(&f)==BSP_FDCAN_OK);
    assert(tx.DataLength==15 && tx.TxEventFifoControl==FDCAN_STORE_TX_EVENTS);
    assert(bsp_fdcan_get_stats(&s)==BSP_FDCAN_OK && s.tx_enqueued==1 && s.tx_complete==0);
    regs.TXEFS=1;HAL_FDCAN_TxEventFifoCallback(&hfdcan1,FDCAN_IT_TX_EVT_FIFO_NEW_DATA);
    rx_count=33;HAL_FDCAN_RxFifo0Callback(&hfdcan1,FDCAN_IT_RX_FIFO0_NEW_MESSAGE);
    assert(bsp_fdcan_get_stats(&s)==BSP_FDCAN_OK && s.rx_drop==1 && s.tx_complete==1);
    for(unsigned i=0;i<32;++i) assert(bsp_fdcan_receive(&f)==BSP_FDCAN_OK && f.len==64 && f.data[63]==0xaa && f.timestamp_ms==123);
    assert(bsp_fdcan_receive(&f)==BSP_FDCAN_EMPTY);
    rx.RxFrameType=FDCAN_REMOTE_FRAME;rx_count=1;
    HAL_FDCAN_RxFifo0Callback(&hfdcan1,FDCAN_IT_RX_FIFO0_MESSAGE_LOST);
    HAL_FDCAN_TxEventFifoCallback(&hfdcan1,FDCAN_IT_TX_EVT_FIFO_ELT_LOST);
    assert(bsp_fdcan_get_stats(&s)==BSP_FDCAN_OK && s.rx_reject==1 && s.rx_hw_lost==1 && s.tx_event_lost==1);
    protocol.BusOff=1;HAL_FDCAN_ErrorStatusCallback(&hfdcan1,FDCAN_IT_BUS_OFF);
    assert(bsp_fdcan_send(&f)==BSP_FDCAN_BUS_OFF);
    protocol.BusOff=0;assert(bsp_fdcan_recover()==BSP_FDCAN_OK);
    protocol.BusOff=1;HAL_FDCAN_ErrorStatusCallback(&hfdcan1,FDCAN_IT_BUS_OFF);
    assert(bsp_fdcan_recover()==BSP_FDCAN_BUS_OFF);
    hfdcan1.ErrorCode=HAL_FDCAN_ERROR_PROTOCOL_DATA;
    HAL_FDCAN_ErrorCallback(&hfdcan1);
    assert(bsp_fdcan_get_stats(&s)==BSP_FDCAN_OK && s.protocol_error==1);
    assert(hfdcan1.ErrorCode==HAL_FDCAN_ERROR_NONE);
    hfdcan1.Init.FrameFormat=FDCAN_FRAME_FD_NO_BRS;
    assert(bsp_fdcan_init(BSP_FDCAN_NORMAL)==BSP_FDCAN_OK);
    f=(can_frame_t){.id=0x201,.len=8,.fd=1,.brs=1};
    assert(bsp_fdcan_send(&f)==BSP_FDCAN_INVALID);
    /* RX data timing must survive NO_BRS initialization and recovery. */
    assert(regs.DBTP==0x1052u);
    f.brs=0;assert(bsp_fdcan_send(&f)==BSP_FDCAN_OK && !tx.BitRateSwitch);
    protocol.BusOff=1;HAL_FDCAN_ErrorStatusCallback(&hfdcan1,FDCAN_IT_BUS_OFF);
    regs.DBTP=0;protocol.BusOff=0;assert(bsp_fdcan_recover()==BSP_FDCAN_OK && regs.DBTP==0x1052u);
    start_error=1;assert(bsp_fdcan_init(BSP_FDCAN_NORMAL)==BSP_FDCAN_HAL_ERROR);
    assert(bsp_fdcan_send(&f)==BSP_FDCAN_NOT_READY);
    puts("PASS BSP: HAL mapping/TDC-order/queue/overflow/TX-events/busoff/recovery/init-failure");
    return 0;
}
