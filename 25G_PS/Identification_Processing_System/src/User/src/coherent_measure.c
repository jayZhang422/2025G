#include "coherent_measure.h"

#include <math.h>

static void accumulate_iq(const float *samples,
                          size_t count,
                          float sample_rate_hz,
                          float frequency_hz,
                          float *i_value,
                          float *q_value)
{
    const float scale = 2.0f / (float)count;
    const float step = 6.28318530717958647692f * frequency_hz / sample_rate_hz;
    float i_sum = 0.0f;
    float q_sum = 0.0f;
    size_t n;

    for (n = 0U; n < count; ++n) {
        const float phase = step * (float)n;
        i_sum += samples[n] * cosf(phase);
        q_sum -= samples[n] * sinf(phase);
    }
    *i_value = scale * i_sum;
    *q_value = scale * q_sum;
}

int coherent_measure_transfer(const float *input,
                              const float *output,
                              size_t sample_count,
                              float sample_rate_hz,
                              float frequency_hz,
                              coherent_response_t *response)
{
    float input_i;
    float input_q;
    float output_i;
    float output_q;
    float denominator;

    if (input == NULL || output == NULL || response == NULL ||
        sample_count == 0U || !isfinite(sample_rate_hz) ||
        !isfinite(frequency_hz) || sample_rate_hz <= 0.0f ||
        frequency_hz < 0.0f || frequency_hz >= sample_rate_hz * 0.5f) {
        return -1;
    }

    accumulate_iq(input, sample_count, sample_rate_hz, frequency_hz,
                  &input_i, &input_q);
    accumulate_iq(output, sample_count, sample_rate_hz, frequency_hz,
                  &output_i, &output_q);
    denominator = input_i * input_i + input_q * input_q;
    if (denominator <= 1.0e-20f) {
        return -2;
    }

    response->real = (output_i * input_i + output_q * input_q) / denominator;
    response->imag = (output_q * input_i - output_i * input_q) / denominator;
    response->magnitude = sqrtf(response->real * response->real +
                                response->imag * response->imag);
    response->phase_rad = atan2f(response->imag, response->real);
    return 0;
}