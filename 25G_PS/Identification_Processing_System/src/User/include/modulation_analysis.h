/******************************************************************************
 * modulation_analysis.h
 *
 * One-frame modulation recognition and parameter estimation for interleaved
 * complex DDC samples. The implementation is hardware-independent.
 ******************************************************************************/

#ifndef USER_INCLUDE_MODULATION_ANALYSIS_H_
#define USER_INCLUDE_MODULATION_ANALYSIS_H_

#include "xil_types.h"

typedef enum {
    MODULATION_UNKNOWN = 0,
    MODULATION_CW,
    MODULATION_AM,
    MODULATION_FM,
    MODULATION_2ASK,
    MODULATION_2FSK,
    MODULATION_2PSK
} modulation_type_t;

typedef struct {
    modulation_type_t type;
    float confidence;
    float carrier_offset_hz;
    float modulation_frequency_hz;
    float am_index;
    float frequency_deviation_hz;
    float fm_index;
    u32 bit_rate_bps;
    float symbol_phase_samples;
    float fsk_low_offset_hz;
    float fsk_high_offset_hz;
    float fsk_index;
} modulation_result_t;

int modulation_analyze_frame(const s16 *interleaved_iq,
                             u32 complex_samples,
                             float sample_rate_hz,
                             modulation_result_t *result);
int modulation_analysis_self_test(void);
const char *modulation_type_name(modulation_type_t type);

#endif /* USER_INCLUDE_MODULATION_ANALYSIS_H_ */
