#include "basic_output.h"

#include <math.h>

static const transfer_function_model_coeffs_t g_known_model = {
    0.0f, 0.0f, 5.0f,
    1.0e-8f, 3.0e-4f, 1.0f
};

static int is_on_grid(float value, float origin, float step)
{
    float index = (value - origin) / step;

    return fabsf(index - roundf(index)) <= 1.0e-3f;
}

int basic_output_validate_request(const open_loop_output_request_t *request)
{
    if (request == 0 || !isfinite(request->frequency_hz) ||
        !isfinite(request->target_output_vpp) ||
        request->frequency_hz < BASIC_OUTPUT_MIN_FREQUENCY_HZ ||
        request->frequency_hz > BASIC_OUTPUT_MAX_FREQUENCY_HZ ||
        request->target_output_vpp < BASIC_OUTPUT_MIN_TARGET_VPP ||
        request->target_output_vpp > BASIC_OUTPUT_MAX_TARGET_VPP ||
        !is_on_grid(request->frequency_hz,
                    BASIC_OUTPUT_MIN_FREQUENCY_HZ,
                    BASIC_OUTPUT_FREQUENCY_STEP_HZ) ||
        !is_on_grid(request->target_output_vpp,
                    BASIC_OUTPUT_MIN_TARGET_VPP,
                    BASIC_OUTPUT_TARGET_VPP_STEP)) {
        return -1;
    }
    return 0;
}

int basic_output_plan(const dac_vpp_calibration_curve_t *calibration,
                      const open_loop_output_request_t *request,
                      open_loop_output_plan_t *plan)
{
    if (basic_output_validate_request(request) != 0 ||
        calibration == 0 || plan == 0) {
        return -1;
    }

    return open_loop_output_plan(&g_known_model, calibration, request, plan);
}

const transfer_function_model_coeffs_t *basic_output_known_model(void)
{
    return &g_known_model;
}
