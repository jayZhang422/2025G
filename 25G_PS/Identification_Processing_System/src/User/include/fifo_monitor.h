/******************************************************************************
 * fifo_monitor.h
 *
 * Polling access to the PL ADC FIFO/AXIS diagnostic monitor.
 ******************************************************************************/

#ifndef USER_INCLUDE_FIFO_MONITOR_H_
#define USER_INCLUDE_FIFO_MONITOR_H_

#include "xil_types.h"

typedef struct {
    u32 status;
    u32 version;
    u64 adc_sample_count;
    u64 fifo_write_count;
    u64 blocked_high_watermark_count;
    u64 blocked_reset_count;
    u64 axis_beat_count;
    u64 frame_count;
    u64 axis_stall_cycle_count;
    u64 last_frame_timestamp;
} fifo_monitor_snapshot_t;

int fifo_monitor_init(void);
int fifo_monitor_snapshot(fifo_monitor_snapshot_t *snapshot);
int fifo_monitor_clear_sticky(void);
void fifo_monitor_print(const char *tag,
                        const fifo_monitor_snapshot_t *snapshot);

#endif /* USER_INCLUDE_FIFO_MONITOR_H_ */
