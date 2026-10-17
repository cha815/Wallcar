/* 作者：Codex；文件：motor_registry.c；说明：静态实例表与统一调用接口。 */
#include <string.h>
#include "../interfaces/motor_registry.h"

void motor_registry_init(motor_registry_t *r) { if(r) memset(r,0,sizeof(*r)); }
const motor_port_t *motor_registry_find(const motor_registry_t *r,uint8_t id) {
    if(!r||!id) return NULL;
    for(unsigned i=0;i<r->count;++i) if(r->ids[i]==id) return &r->ports[i];
    return NULL;
}
/* 重复ID、空能力表及容量不足均拒绝，不改变已有注册。 */
int motor_registry_register(motor_registry_t *r,uint8_t id,const motor_port_t *p) {
    if(!r||!p||!id||!p->read||!p->enable||!p->command||
       r->count>=MOTOR_REGISTRY_CAPACITY||motor_registry_find(r,id)) return 0;
    r->ids[r->count]=id;r->ports[r->count++]=*p;return 1;
}
int motor_port_read(const motor_port_t *p,motor_snapshot_t *out) {
    if(!p||!out||!p->read) return 0;
    p->read(p->ctx,out);return 1;
}
int motor_port_enable(const motor_port_t *p,uint8_t enabled) {
    return p&&p->enable ? p->enable(p->ctx,enabled) : -1;
}
int motor_port_command(const motor_port_t *p,const motor_command_t *c) {
    return p&&p->command&&c ? p->command(p->ctx,c) : -1;
}
int motor_port_velocity(const motor_port_t *p,float velocity,float kd) {
    const motor_command_t command={.velocity=velocity,.kd=kd};
    return motor_port_command(p,&command);
}
