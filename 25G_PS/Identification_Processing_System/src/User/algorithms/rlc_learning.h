#ifndef RLC_LEARNING_H
#define RLC_LEARNING_H

#include <stddef.h>

#include "coherent_transfer_measurement.h"
#include "rlc_filter_classifier.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RLC_LEARNING_MAX_POINTS 64U

typedef struct {
    rlc_filter_class_t filter_class;
    float low_edge_magnitude;
    float high_edge_magnitude;
    float peak_frequency_hz;
    float peak_magnitude;
    size_t point_count;
} rlc_learning_result_t;

/* Measure one coherent response point per frame. */
int rlc_learning_measure_scan(const float *input_frames,
                              const float *output_frames,
                              size_t frame_stride,
                              size_t sample_count,
                              float sample_rate_hz,
                              const float *frequency_hz,
                              size_t point_count,
                              coherent_transfer_response_t *responses);

/* Classify a measured response scan and extract its simple summary. */
int rlc_learning_summarize(const float *frequency_hz,
                           const coherent_transfer_response_t *responses,
                           size_t point_count,
                           rlc_learning_result_t *result);

#ifdef __cplusplus
}
#endif

#endif