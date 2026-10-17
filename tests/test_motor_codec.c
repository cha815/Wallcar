#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "motor_codec.h"
int main(void) {
    uint8_t d[8];
    motor_command_t c={.position=0,.velocity=0,.kp=0,.kd=0,.torque=0};
    assert(motor_codec_pack(&c,d));
    const uint8_t zero[]={0x7f,0xff,0x7f,0xf0,0,0,7,0xff};
    for(unsigned i=0;i<8;++i) assert(d[i]==zero[i]);
    motor_command_t q;
    assert(motor_codec_quantize(&c,&q));
    assert(q.velocity<0&&fabsf(q.velocity)<0.04f&&q.kp==0);
    c.kp=500;c.kd=5;c.position=12.5f;c.velocity=65;c.torque=18;
    assert(motor_codec_pack(&c,d));
    for(unsigned i=0;i<8;++i) assert(d[i]==0xff);
    c.velocity=66;assert(!motor_codec_pack(&c,d));
    c.velocity=NAN;assert(!motor_codec_pack(&c,d));
    motor_feedback_t fb;
    const uint8_t feedback[]={0x7f,0xff,0x7f,0xf7,0xff,65,0,1};
    assert(motor_codec_feedback(feedback,&fb));
    assert(fabsf(fb.position)<0.001 && fabsf(fb.velocity)<0.04 && fabsf(fb.torque)<0.01);
    assert(fb.temperature==25 && fb.enabled && fb.fault==0);
    puts("PASS motor codec: MIT quantization/finite/range/feedback/status");
}
