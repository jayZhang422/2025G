/******************************************************************************
 * iq_demodulator.h
 *
 * AXI-Lite driver for the PL lock-in detector. Results are held by the IP
 * until the driver acknowledges STATUS[0].
 ******************************************************************************/

#ifndef USER_INCLUDE_IQ_DEMODULATOR_H_
#define USER_INCLUDE_IQ_DEMODULATOR_H_

#include "arm_math.h"
#include "xil_types.h"

/** PL 单频锁相/IQ 检测器的 AXI-Lite 访问上下文。 */
typedef struct {
    UINTPTR base_address; /**< IQ IP 核的 PS 映射基址。 */
} iq_demodulator_t;

/** PL 在一个测量窗口内锁存的 IQ 结果快照。 */
typedef struct {
    s64 i_sum;            /**< 同相支路有符号累加值；PL 原值为 48 位。 */
    s64 q_sum;            /**< 正交支路有符号累加值；PL 原值为 48 位。 */
    u16 sample_count;     /**< 本次累加实际包含的采样点数。 */
    u32 result_sequence;  /**< PL 结果序号，用于识别新旧快照。 */
    u32 phase_increment;  /**< 本次测量实际采用的 32 位 NCO 相位步进。 */
} iq_measurement_t;

int iq_demodulator_init(iq_demodulator_t *detector);
int iq_demodulator_measure(iq_demodulator_t *detector, float32_t frequency_hz,
                           iq_measurement_t *measurement);

#endif /* USER_INCLUDE_IQ_DEMODULATOR_H_ */
