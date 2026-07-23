#ifndef OPEN_LOOP_OUTPUT_PLANNER_H
#define OPEN_LOOP_OUTPUT_PLANNER_H

#include <stdint.h>

#include "dac_vpp_calibration.h"
#include "transfer_function_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float frequency_hz;
    float target_output_vpp;
} open_loop_output_request_t;

typedef struct {
    float model_magnitude;
    float required_input_vpp;
    uint16_t amplitude_code;
} open_loop_output_plan_t;

/*
 * Plans one open-loop output setup:
 * required input Vpp = target output Vpp / |H(j2*pi*f)|.
 * No ADC/PID feedback or runtime output correction is performed here.
 */
int open_loop_output_plan(const transfer_function_model_coeffs_t *model,
                              const dac_vpp_calibration_curve_t *calibration,
                              const open_loop_output_request_t *request,
                              open_loop_output_plan_t *plan);

#ifdef __cplusplus
}
#endif

#endif