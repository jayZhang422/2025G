#ifndef BASIC_SERVICE_H
#define BASIC_SERVICE_H

#include <stdint.h>

#include "calibration.h"
#include "known_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float frequency_hz;
    float target_output_vpp;
} basic_output_request_t;

typedef struct {
    float model_magnitude;
    float required_input_vpp;
    uint16_t amplitude_code;
} basic_output_plan_t;

/*
 * Plans one open-loop output setup:
 * required input Vpp = target output Vpp / |H(j2*pi*f)|.
 * No ADC/PID feedback or runtime output correction is performed here.
 */
int basic_service_plan_output(const known_model_coeffs_t *model,
                              const calibration_curve_t *calibration,
                              const basic_output_request_t *request,
                              basic_output_plan_t *plan);

#ifdef __cplusplus
}
#endif

#endif