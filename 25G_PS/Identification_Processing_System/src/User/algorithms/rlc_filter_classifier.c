#include "rlc_filter_classifier.h"

#include <math.h>

static float mean_range(const float *values, size_t first, size_t last)
{
    float sum = 0.0f;
    size_t i;
    for (i = first; i < last; ++i) {
        sum += values[i];
    }
    return sum / (float)(last - first);
}

int rlc_filter_classify(const float *frequency_hz,
                               const float *magnitude,
                               size_t count,
                               rlc_filter_class_t *result)
{
    size_t edge_count;
    size_t i;
    float low_edge;
    float high_edge;
    float interior_max = 0.0f;
    float interior_min = 1.0e30f;
    float edge_max;
    float edge_min;

    if (frequency_hz == NULL || magnitude == NULL || result == NULL ||
        count < 5U) {
        return -1;
    }
    for (i = 1U; i < count; ++i) {
        if (!isfinite(frequency_hz[i]) || !isfinite(magnitude[i]) ||
            frequency_hz[i] <= frequency_hz[i - 1U] || magnitude[i] < 0.0f) {
            return -1;
        }
    }
    if (!isfinite(frequency_hz[0]) || !isfinite(magnitude[0]) ||
        magnitude[0] < 0.0f) {
        return -1;
    }

    edge_count = count / 4U;
    if (edge_count == 0U) {
        edge_count = 1U;
    }
    low_edge = mean_range(magnitude, 0U, edge_count);
    high_edge = mean_range(magnitude, count - edge_count, count);
    edge_max = (low_edge > high_edge) ? low_edge : high_edge;
    edge_min = (low_edge < high_edge) ? low_edge : high_edge;

    for (i = edge_count; i < count - edge_count; ++i) {
        if (magnitude[i] > interior_max) {
            interior_max = magnitude[i];
        }
        if (magnitude[i] < interior_min) {
            interior_min = magnitude[i];
        }
    }

    *result = RLC_FILTER_CLASS_UNKNOWN;
    if (low_edge >= high_edge * 2.0f && low_edge > interior_min * 1.5f) {
        *result = RLC_FILTER_CLASS_LOW_PASS;
    } else if (high_edge >= low_edge * 2.0f && high_edge > interior_min * 1.5f) {
        *result = RLC_FILTER_CLASS_HIGH_PASS;
    } else if (edge_min > 0.0f && interior_max >= edge_max * 1.5f) {
        *result = RLC_FILTER_CLASS_BAND_PASS;
    } else if (interior_min <= edge_min * 0.5f && edge_max <= edge_min * 2.0f) {
        *result = RLC_FILTER_CLASS_BAND_STOP;
    }
    return 0;
}