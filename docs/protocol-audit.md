# 电机协议核对与数据链路

> 最新现场已纠正：此前电机未上电。上电并复位控制板后，ID2参数/状态共21次发送与21帧接收全部成功，无CAN错误。实际CAN_MASTER=0、MIT模式2、FD5M、力矩映射±30Nm；实例已按实际量程和反馈ID解析，已完成上层5rad/s目标10秒试验（均速约4.63rad/s）。旧无ACK与固定量程描述为排查历史，不再代表当前实现。原始证据 `evidence/dual-id2-parameters-result.log`、`evidence/dual-id2-online.*`、`evidence/dual-id2-velocity5-10s-complete.*`。

2026-10-17；依据本地 `../docs/底盘电机/datasheet/泉智博驱动协议手册/泉智博驱动协议手册.md`（该路径相对于Final根目录）及MC02原理图。最新用户确认电机24V正常，CAN通信线路待核实。

## 当前证据

- FDCAN内部回环：82组、8200帧，TX完成/RX=8200，丢失、超时、TEC/REC、协议错误均0；只证明控制器内部路径，不证明外部收发器或电机通信。
- 外部：TX完成0、TEC128、REC0、LEC3=ACK error，节点1/2反馈计数0。没有有效设备回复，不是已经收包后解析失败。
- MC02原理图第4页：CAN1 TX=PD1、RX=PD0；P2 1=CAN_L、2=CAN_H、4=GND，3未接。收发器VDD=VCC_5VS，STBY接地；红字注明CAN需控制板USB或24V供电。电机24V与控制板5V是不同检查点。

## 报文核对

标准11位数据帧，单节点8字节；单独控制需260617及以后固件。

| 用途 | ID1 | ID2 | 载荷 |
|---|---|---|---|
| 读固件参数10 | 0x601 | 0x602 | 67 0A 00 00 00 00 04 76 |
| 查询状态 | 0x101 | 0x102 | FF FF FF FF FF FF FF FA |
| 使能 | 0x101 | 0x102 | FF FF FF FF FF FF FF FC |
| MIT控制 | 0x201 | 0x202 | 位置16位、速度12位、Kp12位、Kd12位、力矩12位 |

源码上述ID、长度和位布局与手册一致。参数值Byte2..5为小端，控制/反馈的跨字节字段高位在前。不能统一当成同一种字节序。

采用当前代码和JSON量程（位置±12.5、速度±65、Kp0..500、Kd0..5、力矩±18），对position0、velocity5、Kp0、Kd1、TorqueFF0独立计算得到MIT载荷 `7F FF 89 D0 00 33 37 FF`。它与手册按范围映射后截断的位布局一致；这是离线计算，尚未发送到电机。手册Python示例中的量程并非本项目JSON量程，必须以实际驱动读回确认。

当前TX FDF=1、BRS=0，线上整帧1Mbps；RX数据段配置5Mbps。手册列出仲裁1Mbps、数据5Mbps/4Mbps；未明确保证FD无BRS兼容。尚未读回参数69（0=FD5M、1=FD4M、2=CAN）、固件版本及量程，不能据静态核对宣称设备实际配置匹配。

反馈帧CAN ID在CAN_MASTER<0时为Motor ID，否则为CAN_MASTER。当前接收按1/2路由，配置校验要求CAN_MASTER=-1；实际仍未读回。参数回复ID=0x600+ID、载荷 `[ID, Index, value LE32, 00, FF]`。

源码Motor ID参数36已从固定1修正为逐实例ID，九组主机回归、目标构建与烧录均通过。它影响ID2配置，不是当前ACK错误的原因。

后续只读比较：FD无BRS、FD_BRS5M、FD_BRS4M、经典CAN1M均无ACK/无接收；TX/RX引脚都有跳变活动。用户确认控制板USB或24V已接；仅保留ID2支路也未恢复。当前需要断电测量H/L电阻与主控到电机的线序通断，不能仅用软件推断线缆正确。

## 反馈上传

电机 → CAN_H/L → MC02收发器 → PD0/FDCAN1 RX → FDCAN1_IT0_IRQHandler → HAL_FDCAN_IRQHandler → HAL_FDCAN_RxFifo0Callback → 通用can_frame_t与HAL_GetTick接收时间 → 32帧BSP队列 → 主循环can_service_poll → 两节点回调各自按ID筛选 → qzb_motor_receive → motor_codec_feedback → 节点snapshot.feedback → motor_port_read → 上层。

接收中断只收帧、取时间戳与入队。解码和参数事务在主循环运行。每节点独立保存position(rad)、velocity(rad/s)、torque(Nm)、temperature(摄氏度)、status/fault/enabled、count、time_ms。count=0不是有效零值；ready是配置通过，不等于持续在线，上层根据时间戳判断反馈年龄。

## 控制下发

上层motor_registry_find(ID) → motor_port_velocity(instance,5,1) → 通用motor_command（Kp0、velocity5、Kd1、TorqueFF0）→ 该实例command回调 → motor_codec_pack / mwQZB_pack_mit → can_service_send → 注入的BSP适配 → bsp_fdcan_send → HAL_FDCAN_AddMessageToTxFifoQ → PD1 → 收发器 → CAN_H/L → 对应电机。

入队成功后实例缓存目标，qzb_motor_tick每2ms刷新；无底盘逻辑或主控PID。电机MIT内部输出T=Kp*(p_ref-p)+Kd*(v_ref-v)+Tff；因此本例为T=1*(5-v)，属于驱动内部速度反馈控制，带载可能有稳态误差。返回1仅代表入队；TX完成、设备回复和实际达到5rad/s分别判断。
