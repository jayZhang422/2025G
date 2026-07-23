#include "known_model.h"

#include <math.h>
#include <stddef.h>

int known_model_eval(const known_model_coeffs_t *coeffs,
                     float frequency_hz,
                     known_model_response_t *response)
{
    const float two_pi = 6.28318530717958647692f;
    float omega;
    float omega_sq;
    float numerator_real;
    float numerator_imag;
    float denominator_real;
    float denominator_imag;
    float denominator_norm;

    if (coeffs == NULL || response == NULL || !isfinite(frequency_hz) ||
        frequency_hz < 0.0f) {
        return -1;
    }

    omega = two_pi * frequency_hz;
    omega_sq = omega * omega;
    numerator_real = coeffs->b2 - coeffs->b0 * omega_sq;
    numerator_imag = coeffs->b1 * omega;
    denominator_real = coeffs->a2 - coeffs->a0 * omega_sq;
    denominator_imag = coeffs->a1 * omega;
    denominator_norm = denominator_real * denominator_real +
                       denominator_imag * denominator_imag;

    if (denominator_norm <= 1.0e-20f) {
        return -1;
    }

    response->real = (numerator_real * denominator_real +
                      numerator_imag * denominator_imag) / denominator_norm;
    response->imag = (numerator_imag * denominator_real -
                      numerator_real * denominator_imag) / denominator_norm;
    response->magnitude = sqrtf(response->real * response->real +
                                response->imag * response->imag);
    response->phase_rad = atan2f(response->imag, response->real);
    return 0;
}