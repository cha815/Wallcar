#ifndef FDCAN_DIAG_H
#define FDCAN_DIAG_H
#include <stdint.h>
typedef enum { FDCAN_DIAG_LOOPBACK=1, FDCAN_DIAG_EXTERNAL=2 } fdcan_diag_mode_t;
typedef uint32_t (*fdcan_diag_clock_fn)(void);
typedef void (*fdcan_diag_log_fn)(const char *line);
void fdcan_diag_init(fdcan_diag_mode_t mode,fdcan_diag_clock_fn clock,fdcan_diag_log_fn log);
void fdcan_diag_tick(void);
int fdcan_diag_done(void);
#endif
