#ifndef TUNING_LINK_H
#define TUNING_LINK_H
#include <stdint.h>
typedef enum { TUNE_ARM,TUNE_STOP,TUNE_CLEAR,TUNE_PING,TUNE_VEL,TUNE_POS,TUNE_INFO,TUNE_READ,TUNE_INVALID } tune_command_kind_t;
typedef struct { tune_command_kind_t kind;float target,kp,kd; } tune_command_t;
int tuning_parse(const char *line,tune_command_t *command);
/* BSP transport is injected by main; parser/packing has no HAL dependency. */
typedef int (*tuning_read_fn)(uint8_t *byte);
typedef int (*tuning_write_fn)(const uint8_t *data,uint16_t length);
void tuning_link_init(tuning_read_fn read,tuning_write_fn write);
int tuning_link_command(tune_command_t *command);
int tuning_link_waveform(const float channels[15]);
/* JustFloat: target and feedback rad/s, followed by 00 00 80 7F. */
int tuning_link_speed_waveform(float target,float feedback);
#endif
