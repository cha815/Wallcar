/* 作者：Codex；文件：test_motor_instances.c；说明：注册与上层接口到设备报文集成验证。 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "First/interfaces/motor_registry.h"
#include "First/device/qzb_motor.h"
#include "First/service/bench_link.h"

static uint32_t now,tx;
static can_queue_t rx;
static uint8_t enabled[2],wrong_mode,wrong_id,busy,custom_id2,duplicate_master;
static can_frame_t last;
static can_service_t service;
static qzb_motor_t nodes[2];
static motor_registry_t registry;

static void feedback(unsigned i) {
    can_frame_t f={.id=i+1,.len=8,.fd=1,.timestamp_ms=now};
    if(duplicate_master||(custom_id2&&i==1)) f.id=0;
    f.data[0]=0x7f;f.data[1]=0xff;f.data[2]=0x7f;f.data[3]=0xf7;f.data[4]=0xff;
    if(custom_id2&&i==1) f.data[3]=0xff; /* 该节点满量程力矩反馈。 */
    f.data[5]=65;f.data[7]=enabled[i];assert(can_queue_push(&rx,&f));
}
static uint32_t parameter(unsigned i,uint8_t p) {
    if(p==10) return 20261017;
    if(p==11) return wrong_mode==i+1?3:2;
    if(p==36) return wrong_id==i+1 ? 99 : i+1;
    if(p==37) return 100;
    if(p==56) return 128;
    if(p==67) return duplicate_master||(custom_id2&&i==1)?0:0xffffffffu;
    if(p==69) return 0;
    float v=0;
    switch(p) {
    case 21:v=18;break;case 24:v=-12.5f;break;case 25:v=12.5f;break;
    case 26:v=-65;break;case 27:v=65;break;case 29:v=500;break;case 31:v=5;break;
    case 34:v=-18;break;case 35:v=18;break;default:break;
    }
    if(custom_id2&&i==1) {
        if(p==21) v=15;
        if(p==34) v=-30;
        if(p==35) v=30;
    }
    uint32_t raw;memcpy(&raw,&v,4);return raw;
}
static can_result_t send(void *ctx,const can_frame_t *f) {
    (void)ctx;if(busy) return CAN_BUSY;
    assert(f->len==8&&f->fd&&!f->brs&&!f->extended);last=*f;++tx;
    unsigned i=(f->id&0xff)-1;assert(i<2);
    if(f->id>=0x600) {
        assert(f->data[0]==0x67&&f->data[1]&&f->data[6]==4&&f->data[7]==0x76);
        can_frame_t r={.id=f->id,.len=8,.fd=1,.timestamp_ms=now};
        r.data[0]=(uint8_t)(i+1);r.data[1]=f->data[1];r.data[7]=0xff;
        uint32_t raw=parameter(i,f->data[1]);
        for(unsigned n=0;n<4;++n) r.data[2+n]=(uint8_t)(raw>>(n*8));
        assert(can_queue_push(&rx,&r));
    } else if(f->id<0x200) {
        if(f->data[7]==0xfc) enabled[i]=1;
        else if(f->data[7]==0xfd) enabled[i]=0;
        else assert(f->data[7]==0xfa);
        feedback(i);
    } else feedback(i);
    return CAN_OK;
}
static can_result_t receive(void *ctx,can_frame_t *f) { (void)ctx;return can_queue_pop(&rx,f)?CAN_OK:CAN_EMPTY; }
static can_result_t health(void *ctx,can_health_t *h) { (void)ctx;*h=(can_health_t){.completed=tx};return CAN_OK; }
static void init(void) {
    now=tx=0;wrong_mode=wrong_id=busy=custom_id2=duplicate_master=0;
    memset(enabled,0,sizeof(enabled));can_queue_reset(&rx);
    can_port_t bus={.send=send,.receive=receive,.health=health};
    assert(can_service_init(&service,&bus));motor_registry_init(&registry);
    for(unsigned i=0;i<2;++i) {
        assert(qzb_motor_init(&nodes[i],&service,(uint8_t)(i+1),now));
        motor_port_t p=qzb_motor_port(&nodes[i]);assert(motor_registry_register(&registry,(uint8_t)(i+1),&p));
    }
}
static void run(unsigned ms) {
    for(unsigned n=0;n<ms;++n) {
        ++now;can_service_poll(&service);
        for(unsigned i=0;i<2;++i) qzb_motor_tick(&nodes[i],now);
    }
}
int main(void) {
    bench_command_t c;
    assert(bench_parse("ENABLE 2",&c)&&c.id==2);
    assert(bench_parse("VEL 1 5 1",&c)&&c.velocity==5&&c.kd==1);
    assert(bench_parse("VEL 2 60 4",&c));
    assert(!bench_parse("VEL 2 nan 1",&c));assert(!bench_parse("VEL 2 66 1",&c));
    assert(!bench_parse("ENABLE 1.5",&c));assert(!bench_parse("VEL 1 5 1 x",&c));
    init();run(60);
    assert(registry.count==2&&!motor_registry_find(&registry,3));
    const motor_port_t *one=motor_registry_find(&registry,1),*two=motor_registry_find(&registry,2);
    assert(one&&two&&one->ctx!=two->ctx);assert(!motor_registry_register(&registry,1,one));
    motor_snapshot_t a,b;assert(motor_port_read(one,&a));assert(motor_port_read(two,&b));
    assert(a.ready&&b.ready&&!a.feedback.enabled&&!b.feedback.enabled);
    assert(a.parameter_count==18&&b.parameter_count==18);
    assert(motor_port_velocity(two,5,1)==-1);
    assert(motor_port_enable(two,1)==1);run(2);assert(!enabled[0]&&enabled[1]);
    assert(motor_port_velocity(two,5,1)==1);assert(last.id==0x202);
    unsigned raw=(last.data[2]<<4)|(last.data[3]>>4);
    assert(fabsf((float)raw/4095*130-65-5)<0.04f);
    assert(nodes[1].streaming&&!nodes[0].streaming);
    assert(motor_port_velocity(two,60,4)==1);assert(motor_port_velocity(two,NAN,1)==-1);
    assert(motor_port_velocity(two,66,1)==-1);
    motor_command_t command={.torque=17};assert(motor_port_command(two,&command)==1);
    busy=1;assert(motor_port_velocity(two,3,1)==0);assert(nodes[1].command.torque==17);
    busy=0;assert(motor_port_enable(two,0)==1);assert(!nodes[1].streaming);run(2);assert(!enabled[1]);
    /* 反馈错节点不能更新ID1，坏包不改变有效计数。 */
    uint32_t old=nodes[0].snapshot.feedback.count;
    can_frame_t bad={.id=2,.len=7};qzb_motor_receive(&nodes[0],&bad);
    assert(nodes[0].snapshot.feedback.count==old);
    init();wrong_mode=2;run(60);
    assert(nodes[0].snapshot.ready&&!nodes[0].snapshot.failed);
    assert(nodes[1].snapshot.failed&&!nodes[1].snapshot.ready);
    assert(motor_port_enable(motor_registry_find(&registry,2),1)==-1);
    init();wrong_id=2;run(60);
    assert(nodes[0].snapshot.ready&&nodes[1].snapshot.failed);
    /* ID2现场配置：CAN_MASTER=0、力矩映射±30，与ID1独立。 */
    init();custom_id2=1;run(60);
    one=motor_registry_find(&registry,1);two=motor_registry_find(&registry,2);
    assert(motor_port_read(two,&b)&&b.ready&&b.feedback_id==0&&b.limits.torque_max==30);
    assert(b.feedback.count&&fabsf(b.feedback.torque-30)<0.001f);
    assert(motor_port_read(one,&a)&&a.ready&&a.feedback_id==1&&a.limits.torque_max==18);
    assert(fabsf(a.feedback.torque)<0.01f);
    assert(motor_port_enable(two,1)==1);run(2);
    command=(motor_command_t){.torque=25};assert(motor_port_command(two,&command)==1);
    raw=((last.data[6]&15u)<<8)|last.data[7];
    assert(fabsf((float)raw/4095*60-30-25)<0.02f);
    init();duplicate_master=1;run(60);
    assert(nodes[0].snapshot.failed&&!nodes[0].snapshot.ready&&nodes[1].snapshot.failed&&!nodes[1].snapshot.ready);
    puts("PASS instances: registration/duplicate IDs/per-node config/feedback/upper velocity5/control routing/protocol ranges/BUSY/no test caps");
    return 0;
}
