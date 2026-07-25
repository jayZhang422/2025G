#include "rlc_learning.h"

#include <math.h>

int rlc_learning_measure_scan(const float *input_frames,
                              const float *output_frames,
                              size_t frame_stride,
                              size_t sample_count,
                              float sample_rate_hz,
                              const float *frequency_hz,
                              size_t point_count,
                              coherent_transfer_response_t *responses)
{
    size_t i;

    if (input_frames == NULL || output_frames == NULL ||
        frequency_hz == NULL || responses == NULL || frame_stride < sample_count ||
        sample_count == 0U || point_count == 0U ||
        point_count > RLC_LEARNING_MAX_POINTS) {
        return -1;
    }

    for (i = 0U; i < point_count; ++i) {
        if (coherent_transfer_measure(input_frames + i * frame_stride,
                                      output_frames + i * frame_stride,
                                      sample_count, sample_rate_hz,
                                      frequency_hz[i], &responses[i]) != 0) {
            return -2;
        }
    }
    return 0;
}

int rlc_learning_summarize(const float *frequency_hz,
                           const coherent_transfer_response_t *responses,
                           size_t point_count,
                           rlc_learning_result_t *result)
{
    float magnitudes[RLC_LEARNING_MAX_POINTS];
    size_t edge_count;
    size_t i;
    size_t peak_index = 0U;

    if (frequency_hz == NULL || responses == NULL || result == NULL ||
        point_count < 5U || point_count > RLC_LEARNING_MAX_POINTS) {
        return -1;
    }

    for (i = 0U; i < point_count; ++i) {
        if (!isfinite(frequency_hz[i]) || !isfinite(responses[i].magnitude) ||
            responses[i].magnitude < 0.0f ||
            !isfinite(responses[i].real) || !isfinite(responses[i].imag) ||
            !isfinite(responses[i].phase_rad)) {
            return -1;
        }
        magnitudes[i] = responses[i].magnitude;
        if (magnitudes[i] > magnitudes[peak_index]) {
            peak_index = i;
        }
    }

    if (rlc_filter_classify(frequency_hz, magnitudes, point_count,
                            &result->filter_class) != 0) {
        return -2;
    }

    edge_count = point_count / 4U;
    if (edge_count == 0U) {
        edge_count = 1U;
    }
    result->low_edge_magnitude = 0.0f;
    result->high_edge_magnitude = 0.0f;
    for (i = 0U; i < edge_count; ++i) {
        result->low_edge_magnitude += magnitudes[i];
        result->high_edge_magnitude += magnitudes[point_count - edge_count + i];
    }
    result->low_edge_magnitude /= (float)edge_count;
    result->high_edge_magnitude /= (float)edge_count;
    result->peak_frequency_hz = frequency_hz[peak_index];
    result->peak_magnitude = magnitudes[peak_index];
    result->point_count = point_count;
    return 0;
}