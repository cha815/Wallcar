# 双电机实例重构与验证

> 最新双机实测：ID1和ID2同时按目标5rad/s、Kd1运行10秒，分别收到432个运行样本。末0.5秒ID1均速4.81781rad/s（4.74603..4.84127），ID2均速4.62043rad/s（4.46032..4.84127）。CAN协议错误、TEC、REC以及两机故障均0；最终均已失能。证据 `evidence/dual-both-velocity5-10s.*`。下文单机/断电排查为历史记录，双机接口实测以本段为准；实际速度误差不掩盖。

> 当前结论更新：用户确认此前电机未上电；上电及控制板复位后ID2通信正常。当前已完成ID2目标5rad/s、10秒实机接口试验，末0.5秒均速4.63147rad/s，范围4.49207..4.80952rad/s；无故障，结束已失能。见 `evidence/dual-id2-velocity5-10s-complete.*`。ID1未接入，不声明双机验收完成。下文原无ACK记录为历史断电条件，已被新证据取代。

当前镜像SHA256：`9A211A65E17A69BB24BB749E4D25F1A02FA0B953DCF2D87DAC581D64F51D3805`，text47352/data472/bss5880，编译和烧录校验通过。已按每节点实际读回量程和反馈ID适配；ID2 CAN_MASTER=0、力矩映射±30Nm、内部Torque Limit15Nm。没有修改驱动参数。

日期：2026-10-17。用户已确认两机架空。

## 最终范围

按用户最新要求，只实现 ID1/ID2 注册实例与上层接口，验证上层调用到底层设备的5rad/s速度控制。新增的双机应用状态机、停车握手、主控PID及台架限幅均已删除。编码遵循 `../代码规范.md`。

## 实现

- Interfaces：CAN帧、通信端口、SI单位反馈与电机能力函数表、注册表公开API。
- Devices：qzb_motor每节点独立参数事务、反馈、MIT指令及周期刷新；保留协议合法性检查。
- Services：总线队列单一消费、节点分发、注册表、串口测试协议。
- BSP：沿用FDCAN1/UART7；硬件接口只在composition绑定。
- Composition：静态创建并注册两个实例，默认失能。上层示例：[motor-instance-api.md](motor-instance-api.md)。
- 历史app/motor_tune与主控PID代码不参与默认镜像构建。

## 验证记录

| 用例 | 配置与输入 | 实际结论 | 证据 |
|---|---|---|---|
| 主机实例集成 | 两节点模拟驱动；公开注册、读取、使能与5rad/s接口；忙返回、参数不符、坏包、协议范围；参数36逐节点ID校验 | 通过 | tests/test_motor_instances.c；evidence/dual-host-tests.log |
| 原有回归 | PID、CAN帧、HAL mock、诊断、编解码、调参链路、原单机应用与设备 | 八组通过；不代表本次硬件通过 | tests/run-host.ps1 |
| H723默认固件 | DIAG_MODE=0，GCC14.3.1 -Og；text46864/data472/bss5776 | 编译通过，无warning/error | evidence/dual-instances-build.log |
| DAPLink烧录 | CMSIS-DAPv2 FAED:4870，SWD1MHz，H72x/73x 1MB | Programming Finished、Verified OK、Resetting Target | evidence/dual-instances-flash.log |
| 默认符号检查 | 已烧录ELF | 有qzb_motor、motor_registry；无dual_motor、motor_tune、control_pid | arm-none-eabi-nm实际检查 |
| 双节点只读通信 | COM12 115200/8N1，FDCAN1 PD0/PD1；仲裁1Mbps/数据5Mbps，TX无BRS | 失败：节点均无反馈，TX完成0、TEC128、REC0，参数未完成；物理原因尚未确认 | evidence/dual-instances-idle.csv / json / log |
| 上层5rad/s控制 | 逐台5rad/s，2秒，调用实例接口 | 未验证：外部CAN通信失败，尚未发送运动命令 | 待通信通过后运行tests/instance-session.py --motion |

当前默认固件ELF SHA256：`CF0F5A5205BEEFF670A24CBAC6338B7DF99796E8C5E61F054515A986C058E394`，已烧录Verified OK。

有限链路诊断 `DIAG_MODE=7`：只读ID2参数10；FD无BRS、FD_BRS5M、FD_BRS4M、经典CAN1M四种均TX完成0、RX0、TEC128，ACK错误。TX与RX引脚采样跳变计数接近，提示本地信号回读存在；不等于电机端收到信号。完整记录 `evidence/dual-link-probe-result.log`。诊断后恢复默认固件。

用户确认MC02已接USB或24V，随后仅保留ID2接线。单节点复测仍无反馈和ACK，见 `evidence/dual-id2-only.*`；待总线断电电阻/线路通断测量，尚未发送运动。

## Git

换线复测：用户更换ID2电机线后，以当前板上默认实例固件采集COM12。共752个普通遥测帧，ID2电机反馈count仍0，总线TX complete仍0，TEC128/REC0、LEC3。通信仍失败；没有使能或运动。证据 `evidence/dual-id2-recabled.csv` / `.json` / `.log`。

本地已初始化；远端 `cha815/Wallcar` 的main当前为77d8cb2，只有README。已核实GitHub登录。后续保留该初始历史，不强推。提交需如实写明外部CAN及运动验证尚未通过。
