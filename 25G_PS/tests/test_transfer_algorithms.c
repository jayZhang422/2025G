#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "open_loop_output_planner.h"
#include "dac_vpp_calibration.h"
#include "coherent_transfer_measurement.h"
#include "rlc_filter_classifier.h"
#include "fundamental_frequency_estimator.h"
#include "response_waveform_generator.h"
#include "transfer_function_model.h"

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

int main(void)
{
    const transfer_function_model_coeffs_t lowpass = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
    const float pi = 3.14159265358979323846f;
    transfer_function_response_t model_response;
    dac_vpp_calibration_curve_t curve = {{{1000U, 1.0f}, {2000U, 2.0f}}, 2U};
    uint16_t code;
    float vpp;
    open_loop_output_request_t request;
    open_loop_output_plan_t plan;
    float input[4096];
    float output[4096];
    const float fs = 10000.0f;
    const float frequency = 1000.0f;
    const float phase_offset = pi / 4.0f;
    coherent_transfer_response_t measured;
    float lowpass_magnitude[5] = {1.0f, 0.8f, 0.4f, 0.2f, 0.1f};
    float bandpass_magnitude[5] = {0.1f, 0.2f, 1.0f, 0.2f, 0.1f};
    float frequencies[5] = {100.0f, 200.0f, 300.0f, 400.0f, 500.0f};
    rlc_filter_class_t filter_class;
    fundamental_frequency_search_config_t estimator_config;
    fundamental_frequency_estimate_t estimate;
    response_waveform_bin_t response_bin;
    uint16_t waveform_table[64];
    size_t n;

    if (transfer_function_model_eval(&lowpass, 1.0f / (2.0f * pi), &model_response) != 0 ||
        !close_enough(model_response.magnitude, 0.70710678f, 0.001f) ||
        !close_enough(model_response.phase_rad, -pi / 4.0f, 0.001f)) {
        return 1;
    }
    if (dac_vpp_calibration_curve_validate(&curve) != 0 ||
        dac_vpp_calibration_vpp_from_code(&curve, 1500U, &vpp) != 0 ||
        !close_enough(vpp, 1.5f, 0.001f) ||
        dac_vpp_calibration_code_for_vpp(&curve, 1.5f, &code) != 0 || code != 1500U) {
        return 2;
    }
    {
        const transfer_function_model_coeffs_t plan_model =
            {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
        const dac_vpp_calibration_curve_t plan_curve =
            {{{1000U, 1.0f}, {4000U, 4.0f}}, 2U};
        request.frequency_hz = 1.0f / (2.0f * pi);
        request.target_output_vpp = 2.0f;
        if (open_loop_output_plan(&plan_model, &plan_curve, &request,
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
    if (coherent_transfer_measure(input, output, 4096U, fs, frequency,
                                   &measured) != 0 ||
        !close_enough(measured.magnitude, 0.5f, 0.002f) ||
        !close_enough(measured.phase_rad, -phase_offset, 0.002f)) {
        return 3;
    }
    if (rlc_filter_classify(frequencies, lowpass_magnitude, 5U,
                                   &filter_class) != 0 ||
        filter_class != RLC_FILTER_CLASS_LOW_PASS) {
        return 4;
    }
    if (rlc_filter_classify(frequencies, bandpass_magnitude, 5U,
                                   &filter_class) != 0 ||
        filter_class != RLC_FILTER_CLASS_BAND_PASS) {
        return 5;
    }
    for (n = 0U; n < 4096U; ++n) {
        const float phase = 2.0f * pi * 1234.0f * (float)n / fs;
        input[n] = cosf(phase);
    }
    estimator_config.min_frequency_hz = 1000.0f;
    estimator_config.max_frequency_hz = 1500.0f;
    estimator_config.step_hz = 1.0f;
    if (fundamental_frequency_estimate(input, 4096U, fs, &estimator_config,
                                     &estimate) != 0 ||
        !close_enough(estimate.frequency_hz, 1234.0f, 1.0f) ||
        estimate.magnitude < 0.8f) {
        return 7;
    }
    response_bin.harmonic = 1U;
    response_bin.real = 0.5f;
    response_bin.imag = 0.0f;
    if (response_waveform_generate_table(input, 4096U, fs, 1234.0f,
                                       &response_bin, 1U, waveform_table,
                                       64U) != 0 ||
        waveform_table[0] < 12000U || waveform_table[0] > 12500U ||
        waveform_table[16] < 8000U || waveform_table[16] > 8400U) {
        return 8;
    }
    puts("TRANSFER_ALGORITHM_SELF_TEST_PASSED");
    return 0;
}