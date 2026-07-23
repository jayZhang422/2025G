#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "basic_service.h"
#include "calibration.h"
#include "coherent_measure.h"
#include "filter_classifier.h"
#include "known_model.h"

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

int main(void)
{
    const known_model_coeffs_t lowpass = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
    const float pi = 3.14159265358979323846f;
    known_model_response_t model_response;
    calibration_curve_t curve = {{{1000U, 1.0f}, {2000U, 2.0f}}, 2U};
    uint16_t code;
    float vpp;
    basic_output_request_t request;
    basic_output_plan_t plan;
    float input[4096];
    float output[4096];
    const float fs = 10000.0f;
    const float frequency = 1000.0f;
    const float phase_offset = pi / 4.0f;
    coherent_response_t measured;
    float lowpass_magnitude[5] = {1.0f, 0.8f, 0.4f, 0.2f, 0.1f};
    float bandpass_magnitude[5] = {0.1f, 0.2f, 1.0f, 0.2f, 0.1f};
    float frequencies[5] = {100.0f, 200.0f, 300.0f, 400.0f, 500.0f};
    filter_class_t filter_class;
    size_t n;

    if (known_model_eval(&lowpass, 1.0f / (2.0f * pi), &model_response) != 0 ||
        !close_enough(model_response.magnitude, 0.70710678f, 0.001f) ||
        !close_enough(model_response.phase_rad, -pi / 4.0f, 0.001f)) {
        return 1;
    }
    if (calibration_curve_validate(&curve) != 0 ||
        calibration_vpp_from_code(&curve, 1500U, &vpp) != 0 ||
        !close_enough(vpp, 1.5f, 0.001f) ||
        calibration_code_for_vpp(&curve, 1.5f, &code) != 0 || code != 1500U) {
        return 2;
    }
    {
        const known_model_coeffs_t plan_model =
            {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
        const calibration_curve_t plan_curve =
            {{{1000U, 1.0f}, {4000U, 4.0f}}, 2U};
        request.frequency_hz = 1.0f / (2.0f * pi);
        request.target_output_vpp = 2.0f;
        if (basic_service_plan_output(&plan_model, &plan_curve, &request,
                                      &plan) != 0 ||
            !close_enough(plan.model_magnitude, 0.70710678f, 0.001f) ||
            !close_enough(plan.required_input_vpp, 2.8284271f, 0.002f) ||
            plan.amplitude_code != 2828U) {
            return 6;
        }
    }
    for (n = 0U; n < 4096U; ++n) {
        const float phase = 2.0f * pi * frequency * (float)n / fs;
        input[n] = cosf(phase);
        output[n] = 0.5f * cosf(phase - phase_offset);
    }
    if (coherent_measure_transfer(input, output, 4096U, fs, frequency,
                                   &measured) != 0 ||
        !close_enough(measured.magnitude, 0.5f, 0.002f) ||
        !close_enough(measured.phase_rad, -phase_offset, 0.002f)) {
        return 3;
    }
    if (filter_classifier_classify(frequencies, lowpass_magnitude, 5U,
                                   &filter_class) != 0 ||
        filter_class != FILTER_CLASS_LOW_PASS) {
        return 4;
    }
    if (filter_classifier_classify(frequencies, bandpass_magnitude, 5U,
                                   &filter_class) != 0 ||
        filter_class != FILTER_CLASS_BAND_PASS) {
        return 5;
    }
    puts("TRANSFER_ALGORITHM_SELF_TEST_PASSED");
    return 0;
}