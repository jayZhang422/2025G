#ifndef BASIC_OUTPUT_H
#define BASIC_OUTPUT_H

#include "dac_vpp_calibration.h"
#include "open_loop_output_planner.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BASIC_OUTPUT_MIN_FREQUENCY_HZ       100.0f
#define BASIC_OUTPUT_MAX_FREQUENCY_HZ       3000.0f
#define BASIC_OUTPUT_FREQUENCY_STEP_HZ      100.0f
#define BASIC_OUTPUT_MIN_TARGET_VPP         1.0f
#define BASIC_OUTPUT_MAX_TARGET_VPP         2.0f
#define BASIC_OUTPUT_TARGET_VPP_STEP        0.1f

int basic_output_validate_request(const open_loop_output_request_t *request);

int basic_output_plan(const dac_vpp_calibration_curve_t *calibration,
                      const open_loop_output_request_t *request,
                      open_loop_output_plan_t *plan);

const transfer_function_model_coeffs_t *basic_output_known_model(void);

#ifdef __cplusplus
}
#endif

#endif
