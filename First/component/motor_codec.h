#ifndef MOTOR_CODEC_H
#define MOTOR_CODEC_H
#include "../interfaces/motor_port.h"
int motor_codec_pack(const motor_command_t *command,uint8_t data[8]);
int motor_codec_feedback(const uint8_t data[8],motor_feedback_t *feedback);
int motor_codec_limits_valid(const motor_limits_t *limits);
int motor_codec_pack_limits(const motor_command_t *command,const motor_limits_t *limits,uint8_t data[8]);
int motor_codec_feedback_limits(const uint8_t data[8],const motor_limits_t *limits,motor_feedback_t *feedback);
/* Returns the exact decoded command corresponding to the packed wire bytes. */
int motor_codec_quantize(const motor_command_t *command,motor_command_t *quantized);
#endif /* MOTOR_CODEC_H */
