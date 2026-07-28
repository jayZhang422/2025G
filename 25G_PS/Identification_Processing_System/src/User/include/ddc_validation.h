/******************************************************************************
 * ddc_validation.h
 *
 * One-frame DDC transport checks and complex-baseband preservation statistics.
 ******************************************************************************/

#ifndef USER_INCLUDE_DDC_VALIDATION_H_
#define USER_INCLUDE_DDC_VALIDATION_H_

#include "xil_types.h"

typedef struct {
    u32 nco_phase_increment;
    u32 expected_build_id;
    u32 expected_decimation;
    u32 expected_adc_sample_rate_hz;
    u32 complex_samples_per_frame;
    u32 timeout_ms;
    u32 minimum_phase_power;
    u32 frequency_cluster_threshold_hz;
    u32 phase_jump_threshold_degrees;
} ddc_validation_config_t;

typedef struct {
    u32 power_min;
    u32 power_max;
    u32 power_mean;
    s32 instantaneous_frequency_min_hz;
    s32 instantaneous_frequency_max_hz;
    s32 instantaneous_frequency_mean_hz;
    u32 valid_frequency_count;
    s32 negative_frequency_mean_hz;
    s32 positive_frequency_mean_hz;
    u32 negative_frequency_count;
    u32 positive_frequency_count;
    u32 maximum_phase_step_millidegrees;
    u32 phase_jump_count;
    u32 dc_power_percent;
    u32 saturation_count;
    u32 zero_sample_count;
    u32 ddc_status;
    u32 ddc_output_count;
    u32 ddc_frame_count;
    u32 dma_length_bytes;
    u32 dma_status;
    u32 ddc_fault;
    u32 dma_error;
} ddc_frame_statistics_t;

extern const ddc_validation_config_t g_ddc_validation_config;

int ddc_validation_analyze_frame(const s16 *interleaved_iq,
                                 u32 complex_samples,
                                 u32 adc_sample_rate_hz,
                                 u32 decimation,
                                 const ddc_validation_config_t *config,
                                 ddc_frame_statistics_t *statistics);
int ddc_validation_run(const ddc_validation_config_t *config);

#endif /* USER_INCLUDE_DDC_VALIDATION_H_ */
