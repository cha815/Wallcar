/* 作者：Codex；文件：bench_link.h；说明：双机台架文本命令及固定二进制遥测。 */
#ifndef BENCH_LINK_H
#define BENCH_LINK_H
#include <stdint.h>
typedef enum { BENCH_INVALID,BENCH_ENABLE,BENCH_DISABLE,BENCH_VEL } bench_kind_t;
typedef struct { bench_kind_t kind;uint8_t id;float velocity,kd; } bench_command_t;
typedef struct {
    int (*read)(uint8_t *byte);
    int (*write)(const uint8_t *data,uint16_t length);
    char line[64];uint8_t used,discard;
} bench_link_t;
void bench_link_init(bench_link_t *link,int (*read)(uint8_t *),int (*write)(const uint8_t *,uint16_t));
int bench_parse(const char *line,bench_command_t *command);
int bench_link_read(bench_link_t *link,bench_command_t *command);
int bench_link_write(bench_link_t *link,const float values[15]);
#endif /* BENCH_LINK_H */
