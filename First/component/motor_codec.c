#include "motor_codec.h"
#include "mw_canQZB.h"
#include <math.h>
#include <stddef.h>
static int valid(float v,float lo,float hi) { return isfinite(v) && v>=lo && v<=hi; }
static float decode(uint16_t v,float lo,float hi,uint16_t scale) { return lo+(float)v/scale*(hi-lo); }
int motor_codec_pack(const motor_command_t *c,uint8_t d[8]) {
    if(!c||!d||!valid(c->position,-12.5f,12.5f)||!valid(c->velocity,-65,65)||
       !valid(c->kp,0,500)||!valid(c->kd,0,5)||!valid(c->torque,-18,18)) return 0;
    mwQZB_pack_mit(d,c->position,c->velocity,c->kp,c->kd,c->torque);
    return 1;
}
int motor_codec_feedback(const uint8_t d[8],motor_feedback_t *f) {
    if(!d||!f) return 0;
    f->position=decode((uint16_t)((d[0]<<8)|d[1]),-12.5f,12.5f,65535);
    f->velocity=decode((uint16_t)((d[2]<<4)|(d[3]>>4)),-65,65,4095);
    f->torque=decode((uint16_t)(((d[3]&15)<<8)|d[4]),-18,18,4095);
    f->temperature=(int16_t)d[5]-40;
    f->status=(uint16_t)((d[6]<<8)|d[7]);f->enabled=(uint8_t)(f->status&1u);
    f->fault=(uint16_t)(f->status&0xfffeu);
    return 1;
}
int motor_codec_quantize(const motor_command_t *command,motor_command_t *out) {
    uint8_t d[8];
    if(!out||!motor_codec_pack(command,d)) return 0;
    out->position=decode((uint16_t)((d[0]<<8)|d[1]),-12.5f,12.5f,65535);
    out->velocity=decode((uint16_t)((d[2]<<4)|(d[3]>>4)),-65,65,4095);
    out->kp=decode((uint16_t)(((d[3]&15)<<8)|d[4]),0,500,4095);
    out->kd=decode((uint16_t)((d[5]<<4)|(d[6]>>4)),0,5,4095);
    out->torque=decode((uint16_t)(((d[6]&15)<<8)|d[7]),-18,18,4095);
    return 1;
}
/* 每节点真实量程校验，拒绝非有限值与退化映射。 */
static int span_valid(float lo,float hi) { return isfinite(lo)&&isfinite(hi)&&lo<hi&&isfinite(hi-lo); }
int motor_codec_limits_valid(const motor_limits_t *l) {
    return l&&span_valid(l->position_min,l->position_max)&&span_valid(l->velocity_min,l->velocity_max)&&
        span_valid(l->kp_min,l->kp_max)&&span_valid(l->kd_min,l->kd_max)&&span_valid(l->torque_min,l->torque_max);
}
static uint16_t encode_range(float value,float lo,float hi,uint16_t scale) {
    if(value<=lo) return 0;
    if(value>=hi) return scale;
    return (uint16_t)((value-lo)/(hi-lo)*scale);
}
int motor_codec_pack_limits(const motor_command_t *c,const motor_limits_t *l,uint8_t d[8]) {
    if(!c||!d||!motor_codec_limits_valid(l)||!valid(c->position,l->position_min,l->position_max)||
       !valid(c->velocity,l->velocity_min,l->velocity_max)||!valid(c->kp,l->kp_min,l->kp_max)||
       !valid(c->kd,l->kd_min,l->kd_max)||!valid(c->torque,l->torque_min,l->torque_max)) return 0;
    uint16_t p=encode_range(c->position,l->position_min,l->position_max,65535);
    uint16_t v=encode_range(c->velocity,l->velocity_min,l->velocity_max,4095);
    uint16_t kp=encode_range(c->kp,l->kp_min,l->kp_max,4095);
    uint16_t kd=encode_range(c->kd,l->kd_min,l->kd_max,4095);
    uint16_t t=encode_range(c->torque,l->torque_min,l->torque_max,4095);
    d[0]=(uint8_t)(p>>8);d[1]=(uint8_t)p;d[2]=(uint8_t)(v>>4);d[3]=(uint8_t)((v<<4)|(kp>>8));
    d[4]=(uint8_t)kp;d[5]=(uint8_t)(kd>>4);d[6]=(uint8_t)((kd<<4)|(t>>8));d[7]=(uint8_t)t;return 1;
}
int motor_codec_feedback_limits(const uint8_t d[8],const motor_limits_t *l,motor_feedback_t *f) {
    if(!motor_codec_limits_valid(l)||!motor_codec_feedback(d,f)) return 0;
    f->position=decode((uint16_t)((d[0]<<8)|d[1]),l->position_min,l->position_max,65535);
    f->velocity=decode((uint16_t)((d[2]<<4)|(d[3]>>4)),l->velocity_min,l->velocity_max,4095);
    f->torque=decode((uint16_t)(((d[3]&15)<<8)|d[4]),l->torque_min,l->torque_max,4095);
    return 1;
}
