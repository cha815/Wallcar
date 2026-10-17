#include "tuning_link.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
static tuning_read_fn read_byte;
static tuning_write_fn write_frame;
static char line[96];
static uint8_t used,discard;
static const char *skip(const char *s) { while(isspace((unsigned char)*s)) ++s;return s; }
static int number(const char **p,float *value) {
    char *end;const char *start=skip(*p);
    *value=strtof(start,&end);
    if(end==start||!isfinite(*value)||(*end && !isspace((unsigned char)*end))) return 0;
    *p=end;return 1;
}
int tuning_parse(const char *s,tune_command_t *c) {
    if(!s||!c) return 0;
    memset(c,0,sizeof(*c));s=skip(s);
    if(!strncmp(s,"INFO",4)&&!*skip(s+4)) { c->kind=TUNE_INFO;return 1; }
    if(!strncmp(s,"READ",4)&&isspace((unsigned char)s[4])) {
        s+=4;c->kind=TUNE_READ;
        if(!number(&s,&c->target)||*skip(s)) return 0;
        return c->target==49||c->target==50||c->target==66||c->target==71;
    }
    const char *simple[]={"ARM","STOP","CLEAR","PING"};
    for(unsigned i=0;i<4;++i) {
        size_t len=strlen(simple[i]);
        if(!strncmp(s,simple[i],len)&&!*skip(s+len)) { c->kind=(tune_command_kind_t)i;return 1; }
    }
    if(!strncmp(s,"VEL",3)&&isspace((unsigned char)s[3])) {
        s+=3;c->kind=TUNE_VEL;
        if(!number(&s,&c->target)||!number(&s,&c->kd)||*skip(s)) return 0;
        return fabsf(c->target)<=5 && c->kd>=0 && c->kd<=2;
    }
    if(!strncmp(s,"POS",3)&&isspace((unsigned char)s[3])) {
        s+=3;c->kind=TUNE_POS;
        if(!number(&s,&c->target)||!number(&s,&c->kp)||!number(&s,&c->kd)||*skip(s)) return 0;
        return fabsf(c->target)<=0.5f && c->kp>=0 && c->kp<=5 && c->kd>=0 && c->kd<=2;
    }
    c->kind=TUNE_INVALID;return 0;
}
void tuning_link_init(tuning_read_fn read,tuning_write_fn write) { read_byte=read;write_frame=write;used=discard=0; }
int tuning_link_command(tune_command_t *c) {
    if(!read_byte||!c) return 0;
    uint8_t b;
    for(unsigned n=0;n<128 && read_byte(&b);++n) {
        if(b=='\r') continue;
        if(b=='\n') {
            line[used]=0;
            if(discard || !tuning_parse(line,c)) c->kind=TUNE_INVALID;
            used=discard=0;return 1;
        }
        if(b<32 || b>126 || used>=sizeof(line)-1) discard=1;
        else if(!discard) line[used++]=(char)b;
    }
    return 0;
}
int tuning_link_waveform(const float values[15]) {
    _Static_assert(sizeof(float)==4,"JustFloat requires float32");
    if(!values||!write_frame) return -1;
    uint8_t packet[64];memcpy(packet,values,60);
    packet[60]=packet[61]=0;packet[62]=0x80;packet[63]=0x7f;
    return write_frame(packet,sizeof(packet));
}
int tuning_link_speed_waveform(float target,float feedback) {
    _Static_assert(sizeof(float)==4,"JustFloat requires float32");
    if(!write_frame || !isfinite(target) || !isfinite(feedback)) return -1;
    const float values[2]={target,feedback};
    uint8_t packet[12];memcpy(packet,values,8);
    packet[8]=packet[9]=0;packet[10]=0x80;packet[11]=0x7f;
    return write_frame(packet,sizeof(packet));
}
