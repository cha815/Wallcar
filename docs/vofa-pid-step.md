# 主控速度PID固定5rad/s试验

日期：2026-10-14。镜像：`DIAG_MODE=6 BUILD_DIR=build/vofa-pid-fixed5`。

## 控制与接口

用户确认直接使用电机反馈速度做PID。现有角度在±12.5rad饱和，不用于持续差分测速。主控PID输出力矩，驱动沿用现有ID1标准FD报文0x201/8字节，BRS关闭，报文Kp/Kd及目标速度固定0。只使用力矩字段，驱动Control Mode参数不改写，内部电流控制仍由驱动完成。

主控PID初值Kp=1.0、Ki=0.1、Kd=0，配置在`First/app/motor_tune.c`的`pid_config`，属于待整定参数。通用组件`control_pid_init/reset/step`不依赖HAL，含条件积分抗饱和、测量值微分和20ms一阶微分滤波。输出±5.99Nm，使协议量化后的正负力矩均不超过6Nm。

`motor_tune_pid_demo_init(now_ms)`选择自主固定速度模式；main统一装配BSP、设备、UART服务和应用。主循环2ms运行，只有新反馈count和正时间戳差才计算PID，采用实际反馈间隔；成功发送后才提交PID状态副本，发送忙时不提交积分/微分历史。此模式拒绝交互MIT速度/位置接口，保持报文增益为0。

## 固定目标

使能确认后，主控PID参考速度始终为5rad/s，已取消12秒周期阶跃。修改位置：First/app/motor_tune.c中的MOTOR_TUNE_PID_TARGET_RAD_S。

目标是PID参考值，实际速度是否可达需由VOFA波形判断。此前约9rad/s的现象不能通过替换控制器直接证明已消除。

## 启停与保护

上电验证实际参数及新鲜反馈；若旧程序留下使能状态，先取消旧发送并提交失能，要求TX完成、无pending和更新的失能反馈，再等待速度≤0.5rad/s，最多5秒。只在启动时清除纯CAN超时0x2000一次，并等待更新的无故障反馈；其他保护不自动清除。

保留温度80°C门禁、反馈200ms超时、连续发送忙100ms、运行速度超过62rad/s和反馈力矩超过12Nm等停止条件。STOP/故障清空PID动态状态，锁存自主启动；CLEAR不能自动恢复。复位重新执行启动流程。自主试验不依赖主机PING，未改驱动保护参数或Flash。

## VOFA+

COM12，UART7，115200/8N1，JustFloat，100Hz；每帧12字节：两枚小端float32，随后`00 00 80 7F`。

- 通道0：主控PID固定目标速度，rad/s。
- 通道1：电机反馈速度，rad/s。

失能/故障时目标通道置0；保留ASCII `STOP` 加换行请求失能。旧15通道CSV脚本不适用于此镜像。

## 验证与代码规范

按用户贴出的规范调整本轮文件：系统头文件优先、用户头双引号及相对路径、文件头作者/名称/说明/日期、公共函数及关键逻辑前置注释、条件编译尾注释、试验代码`only forDEBUG`和增益整定TODO。项目根include用于解析`./device`等兄弟层路径。保留CubeMX版权和USER区。

8组主机测试通过：PID算术/抗饱和/微分、超过24秒保持固定目标5rad/s、纯力矩、负纠正力矩、角度饱和无关性、启动接管、停车错误后CLEAR不重启、公开接口门禁、BUSY回滚、重复反馈和100ms忙保护。独立code-reviewer发现的Important及Minor均已修复，并完成只读复核。

目标编译成功，无警告：text52784/data472/bss6776。ELF SHA256：`FF13DEC480675D94322F526212C54F6E515CD1308E163954A8ED467EB3910471`。构建日志`evidence/vofa-pid-fixed5-build.log`，最后一步烧录结果查`evidence/vofa-pid-fixed5-flash.log`的`Verified OK`。

按用户要求，烧录校验/复位后不做任何串口采集、控制或工具操作；固定5rad/s实机响应待用户观察，不能用主机测试替代实机验收。

本轮力矩范围提高到6Nm：PID输出±5.99Nm，量化发送预算6Nm，PID反馈停车阈值12Nm。启动增加只读Index21，实际驱动Torque Limit必须有限且≥6Nm，配置不满足时拒绝启动，不改写驱动/Flash。8组测试通过（含量化正负边界、反馈8Nm允许/12.1Nm停车、驱动5.9/NAN拒绝和6Nm通过）；仅一次简短只读审查，无新增重要问题。

