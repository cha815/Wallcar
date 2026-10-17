# 电机实例与上层接口

`DIAG_MODE=0` 在启动装配中注册 FDCAN1 ID1～4。实例、回调上下文和反馈互相独立；主循环调用 `firmware_tick(HAL_GetTick())`。下例展示前两台，ID3/4使用相同查找和调用接口。

依赖：上层 → `interfaces/motor_registry.h` → Devices（`qzb_motor`）→ CAN Service → 注入的 CAN Port → BSP/HAL。`composition/firmware.c` 绑定具体实现。接口头文件不依赖HAL或RTOS。

```c
#include "composition/firmware.h"
#include "interfaces/motor_registry.h"

const motor_port_t *motor1 = motor_registry_find(firmware_motors(), 1);
const motor_port_t *motor2 = motor_registry_find(firmware_motors(), 2);
motor_snapshot_t feedback;

// 主循环中读取，ready为实际参数读回校验结果。
motor_port_read(motor1, &feedback);
if (feedback.ready && !feedback.failed) {
    motor_port_enable(motor1, 1);
}

// 后续主循环确认feedback.feedback.enabled后调用；返回值需处理BUSY。
motor_port_velocity(motor1, 5.0f, 1.0f);
motor_port_velocity(motor2, 5.0f, 1.0f);
```

示例第二台也需独立使能并确认反馈。各实例互不隐式联动。上述速度接口使用 MIT：Kp=0、速度目标5 rad/s、Kd=1、力矩前馈0，由电机内部完成速度反馈闭环。实际存在阻尼速度误差，应读取反馈判断，不能以入队成功代替实际速度。

`motor_port_command()` 提供通用 MIT position / velocity / Kp / Kd / torque。每节点从参数24..35读取自身映射范围，保存在 `snapshot.limits`，用于编解码；不添加台架限速、限力矩或斜坡。ID2实测力矩映射±30 N·m，其内部Torque Limit为15 N·m，两者含义不同；均不改写。旧单机诊断的固定±18映射不用于新实例。

所有接口在主循环调用，禁止ISR直接调用。发送返回1=本次帧已入队，0=忙（未接受新目标），-1=无效/设备未就绪/发送错误。成功目标由设备层每2ms刷新。失能是基础设备命令 `motor_port_enable(motor,0)`，不包含停车策略或底盘状态机。上层自行管理目标生命周期。

`feedback.count==0`表示从未收到反馈，不能解释为有效零值。`time_ms`是BSP接收单调毫秒时间；调用方根据自身用途判断数据年龄。`ready`表示固定编码量程、MIT模式及相关配置已真实读回匹配；`failed`表示配置不符或事务失败。节点ID只用于查找对应实例，不把总线流量当作任意电机在线。

配置只读，不写模式、不保存Flash、不自动清错。CAN_MASTER<0时按Motor ID收反馈，否则按实际CAN_MASTER收反馈；ID2实测为0，快照通过feedback_id提供实际标识符。同总线两节点若配置成相同反馈ID，由于载荷不含节点ID，实例报告配置冲突，不猜测来源。默认镜像不自动使能。原有单机调参应用保留作为历史诊断，未链接进默认镜像。

台架UART入口同样调用这些公开接口：`ENABLE 1`、`VEL 1 5 1`、`DISABLE 1`，ID2同理。遥测为15个小端float32和JustFloat尾标，字段见 `tests/instance-session.py`。负时间-1记录参数及总线诊断，参数原始值拆为两个uint16避免float精度损失。
