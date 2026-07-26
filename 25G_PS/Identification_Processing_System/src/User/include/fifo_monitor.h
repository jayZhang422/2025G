/******************************************************************************
 * fifo_monitor.h
 *
 * Polling access to the PL ADC FIFO/AXIS diagnostic monitor.
 ******************************************************************************/

#ifndef USER_INCLUDE_FIFO_MONITOR_H_
#define USER_INCLUDE_FIFO_MONITOR_H_

#include "xil_types.h"

/** 某一时刻从 PL ADC FIFO/AXIS 监视器读取的一致性诊断快照。 */
typedef struct {
    u32 status;                       /**< 当前状态位集合。 */
    u32 version;                      /**< PL 监视器寄存器接口版本。 */
    u64 adc_sample_count;             /**< ADC 侧观察到的累计样本数。 */
    u64 fifo_write_count;             /**< 成功写入 FIFO 的累计样本数。 */
    u64 blocked_high_watermark_count; /**< 因 FIFO 高水位阻塞的累计次数。 */
    u64 blocked_reset_count;          /**< 因复位状态阻塞的累计次数。 */
    u64 axis_beat_count;              /**< AXI-Stream 成功握手的累计拍数。 */
    u64 frame_count;                  /**< 已输出完整 AXIS 帧的累计数量。 */
    u64 axis_stall_cycle_count;       /**< AXIS valid 等待 ready 的累计周期。 */
    u64 last_frame_timestamp;         /**< 最近完整帧结束时的 PL 时间戳。 */
} fifo_monitor_snapshot_t;

int fifo_monitor_init(void);
int fifo_monitor_snapshot(fifo_monitor_snapshot_t *snapshot);
int fifo_monitor_clear_sticky(void);
void fifo_monitor_print(const char *tag,
                        const fifo_monitor_snapshot_t *snapshot);

#endif /* USER_INCLUDE_FIFO_MONITOR_H_ */
