# 四电机接口实测

2026-10-17，STM32H723 / MC02，FDCAN1接入ID1～4，COM12采集。DAPLink物理拔插及模块重新上电后烧录成功，OpenOCD确认Verified OK；此前CMD_INFO失败记录为历史故障。

默认镜像`First/build/four-instances/First.elf`，DIAG_MODE=0；SHA256：`8DD07187C83F47FEFEF7D11E1B8BFB76C4756DB898D77C69DBBE6E0D749C3787`。设备注册、CAN订阅和遥测轮询扩展至四实例，公共接口调用方式保持一致。

先只读确认四实例ready=1、反馈更新且故障为0，再通过公开接口同时发送目标5rad/s，Kp=0、Kd=1、力矩前馈0，采集10秒。每台216个运行样本，末0.5秒速度如下。

| ID | 平均速度 rad/s | 最小 | 最大 |
|---|---:|---:|---:|
| 1 | 4.81746 | 4.74603 | 4.87302 |
| 2 | 4.63228 | 4.46032 | 4.77778 |
| 3 | 4.80423 | 4.71429 | 4.87302 |
| 4 | 4.75926 | 4.68254 | 4.80952 |

CAN错误、Bus-Off、丢帧计数及电机故障均0；结束四台均收到已失能反馈。最终失能快照仍有惯性转速，不能据此声称机械速度已为零。目标与实际存在稳态误差，未添加主控PID、底盘控制或台架限幅。

构建、烧录、在线及运行证据分别为`evidence/dual-four-build.log`、`evidence/dual-four-flash-usb-reset.log`、`evidence/dual-four-online.*`、`evidence/dual-four-velocity5-10s.*`。运动脚本退出码0；本次直接编译和板端验证，未重复运行主机测试。
