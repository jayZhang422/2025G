#include "response_waveform_generator.h"

#include <math.h>
#include <stddef.h>

static float clamp_unit(float value)
{
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

int response_waveform_generate_table(const float *input_samples,
                                   size_t sample_count,
                                   float sample_rate_hz,
                                   float fundamental_hz,
                                   const response_waveform_bin_t *responses,
                                   size_t response_count,
                                   uint16_t *table,
                                   size_t table_count)
{
    const float two_pi = 6.28318530717958647692f;
    float input_mean = 0.0f;
    float input_real[RESPONSE_WAVEFORM_MAX_BINS];
    float input_imag[RESPONSE_WAVEFORM_MAX_BINS];
    float output_real[RESPONSE_WAVEFORM_MAX_BINS];
    float output_imag[RESPONSE_WAVEFORM_MAX_BINS];
    size_t n;
    size_t k;
    size_t m;

    if (input_samples == NULL || responses == NULL || table == NULL ||
        sample_count < 4U || response_count == 0U ||
        response_count > RESPONSE_WAVEFORM_MAX_BINS ||
        table_count == 0U || !isfinite(sample_rate_hz) ||
        !isfinite(fundamental_hz) || sample_rate_hz <= 0.0f ||
        fundamental_hz <= 0.0f ||
        fundamental_hz >= sample_rate_hz * 0.5f) {
        return -1;
    }

    for (n = 0U; n < sample_count; ++n) {
        if (!isfinite(input_samples[n])) {
            return -1;
        }
        input_mean += input_samples[n];
    }
    input_mean /= (float)sample_count;

    for (k = 0U; k < response_count; ++k) {
        const unsigned int harmonic = responses[k].harmonic;
        const float frequency = fundamental_hz * (float)harmonic;
        const float phase_step = two_pi * frequency / sample_rate_hz;
        float i_sum = 0.0f;
        float q_sum = 0.0f;

        if (harmonic == 0U || frequency >= sample_rate_hz * 0.5f ||
            !isfinite(responses[k].real) || !isfinite(responses[k].imag) ||
            (k > 0U && harmonic <= responses[k - 1U].harmonic)) {
            return -1;
        }
        for (n = 0U; n < sample_count; ++n) {
            const float phase = phase_step * (float)n;
            const float centered = input_samples[n] - input_mean;
            i_sum += centered * cosf(phase);
            q_sum -= centered * sinf(phase);
        }
        input_real[k] = (2.0f / (float)sample_count) * i_sum;
        input_imag[k] = (2.0f / (float)sample_count) * q_sum;
        output_real[k] = input_real[k] * responses[k].real -
                        input_imag[k] * responses[k].imag;
        output_imag[k] = input_real[k] * responses[k].imag +
                        input_imag[k] * responses[k].real;
    }

    for (m = 0U; m < table_count; ++m) {
        float value = input_mean;
        for (k = 0U; k < response_count; ++k) {
            const float phase = two_pi * (float)responses[k].harmonic *
                                (float)m / (float)table_count;
            value += output_real[k] * cosf(phase) -
                     output_imag[k] * sinf(phase);
        }
        value = clamp_unit(value);
        table[m] = (uint16_t)lroundf((value + 1.0f) *
                                     (0.5f * (float)RESPONSE_WAVEFORM_DAC_MAX));
    }
    return 0;
}