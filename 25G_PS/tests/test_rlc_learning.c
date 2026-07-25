#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "rlc_learning.h"

static int close_enough(float actual, float expected, float tolerance)
{
    return fabsf(actual - expected) <= tolerance;
}

int main(void)
{
    const float pi = 3.14159265358979323846f;
    const float fs = 10000.0f;
    const float frequencies[5] = {100.0f, 200.0f, 300.0f, 400.0f, 500.0f};
    const float gains[5] = {1.0f, 0.8f, 0.4f, 0.2f, 0.1f};
    float input_frames[5U * 4096U];
    float output_frames[5U * 4096U];
    coherent_transfer_response_t responses[5];
    rlc_learning_result_t result;
    size_t point;
    size_t sample;

    for (point = 0U; point < 5U; ++point) {
        for (sample = 0U; sample < 4096U; ++sample) {
            const float phase = 2.0f * pi * frequencies[point] *
                                (float)sample / fs;
            input_frames[point * 4096U + sample] = cosf(phase);
            output_frames[point * 4096U + sample] =
                gains[point] * cosf(phase);
        }
    }

    if (rlc_learning_measure_scan(input_frames, output_frames, 4096U,
                                  4096U, fs, frequencies, 5U,
                                  responses) != 0 ||
        rlc_learning_summarize(frequencies, responses, 5U, &result) != 0 ||
        result.filter_class != RLC_FILTER_CLASS_LOW_PASS ||
        !close_enough(result.peak_frequency_hz, 100.0f, 0.001f) ||
        !close_enough(result.peak_magnitude, 1.0f, 0.002f) ||
        result.point_count != 5U) {
        return EXIT_FAILURE;
    }

    puts("RLC_LEARNING_SELF_TEST_PASSED");
    return EXIT_SUCCESS;
}