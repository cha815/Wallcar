/* 作者：Codex；文件：motor_registry.h；说明：电机实例注册与上层能力接口。 */
#ifndef MOTOR_REGISTRY_H
#define MOTOR_REGISTRY_H
#include "motor_port.h"
#define MOTOR_REGISTRY_CAPACITY 4u
typedef struct {
    motor_port_t ports[MOTOR_REGISTRY_CAPACITY];
    uint8_t ids[MOTOR_REGISTRY_CAPACITY],count;
} motor_registry_t;
/* registry、ctx与port对象均由调用方持有；注册复制函数表，不分配内存。
 * 查找结果直到registry重新初始化前有效；所有接口仅主循环调用。 */
void motor_registry_init(motor_registry_t *registry);
int motor_registry_register(motor_registry_t *registry,uint8_t id,const motor_port_t *port);
const motor_port_t *motor_registry_find(const motor_registry_t *registry,uint8_t id);
int motor_port_read(const motor_port_t *motor,motor_snapshot_t *out);
int motor_port_enable(const motor_port_t *motor,uint8_t enabled);
/* MIT速度闭环：Kp=0、TorqueFF=0、Kd=kd；单位rad/s、N m/(rad/s)。
 * 返回1为本次命令入队，0总线忙，-1无效；设备层周期刷新最后入队命令。
 * 速度方向沿驱动定义；未收到使能反馈前不能设定运动命令。 */
int motor_port_velocity(const motor_port_t *motor,float velocity,float kd);
/* 通用MIT命令：rad / rad/s / N m，仅协议量程校验，无额外台架限幅。 */
int motor_port_command(const motor_port_t *motor,const motor_command_t *command);
#endif /* MOTOR_REGISTRY_H */
