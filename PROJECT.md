# Wallcar 固件工程

- 根目录：Final；First 为 STM32H723 / MC02 裸机工程，Second 不在本次重构范围。
- FDCAN1：PD0 RX、PD1 TX，仲裁 1 Mbps，接收数据段 5 Mbps，发送 FD 无 BRS。
- 电机：泉智博，当前接入 ID1 / ID2；协议资料位于 `../docs/底盘电机`。
- UART7：115200/8N1，DAPLink 串口当前枚举 COM12。
- 编码规范：`../代码规范.md`；应用技能：embedded-development-workflow 及其架构、验证、调试、Git 技能。
- 构建：`First/Makefile`；主机验证：`tests/run-host.ps1`。
- 旧单机调参实现和证据保留用于回归，不作为当前双机结果。
- 默认固件：设备实例注册与MIT接口，`DIAG_MODE=0`；历史单机应用仅在非零诊断构建中参与编译。
- 上层API：[docs/motor-instance-api.md](docs/motor-instance-api.md)；当前任务入口：[CURRENT.md](CURRENT.md)。
