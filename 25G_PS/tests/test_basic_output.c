#include <math.h>
#include <stdio.h>

#include "basic_output.h"
#include "basic_output_ui.h"

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

int main(void)
{
    const dac_vpp_calibration_curve_t curve = {
        {
            {0U, 0.0f}, {1024U, 0.346f}, {2048U, 0.690f},
            {4096U, 1.390f}, {6144U, 2.070f}, {8191U, 2.750f},
            {16383U, 5.470f}
        },
        7U
    };
    open_loop_output_request_t request = {1000.0f, 2.0f};
    open_loop_output_plan_t plan;
    basic_output_ui_t ui;

    if (basic_output_validate_request(&request) != 0 ||
        basic_output_plan(&curve, &request, &plan) != 0 ||
        !(plan.model_magnitude > 0.0f) ||
        !close_enough(plan.required_input_vpp,
                      request.target_output_vpp / plan.model_magnitude,
                      0.0001f)) {
        return 1;
    }

    request.frequency_hz = 3000.0f;
    request.target_output_vpp = 2.0f;
    if (basic_output_plan(&curve, &request, &plan) != 0 ||
        plan.required_input_vpp < 2.4f || plan.required_input_vpp > 2.6f ||
        plan.amplitude_code < 7000U || plan.amplitude_code > 8000U) {
        return 5;
    }

    request.frequency_hz = 1050.0f;
    if (basic_output_validate_request(&request) == 0) {
        return 2;
    }
    request.frequency_hz = 3000.0f;
    request.target_output_vpp = 2.1f;
    if (basic_output_validate_request(&request) == 0) {
        return 3;
    }

    if (basic_output_ui_init(&ui, 1000.0f, 2.0f) != 0 ||
        ui.selected_field != BASIC_OUTPUT_FIELD_FREQUENCY ||
        basic_output_ui_handle(&ui, BASIC_OUTPUT_UI_INCREMENT) != 0 ||
        ui.request.frequency_hz != 1100.0f ||
        basic_output_ui_handle(&ui, BASIC_OUTPUT_UI_SELECT) != 0 ||
        ui.selected_field != BASIC_OUTPUT_FIELD_TARGET_VPP ||
        basic_output_ui_handle(&ui, BASIC_OUTPUT_UI_DECREMENT) != 0 ||
        !close_enough(ui.request.target_output_vpp, 1.9f, 0.0001f) ||
        basic_output_ui_handle(&ui, BASIC_OUTPUT_UI_START) != 0 ||
        basic_output_ui_handle(&ui, BASIC_OUTPUT_UI_INCREMENT) == 0 ||
        basic_output_ui_handle(&ui, BASIC_OUTPUT_UI_RESET) != 0 ||
        ui.running != 0) {
        return 4;
    }

    puts("BASIC34_OUTPUT_SELF_TEST_PASSED");
    return 0;
}
