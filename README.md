# Wallcar

STM32H723 / MC02 裸机电机实例接口工程。FDCAN1注册ID1～4，按每台电机实际读回量程与CAN_MASTER解析反馈，供上层通过统一C接口调用。

- 固件：[First](First)，默认`DIAG_MODE=0`，上电不自动使能。
- 接口与示例：[motor-instance-api.md](docs/motor-instance-api.md)。
- 实测与证据：[dual-motor-refactor.md](docs/dual-motor-refactor.md)。
- 状态入口：[PROJECT.md](PROJECT.md)、[CURRENT.md](CURRENT.md)。
- 编译：在First内运行`mingw32-make DIAG_MODE=0 BUILD_DIR=build/instances GCC_PATH=<ARM-GCC-bin>`。

四机已同时进行目标5rad/s、10秒空载接口测试：ID1～4末段平均分别4.81746、4.63228、4.80423、4.75926rad/s，CAN与电机故障均0，结束均反馈失能。证据见[四机记录](docs/four-motor-test.md)。该结果是MIT阻尼速度反馈控制的实际响应，不代表精确速度跟踪或整车性能验收。

工程不新增底盘控制、主控PID或停车状态机。历史单机调参源码仅在非零诊断镜像编译，默认固件使用独立注册实例。
