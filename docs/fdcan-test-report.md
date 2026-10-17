# FDCAN 与 UART7 验证报告

> 这是原只读目标的阶段报告。用户后续已转换为MIT调参，最新实现与真实证据见 `motor-tuning-guide.md`、`motor-tuning-review.md` 和 `docs/evidence/tuning-*.csv`。后续FDCAN已通过正确CAN1引脚真实通信，下面“尚未验证”是转换目标前的历史状态，不是当前结论。

日期：2026-10-14；目标：MC02 / STM32H723；电机：仅ID1接入。

## 当前结论

**UART7真实收发已验证。FDCAN源代码与主机测试已实现并通过；当前FDCAN目标构建、内部回环和ID1外部通信尚未验证。** 回环目标构建的沙箱外执行请求被拒绝，未通过其他命令绕过该拒绝。本报告不把模拟测试或UART固件烧录当作FDCAN通信通过。

## 已取得证据

| 项目 | 实际结果 | 证据 |
|---|---|---|
| 原CubeMX基线构建 | ELF/HEX/BIN生成成功 | 计划阶段已执行，text=21160、data=16、bss=2120 |
| USB/SWD探针 | CMSIS-DAP v2，VID:PID=FAED:4870，序号102021326858，FW Horco v0.2 | `evidence/probe-identify.log` |
| 目标识别 | Cortex-M7，STM32H72x/73x，1MB单Bank Flash | `evidence/uart-flash.log` |
| 原板上Flash备份 | 1048576字节 | `evidence/original/onboard-before.bin` |
| UART专用构建 | 成功；无warning/error；text=23004、data=96、bss=2296 | `evidence/uart-build.log` |
| UART固件烧录 | Programming Finished / Verified OK / Resetting Target | `evidence/uart-flash.log` |
| UART7 TX | COM12，115200/8N1，收到H723带tick心跳 | `evidence/uart-serial.log` |
| UART7 RX与回显 | 发送`UART7_ECHO_102021326858`，原样返回 | 同上；证明无线DAPLink串口桥接UART7双向可用 |
| 主机测试 | 全部通过，见下表 | `tests/run-host.ps1`实际执行结果 |

备份SHA256：`DC9F37D06EDDC75689D639C23438D943838C8DEE12F4B1E141B1DE65FAB866AF`。
实际烧录的UART测试ELF SHA256：`6954B5037E42B0E4CC10AD8486733169334CD875E15FA7124260914CFC288F6D`。

UART证据来自添加完整FDCAN代码前的UART专用构建；不要把该ELF称作最新全部源码的构建验证。

## 主机测试覆盖

| 测试 | 已验证的行为 | 局限 |
|---|---|---|
| `test_core` | 16种DLC、标准/扩展ID上限、FD/BRS限制、32帧队列满和环绕、只读白名单、ID1报文、反馈格式 | 不访问硬件 |
| `test_bsp` | HAL映射、TDC在Start前、TX入队与完成分离、接收溢出、remote拒绝、事件丢失、bus-off锁存、一次恢复、初始化失败 | HAL由最小mock替代；不证明真实HAL集成或总线信号 |
| `test_diag` | 四组各1000次查询、20参数读取、3次连续超时终止、发送返回前回包、82组×100帧回环流程 | 模拟对端，不证明真实电机响应 |

TDD观察到了实现不存在导致的失败；快速回包时间戳与本地HAL协议错误回调路由分别经过失败→修复→全部测试通过。

## 待执行的真实FDCAN验收

配置：保留120MHz内核时钟，仲裁1Mbps（采样82.5%）、数据5Mbps（采样75%），TDC offset18/filter0为初值。FIFO0=32，TX FIFO=8，TX event=8，RX/TX载荷64字节，Message RAM=2944字节。

- `DIAG_MODE=1`：82组合法组合，每组100帧，共8200帧。要求ID、长度、FD/BRS、载荷一致，超时和错误计数均0。
- `DIAG_MODE=2`：先只读20个参数，再执行TX无BRS/有BRS分别对应8B单独查询和64B广播查询，共4组；每组1000匹配响应，100ms响应截止，3次连续失败终止。
- 真实参数、固件能力、参数37超时和参数56保护Bit7尚未读取；JSON不替代真实读回。
- 状态查询不支持时记录“不支持/未验证”，不写参数或升级固件绕过。
- TX入队、TX event完成、RX回复分别计数；日志分别记录TX/RX FD、BRS、长度。无BRS请求通过不能推出电机回复也是无BRS。
- 帧没有请求序号；测试单事务串行并排除旧队列数据，要求TX完成与新鲜匹配回复。若电机存在周期主动上报，应另行记录，不能宣称每个状态帧均是请求的唯一应答。
- 断连、恢复和四电机联合测试尚未执行。缺ACK不一定进入bus-off，断连验收至少要求超时并保留TEC/REC/LEC等数据；bus-off恢复另按故障现象确认。

历史参考代码提到5Mbps BRS发送出现bus-off、无BRS请求稳定。该现象尚未复现，也没有宣称已修复。

## 安全边界和下一步

电机接口只允许ID1参数读取和状态查询，Index=0被拒绝；不包含写参数、使能、失能、运动、设零、清错或OTA入口。循环测试只在独立诊断镜像运行，正常`DIAG_MODE=0`不发送测试帧。

按 `verify.md` 构建并运行回环镜像，再运行ID1只读镜像。板上当前为UART测试镜像；未经FDCAN目标构建和真实日志确认，不能声明FDCAN通信完成。
