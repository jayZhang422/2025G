#include "open_loop_output_planner.h"

#include <math.h>

int open_loop_output_plan(const transfer_function_model_coeffs_t *model,
                              const dac_vpp_calibration_curve_t *calibration,
                              const open_loop_output_request_t *request,
                              open_loop_output_plan_t *plan)
{
    transfer_function_response_t response;

    if (model == NULL || calibration == NULL || request == NULL ||
        plan == NULL || !isfinite(request->frequency_hz) ||
        !isfinite(request->target_output_vpp) ||
        request->frequency_hz < 0.0f || request->target_output_vpp <= 0.0f ||
        transfer_function_model_eval(model, request->frequency_hz, &response) != 0 ||
        response.magnitude <= 1.0e-12f) {
        return -1;
    }

    plan->model_magnitude = response.magnitude;
    plan->required_input_vpp = request->target_output_vpp / response.magnitude;
    return dac_vpp_calibration_code_for_vpp(calibration, plan->required_input_vpp,
                                    &plan->amplitude_code);
}