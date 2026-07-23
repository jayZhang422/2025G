#include "frequency_estimator.h"

#include <math.h>
#include <stddef.h>

#define FREQUENCY_ESTIMATOR_MAX_BINS 1000000U

int frequency_estimator_estimate(const float *samples,
                                 size_t sample_count,
                                 float sample_rate_hz,
                                 const frequency_estimator_config_t *config,
                                 frequency_estimate_t *estimate)
{
    const float two_pi = 6.28318530717958647692f;
    float mean = 0.0f;
    float best_magnitude = -1.0f;
    float best_frequency = 0.0f;
    size_t bin_count;
    size_t n;
    size_t bin;

    if (samples == NULL || config == NULL || estimate == NULL ||
        sample_count < 4U || !isfinite(sample_rate_hz) ||
        sample_rate_hz <= 0.0f || !isfinite(config->min_frequency_hz) ||
        !isfinite(config->max_frequency_hz) || !isfinite(config->step_hz) ||
        config->min_frequency_hz < 0.0f ||
        config->max_frequency_hz <= config->min_frequency_hz ||
        config->max_frequency_hz >= sample_rate_hz * 0.5f ||
        config->step_hz <= 0.0f) {
        return -1;
    }

    bin_count = (size_t)floorf((config->max_frequency_hz -
                                config->min_frequency_hz) /
                               config->step_hz) + 1U;
    if (bin_count == 0U || bin_count > FREQUENCY_ESTIMATOR_MAX_BINS) {
        return -1;
    }

    for (n = 0U; n < sample_count; ++n) {
        if (!isfinite(samples[n])) {
            return -1;
        }
        mean += samples[n];
    }
    mean /= (float)sample_count;

    for (bin = 0U; bin < bin_count; ++bin) {
        const float frequency = config->min_frequency_hz +
                                config->step_hz * (float)bin;
        const float phase_step = two_pi * frequency / sample_rate_hz;
        float i_sum = 0.0f;
        float q_sum = 0.0f;
        float magnitude;
        for (n = 0U; n < sample_count; ++n) {
            const float phase = phase_step * (float)n;
            const float centered = samples[n] - mean;
            i_sum += centered * cosf(phase);
            q_sum -= centered * sinf(phase);
        }
        magnitude = (2.0f / (float)sample_count) *
                    sqrtf(i_sum * i_sum + q_sum * q_sum);
        if (magnitude > best_magnitude) {
            best_magnitude = magnitude;
            best_frequency = frequency;
        }
    }

    estimate->frequency_hz = best_frequency;
    estimate->magnitude = best_magnitude;
    return 0;
}