#ifndef WAVEFORM_INFERENCE_H
#define WAVEFORM_INFERENCE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WAVEFORM_INFERENCE_MAX_BINS 64U
#define WAVEFORM_INFERENCE_TABLE_SIZE 4096U
#define WAVEFORM_INFERENCE_DAC_MAX 16383U

typedef struct {
    unsigned int harmonic;
    float real;
    float imag;
} waveform_response_bin_t;

/*
 * Input and response samples use normalized full-scale units.
 * The response bin is H_k = real + j*imag under the I+jQ convention.
 * The output table is unsigned 14-bit DAC code, centered at midscale.
 */
int waveform_inference_build_table(const float *input_samples,
                                   size_t sample_count,
                                   float sample_rate_hz,
                                   float fundamental_hz,
                                   const waveform_response_bin_t *responses,
                                   size_t response_count,
                                   uint16_t *table,
                                   size_t table_count);

#ifdef __cplusplus
}
#endif

#endif