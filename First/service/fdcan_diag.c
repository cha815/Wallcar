#include "fdcan_diag.h"
#include "can_probe.h"
#include "motor_ro.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Parameter 0 is deliberately absent: even a read would save motor Flash. */
static const uint8_t indices[]={10,11,24,25,26,27,28,29,30,31,32,33,34,35,36,37,45,56,67,69};
static const uint8_t lengths[]={0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
static fdcan_diag_mode_t mode;
static fdcan_diag_clock_fn clock_ms;
static fdcan_diag_log_fn logger;
static uint8_t finished,waiting,matched,consecutive,stage,case_no,param_no,rx_fd,rx_brs,rx_len;
static uint32_t sent_ms,next_ms,last_summary,tx_before,requests,responses,timeouts,sample;
static bsp_fdcan_stats_t case_start;
static can_frame_t expected;

static void logf_line(const char *format,...) {
    char text[256];
    va_list args; va_start(args,format);
    vsnprintf(text,sizeof(text),format,args); va_end(args);
    if (logger) logger(text);
}
static int counters_clean(const bsp_fdcan_stats_t *s) {
    return s->rx_drop==case_start.rx_drop && s->rx_hw_lost==case_start.rx_hw_lost &&
        s->rx_reject==case_start.rx_reject && s->tx_event_lost==case_start.tx_event_lost &&
        s->bus_off_count==case_start.bus_off_count && s->hal_error==case_start.hal_error &&
        s->protocol_error==case_start.protocol_error && !s->bus_off &&
        !s->tx_error_counter && !s->rx_error_counter;
}
static void summary(const char *tag,const bsp_fdcan_stats_t *s) {
    logf_line("%s stage=%u case=%u req=%lu rsp=%lu timeout=%lu tx=%lu done=%lu rx=%lu drop=%lu lost=%lu evt=%lu bo=%lu pe=%lu hal=%lu TEC=%u REC=%u LEC=%u DLEC=%u\r\n",
        tag,stage,case_no,(unsigned long)requests,(unsigned long)responses,(unsigned long)timeouts,
        (unsigned long)s->tx_enqueued,(unsigned long)s->tx_complete,(unsigned long)s->rx,
        (unsigned long)s->rx_drop,(unsigned long)s->rx_hw_lost,(unsigned long)s->tx_event_lost,
        (unsigned long)s->bus_off_count,(unsigned long)s->protocol_error,(unsigned long)s->hal_error,
        s->tx_error_counter,s->rx_error_counter,s->last_error_code,s->data_error_code);
}
static void stop_test(const char *reason,const bsp_fdcan_stats_t *s) {
    logf_line("FAIL reason=%s\r\n",reason); summary("FINAL",s); finished=1;
    if (s->bus_off) logf_line("RECOVER attempt=1 result=%u; test remains failed\r\n",can_probe_recover());
}
static void reset_case(uint32_t now) {
    requests=responses=timeouts=sample=0;
    consecutive=waiting=matched=0;
    next_ms=now; last_summary=now;
    (void)can_probe_stats(&case_start);
}
static void loop_frame(void) {
    memset(&expected,0,sizeof(expected));
    unsigned n=case_no;
    if (n<18) {
        expected.len=lengths[n/2]; expected.extended=n%2;
    } else {
        n-=18; expected.len=lengths[n/4]; expected.extended=(n/2)%2;
        expected.fd=1; expected.brs=n%2;
    }
    expected.id=expected.extended ? 0x1234567u : 0x321u;
    for(unsigned i=0;i<expected.len;++i)
        expected.data[i]=(uint8_t)(sample*17u+i*29u+case_no);
}
void fdcan_diag_init(fdcan_diag_mode_t test_mode,fdcan_diag_clock_fn clock,fdcan_diag_log_fn log) {
    mode=test_mode; clock_ms=clock; logger=log;
    finished=0; stage=0; case_no=param_no=0;
    if (!clock || !log || (mode!=FDCAN_DIAG_LOOPBACK && mode!=FDCAN_DIAG_EXTERNAL)) { finished=1; return; }
    reset_case(clock_ms());
    logf_line("FDCAN_DIAG H723 mode=%u nominal=1000000 data=5000000 ID1_READ_ONLY\r\n",mode);
}
int fdcan_diag_done(void) { return finished; }
void fdcan_diag_tick(void) {
    if (finished) return;
    uint32_t now=clock_ms();
    bsp_fdcan_stats_t s;
    if (can_probe_stats(&s)!=BSP_FDCAN_OK) { finished=1; logf_line("FAIL stats\r\n"); return; }
    if (s.bus_off) { stop_test("BUS_OFF",&s); return; }
    if ((uint32_t)(now-last_summary)>=1000) { summary("PROGRESS",&s); last_summary=now; }
    can_frame_t f;
    /* Drain every tick, including between transactions: never consume stale queued responses. */
    while(can_probe_receive(&f)==BSP_FDCAN_OK) {
        if (!waiting || matched || (int32_t)(f.timestamp_ms-sent_ms)<0) continue;
        int valid=0;
        if (mode==FDCAN_DIAG_LOOPBACK) {
            valid=f.id==expected.id && f.len==expected.len && f.extended==expected.extended &&
                f.fd==expected.fd && f.brs==expected.brs && !memcmp(f.data,expected.data,f.len);
            if (!valid) { stop_test("LOOPBACK_MISMATCH",&s); return; }
        } else if (!stage) {
            uint32_t raw;
            valid=motor_ro_parameter_response(&f,indices[param_no],&raw);
            if (valid) {
                logf_line("PARAM index=%u raw=0x%08lX RX_fd=%u RX_brs=%u RX_len=%u\r\n",
                    indices[param_no],(unsigned long)raw,f.fd,f.brs,f.len);
                if ((indices[param_no]==36 && raw!=1) || (indices[param_no]==11 && raw!=2) ||
                    (indices[param_no]==67 && raw!=0xffffffffu)) {
                    stop_test("ID_MODE_OR_FEEDBACK_ID_MISMATCH",&s); return;
                }
            }
        } else {
            uint16_t status;
            valid=motor_ro_status_response(&f,&status);
            if (valid && status!=0) { stop_test("MOTOR_ENABLED_OR_FAULT",&s); return; }
        }
        if(valid) { matched=1; rx_fd=f.fd;rx_brs=f.brs;rx_len=f.len; }
    }
    if (waiting && matched && s.tx_complete>tx_before) {
        waiting=0; ++responses; ++sample; consecutive=0;
        next_ms=now+(mode==FDCAN_DIAG_LOOPBACK ? 1u : 20u);
        if (mode==FDCAN_DIAG_EXTERNAL && !stage) {
            ++param_no;
            if (param_no==sizeof(indices)) { stage=1;case_no=0;reset_case(now); }
        } else if(sample==(mode==FDCAN_DIAG_LOOPBACK ? 100u : 1000u)) {
            if (!counters_clean(&s) || timeouts) { stop_test("CASE_COUNTERS",&s); return; }
            logf_line("PASS %s case=%u TX_fd=%u TX_brs=%u TX_len=%u RX_fd=%u RX_brs=%u RX_len=%u req=%lu rsp=%lu timeout=0\r\n",
                mode==FDCAN_DIAG_LOOPBACK ? "LOOPBACK" : "QUERY",case_no,
                mode==FDCAN_DIAG_LOOPBACK ? expected.fd : 1u,
                mode==FDCAN_DIAG_LOOPBACK ? expected.brs : case_no/2u,
                mode==FDCAN_DIAG_LOOPBACK ? expected.len : (case_no%2 ? 64u : 8u),
                rx_fd,rx_brs,rx_len,(unsigned long)requests,(unsigned long)responses);
            ++case_no;
            if (case_no==(mode==FDCAN_DIAG_LOOPBACK ? 82u : 4u)) {
                summary("ALL_CASES_PASS",&s); finished=1; return;
            }
            reset_case(now);
        }
    } else if(waiting && (uint32_t)(now-sent_ms)>=100u) {
        ++timeouts; ++consecutive; waiting=0; matched=0;
        logf_line("TIMEOUT stage=%u case=%u index=%u consecutive=%u\r\n",stage,case_no,
            stage ? 0u : indices[param_no],consecutive);
        next_ms=now+20;
        /* No wire sequence number exists; any timeout disqualifies this case from PASS. */
        if (consecutive>=3 || mode==FDCAN_DIAG_LOOPBACK) { stop_test("RESPONSE_TIMEOUT_OR_QUERY_UNSUPPORTED",&s); return; }
    }
    if (!waiting && (int32_t)(now-next_ms)>=0) {
        bsp_fdcan_result_t result;
        sent_ms=now; /* A reply can arrive in an ISR before send() returns. */
        tx_before=s.tx_complete;
        if (mode==FDCAN_DIAG_LOOPBACK) { loop_frame(); result=can_probe_send(&expected); }
        else if (!stage) result=motor_ro_read_parameter(indices[param_no],0);
        else result=motor_ro_query_status(case_no%2,case_no/2);
        if (result==BSP_FDCAN_BUSY) { stop_test("TX_BUSY",&s); return; }
        if (result!=BSP_FDCAN_OK) { stop_test("TX_ERROR",&s); return; }
        ++requests; waiting=1; matched=0;
    }
}
