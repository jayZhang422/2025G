#ifndef KNOWN_MODEL_H
#define KNOWN_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float b0;
    float b1;
    float b2;
    float a0;
    float a1;
    float a2;
} known_model_coeffs_t;

typedef struct {
    float real;
    float imag;
    float magnitude;
    float phase_rad;
} known_model_response_t;

/* H(s) = (b0*s^2 + b1*s + b2) / (a0*s^2 + a1*s + a2). */
int known_model_eval(const known_model_coeffs_t *coeffs,
                     float frequency_hz,
                     known_model_response_t *response);

#ifdef __cplusplus
}
#endif

#endif