# ID1 MIT 电机调参设计（2026-10-14）

用户目标已转换：旧只读通信验收和四轮底盘实施暂停。当前目标是单台已架空的ID1，周期下发MIT目标与Kp/Kd，UART7实时接收调参命令并输出VOFA+ JustFloat，先速度Kd再位置Kp/Kd，实际采集波形后确定候选增益。

## 接口与分层
- component/motor_codec：有限值与量程检查、MIT封包、反馈解包，独立主机测试。
- bsp/bsp_fdcan：保留通用FDCAN1；bsp/bsp_uart_stream：非阻塞UART7字节接收与固定帧发送，不使用DTCM上的DMA。
- device/motor：ID1通信、只读参数核对、状态请求、MIT命令与反馈快照；上层不访问HAL。
- service/tuning_link：行命令解析、JustFloat打包；应用不处理串口硬件。
- app/motor_tune：使能/停止/故障状态、2ms控制周期、目标和增益限幅、反馈与主机失联保护。
- main只装配并调用tick；DIAG_MODE=4为调参固件，原诊断镜像继续独立保留。

## 调参协议
UART7 COM12，115200/8N1。文本命令以换行结束：`ARM`、`STOP`、`CLEAR`、`PING`、`VEL velocity kd`、`POS relative_position kp kd`。
ARM前核实实际ID/模式/量程/CAN_MASTER/ProtocolType/超时保护；默认失能，ARM仅零目标使能，不自动跑测试。POS以本次ARM时位置为基准；接收完整命令后原子更新目标与增益，非法值拒绝。无写Flash/OTA入口。
主机每100ms发送PING；500ms没有完整有效心跳/控制命令，或200ms没有有效电机反馈，退出运行并尝试失能。硬件发送错误和电机故障锁存，CLEAR不自动使能。

首轮软限制：速度±5rad/s、位置相对基准±0.5rad、Kp 0..5、Kd 0..2、速度斜坡10rad/s²；命令力矩前馈0。用反馈约束最大位置/速度误差产生的预计力矩到3Nm（MIT Kp/Kd量化后再核对）；这些是台架候选，不改变电机内部18Nm限幅。
使能先零目标后FC，等待反馈使能位；停止优先取消待发运动帧再FD，等待反馈失能。未确认反馈不得宣称停机。最多一次bus-off恢复，恢复后保持失能/故障不重放旧目标。

## 波形与验收
100Hz，15个float32小端通道：time_s、target_pos、actual_pos、target_vel、actual_vel、actual_torque、kp、kd、temperature、state、fault、command_result、enabled、feedback_age_ms、stop_confirmed；尾部00 00 80 7F。固定64字节帧由UART中断发送，禁止阻塞控制周期。目标与增益为最后成功提交的量化值，不显示未发送的内部目标；停机判定必须包含新鲜失能反馈和stop_confirmed，不把FAULT当作停机确认。
先实际通信与反馈，再小幅速度阶跃比较Kd候选，再0.1rad相对位置阶跃比较Kp/Kd。记录上升时间、超调、稳态误差及振荡；每轮完成STOP并确认失能。交付波形CSV/图、候选参数与复测条件，不将空载参数宣称为底盘带载最优参数。
