/* 作者：Codex；文件：can_link_probe.h；说明：ID2只读物理链路诊断入口。 */
#ifndef CAN_LINK_PROBE_H
#define CAN_LINK_PROBE_H
#include <stdint.h>
void can_link_probe_init(uint32_t now);
void can_link_probe_tick(uint32_t now);
#endif /* CAN_LINK_PROBE_H */
