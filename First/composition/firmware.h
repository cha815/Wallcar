/* 作者：Codex；文件：firmware.h；说明：双机默认镜像启动装配入口。 */
#ifndef FIRMWARE_H
#define FIRMWARE_H
#include <stdint.h>
#include "../interfaces/motor_registry.h"
int firmware_init(uint32_t now);
void firmware_tick(uint32_t now);
/* 上层通过ID取实例，实例表仅启动时注册。 */
const motor_registry_t *firmware_motors(void);
#endif /* FIRMWARE_H */
