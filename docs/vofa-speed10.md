# UART7 双通道速度波形

本轮专用固件：`DIAG_MODE=5 BUILD_DIR=build/vofa-speed10`。

MC02复位后核对ID1参数并等待新鲜、失能、无故障反馈；符合使能条件后启动一次，MIT模式Kp=0、前馈力矩=0、Kd请求值1，目标速度10rad/s，以10rad/s²斜坡加速。此模式自主持续运行，不要求电脑发送PING。STOP或故障后不会自动重启，重新复位会再次执行启动流程。保留80°C温度门禁、反馈超时、发送故障及3Nm预测约束；跟踪误差大时该约束可能临时降低发送Kd，不修改电机保护参数。

## VOFA+ 设置

- 串口：COM12，无线DAPLink连接UART7；115200、8N1。
- 数据引擎：JustFloat。
- 通道0：最后成功提交的目标速度，rad/s，包含加速斜坡和协议量化；设置10rad/s量化后约9.984rad/s。
- 通道1：电机反馈速度，rad/s。
- 输出100Hz，两枚小端float32（8字节）加尾部 `00 00 80 7F`，每帧12字节。失能/故障时目标通道置0，反馈通道仍显示收到的速度。

可发送ASCII `STOP` 加换行请求失能。此固件使用双通道，旧15通道CSV采集脚本不适用于它。

## 烧录前验证记录

7组主机回归通过：自主启动/无心跳持续运行/达到10rad/s/位置饱和/STOP锁存/故障清除后不自动重启/双通道字节序与帧尾。独立code-reviewer未发现Critical或Important问题；这些结果不代替10rad/s实机波形验收。

目标编译成功，无警告，text51152/data472/bss6712。ELF SHA256：`DE35FAC7AD0612C4D2835CACB13E0FB9015DEB6BF68E6301CD5C8A1B09D6644E`。

构建日志：`evidence/vofa-speed10-build.log`；最后一步烧录结果由 `evidence/vofa-speed10-flash.log` 的 `Verified OK` 确认。按用户要求，烧录校验/复位后不再连接串口、发送STOP或运行任何采集，实际运行波形由用户用VOFA+观察。
