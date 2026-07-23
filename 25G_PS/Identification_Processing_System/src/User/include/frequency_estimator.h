#ifndef FREQUENCY_ESTIMATOR_H
#define FREQUENCY_ESTIMATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float min_frequency_hz;
    float max_frequency_hz;
    float step_hz;
} frequency_estimator_config_t;

typedef struct {
    float frequency_hz;
    float magnitude;
} frequency_estimate_t;

int frequency_estimator_estimate(const float *samples,
                                 size_t sample_count,
                                 float sample_rate_hz,
                                 const frequency_estimator_config_t *config,
                                 frequency_estimate_t *estimate);

#ifdef __cplusplus
}
#endif

#endif