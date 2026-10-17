/* 作者：Codex；文件：can_link_probe.c；说明：仅DIAG_MODE7使用的ID2只读链路比较。
 * 直接装配BSP/HAL用于硬件诊断，不进入正常产品镜像；无使能、运动或参数写入。
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "can_link_probe.h"
#include "bsp_fdcan.h"
#include "bsp_uart_log.h"
#include "fdcan.h"

static const char *names[]={"FD_NO_BRS_RX5M","FD_BRS_5M","FD_BRS_4M","CLASSIC_1M"};
static const uint8_t indices[]={10,11,21,24,25,26,27,28,29,30,31,34,35,36,37,56,67,69,70,71};
enum { PARAMETER_COUNT=sizeof(indices) };
static uint32_t phase_ms,sent_ms,baseline,tx_edges,rx_edges;
static uint8_t profile,request,started,waiting,matched,finished,last_tx,last_rx;

static void log_line(const char *format,...) {
    char line[256];va_list args;va_start(args,format);
    vsnprintf(line,sizeof(line),format,args);va_end(args);bsp_uart_log_write(line);
}
static void sample_pins(void) {
    uint8_t tx=(uint8_t)HAL_GPIO_ReadPin(GPIOD,GPIO_PIN_1);
    uint8_t rx=(uint8_t)HAL_GPIO_ReadPin(GPIOD,GPIO_PIN_0);
    if(tx!=last_tx) { ++tx_edges;last_tx=tx; }
    if(rx!=last_rx) { ++rx_edges;last_rx=rx; }
}
static void end_profile(uint32_t now,const bsp_fdcan_stats_t *s) {
    log_line("SUMMARY %s queued=%lu done=%lu rx=%lu TEC=%u REC=%u LEC=%u DLEC=%u busoff=%u TX_edges=%lu RX_edges=%lu TX_pin=%u RX_pin=%u\r\n",
        names[profile],(unsigned long)s->tx_enqueued,(unsigned long)s->tx_complete,(unsigned long)s->rx,
        s->tx_error_counter,s->rx_error_counter,s->last_error_code,s->data_error_code,s->bus_off,
        (unsigned long)tx_edges,(unsigned long)rx_edges,last_tx,last_rx);
    /* 只取消本诊断的未完成只读帧，不发送任何电机状态切换命令。 */
    (void)bsp_fdcan_cancel();
    if(++profile==4||(profile==1&&s->rx)) { finished=1;log_line("LINK_PROBE_DONE ID2_READ_ONLY\r\n"); }
    started=waiting=0;request=0;phase_ms=now;
}
void can_link_probe_init(uint32_t now) {
    profile=request=started=waiting=matched=finished=0;phase_ms=now;
    tx_edges=rx_edges=0;last_tx=last_rx=0;
    log_line("LINK_PROBE ID2_READ_ONLY no_enable no_motion no_parameter_write\r\n");
}
void can_link_probe_tick(uint32_t now) {
    if(finished) return;
    if(!started) {
        if((uint32_t)(now-phase_ms)<1500) return;
        hfdcan1.Init.FrameFormat=profile==3?FDCAN_FRAME_CLASSIC:
            (profile==0?FDCAN_FRAME_FD_NO_BRS:FDCAN_FRAME_FD_BRS);
        hfdcan1.Init.DataTimeSeg1=profile==2?21:17;
        hfdcan1.Init.DataTimeSeg2=profile==2?8:6;
        bsp_fdcan_result_t r=bsp_fdcan_init(BSP_FDCAN_NORMAL);
        log_line("PROFILE %s init=%u nominal=1000000 data=%lu\r\n",names[profile],r,
            (unsigned long)(profile==2?4000000:5000000));
        if(r!=BSP_FDCAN_OK) { finished=1;return; }
        tx_edges=rx_edges=0;last_tx=(uint8_t)HAL_GPIO_ReadPin(GPIOD,GPIO_PIN_1);
        last_rx=(uint8_t)HAL_GPIO_ReadPin(GPIOD,GPIO_PIN_0);started=1;
    }
    sample_pins();
    can_frame_t f;
    while(bsp_fdcan_receive(&f)==BSP_FDCAN_OK) {
        char hex[193];size_t at=0;
        for(unsigned i=0;i<f.len;++i) at+=(size_t)snprintf(hex+at,sizeof(hex)-at,"%02X ",f.data[i]);
        hex[at]=0;
        log_line("RX id=0x%03lX len=%u fd=%u brs=%u data=%s\r\n",(unsigned long)f.id,f.len,f.fd,f.brs,hex);
        if(waiting&&(int32_t)(f.timestamp_ms-sent_ms)>=0&&f.len==8&&!f.extended) {
            uint8_t index=request<PARAMETER_COUNT?indices[request]:0;
            if((request==PARAMETER_COUNT&&f.id==2)||(request<PARAMETER_COUNT&&f.id==0x602&&f.data[0]==2&&
                f.data[1]==index&&f.data[6]==0&&f.data[7]==0xff)) matched=1;
        }
    }
    bsp_fdcan_stats_t s;
    if(bsp_fdcan_get_stats(&s)!=BSP_FDCAN_OK) { finished=1;log_line("FAIL stats\r\n");return; }
    if(waiting) {
        if((matched&&s.tx_complete>baseline)||(uint32_t)(now-sent_ms)>=150) {
            log_line("RESULT %s request=%u ack=%u matched=%u TEC=%u REC=%u LEC=%u DLEC=%u\r\n",
                names[profile],request,s.tx_complete>baseline,matched,s.tx_error_counter,s.rx_error_counter,
                s.last_error_code,s.data_error_code);
            waiting=0;
            if(s.tx_complete==baseline||++request==PARAMETER_COUNT+1) { end_profile(now,&s);return; }
        } else return;
    }
    f=(can_frame_t){.id=request==PARAMETER_COUNT?0x102u:0x602u,.len=8,.fd=profile!=3,.brs=profile==1||profile==2};
    if(request==PARAMETER_COUNT) { memset(f.data,0xff,8);f.data[7]=0xfa; }
    else { f.data[0]=0x67;f.data[1]=indices[request];f.data[6]=4;f.data[7]=0x76; }
    baseline=s.tx_complete;sent_ms=now;matched=0;
    bsp_fdcan_result_t r=bsp_fdcan_send(&f);
    log_line("TX %s id=0x%03lX request=%u result=%u\r\n",names[profile],(unsigned long)f.id,request,r);
    if(r!=BSP_FDCAN_OK) { end_profile(now,&s);return; }
    waiting=1;
}
