#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fdcan_diag.h"
#include "can_probe.h"
static uint32_t now;
static bsp_fdcan_stats_t stats;
static can_frame_t pending;
static int available;
static unsigned query8,query64,parameter;
static int drop;
static int advance_on_submit;
static unsigned pass_cases;
static uint32_t millis(void) { return now; }
static void log_line(const char *s) { if(strstr(s,"PASS QUERY")) ++pass_cases; }
bsp_fdcan_result_t can_probe_send(const can_frame_t *f) { pending=*f; pending.timestamp_ms=now; available=1; ++stats.tx_complete; ++stats.tx_enqueued; return BSP_FDCAN_OK; }
bsp_fdcan_result_t can_probe_receive(can_frame_t *f) { if(!available || drop) return BSP_FDCAN_EMPTY; *f=pending; available=0; ++stats.rx; return BSP_FDCAN_OK; }
bsp_fdcan_result_t can_probe_stats(bsp_fdcan_stats_t *s) { *s=stats; return BSP_FDCAN_OK; }
bsp_fdcan_result_t can_probe_recover(void) { return BSP_FDCAN_OK; }
bsp_fdcan_result_t motor_ro_read_parameter(uint8_t i,uint8_t brs) {
    (void)brs; assert(i!=0); ++parameter;
    pending=(can_frame_t){.id=0x601,.len=8,.fd=1,.brs=1};
    pending.data[0]=1;pending.data[1]=i;pending.data[2]= i==36 ? 1 : (i==11 ? 2 : 0);
    if(i==67) memset(pending.data+2,0xff,4);
    pending.timestamp_ms=now;
    pending.data[7]=0xff; available=1; ++stats.tx_complete; ++stats.tx_enqueued;
    return BSP_FDCAN_OK;
}
bsp_fdcan_result_t motor_ro_query_status(uint8_t broadcast,uint8_t brs) {
    (void)brs; if(broadcast) ++query64; else ++query8;
    pending=(can_frame_t){.id=1,.len=8,.fd=1,.brs=1,.timestamp_ms=now}; available=1;
    ++stats.tx_complete; ++stats.tx_enqueued;
    if(advance_on_submit) ++now;
    return BSP_FDCAN_OK;
}
int motor_ro_parameter_response(const can_frame_t *f,uint8_t i,uint32_t *raw) {
    if(f->id!=0x601||f->data[1]!=i) return 0;
    *raw=f->data[1]==67 ? 0xffffffffu : f->data[2]; return 1;
}
int motor_ro_status_response(const can_frame_t *f,uint16_t *status) {
    if(f->id!=1) return 0;
    *status=0; return 1;
}
int main(void) {
    fdcan_diag_init(FDCAN_DIAG_EXTERNAL,millis,log_line);
    for(now=0;now<200000&&!fdcan_diag_done();++now) fdcan_diag_tick();
    assert(fdcan_diag_done() && pass_cases==4 && query8==2000 && query64==2000 && parameter==20);
    memset(&stats,0,sizeof(stats)); available=0;query8=query64=parameter=pass_cases=0;drop=1;now=0;
    fdcan_diag_init(FDCAN_DIAG_EXTERNAL,millis,log_line);
    for(now=0;now<2000&&!fdcan_diag_done();++now) fdcan_diag_tick();
    assert(fdcan_diag_done() && parameter==3 && query8==0 && query64==0);
    memset(&stats,0,sizeof(stats)); available=0;query8=query64=parameter=pass_cases=0;drop=0;now=0;advance_on_submit=1;
    fdcan_diag_init(FDCAN_DIAG_EXTERNAL,millis,log_line);
    for(now=0;now<200000&&!fdcan_diag_done();++now) fdcan_diag_tick();
    assert(fdcan_diag_done() && pass_cases==4);
    memset(&stats,0,sizeof(stats)); available=0;now=0;advance_on_submit=0;
    fdcan_diag_init(FDCAN_DIAG_LOOPBACK,millis,log_line);
    for(now=0;now<30000&&!fdcan_diag_done();++now) fdcan_diag_tick();
    assert(fdcan_diag_done() && stats.tx_enqueued==8200 && stats.rx==8200);
    puts("PASS diagnostic: 4x1000 matched responses / parameter whitelist / 3-timeout abort");
    return 0;
}
