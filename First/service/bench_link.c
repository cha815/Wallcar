/* 作者：Codex；文件：bench_link.c；说明：有界命令解析，无硬件或业务依赖。 */
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bench_link.h"

static const char *skip(const char *p) { while(isspace((unsigned char)*p)) ++p;return p; }
int bench_parse(const char *line,bench_command_t *c) {
    if(!line||!c) return 0;
    memset(c,0,sizeof(*c));const char *s=skip(line);
    if(!strncmp(s,"ENABLE",6)&&isspace((unsigned char)s[6])) { c->kind=BENCH_ENABLE;s+=6; }
    else if(!strncmp(s,"DISABLE",7)&&isspace((unsigned char)s[7])) { c->kind=BENCH_DISABLE;s+=7; }
    else if(!strncmp(s,"VEL",3)&&isspace((unsigned char)s[3])) { c->kind=BENCH_VEL;s+=3; }
    else return 0;
    s=skip(s);char *end;unsigned long id=strtoul(s,&end,10);
    if(end==s||id<1||id>255||(*end&&!isspace((unsigned char)*end))) return 0;
    c->id=(uint8_t)id;s=skip(end);
    if(c->kind!=BENCH_VEL) return !*s;
    c->velocity=strtof(s,&end);
    if(end==s||!isfinite(c->velocity)||fabsf(c->velocity)>65||!isspace((unsigned char)*end)) return 0;
    s=skip(end);c->kd=strtof(s,&end);
    return end!=s&&!*skip(end)&&isfinite(c->kd)&&c->kd>=0&&c->kd<=5;
}
void bench_link_init(bench_link_t *l,int (*read)(uint8_t *),int (*write)(const uint8_t *,uint16_t)) {
    memset(l,0,sizeof(*l));l->read=read;l->write=write;
}
/* 最多128字节/轮，溢出整行丢弃，不解析尾部为新命令。 */
int bench_link_read(bench_link_t *l,bench_command_t *c) {
    uint8_t b;
    for(unsigned n=0;n<128&&l->read(&b);++n) {
        if(b=='\r') continue;
        if(b=='\n') {
            l->line[l->used]=0;
            if(l->discard||!bench_parse(l->line,c)) *c=(bench_command_t){.kind=BENCH_INVALID};
            l->used=l->discard=0;return 1;
        }
        if(b<32||b>126||l->used>=sizeof(l->line)-1) l->discard=1;
        else if(!l->discard) l->line[l->used++]=(char)b;
    }
    return 0;
}
/* ARM Cortex-M与本轮主机均为小端IEEE754 float32，尾标与VOFA兼容。 */
int bench_link_write(bench_link_t *l,const float values[15]) {
    _Static_assert(sizeof(float)==4,"float32 telemetry required");
    uint8_t data[64];memcpy(data,values,60);
    data[60]=data[61]=0;data[62]=0x80;data[63]=0x7f;
    return l->write(data,64);
}
