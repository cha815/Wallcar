#include "bsp_fdcan.h"
#include "fdcan.h"
#include <string.h>

static can_queue_t rx_queue;
static bsp_fdcan_stats_t stats;
static uint8_t ready, recovered, marker;
static bsp_fdcan_mode_t active_mode;

static uint32_t lock(void) {
    uint32_t key=__get_PRIMASK();
    __disable_irq();
    return key;
}
static void unlock(uint32_t key) { __set_PRIMASK(key); }
static bsp_fdcan_result_t hal_failure(void) {
    ++stats.hal_error;
    stats.last_hal_error=hfdcan1.ErrorCode;
    return BSP_FDCAN_HAL_ERROR;
}
static bsp_fdcan_result_t start(void) {
    hfdcan1.Init.Mode=active_mode==BSP_FDCAN_LOOPBACK ?
        FDCAN_MODE_INTERNAL_LOOPBACK : FDCAN_MODE_NORMAL;
    if (HAL_FDCAN_DeInit(&hfdcan1)!=HAL_OK || HAL_FDCAN_Init(&hfdcan1)!=HAL_OK)
        return hal_failure();
    /* This HAL only writes DBTP in FD_BRS mode. BRSE controls transmission;
     * replies can still use BRS, so program RX data timing while INIT/CCE are set.
     * Repeated here because HAL_DeInit also resets it during recovery. */
    if (hfdcan1.Init.FrameFormat==FDCAN_FRAME_FD_NO_BRS) {
        hfdcan1.Instance->DBTP=((hfdcan1.Init.DataSyncJumpWidth-1u)<<FDCAN_DBTP_DSJW_Pos) |
            ((hfdcan1.Init.DataTimeSeg1-1u)<<FDCAN_DBTP_DTSEG1_Pos) |
            ((hfdcan1.Init.DataTimeSeg2-1u)<<FDCAN_DBTP_DTSEG2_Pos) |
            ((hfdcan1.Init.DataPrescaler-1u)<<FDCAN_DBTP_DBRP_Pos);
    }
    /* READY-state operations must precede HAL_FDCAN_Start. */
    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,FDCAN_ACCEPT_IN_RX_FIFO0,
            FDCAN_ACCEPT_IN_RX_FIFO0,FDCAN_REJECT_REMOTE,FDCAN_REJECT_REMOTE)!=HAL_OK ||
        HAL_FDCAN_ConfigTxDelayCompensation(&hfdcan1,18,0)!=HAL_OK ||
        HAL_FDCAN_EnableTxDelayCompensation(&hfdcan1)!=HAL_OK)
        return hal_failure();
    uint32_t notifications=FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_MESSAGE_LOST |
        FDCAN_IT_TX_EVT_FIFO_NEW_DATA | FDCAN_IT_TX_EVT_FIFO_ELT_LOST |
        FDCAN_IT_BUS_OFF | FDCAN_IT_ARB_PROTOCOL_ERROR | FDCAN_IT_DATA_PROTOCOL_ERROR |
        FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE;
    if (HAL_FDCAN_ActivateNotification(&hfdcan1,notifications,0)!=HAL_OK ||
        HAL_FDCAN_Start(&hfdcan1)!=HAL_OK) return hal_failure();
    ready=1;
    return BSP_FDCAN_OK;
}
bsp_fdcan_result_t bsp_fdcan_init(bsp_fdcan_mode_t mode) {
    if (mode!=BSP_FDCAN_NORMAL && mode!=BSP_FDCAN_LOOPBACK) return BSP_FDCAN_INVALID;
    uint32_t key=lock();
    ready=recovered=marker=0;
    memset(&stats,0,sizeof(stats));
    can_queue_reset(&rx_queue);
    active_mode=mode;
    unlock(key);
    return start();
}
bsp_fdcan_result_t bsp_fdcan_send(const can_frame_t *frame) {
    if (!ready) return BSP_FDCAN_NOT_READY;
    if (stats.bus_off) return BSP_FDCAN_BUS_OFF;
    if (!can_frame_valid(frame)) return BSP_FDCAN_INVALID;
    if (frame->brs && hfdcan1.Init.FrameFormat!=FDCAN_FRAME_FD_BRS)
        return BSP_FDCAN_INVALID;
    if (!HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1)) {
        ++stats.tx_busy; return BSP_FDCAN_BUSY;
    }
    uint8_t dlc=0;
    (void)can_length_to_dlc(frame->len,&dlc);
    FDCAN_TxHeaderTypeDef h={0};
    h.Identifier=frame->id;
    h.IdType=frame->extended ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
    h.TxFrameType=FDCAN_DATA_FRAME;
    h.DataLength=dlc; /* This bundled HAL uses unshifted 0..15 DLC. */
    h.ErrorStateIndicator=FDCAN_ESI_ACTIVE;
    h.BitRateSwitch=frame->brs ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
    h.FDFormat=frame->fd ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
    h.TxEventFifoControl=FDCAN_STORE_TX_EVENTS;
    h.MessageMarker=marker++;
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1,&h,frame->data)!=HAL_OK) {
        ++stats.tx_error;
        return hal_failure();
    }
    ++stats.tx_enqueued;
    return BSP_FDCAN_OK;
}
bsp_fdcan_result_t bsp_fdcan_receive(can_frame_t *frame) {
    if (!frame) return BSP_FDCAN_INVALID;
    if (!ready) return BSP_FDCAN_NOT_READY;
    uint32_t key=lock();
    int found=can_queue_pop(&rx_queue,frame);
    unlock(key);
    return found ? BSP_FDCAN_OK : BSP_FDCAN_EMPTY;
}
bsp_fdcan_result_t bsp_fdcan_cancel(void) {
    if(!ready) return BSP_FDCAN_NOT_READY;
    if(HAL_FDCAN_AbortTxRequest(&hfdcan1,0xffu)!=HAL_OK) return hal_failure();
    return BSP_FDCAN_OK;
}
bsp_fdcan_result_t bsp_fdcan_tx_pending(uint8_t *pending) {
    if(!pending) return BSP_FDCAN_INVALID;
    if(!ready) return BSP_FDCAN_NOT_READY;
    *pending=HAL_FDCAN_IsTxBufferMessagePending(&hfdcan1,0xffu)!=0;
    return BSP_FDCAN_OK;
}
bsp_fdcan_result_t bsp_fdcan_get_stats(bsp_fdcan_stats_t *out) {
    if (!out) return BSP_FDCAN_INVALID;
    uint32_t key=lock();
    if (ready) {
        FDCAN_ProtocolStatusTypeDef p;
        FDCAN_ErrorCountersTypeDef e;
        if (HAL_FDCAN_GetProtocolStatus(&hfdcan1,&p)!=HAL_OK ||
            HAL_FDCAN_GetErrorCounters(&hfdcan1,&e)!=HAL_OK) {
            (void)hal_failure(); *out=stats; unlock(key); return BSP_FDCAN_HAL_ERROR;
        }
        if (p.BusOff && !stats.bus_off) ++stats.bus_off_count;
        if (p.BusOff) stats.bus_off=1; /* Latch until explicit recovery. */
        stats.error_passive=(uint8_t)p.ErrorPassive;
        stats.warning=(uint8_t)p.Warning;
        stats.tx_error_counter=(uint8_t)e.TxErrorCnt;
        stats.rx_error_counter=(uint8_t)e.RxErrorCnt;
        if (p.LastErrorCode!=7) stats.last_error_code=(uint8_t)p.LastErrorCode;
        if (p.DataLastErrorCode!=7) stats.data_error_code=(uint8_t)p.DataLastErrorCode;
    }
    *out=stats;
    unlock(key);
    return BSP_FDCAN_OK;
}
bsp_fdcan_result_t bsp_fdcan_recover(void) {
    if (!stats.bus_off) return BSP_FDCAN_INVALID;
    if (recovered) return BSP_FDCAN_BUS_OFF;
    recovered=1; ready=0;
    uint32_t key=lock(); can_queue_reset(&rx_queue); unlock(key);
    bsp_fdcan_result_t result=start();
    if (result==BSP_FDCAN_OK) {
        key=lock(); stats.bus_off=0; ++stats.recoveries; unlock(key);
    }
    return result;
}
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *h,uint32_t flags) {
    if (h!=&hfdcan1) return;
    if (flags&FDCAN_IT_RX_FIFO0_MESSAGE_LOST) ++stats.rx_hw_lost;
    while (HAL_FDCAN_GetRxFifoFillLevel(h,FDCAN_RX_FIFO0)) {
        FDCAN_RxHeaderTypeDef header;
        can_frame_t frame={0};
        if (HAL_FDCAN_GetRxMessage(h,FDCAN_RX_FIFO0,&header,frame.data)!=HAL_OK) {
            (void)hal_failure(); break;
        }
        ++stats.rx;
        frame.id=header.Identifier;
        frame.extended=header.IdType==FDCAN_EXTENDED_ID;
        frame.fd=header.FDFormat==FDCAN_FD_CAN;
        frame.brs=header.BitRateSwitch==FDCAN_BRS_ON;
        frame.timestamp_ms=HAL_GetTick();
        if (header.RxFrameType!=FDCAN_DATA_FRAME ||
            !can_dlc_to_length(header.DataLength,&frame.len) || !can_frame_valid(&frame)) {
            ++stats.rx_reject; continue;
        }
        if (!can_queue_push(&rx_queue,&frame)) ++stats.rx_drop;
    }
}
void HAL_FDCAN_TxEventFifoCallback(FDCAN_HandleTypeDef *h,uint32_t flags) {
    if (h!=&hfdcan1) return;
    if (flags&FDCAN_IT_TX_EVT_FIFO_ELT_LOST) ++stats.tx_event_lost;
    while (h->Instance->TXEFS & FDCAN_TXEFS_EFFL) {
        FDCAN_TxEventFifoTypeDef event;
        if (HAL_FDCAN_GetTxEvent(h,&event)!=HAL_OK) { (void)hal_failure(); break; }
        ++stats.tx_complete;
    }
}
void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *h,uint32_t flags) {
    if (h!=&hfdcan1) return;
    stats.last_error_flags=flags;
    if (flags&FDCAN_IT_BUS_OFF) {
        FDCAN_ProtocolStatusTypeDef p;
        if (HAL_FDCAN_GetProtocolStatus(h,&p)==HAL_OK && p.BusOff) {
            if (!stats.bus_off) ++stats.bus_off_count;
            stats.bus_off=1;
        }
    }
    if (flags&(FDCAN_IT_ARB_PROTOCOL_ERROR|FDCAN_IT_DATA_PROTOCOL_ERROR)) ++stats.protocol_error;
}
void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *h) {
    if (h!=&hfdcan1) return;
    /* This HAL routes PEA/PED to ErrorCallback, not ErrorStatusCallback.
       ErrorCode is sticky: consume it so later RX interrupts are not counted again. */
    uint32_t errors=h->ErrorCode;
    stats.last_hal_error=errors;
    if (errors&(HAL_FDCAN_ERROR_PROTOCOL_ARBT|HAL_FDCAN_ERROR_PROTOCOL_DATA)) ++stats.protocol_error;
    if (errors&~(HAL_FDCAN_ERROR_PROTOCOL_ARBT|HAL_FDCAN_ERROR_PROTOCOL_DATA)) ++stats.hal_error;
    h->ErrorCode=HAL_FDCAN_ERROR_NONE;
}
