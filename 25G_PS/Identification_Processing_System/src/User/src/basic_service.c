#include "basic_service.h"

#include <math.h>

int basic_service_plan_output(const known_model_coeffs_t *model,
                              const calibration_curve_t *calibration,
                              const basic_output_request_t *request,
                              basic_output_plan_t *plan)
{
    known_model_response_t response;

    if (model == NULL || calibration == NULL || request == NULL ||
        plan == NULL || !isfinite(request->frequency_hz) ||
        !isfinite(request->target_output_vpp) ||
        request->frequency_hz < 0.0f || request->target_output_vpp <= 0.0f ||
        known_model_eval(model, request->frequency_hz, &response) != 0 ||
        response.magnitude <= 1.0e-12f) {
        return -1;
    }

    plan->model_magnitude = response.magnitude;
    plan->required_input_vpp = request->target_output_vpp / response.magnitude;
    return calibration_code_for_vpp(calibration, plan->required_input_vpp,
                                    &plan->amplitude_code);
}