# FDCAN implementation ledger — approved plan 2026-10-14

Latest objective: DIAG_MODE6 main-controller speed PID -> torque only, driver Kp/Kd0, feedback speed confirmed by user, 12s repeating20/30/60/10rad/s, UART7 two-channel100Hz. Eight host suites pass; target final build52784/472/6776, independent review and fixes verified. Final step is program/verify/reset; after that no tools or serial operation per user. Actual high-speed response not measured by agent. See vofa-pid-step.md.

## Current objective changed by user
Latest 2026-10-14: reference mw_canQZB core ported to CAN1/ID1, IOC + HAL FD_NO_BRS, RX DBTP preserved. Reference image flashed/verified after wireless DAPLink reconnect. User-commanded VEL 5 1 test accepted: last running second mean4.8095rad/s, range4.6825..4.9365, temperature34C, fault0, fresh STOP confirmed. Source-only80C temperature gates pass seven host suites; final rebuild rejected, therefore NOT flashed. Historical blocked results below are superseded by this actual motion result, not a diagnosis of the earlier anomaly. See motor-tuning-guide.md and evidence/reference-vel5-kd1.csv/png.
2026-10-14: implement ID1 MIT tuning, UART commands + VOFA JustFloat, real waveform-based speed/position tuning. User confirmed motor is unloaded and permitted small motion. See motor-tuning-design.md. Earlier readonly-only restriction is superseded for this tuning firmware; no motor Flash writes are authorized or needed.
Old readonly review stopped before findings; no review approval claimed. FDCAN target builds still needed before motion.

## New objective evidence
- Target DIAG_MODE=4 built without warnings and flashed with Verified OK. Seven host suites pass.
- User confirmed physical connector CAN1; corrected original PA11/PA12 to MC02 PD0/PD1 and synchronized IOC. Real TX/RX now match with zero protocol/TEC/REC errors.
- UART nonblocking JustFloat is 15 float32 + tail =64B at100Hz; runtime commands and real feedback verified.
- Confirmed original CAN timeout0x2000, explicit CLEAR returned OFF/fault0; no Flash write.
- First ARM triggered motor protection0x0002; motion command never accepted. Stop confirmed by new disabled feedback. Voltage threshold80.0V, error snapshot100.0956V, reported temperature142C; user states24V supply. Motion tuning blocked by this unresolved hardware/measurement condition, not considered gain-tuned.
- Review fixes: STOP after FD requires fresh count/time + TXcomplete + no pending; actual quantized submitted targets exported; bounded targets preserved while reducing gains; all MIT paths share quantized torque check; slight future timestamp tolerated; explicit configuration retry diagnostic; runner exports stop confirmation. Unit regressions passed.
- Remaining: verify measured motor supply/driver analog sampling and temperature; collect usable motion responses only after motor protection issue is resolved. Old four-wheel spec remains an earlier draft, not current acceptance.
- User measured supply at motor terminals:24.7V, current yellow power LED. This does not explain historical100.0956V fault snapshot; no threshold/calibration changes made. Final firmware retains fault protection and defaults disabled.

Authority: the user-approved plan in this conversation. Sources: ../AGENT.md, ../docs/request.md, ../嵌入式软件分层架构总结.md, Motorevo protocol manual and existing CubeMX project.

- Task 1: complete — native host tests passed (DLC, flags, 32-slot queue, ID1-only parameter whitelist, status/parameter frames).
- Task 2: implementation present — HAL-mock and service tests passed. Current FDCAN target build pending: external execution request for loopback build was rejected; no target-build success asserted.
- Task 3: UART7 verified — COM12 receives H723 heartbeat and echoes UART7_ECHO_102021326858. Original 1MB flash backed up; UART image flash verified. Hardware FDCAN loopback / ID1 read-only validation pending.
- Task 4: test report, chassis spec, whole-change review — pending.

Ruling: no Git worktree or commits because this folder is not a Git repository; preserve generated source backups under docs/evidence/original before edits. No global configuration changes.
Ruling: diagnostic logging uses blocking UART only in the standalone test images; the normal image does not run tests or log from interrupts.
Pre-flight: frame model and queue belong to component; BSP translates HAL; device owns motor IDs and whitelist; service orchestrates tests through device; main initializes layers.

Ruling: 20 whitelisted parameter indices, not 22 (the concrete whitelist contains 20). Index 0 is prohibited regardless of R/W.
Ruling: TX request and RX response BRS are reported separately. A protocol reply has no wire sequence number; each test is serial, queued old frames are drained, TX completion and a fresh matching reply are required, and any timeout disqualifies the case.
Regression evidence: reply before send returns failed before capturing time before submission; fix passed full host suite. Bundled HAL routes protocol errors through ErrorCallback and retains ErrorCode; counting/consuming these errors was verified RED→GREEN.
