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

typedef struct {
    UINTPTR base_address;
} iq_demodulator_t;

typedef struct {
    s64 i_sum;
    s64 q_sum;
    u16 sample_count;
    u32 result_sequence;
    u32 phase_increment;
} iq_measurement_t;

int iq_demodulator_init(iq_demodulator_t *detector);
int iq_demodulator_measure(iq_demodulator_t *detector, float32_t frequency_hz,
                           iq_measurement_t *measurement);

#endif /* USER_INCLUDE_IQ_DEMODULATOR_H_ */
