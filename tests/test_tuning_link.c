#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tuning_link.h"
static uint8_t bytes[64];static uint16_t length;
static int writer(const uint8_t *data,uint16_t len) { memcpy(bytes,data,len);length=len;return 1; }
int main(void) {
    tune_command_t c;
    assert(tuning_parse("VEL 1.5 0.5",&c)&&c.kind==TUNE_VEL&&c.target==1.5f&&c.kd==0.5f&&c.kp==0);
    assert(tuning_parse("POS 0.1 5 0.2",&c)&&c.kind==TUNE_POS&&c.kp==5);
    assert(tuning_parse("STOP",&c)&&c.kind==TUNE_STOP);
    assert(!tuning_parse("VEL nan 1",&c));
    assert(!tuning_parse("VEL 6 1",&c));
    assert(!tuning_parse("POS 0.1 5 0.2 junk",&c));
    assert(!tuning_parse("STOP ARM",&c));
    tuning_link_init(NULL,writer);
    float values[15]={1,2,3};assert(tuning_link_waveform(values)==1&&length==64);
    assert(bytes[0]==0&&bytes[1]==0&&bytes[2]==0x80&&bytes[3]==0x3f);
    assert(bytes[60]==0&&bytes[61]==0&&bytes[62]==0x80&&bytes[63]==0x7f);
    const uint8_t speed_packet[12]={0,0,0x20,0x41,0,0,0x10,0x41,0,0,0x80,0x7f};
    assert(tuning_link_speed_waveform(10,9)==1 && length==12);
    assert(memcmp(bytes,speed_packet,12)==0);
    puts("PASS tuning link: strict commands/limits/NaN/JustFloat layout");
}
