/******************************************************************************
 * signal_processing.h
 *
 * Two-component signal identification for the AD9226 DMA frame.
 ******************************************************************************/

#ifndef USER_INCLUDE_SIGNAL_PROCESSING_H_
#define USER_INCLUDE_SIGNAL_PROCESSING_H_

#include "arm_math.h"
#include "xstatus.h"
#include "xil_types.h"

typedef enum {
    SIGNAL_WAVE_SINE = 0,
    SIGNAL_WAVE_TRIANGLE = 1
} signal_waveform_t;

typedef struct {
    float32_t frequency_hz;
    float32_t fundamental_amplitude;
    float32_t measured_phase_rad;
    signal_waveform_t waveform;
} signal_component_t;

typedef struct {
    signal_component_t channel_a;
    signal_component_t channel_b;
    float32_t normalized_residual;
} signal_analysis_result_t;

/*
 * Convert ADC codes, perform a windowed FFT coarse search, then select the
 * sine/triangle pair with the smallest joint time-domain residual.
 */
int signal_analyze_frame(const u16 *raw_samples,
                         arm_rfft_fast_instance_f32 *fft_instance,
                         float32_t *time_domain,
                         float32_t *fft_input,
                         float32_t *fft_spectrum,
                         float32_t *fft_magnitude,
                         float32_t *model_workspace,
                         signal_analysis_result_t *result);

/* Low-pass only the values safe to update during phase-continuous tracking. */
void signal_track_result(signal_analysis_result_t *tracked,
                         const signal_analysis_result_t *measurement,
                         float32_t frequency_alpha);

const char *signal_waveform_name(signal_waveform_t waveform);

/* Algorithm-only regression; it does not access DMA or the DDS control BRAM. */
int signal_run_self_tests(void);

#endif /* USER_INCLUDE_SIGNAL_PROCESSING_H_ */
