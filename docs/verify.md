# 当前验证步骤与结果

日期：2026-10-14。唯一验证清单，本轮目标为DIAG_MODE=6主控PID固定5rad/s，取消周期阶跃。接口详见vofa-pid-step.md。

## 主机验证

执行tests/run-host.ps1，8组全部通过。先将应用回归修改为固定5rad/s，旧周期实现失败于目标断言；取消周期逻辑后通过。覆盖超过24秒目标不变、纯力矩Kp/Kd及速度字段0、输出±5.99Nm与量化预算≤6Nm、超目标负力矩、BUSY回滚、重复反馈不积分、连续忙100ms停车及启停锁存。旧BSP、协议、串口与设备门禁回归也通过。Mock不能证明实机速度跟踪效果。

## 编译及审查

First目录使用DIAG_MODE=6 BUILD_DIR=build/vofa-pid-fixed5。构建退出0，无warning/error。text52784/data472/bss6776。ELF SHA256：FF13DEC480675D94322F526212C54F6E515CD1308E163954A8ED467EB3910471。日志evidence/vofa-pid-fixed5-build.log。独立code_reviewer_pid一次简短只读审查通过，未发现重要问题。

## 最后烧录操作

无线CMSIS-DAP，经OpenOCD执行program First/build/vofa-pid-fixed5/First.elf verify reset exit。期望Verified OK并退出0；实际结果见最终操作生成的evidence/vofa-pid-fixed5-flash.log。所有文件更新、检查和审查均在最终操作前完成。按用户要求，成功烧录复位后不再操作设备，暂无本轮串口板端验收。

## 用户观察

VOFA+ COM12、115200/8N1、JustFloat，通道0目标、通道1反馈，100Hz。使能确认后目标固定5rad/s，不再循环。主控PID保持1/.1/0，驱动报文Kp/Kd0，发送预算6Nm，反馈力矩停车阈值12Nm。实际可达速度及稳定性等待用户观察。
