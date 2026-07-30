/******************************************************************************
 * g26_signal_analysis.h
 *
 * 4096-sample harmonic analysis for the 2026 G-question signal path.
 ******************************************************************************/

#ifndef USER_INCLUDE_G26_SIGNAL_ANALYSIS_H_
#define USER_INCLUDE_G26_SIGNAL_ANALYSIS_H_

#include "arm_math.h"
#include "xil_types.h"

#define G26_SIGNAL_SAMPLE_COUNT       4096U
#define G26_SIGNAL_MAX_COMPONENTS     3U

#define G26_SIGNAL_OK                  0
#define G26_SIGNAL_ERROR              (-1)

typedef struct {
    float32_t frequency_hz;
    float32_t amplitude_mv;
    float32_t phase_rad;
    u16 harmonic_order;
} g26_signal_component_t;

typedef struct {
    g26_signal_component_t components[G26_SIGNAL_MAX_COMPONENTS];
    u32 component_count;
    float32_t fundamental_frequency_hz;
    float32_t dc_mv;
    float32_t rms_mv;
    float32_t upp_mv;
    float32_t normalized_residual;
} g26_signal_result_t;

/** Initialize the shared CMSIS 4096-point real FFT instance. */
int g26_signal_analysis_init(void);

/**
 * Analyze exactly one PL-FIR frame. samples are signed FIR outputs; no raw ADC
 * unpacking or PS-side filtering is performed. mv_per_code may be negative to
 * include a measured polarity inversion in the scalar calibration.
 */
int g26_signal_analyze(
    const s16 samples[G26_SIGNAL_SAMPLE_COUNT],
    float32_t sample_rate_hz,
    float32_t mv_per_code,
    g26_signal_result_t *result);

/**
 * Generate one or three complete, phase-aligned AC waveform periods. The first
 * and last output points coincide so a display can draw complete periods.
 */
int g26_signal_generate_waveform(
    const g26_signal_result_t *result,
    u32 period_count,
    float32_t *output_mv,
    u32 output_count);

/** Run synthetic 2/3-line harmonic and high-density Upp regressions. */
int g26_signal_analysis_self_test(void);

#endif /* USER_INCLUDE_G26_SIGNAL_ANALYSIS_H_ */
