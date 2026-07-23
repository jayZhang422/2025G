#ifndef FUNDAMENTAL_FREQUENCY_ESTIMATOR_H
#define FUNDAMENTAL_FREQUENCY_ESTIMATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float min_frequency_hz;
    float max_frequency_hz;
    float step_hz;
} fundamental_frequency_search_config_t;

typedef struct {
    float frequency_hz;
    float magnitude;
} fundamental_frequency_estimate_t;

int fundamental_frequency_estimate(const float *samples,
                                 size_t sample_count,
                                 float sample_rate_hz,
                                 const fundamental_frequency_search_config_t *config,
                                 fundamental_frequency_estimate_t *estimate);

#ifdef __cplusplus
}
#endif

#endif