#ifndef MOCK_FDCAN_H
#define MOCK_FDCAN_H
#include <stdint.h>
#include <string.h>
typedef enum { HAL_OK=0, HAL_ERROR=1 } HAL_StatusTypeDef;
typedef struct { uint32_t TXEFS, DBTP; } MockRegs;
typedef struct { uint32_t Mode, FrameFormat, DataPrescaler, DataSyncJumpWidth, DataTimeSeg1, DataTimeSeg2; } MockInit;
typedef struct { MockRegs *Instance; MockInit Init; uint32_t ErrorCode; } FDCAN_HandleTypeDef;
typedef struct { uint32_t Identifier,IdType,TxFrameType,DataLength,ErrorStateIndicator,BitRateSwitch,FDFormat,TxEventFifoControl,MessageMarker; } FDCAN_TxHeaderTypeDef;
typedef struct { uint32_t Identifier,IdType,RxFrameType,DataLength,BitRateSwitch,FDFormat; } FDCAN_RxHeaderTypeDef;
typedef struct { uint32_t MessageMarker; } FDCAN_TxEventFifoTypeDef;
typedef struct { uint32_t BusOff,ErrorPassive,Warning,LastErrorCode,DataLastErrorCode; } FDCAN_ProtocolStatusTypeDef;
typedef struct { uint32_t TxErrorCnt,RxErrorCnt; } FDCAN_ErrorCountersTypeDef;
extern FDCAN_HandleTypeDef hfdcan1;
#define FDCAN_MODE_NORMAL 0u
#define FDCAN_FRAME_FD_NO_BRS 0x100u
#define FDCAN_FRAME_FD_BRS 0x300u
#define FDCAN_DBTP_DSJW_Pos 0u
#define FDCAN_DBTP_DTSEG2_Pos 4u
#define FDCAN_DBTP_DTSEG1_Pos 8u
#define FDCAN_DBTP_DBRP_Pos 16u
#define FDCAN_MODE_INTERNAL_LOOPBACK 1u
#define FDCAN_ACCEPT_IN_RX_FIFO0 0u
#define FDCAN_REJECT_REMOTE 1u
#define FDCAN_RX_FIFO0 0u
#define FDCAN_STANDARD_ID 0u
#define FDCAN_EXTENDED_ID 1u
#define FDCAN_DATA_FRAME 0u
#define FDCAN_REMOTE_FRAME 1u
#define FDCAN_FD_CAN 1u
#define FDCAN_CLASSIC_CAN 0u
#define FDCAN_BRS_ON 1u
#define FDCAN_BRS_OFF 0u
#define FDCAN_ESI_ACTIVE 0u
#define FDCAN_STORE_TX_EVENTS 1u
#define FDCAN_TXEFS_EFFL 0x3fu
#define FDCAN_IT_RX_FIFO0_NEW_MESSAGE (1u<<0)
#define FDCAN_IT_RX_FIFO0_MESSAGE_LOST (1u<<1)
#define FDCAN_IT_TX_EVT_FIFO_NEW_DATA (1u<<2)
#define FDCAN_IT_TX_EVT_FIFO_ELT_LOST (1u<<3)
#define FDCAN_IT_BUS_OFF (1u<<4)
#define FDCAN_IT_ARB_PROTOCOL_ERROR (1u<<5)
#define FDCAN_IT_DATA_PROTOCOL_ERROR (1u<<6)
#define FDCAN_IT_ERROR_PASSIVE (1u<<7)
#define FDCAN_IT_ERROR_WARNING (1u<<8)
#define HAL_FDCAN_ERROR_PROTOCOL_ARBT FDCAN_IT_ARB_PROTOCOL_ERROR
#define HAL_FDCAN_ERROR_PROTOCOL_DATA FDCAN_IT_DATA_PROTOCOL_ERROR
#define HAL_FDCAN_ERROR_NONE 0u
static inline uint32_t __get_PRIMASK(void) { return 0; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t x) { (void)x; }
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_FDCAN_Init(FDCAN_HandleTypeDef *h);
HAL_StatusTypeDef HAL_FDCAN_DeInit(FDCAN_HandleTypeDef *h);
HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(FDCAN_HandleTypeDef *h,uint32_t a,uint32_t b,uint32_t c,uint32_t d);
HAL_StatusTypeDef HAL_FDCAN_ConfigTxDelayCompensation(FDCAN_HandleTypeDef *h,uint32_t a,uint32_t b);
HAL_StatusTypeDef HAL_FDCAN_EnableTxDelayCompensation(FDCAN_HandleTypeDef *h);
HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(FDCAN_HandleTypeDef *h,uint32_t a,uint32_t b);
HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *h);
uint32_t HAL_FDCAN_GetTxFifoFreeLevel(const FDCAN_HandleTypeDef *h);
HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef *h,const FDCAN_TxHeaderTypeDef *t,const uint8_t *d);
uint32_t HAL_FDCAN_GetRxFifoFillLevel(const FDCAN_HandleTypeDef *h,uint32_t fifo);
HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(FDCAN_HandleTypeDef *h,uint32_t fifo,FDCAN_RxHeaderTypeDef *r,uint8_t *d);
HAL_StatusTypeDef HAL_FDCAN_GetTxEvent(FDCAN_HandleTypeDef *h,FDCAN_TxEventFifoTypeDef *event);
HAL_StatusTypeDef HAL_FDCAN_GetProtocolStatus(const FDCAN_HandleTypeDef *h,FDCAN_ProtocolStatusTypeDef *p);
HAL_StatusTypeDef HAL_FDCAN_GetErrorCounters(const FDCAN_HandleTypeDef *h,FDCAN_ErrorCountersTypeDef *p);
HAL_StatusTypeDef HAL_FDCAN_AbortTxRequest(FDCAN_HandleTypeDef *h,uint32_t mask);
uint32_t HAL_FDCAN_IsTxBufferMessagePending(const FDCAN_HandleTypeDef *h,uint32_t mask);
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *h,uint32_t flags);
void HAL_FDCAN_TxEventFifoCallback(FDCAN_HandleTypeDef *h,uint32_t flags);
void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *h,uint32_t flags);
void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *h);
#endif
