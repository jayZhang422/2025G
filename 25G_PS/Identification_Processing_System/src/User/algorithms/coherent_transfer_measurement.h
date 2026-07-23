#ifndef COHERENT_TRANSFER_MEASUREMENT_H
#define COHERENT_TRANSFER_MEASUREMENT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float real;
    float imag;
    float magnitude;
    float phase_rad;
} coherent_transfer_response_t;

/*
 * Calculates H(f) = Y(f) / X(f) using the synchronous I/Q equations.
 * Samples are centered, equally spaced, and represented as floating point.
 */
int coherent_transfer_measure(const float *input,
                              const float *output,
                              size_t sample_count,
                              float sample_rate_hz,
                              float frequency_hz,
                              coherent_transfer_response_t *response);

#ifdef __cplusplus
}
#endif

#endif