# 当前任务

最近更新：2026-10-17（Asia/Shanghai）。

- 唯一目标：将First实例注册扩展到ID1～4，四台同时目标5rad/s运行10秒，并保存代码与证据。
- 用户范围：只做设备实例和接口；不新增底盘控制、主控PID、停车状态机或测试限幅。用户要求直接编译烧录、实际观察，最新要求ID2运行10秒。
- 当前阶段：用户拔插USB并重启DAPLink后，四实例固件烧录Verified OK；四节点在线及同时10秒运动验证通过。
- 四实例改动：registry/service容量4；composition注册ID1～4并轮询四节点遥测；台架脚本识别ID1～4、支持同时运行。构建产物 `First/build/four-instances/First.elf`，text47344/data472/bss6360。`docs/evidence/dual-four-build.log`。
- 烧录现场：Bulk两次CMD_INFO失败；HID尝试无响应已中止，没有Verified OK。用户重连后COM12恢复，但板上仍输出旧双实例固件的ID1/2，不能声称四实例已烧录。日志 `dual-four-flash.log`、`dual-four-flash-reconnected.log`、`dual-four-flash-hid.log`。
- USB设备已核实FAED:4870，父实例USB\\VID_FAED&PID_4870\\102021326858；无残留OpenOCD进程。pnputil重启USB被Windows以Access is denied拒绝；已请求用户物理拔插电脑端USB并重启板端DAPLink模块，等待回复。
- 用户已纠正现场：之前电机未上电。上电并复位控制板清除旧Bus-Off后，ID2只读诊断21次发送/21次接收，TEC/REC/协议错误全0。此前无ACK记录保留为断电条件证据，不能继续当作当前线路故障。
- ID2实际配置：ID=2、MIT模式2、CAN_MASTER=0（反馈CAN ID=0）、ProtocolType=0、力矩映射±30Nm、内部Torque Limit=15Nm、CAN超时1000ms。未改写电机参数。证据 `docs/evidence/dual-id2-parameters-result.log`。
- 实现：每实例保存实际读回映射范围和反馈ID；上层snapshot提供limits/feedback_id；共享反馈ID冲突时拒绝混淆节点。默认上电不使能。无额外台架限幅。
- 板上固件：`First/build/instances/First.elf`，SHA256 `9A211A65E17A69BB24BB749E4D25F1A02FA0B953DCF2D87DAC581D64F51D3805`；编译通过，烧录Verified OK。实际量程/反馈ID适配九组回归通过；后续重复ID冲突补充修改按用户要求直接编译烧录，未重复主机测试。
- 通信复测：ID2 ready=1，反馈持续更新，CAN错误0；`docs/evidence/dual-id2-online.csv` / `.json` / `.log`。
- 10秒速度试验：ID2，目标5rad/s、Kd1、Kp0、TorqueFF0；第一次因采集脚本把无线串口短暂延迟当故障提前结束，结束已失能。修正采集脚本后完整执行用户授权10秒，稳态末0.5秒均速4.63147rad/s，范围4.49207..4.80952；实际存在约0.37rad/s误差。反馈432个运行样本，无电机故障。结束通过基础失能接口，反馈已失能。
- 最终证据：`docs/evidence/dual-id2-velocity5-10s-complete.csv` / `.json` / `.log`；短试失败证据另保留 `dual-id2-velocity5-10s.*`。
- 最新重复试验：初始ID2反馈已90秒未更新，TEC128/LEC3；复位后仍TX完成0、反馈0，未执行使能或运动。证据 `docs/evidence/dual-id2-velocity5-10s-repeat.*`、`dual-id2-velocity5-10s-repeat-reset.*`。不能将上一次正常运行替代本次失败。下一步确认电机当前24V/黄灯及CAN连接，再复测10秒。
- 后续retry2已通过：复位后ID1/ID2均有实际反馈、配置通过；仅ID2执行目标5rad/s、10秒、Kd1。末0.5秒均速4.63699，范围4.49207..4.74603rad/s，432个运行样本；CAN错误0、电机故障0，结束反馈已失能。证据 `docs/evidence/dual-id2-velocity5-10s-retry2.csv` / `.json` / `.log`。此前失联记录为历史条件，不是当前状态。
- 最新双机同时测试通过：目标均5rad/s、Kd1、10秒，ID1末0.5秒均速4.81781（4.74603..4.84127），ID2均速4.62043（4.46032..4.84127）rad/s；各432个运行样本，CAN错误0、两机故障0，最终均已失能。证据 `docs/evidence/dual-both-velocity5-10s.csv` / `.json` / `.log`。实际误差须保留，不宣称精确5rad/s。
- Git交付：代码提交`cff5520`，分支main，远端`https://github.com/cha815/Wallcar`；保留原始提交77d8cb2。代码与本记录沿main正常推送，同步状态以`git status`和远端main为准。
- 最新四机结果：ID1～4目标均5rad/s，Kp0/Kd1/TorqueFF0，同时10秒，每台216个运行样本；末0.5秒均速分别4.81746、4.63228、4.80423、4.75926rad/s。CAN错误和电机故障均0，结束均反馈失能。实际稳态误差保留。见`docs/four-motor-test.md`及`dual-four-velocity5-10s.*`。
- 当前板端镜像：`First/build/four-instances/First.elf`，SHA256 `8DD07187C83F47FEFEF7D11E1B8BFB76C4756DB898D77C69DBBE6E0D749C3787`；烧录证据`dual-four-flash-usb-reset.log`。上面的DAP故障和双机镜像条目均为历史记录。
- 下一步：保存四实例改动与实测证据并沿main正常推送；双机交付cff5520/92800bd保留。
- 文档：[接口](docs/motor-instance-api.md)、[任务与证据](docs/dual-motor-refactor.md)、[协议链路](docs/protocol-audit.md)。
