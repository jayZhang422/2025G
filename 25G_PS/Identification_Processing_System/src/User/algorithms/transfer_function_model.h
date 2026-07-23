#ifndef TRANSFER_FUNCTION_MODEL_H
#define TRANSFER_FUNCTION_MODEL_H

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
} transfer_function_model_coeffs_t;

typedef struct {
    float real;
    float imag;
    float magnitude;
    float phase_rad;
} transfer_function_response_t;

/* H(s) = (b0*s^2 + b1*s + b2) / (a0*s^2 + a1*s + a2). */
int transfer_function_model_eval(const transfer_function_model_coeffs_t *coeffs,
                     float frequency_hz,
                     transfer_function_response_t *response);

#ifdef __cplusplus
}
#endif

#endif