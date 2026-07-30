/******************************************************************************
 * g26_signal_analysis.c
 ******************************************************************************/

#include "../include/g26_signal_analysis.h"

#include "../include/app_buffers.h"

#include <math.h>
#include <string.h>

#if APP_FFT_LEN != G26_SIGNAL_SAMPLE_COUNT
#error "The G26 analyzer requires the existing 4096-point app workspaces"
#endif

#define G26_PI                         3.14159265358979323846f
#define G26_TWO_PI                     (2.0f * G26_PI)
#define G26_MIN_FREQUENCY_HZ           10000.0f
#define G26_MAX_FREQUENCY_HZ           500000.0f
#define G26_PEAK_CAPACITY              16
#define G26_BASE_ANCHOR_CAPACITY       6
#define G26_BASE_HYPOTHESIS_CAPACITY   320
#define G26_HARMONIC_ORDER_CAPACITY    16
#define G26_MAX_LS_COLUMNS             7
#define G26_MIN_CANDIDATE_MV           0.5f
#define G26_HARMONIC_MATCH_BINS        1.0f
#define G26_BASE_MERGE_BINS            0.1f
#define G26_EDGE_TOLERANCE_BINS         0.5f
#define G26_FREQUENCY_REFINEMENT_STEPS 5
#define G26_FREQUENCY_REFINEMENT_ROUNDS 4
#define G26_UPP_POINTS                 4096U
#define G26_EPSILON                    1.0e-12f
#define G26_GAIN_CORRECTION_MIN        0.95f
#define G26_GAIN_CORRECTION_MAX        1.10f

typedef struct {
    float32_t frequency_hz;
    float32_t spectrum_amplitude_mv;
} g26_candidate_t;

typedef struct {
    float32_t sine;
    float32_t cosine;
    float32_t sine_step;
    float32_t cosine_step;
} g26_oscillator_t;

typedef struct {
    u16 harmonic_order;
    float32_t amplitude_mv;
    float32_t phase_rad;
} g26_test_component_t;

typedef struct {
    float32_t sample_rate_hz;
    float32_t mv_per_code;
    float32_t fundamental_hz;
    float32_t dc_mv;
    u32 input_component_count;
    u32 expected_component_count;
    g26_test_component_t components[G26_SIGNAL_MAX_COMPONENTS];
} g26_test_case_t;

typedef struct {
    float32_t frequency_hz;
    float32_t amplitude_scale;
} g26_gain_calibration_t;

/* Unity baseline for the replacement analog filter; update after its sweep. */
static const g26_gain_calibration_t g26_gain_calibration[] = {
    { 10000.0f, 1.049817f},
    {200000.0f, 1.019856f},
    {250000.0f, 1.013588f},
    {300000.0f, 0.989776f},
    {400000.0f, 0.952547f},
    {500000.0f, 0.962268f}
};

static arm_rfft_fast_instance_f32 g26_fft;
static int g26_fft_initialized;

static void g26_oscillator_init(g26_oscillator_t *oscillator,
                                float32_t frequency_hz,
                                float32_t sample_rate_hz)
{
    float32_t step = G26_TWO_PI * frequency_hz / sample_rate_hz;

    oscillator->sine = 0.0f;
    oscillator->cosine = 1.0f;
    oscillator->sine_step = sinf(step);
    oscillator->cosine_step = cosf(step);
}

static void g26_oscillator_advance(g26_oscillator_t *oscillator,
                                   u32 sample_index)
{
    float32_t sine = oscillator->sine * oscillator->cosine_step +
                     oscillator->cosine * oscillator->sine_step;
    float32_t cosine = oscillator->cosine * oscillator->cosine_step -
                       oscillator->sine * oscillator->sine_step;

    oscillator->sine = sine;
    oscillator->cosine = cosine;
    if ((sample_index & 127U) == 127U) {
        float32_t norm = sqrtf(sine * sine + cosine * cosine);

        if (norm > G26_EPSILON) {
            oscillator->sine /= norm;
            oscillator->cosine /= norm;
        }
    }
}

static float32_t g26_wrap_phase(float32_t phase_rad)
{
    while (phase_rad > G26_PI) {
        phase_rad -= G26_TWO_PI;
    }
    while (phase_rad <= -G26_PI) {
        phase_rad += G26_TWO_PI;
    }
    return phase_rad;
}

static void g26_prepare_fft(const s16 *samples, float32_t mv_per_code,
                            float32_t *window_sum)
{
    float32_t sum = 0.0f;
    float32_t mean;
    u32 index;

    for (index = 0U; index < G26_SIGNAL_SAMPLE_COUNT; index++) {
        g_time_domain_buffer[index] = (float32_t)samples[index] * mv_per_code;
        sum += g_time_domain_buffer[index];
    }
    mean = sum / (float32_t)G26_SIGNAL_SAMPLE_COUNT;
    *window_sum = 0.0f;
    for (index = 0U; index < G26_SIGNAL_SAMPLE_COUNT; index++) {
        float32_t window = 0.5f - 0.5f * cosf(
            G26_TWO_PI * (float32_t)index /
            (float32_t)(G26_SIGNAL_SAMPLE_COUNT - 1U));

        *window_sum += window;
        g_fft_input_buffer[index] =
            (g_time_domain_buffer[index] - mean) * window;
    }
}

static void g26_compute_magnitudes(float32_t window_sum)
{
    u32 bin;

    g_fft_magnitude_buffer[0] = fabsf(g_fft_spectrum_buffer[0]) / window_sum;
    for (bin = 1U; bin < APP_SPEC_LEN; bin++) {
        float32_t real = g_fft_spectrum_buffer[2U * bin];
        float32_t imaginary = g_fft_spectrum_buffer[2U * bin + 1U];

        g_fft_magnitude_buffer[bin] =
            2.0f * sqrtf(real * real + imaginary * imaginary) / window_sum;
    }
}

static float32_t g26_refined_frequency(u32 bin, float32_t bin_width_hz)
{
    float32_t left = logf(fmaxf(g_fft_magnitude_buffer[bin - 1U],
                                G26_EPSILON));
    float32_t center = logf(fmaxf(g_fft_magnitude_buffer[bin],
                                  G26_EPSILON));
    float32_t right = logf(fmaxf(g_fft_magnitude_buffer[bin + 1U],
                                 G26_EPSILON));
    float32_t denominator = left - 2.0f * center + right;
    float32_t offset = 0.0f;

    if (fabsf(denominator) > G26_EPSILON) {
        offset = 0.5f * (left - right) / denominator;
        if (offset > 0.5f) {
            offset = 0.5f;
        } else if (offset < -0.5f) {
            offset = -0.5f;
        }
    }
    return ((float32_t)bin + offset) * bin_width_hz;
}

static void g26_insert_candidate(g26_candidate_t *candidates,
                                 int *candidate_count,
                                 float32_t frequency_hz,
                                 float32_t amplitude_mv)
{
    int index;
    int position;

    for (index = 0; index < *candidate_count; index++) {
        if (candidates[index].frequency_hz == frequency_hz) {
            if (amplitude_mv <= candidates[index].spectrum_amplitude_mv) {
                return;
            }
            candidates[index].spectrum_amplitude_mv = amplitude_mv;
            position = index;
            while (position > 0 &&
                   candidates[position].spectrum_amplitude_mv >
                   candidates[position - 1].spectrum_amplitude_mv) {
                g26_candidate_t temporary = candidates[position];
                candidates[position] = candidates[position - 1];
                candidates[position - 1] = temporary;
                position--;
            }
            return;
        }
    }

    if (*candidate_count < G26_PEAK_CAPACITY) {
        position = (*candidate_count)++;
    } else {
        if (amplitude_mv <=
            candidates[G26_PEAK_CAPACITY - 1].spectrum_amplitude_mv) {
            return;
        }
        position = G26_PEAK_CAPACITY - 1;
    }
    candidates[position].frequency_hz = frequency_hz;
    candidates[position].spectrum_amplitude_mv = amplitude_mv;
    while (position > 0 &&
           candidates[position].spectrum_amplitude_mv >
           candidates[position - 1].spectrum_amplitude_mv) {
        g26_candidate_t temporary = candidates[position];
        candidates[position] = candidates[position - 1];
        candidates[position - 1] = temporary;
        position--;
    }
}

static int g26_find_candidates(float32_t sample_rate_hz,
                               g26_candidate_t *candidates)
{
    float32_t bin_width_hz = sample_rate_hz /
                             (float32_t)G26_SIGNAL_SAMPLE_COUNT;
    float32_t max_frequency_hz = fminf(
        G26_MAX_FREQUENCY_HZ, 0.5f * sample_rate_hz - bin_width_hz);
    float32_t edge_tolerance_hz = G26_EDGE_TOLERANCE_BINS * bin_width_hz;
    int first_bin = (int)floorf(G26_MIN_FREQUENCY_HZ / bin_width_hz);
    int last_bin = (int)ceilf(max_frequency_hz / bin_width_hz);
    int candidate_count = 0;
    int bin;

    if (first_bin < 1) {
        first_bin = 1;
    }
    if (last_bin > (int)APP_SPEC_LEN - 2) {
        last_bin = (int)APP_SPEC_LEN - 2;
    }
    for (bin = first_bin; bin <= last_bin; bin++) {
        float32_t amplitude_mv = g_fft_magnitude_buffer[bin];

        if (amplitude_mv < G26_MIN_CANDIDATE_MV ||
            amplitude_mv <= g_fft_magnitude_buffer[bin - 1] ||
            amplitude_mv < g_fft_magnitude_buffer[bin + 1]) {
            continue;
        }
        {
            float32_t refined_hz = g26_refined_frequency(
                (u32)bin, bin_width_hz);

            if (refined_hz >= G26_MIN_FREQUENCY_HZ - edge_tolerance_hz &&
                refined_hz <= max_frequency_hz + edge_tolerance_hz) {
                refined_hz = fminf(fmaxf(refined_hz,
                                         G26_MIN_FREQUENCY_HZ),
                                    max_frequency_hz);
                g26_insert_candidate(candidates, &candidate_count,
                                     refined_hz, amplitude_mv);
            }
        }
    }
    return candidate_count;
}

static int g26_solve_linear(float32_t matrix[G26_MAX_LS_COLUMNS]
                                            [G26_MAX_LS_COLUMNS + 1],
                            int column_count,
                            float32_t *solution)
{
    int pivot_column;

    for (pivot_column = 0; pivot_column < column_count; pivot_column++) {
        int pivot_row = pivot_column;
        int row;
        int column;
        float32_t pivot_magnitude = fabsf(matrix[pivot_row][pivot_column]);

        for (row = pivot_column + 1; row < column_count; row++) {
            float32_t magnitude = fabsf(matrix[row][pivot_column]);

            if (magnitude > pivot_magnitude) {
                pivot_magnitude = magnitude;
                pivot_row = row;
            }
        }
        if (pivot_magnitude < 1.0e-6f) {
            return G26_SIGNAL_ERROR;
        }
        if (pivot_row != pivot_column) {
            for (column = pivot_column; column <= column_count; column++) {
                float32_t temporary = matrix[pivot_column][column];
                matrix[pivot_column][column] = matrix[pivot_row][column];
                matrix[pivot_row][column] = temporary;
            }
        }
        {
            float32_t inverse = 1.0f /
                                matrix[pivot_column][pivot_column];

            for (column = pivot_column; column <= column_count; column++) {
                matrix[pivot_column][column] *= inverse;
            }
        }
        for (row = 0; row < column_count; row++) {
            float32_t factor;

            if (row == pivot_column) {
                continue;
            }
            factor = matrix[row][pivot_column];
            for (column = pivot_column; column <= column_count; column++) {
                matrix[row][column] -= factor *
                                       matrix[pivot_column][column];
            }
        }
    }
    for (pivot_column = 0; pivot_column < column_count; pivot_column++) {
        solution[pivot_column] = matrix[pivot_column][column_count];
    }
    return G26_SIGNAL_OK;
}

static int g26_fit_model(const float32_t *frequencies_hz,
                         u32 component_count,
                         float32_t sample_rate_hz,
                         g26_signal_result_t *result)
{
    float32_t normal[G26_MAX_LS_COLUMNS][G26_MAX_LS_COLUMNS + 1] = {{0.0f}};
    float32_t solution[G26_MAX_LS_COLUMNS];
    g26_oscillator_t oscillators[G26_SIGNAL_MAX_COMPONENTS];
    int column_count = 1 + 2 * (int)component_count;
    u32 component;
    u32 index;
    int row_index;
    int column_index;

    for (component = 0U; component < component_count; component++) {
        g26_oscillator_init(&oscillators[component],
                            frequencies_hz[component], sample_rate_hz);
    }
    for (index = 0U; index < G26_SIGNAL_SAMPLE_COUNT; index++) {
        float32_t row[G26_MAX_LS_COLUMNS];

        row[0] = 1.0f;
        for (component = 0U; component < component_count; component++) {
            row[1 + 2 * component] = oscillators[component].sine;
            row[2 + 2 * component] = oscillators[component].cosine;
        }
        for (row_index = 0; row_index < column_count; row_index++) {
            normal[row_index][column_count] +=
                row[row_index] * g_time_domain_buffer[index];
            for (column_index = 0; column_index <= row_index;
                 column_index++) {
                normal[row_index][column_index] +=
                    row[row_index] * row[column_index];
            }
        }
        for (component = 0U; component < component_count; component++) {
            g26_oscillator_advance(&oscillators[component], index);
        }
    }
    for (row_index = 0; row_index < column_count; row_index++) {
        for (column_index = row_index + 1; column_index < column_count;
             column_index++) {
            normal[row_index][column_index] =
                normal[column_index][row_index];
        }
    }
    if (g26_solve_linear(normal, column_count, solution) != G26_SIGNAL_OK) {
        return G26_SIGNAL_ERROR;
    }

    memset(result, 0, sizeof(*result));
    result->component_count = component_count;
    result->fundamental_frequency_hz = frequencies_hz[0];
    result->dc_mv = solution[0];
    for (component = 0U; component < component_count; component++) {
        float32_t sine_coefficient = solution[1 + 2 * component];
        float32_t cosine_coefficient = solution[2 + 2 * component];

        result->components[component].frequency_hz = frequencies_hz[component];
        result->components[component].amplitude_mv = sqrtf(
            sine_coefficient * sine_coefficient +
            cosine_coefficient * cosine_coefficient);
        result->components[component].phase_rad = g26_wrap_phase(
            atan2f(cosine_coefficient, sine_coefficient));
        result->components[component].harmonic_order = (u16)floorf(
            frequencies_hz[component] / frequencies_hz[0] + 0.5f);
    }

    {
        float32_t signal_energy = 0.0f;
        float32_t error_energy = 0.0f;

        for (component = 0U; component < component_count; component++) {
            g26_oscillator_init(&oscillators[component],
                                frequencies_hz[component], sample_rate_hz);
        }
        for (index = 0U; index < G26_SIGNAL_SAMPLE_COUNT; index++) {
            float32_t model = 0.0f;
            float32_t centered =
                g_time_domain_buffer[index] - result->dc_mv;
            float32_t error;

            for (component = 0U; component < component_count; component++) {
                model += solution[1 + 2 * component] *
                         oscillators[component].sine +
                         solution[2 + 2 * component] *
                         oscillators[component].cosine;
                g26_oscillator_advance(&oscillators[component], index);
            }
            g_model_buffer[index] = model;
            error = centered - model;
            signal_energy += centered * centered;
            error_energy += error * error;
        }
        if (signal_energy <= G26_EPSILON) {
            return G26_SIGNAL_ERROR;
        }
        result->residual_sse = error_energy;
        result->normalized_residual = sqrtf(error_energy / signal_energy);
    }
    return G26_SIGNAL_OK;
}

static int g26_harmonic_order(float32_t base_hz, float32_t frequency_hz,
                              float32_t tolerance_hz, u16 *order_out)
{
    float32_t order;

    if (base_hz < G26_MIN_FREQUENCY_HZ || frequency_hz < base_hz) {
        return 0;
    }
    order = floorf(frequency_hz / base_hz + 0.5f);
    if (order < 1.0f || order >
        G26_MAX_FREQUENCY_HZ / G26_MIN_FREQUENCY_HZ ||
        fabsf(frequency_hz - order * base_hz) > tolerance_hz) {
        return 0;
    }
    if (order_out != NULL) {
        *order_out = (u16)order;
    }
    return 1;
}

static int g26_model_is_usable(const g26_signal_result_t *result)
{
    u32 component;

    if (!isfinite(result->residual_sse) ||
        !isfinite(result->normalized_residual) ||
        result->residual_sse < 0.0f) {
        return 0;
    }
    for (component = 0U; component < result->component_count; component++) {
        if (!isfinite(result->components[component].amplitude_mv) ||
            !isfinite(result->components[component].phase_rad)) {
            return 0;
        }
    }
    return 1;
}

static float32_t g26_model_bic(const g26_signal_result_t *result)
{
    float32_t sample_count = (float32_t)G26_SIGNAL_SAMPLE_COUNT;
    float32_t parameter_count = 2.0f +
        2.0f * (float32_t)result->component_count;
    float32_t mean_sse = fmaxf(result->residual_sse / sample_count,
                               G26_EPSILON);

    return sample_count * logf(mean_sse) +
           parameter_count * logf(sample_count);
}

static int g26_fundamental_is_significant(
    const g26_signal_result_t *result)
{
    float32_t sample_count = (float32_t)G26_SIGNAL_SAMPLE_COUNT;
    float32_t degrees_of_freedom = sample_count -
        (1.0f + 2.0f * (float32_t)result->component_count);
    float32_t amplitude_sigma = sqrtf(
        2.0f * fmaxf(result->residual_sse, G26_EPSILON) /
        (sample_count * degrees_of_freedom));

    return result->components[0].amplitude_mv >=
           fmaxf(3.0f * amplitude_sigma, G26_MIN_CANDIDATE_MV);
}

static void g26_consider_model(const float32_t *frequencies_hz,
                               u32 component_count,
                               float32_t sample_rate_hz,
                               g26_signal_result_t *best,
                               float32_t *best_bic,
                               int *found)
{
    g26_signal_result_t candidate;

    if (g26_fit_model(frequencies_hz, component_count, sample_rate_hz,
                      &candidate) != G26_SIGNAL_OK ||
        !g26_model_is_usable(&candidate)) {
        return;
    }
    candidate.model_bic = g26_model_bic(&candidate);
    if (!*found || candidate.model_bic < *best_bic) {
        *best = candidate;
        *best_bic = candidate.model_bic;
        *found = 1;
    }
}

static void g26_add_base_hypothesis(float32_t *hypotheses,
                                    int *hypothesis_count,
                                    float32_t base_hz,
                                    float32_t merge_hz)
{
    int index;

    if (base_hz < G26_MIN_FREQUENCY_HZ) {
        if (G26_MIN_FREQUENCY_HZ - base_hz > merge_hz) {
            return;
        }
        base_hz = G26_MIN_FREQUENCY_HZ;
    }
    if (base_hz > G26_MAX_FREQUENCY_HZ) {
        return;
    }
    for (index = 0; index < *hypothesis_count; index++) {
        if (fabsf(hypotheses[index] - base_hz) <= merge_hz) {
            return;
        }
    }
    if (*hypothesis_count < G26_BASE_HYPOTHESIS_CAPACITY) {
        hypotheses[(*hypothesis_count)++] = base_hz;
    }
}

static int g26_build_base_hypotheses(const g26_candidate_t *candidates,
                                     int candidate_count,
                                     float32_t bin_width_hz,
                                     float32_t *hypotheses)
{
    int anchor_count = candidate_count;
    int hypothesis_count = 0;
    int anchor;

    if (anchor_count > G26_BASE_ANCHOR_CAPACITY) {
        anchor_count = G26_BASE_ANCHOR_CAPACITY;
    }
    for (anchor = 0; anchor < anchor_count; anchor++) {
        int max_order = (int)ceilf(candidates[anchor].frequency_hz /
                                   G26_MIN_FREQUENCY_HZ);
        int order;

        for (order = 1; order <= max_order; order++) {
            g26_add_base_hypothesis(
                hypotheses, &hypothesis_count,
                candidates[anchor].frequency_hz / (float32_t)order,
                G26_BASE_MERGE_BINS * bin_width_hz);
        }
    }
    return hypothesis_count;
}

static int g26_collect_harmonic_orders(float32_t base_hz,
                                       const g26_candidate_t *candidates,
                                       int candidate_count,
                                       float32_t tolerance_hz,
                                       u16 *orders)
{
    int order_count = 0;
    int candidate;

    for (candidate = 0; candidate < candidate_count; candidate++) {
        u16 order;
        int existing;

        if (!g26_harmonic_order(base_hz,
                                candidates[candidate].frequency_hz,
                                tolerance_hz, &order) || order < 2U) {
            continue;
        }
        for (existing = 0; existing < order_count; existing++) {
            if (orders[existing] == order) {
                break;
            }
        }
        if (existing == order_count &&
            order_count < G26_HARMONIC_ORDER_CAPACITY) {
            orders[order_count++] = order;
        }
    }
    return order_count;
}

static void g26_refine_model_frequency(float32_t sample_rate_hz,
                                       g26_signal_result_t *result)
{
    g26_signal_result_t best = *result;
    u16 orders[G26_SIGNAL_MAX_COMPONENTS];
    float32_t center_hz = result->fundamental_frequency_hz;
    float32_t bin_width_hz = sample_rate_hz /
                             (float32_t)G26_SIGNAL_SAMPLE_COUNT;
    float32_t step_hz;
    u16 maximum_order = 1U;
    u32 component;
    int round;

    for (component = 0U; component < result->component_count; component++) {
        orders[component] = result->components[component].harmonic_order;
        if (orders[component] > maximum_order) {
            maximum_order = orders[component];
        }
    }
    step_hz = 0.5f * bin_width_hz / (float32_t)maximum_order;

    for (round = 0; round < G26_FREQUENCY_REFINEMENT_ROUNDS; round++) {
        g26_signal_result_t round_best = best;
        float32_t round_center = center_hz;
        int point;

        for (point = 0; point < G26_FREQUENCY_REFINEMENT_STEPS; point++) {
            float32_t frequencies_hz[G26_SIGNAL_MAX_COMPONENTS];
            float32_t base_hz = round_center +
                (float32_t)(point - G26_FREQUENCY_REFINEMENT_STEPS / 2) *
                step_hz;
            g26_signal_result_t candidate;

            if (base_hz < G26_MIN_FREQUENCY_HZ ||
                base_hz * (float32_t)maximum_order >
                G26_MAX_FREQUENCY_HZ) {
                continue;
            }
            for (component = 0U; component < result->component_count;
                 component++) {
                frequencies_hz[component] = base_hz *
                                             (float32_t)orders[component];
            }
            if (g26_fit_model(frequencies_hz, result->component_count,
                              sample_rate_hz, &candidate) == G26_SIGNAL_OK &&
                g26_model_is_usable(&candidate) &&
                candidate.residual_sse < round_best.residual_sse) {
                round_best = candidate;
                center_hz = base_hz;
            }
        }
        best = round_best;
        step_hz *= 0.25f;
    }

    best.model_bic = g26_model_bic(&best);
    *result = best;
}

static void g26_compute_model_metrics(g26_signal_result_t *result)
{
    float32_t minimum = 1.0e30f;
    float32_t maximum = -1.0e30f;
    float32_t rms_square = 0.0f;
    float32_t reference_phase = result->components[0].phase_rad;
    u32 component;
    u32 point;

    for (component = 0U; component < result->component_count; component++) {
        float32_t amplitude = result->components[component].amplitude_mv;

        rms_square += 0.5f * amplitude * amplitude;
    }
    result->rms_mv = sqrtf(rms_square);

    for (point = 0U; point < G26_UPP_POINTS; point++) {
        float32_t cycle = (float32_t)point / (float32_t)G26_UPP_POINTS;
        float32_t value = 0.0f;

        for (component = 0U; component < result->component_count; component++) {
            const g26_signal_component_t *line = &result->components[component];
            float32_t relative_phase = line->phase_rad -
                (float32_t)line->harmonic_order * reference_phase;

            value += line->amplitude_mv * sinf(
                G26_TWO_PI * (float32_t)line->harmonic_order * cycle +
                relative_phase);
        }
        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
    }
    result->upp_mv = maximum - minimum;
}

static float32_t g26_gain_correction(float32_t frequency_hz)
{
    u32 point;
    float32_t correction;

    if (frequency_hz <= g26_gain_calibration[0].frequency_hz) {
        return g26_gain_calibration[0].amplitude_scale;
    }
    for (point = 1U;
         point < sizeof(g26_gain_calibration) /
                     sizeof(g26_gain_calibration[0]);
         point++) {
        const g26_gain_calibration_t *lower =
            &g26_gain_calibration[point - 1U];
        const g26_gain_calibration_t *upper =
            &g26_gain_calibration[point];

        if (frequency_hz <= upper->frequency_hz) {
            float32_t fraction = (frequency_hz - lower->frequency_hz) /
                (upper->frequency_hz - lower->frequency_hz);

            correction = lower->amplitude_scale + fraction *
                (upper->amplitude_scale - lower->amplitude_scale);
            return fminf(fmaxf(correction, G26_GAIN_CORRECTION_MIN),
                         G26_GAIN_CORRECTION_MAX);
        }
    }
    correction = g26_gain_calibration[
        sizeof(g26_gain_calibration) /
        sizeof(g26_gain_calibration[0]) - 1U].amplitude_scale;
    return fminf(fmaxf(correction, G26_GAIN_CORRECTION_MIN),
                 G26_GAIN_CORRECTION_MAX);
}

int g26_signal_apply_amplitude_calibration(g26_signal_result_t *result)
{
    u32 component;

    if (result == NULL || result->component_count < 2U ||
        result->component_count > G26_SIGNAL_MAX_COMPONENTS) {
        return G26_SIGNAL_ERROR_INPUT;
    }
    for (component = 0U; component < result->component_count; component++) {
        g26_signal_component_t *line = &result->components[component];

        if (!isfinite(line->frequency_hz) ||
            !isfinite(line->amplitude_mv) || line->amplitude_mv < 0.0f) {
            return G26_SIGNAL_ERROR_INPUT;
        }
        line->amplitude_mv *= g26_gain_correction(line->frequency_hz);
    }
    g26_compute_model_metrics(result);
    return G26_SIGNAL_OK;
}

int g26_signal_analysis_init(void)
{
    if (arm_rfft_fast_init_f32(&g26_fft, G26_SIGNAL_SAMPLE_COUNT) !=
        ARM_MATH_SUCCESS) {
        g26_fft_initialized = 0;
        return G26_SIGNAL_ERROR;
    }
    g26_fft_initialized = 1;
    return G26_SIGNAL_OK;
}

int g26_signal_analyze(const s16 samples[G26_SIGNAL_SAMPLE_COUNT],
                       float32_t sample_rate_hz,
                       float32_t mv_per_code,
                       g26_signal_result_t *result)
{
    g26_candidate_t candidates[G26_PEAK_CAPACITY];
    float32_t base_hypotheses[G26_BASE_HYPOTHESIS_CAPACITY];
    g26_signal_result_t best_pair;
    g26_signal_result_t best_triple;
    g26_signal_result_t best;
    float32_t pair_bic = 1.0e30f;
    float32_t triple_bic = 1.0e30f;
    float32_t window_sum;
    float32_t bin_width_hz;
    int candidate_count;
    int hypothesis_count;
    int hypothesis;
    int pair_found = 0;
    int triple_found = 0;

    if (samples == 0 || result == 0 || !isfinite(sample_rate_hz) ||
        !isfinite(mv_per_code) || sample_rate_hz <= 2.0f *
        G26_MIN_FREQUENCY_HZ || fabsf(mv_per_code) <= G26_EPSILON) {
        return G26_SIGNAL_ERROR_INPUT;
    }
    if (!g26_fft_initialized &&
        g26_signal_analysis_init() != G26_SIGNAL_OK) {
        return G26_SIGNAL_ERROR_FFT;
    }

    g26_prepare_fft(samples, mv_per_code, &window_sum);
    arm_rfft_fast_f32(&g26_fft, g_fft_input_buffer,
                      g_fft_spectrum_buffer, 0);
    g26_compute_magnitudes(window_sum);
    candidate_count = g26_find_candidates(sample_rate_hz, candidates);
    if (candidate_count < 1) {
        return G26_SIGNAL_ERROR_NO_CANDIDATE;
    }

    bin_width_hz = sample_rate_hz / (float32_t)G26_SIGNAL_SAMPLE_COUNT;
    hypothesis_count = g26_build_base_hypotheses(
        candidates, candidate_count, bin_width_hz, base_hypotheses);
    for (hypothesis = 0; hypothesis < hypothesis_count; hypothesis++) {
        u16 orders[G26_HARMONIC_ORDER_CAPACITY];
        float32_t base_hz = base_hypotheses[hypothesis];
        int order_count = g26_collect_harmonic_orders(
            base_hz, candidates, candidate_count,
            G26_HARMONIC_MATCH_BINS * bin_width_hz, orders);
        int second;

        for (second = 0; second < order_count; second++) {
            float32_t pair[2];
            int third;

            pair[0] = base_hz;
            pair[1] = base_hz * (float32_t)orders[second];
            g26_consider_model(pair, 2U, sample_rate_hz, &best_pair,
                               &pair_bic, &pair_found);

            for (third = second + 1; third < order_count; third++) {
                float32_t triple[3];
                u16 lower_order = orders[second];
                u16 upper_order = orders[third];

                if (lower_order > upper_order) {
                    u16 temporary = lower_order;
                    lower_order = upper_order;
                    upper_order = temporary;
                }
                triple[0] = base_hz;
                triple[1] = base_hz * (float32_t)lower_order;
                triple[2] = base_hz * (float32_t)upper_order;
                g26_consider_model(triple, 3U, sample_rate_hz, &best_triple,
                                   &triple_bic, &triple_found);
            }
        }
    }
    if (!pair_found && !triple_found) {
        return G26_SIGNAL_ERROR_NO_MODEL;
    }
    if (pair_found) {
        g26_refine_model_frequency(sample_rate_hz, &best_pair);
        if (g26_fundamental_is_significant(&best_pair)) {
            pair_bic = best_pair.model_bic;
        } else {
            pair_found = 0;
        }
    }
    if (triple_found) {
        g26_refine_model_frequency(sample_rate_hz, &best_triple);
        if (g26_fundamental_is_significant(&best_triple)) {
            triple_bic = best_triple.model_bic;
        } else {
            triple_found = 0;
        }
    }
    if (!pair_found && !triple_found) {
        return G26_SIGNAL_ERROR_NO_MODEL;
    }
    if (!triple_found || (pair_found && pair_bic <= triple_bic)) {
        best = best_pair;
        best.delta_bic = triple_found ? triple_bic - pair_bic : 1.0e30f;
    } else {
        best = best_triple;
        best.delta_bic = pair_found ? pair_bic - triple_bic : 1.0e30f;
    }
    g26_compute_model_metrics(&best);
    *result = best;
    return G26_SIGNAL_OK;
}

int g26_signal_generate_waveform(const g26_signal_result_t *result,
                                 u32 period_count,
                                 float32_t *output_mv,
                                 u32 output_count)
{
    float32_t reference_phase;
    u32 point;
    u32 component;

    if (result == 0 || output_mv == 0 || output_count < 2U ||
        (period_count != 1U && period_count != 3U) ||
        result->component_count < 2U ||
        result->component_count > G26_SIGNAL_MAX_COMPONENTS) {
        return G26_SIGNAL_ERROR;
    }
    reference_phase = result->components[0].phase_rad;
    for (point = 0U; point < output_count; point++) {
        float32_t cycle = (float32_t)period_count * (float32_t)point /
                          (float32_t)(output_count - 1U);
        float32_t value = 0.0f;

        for (component = 0U; component < result->component_count; component++) {
            const g26_signal_component_t *line = &result->components[component];
            float32_t relative_phase = line->phase_rad -
                (float32_t)line->harmonic_order * reference_phase;

            value += line->amplitude_mv * sinf(
                G26_TWO_PI * (float32_t)line->harmonic_order * cycle +
                relative_phase);
        }
        output_mv[point] = value;
    }
    return G26_SIGNAL_OK;
}

static void g26_synthesize_case(const g26_test_case_t *test_case,
                                s16 *samples)
{
    u32 index;
    u32 component;

    for (index = 0U; index < G26_SIGNAL_SAMPLE_COUNT; index++) {
        float32_t value_mv = test_case->dc_mv;
        long code;

        for (component = 0U; component < test_case->input_component_count;
             component++) {
            const g26_test_component_t *line =
                &test_case->components[component];
            float32_t frequency_hz = test_case->fundamental_hz *
                                     (float32_t)line->harmonic_order;

            value_mv += line->amplitude_mv * sinf(
                G26_TWO_PI * frequency_hz * (float32_t)index /
                test_case->sample_rate_hz + line->phase_rad);
        }
        code = lroundf(value_mv / test_case->mv_per_code);
        if (code > 32767L) {
            code = 32767L;
        } else if (code < -32768L) {
            code = -32768L;
        }
        samples[index] = (s16)code;
    }
}

static float32_t g26_test_expected_upp(const g26_test_case_t *test_case,
                                       u32 point_count)
{
    float32_t minimum = 1.0e30f;
    float32_t maximum = -1.0e30f;
    u32 point;
    u32 component;

    for (point = 0U; point < point_count; point++) {
        float32_t cycle = (float32_t)point / (float32_t)point_count;
        float32_t value = 0.0f;

        for (component = 0U; component < test_case->expected_component_count;
             component++) {
            const g26_test_component_t *line =
                &test_case->components[component];

            value += line->amplitude_mv * sinf(
                G26_TWO_PI * (float32_t)line->harmonic_order * cycle +
                line->phase_rad);
        }
        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
    }
    return maximum - minimum;
}

static float32_t g26_test_sampled_upp(const g26_test_case_t *test_case)
{
    float32_t minimum = 1.0e30f;
    float32_t maximum = -1.0e30f;
    u32 index;
    u32 component;

    for (index = 0U; index < G26_SIGNAL_SAMPLE_COUNT; index++) {
        float32_t value = 0.0f;

        for (component = 0U; component < test_case->expected_component_count;
             component++) {
            const g26_test_component_t *line =
                &test_case->components[component];

            value += line->amplitude_mv * sinf(
                G26_TWO_PI * test_case->fundamental_hz *
                (float32_t)line->harmonic_order * (float32_t)index /
                test_case->sample_rate_hz + line->phase_rad);
        }
        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
    }
    return maximum - minimum;
}

static int g26_test_one_case(const g26_test_case_t *test_case,
                             int require_sparse_upp_failure)
{
    /* Single-digit stages let the caller report case * 10 + stage. */
    s16 *samples = (s16 *)(void *)g_adc_raw_buffer;
    g26_signal_result_t result;
    float32_t expected_rms_square = 0.0f;
    float32_t expected_upp;
    u32 component;

    g26_synthesize_case(test_case, samples);
    if (g26_signal_analyze(samples, test_case->sample_rate_hz,
                           test_case->mv_per_code, &result) !=
        G26_SIGNAL_OK) {
        return -1;
    }
    if (result.component_count != test_case->expected_component_count) {
        return -2;
    }
    if (fabsf(result.fundamental_frequency_hz -
              test_case->fundamental_hz) >
        0.25f * test_case->sample_rate_hz /
        (float32_t)G26_SIGNAL_SAMPLE_COUNT) {
        return -3;
    }
    for (component = 0U; component < test_case->expected_component_count;
         component++) {
        const g26_test_component_t *expected =
            &test_case->components[component];
        const g26_signal_component_t *actual = &result.components[component];

        expected_rms_square += 0.5f * expected->amplitude_mv *
                               expected->amplitude_mv;
        if (actual->harmonic_order != expected->harmonic_order ||
            fabsf(actual->amplitude_mv - expected->amplitude_mv) > 0.6f ||
            fabsf(g26_wrap_phase(actual->phase_rad -
                                 expected->phase_rad)) > 0.04f) {
            return -4;
        }
    }
    expected_upp = g26_test_expected_upp(test_case, 32768U);
    if (fabsf(result.rms_mv - sqrtf(expected_rms_square)) > 0.5f ||
        fabsf(result.upp_mv - expected_upp) > 0.8f ||
        fabsf(result.dc_mv - test_case->dc_mv) > 0.5f ||
        result.normalized_residual > 0.02f) {
        return -5;
    }
    if (require_sparse_upp_failure &&
        expected_upp - g26_test_sampled_upp(test_case) < 30.0f) {
        return -6;
    }
    if (g26_signal_generate_waveform(&result, 1U, g_model_buffer, 640U) !=
            G26_SIGNAL_OK ||
        fabsf(g_model_buffer[0] - g_model_buffer[639]) > 0.01f) {
        return -7;
    }
    if (g26_signal_generate_waveform(&result, 3U, g_model_buffer, 640U) !=
            G26_SIGNAL_OK ||
        fabsf(g_model_buffer[0] - g_model_buffer[639]) > 0.01f) {
        return -8;
    }
    return G26_SIGNAL_OK;
}

int g26_signal_analysis_self_test(void)
{
    static const g26_test_case_t cases[] = {
        {
            5120060.0f / 3.0f, 0.25f, 10000.0f, 3.0f, 3U, 3U,
            {{1U, 40.0f, 0.30f},
             {3U, 25.0f, -1.00f},
             {4U, 15.0f, 0.60f}}
        },
        {
            5120060.0f / 3.0f, 0.25f, 100000.0f, -2.0f, 3U, 3U,
            {{1U, 42.0f, -0.25f},
             {2U, 28.0f, 1.10f},
             {5U, 5.0f, -0.80f}}
        },
        {
            5120060.0f / 3.0f, 0.25f, 50500.0f, -1.0f, 2U, 2U,
            {{1U, 160.0f, 0.45f},
             {2U, 150.0f, -0.70f},
             {5U, 4.0f, 1.20f}}
        },
        {
            1280000.0f, 0.25f, 80000.0f, 1.0f, 2U, 2U,
            {{1U, 5.0f, 0.15f},
             {4U, 125.0f, G26_PI / 4.0f},
             {0U, 0.0f, 0.0f}}
        },
        {
            5120060.0f / 3.0f, 0.25f, 12700.0f, 0.5f, 3U, 3U,
            {{1U, 18.0f, -0.35f},
             {3U, 45.0f, 0.90f},
             {4U, 30.0f, -1.20f}}
        },
        {
            5120060.0f / 3.0f, 0.05f, 10500.0f, -0.2f, 3U, 3U,
            {{1U, 5.0f, 0.25f},
             {3U, 40.0f, -0.60f},
             {4U, 30.0f, 1.10f}}
        },
        {
            5120060.0f / 3.0f, 0.25f, 250000.0f, 0.0f, 2U, 2U,
            {{1U, 125.0f, 0.20f},
             {2U, 62.5f, -0.75f},
             {0U, 0.0f, 0.0f}}
        }
    };
    u32 index;

    if (g26_signal_analysis_init() != G26_SIGNAL_OK) {
        return -100;
    }
    {
        g26_signal_result_t result = {0};

        result.component_count = 3U;
        result.components[0].amplitude_mv = 0.106f;
        if (g26_fundamental_is_significant(&result)) {
            return -91;
        }
        result.components[0].amplitude_mv = 5.0f;
        if (!g26_fundamental_is_significant(&result)) {
            return -91;
        }
    }
    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++) {
        int test_status = g26_test_one_case(&cases[index], index == 3U);

        if (test_status != G26_SIGNAL_OK) {
            return -((int)(index + 1U) * 10 - test_status);
        }
    }
    {
        g26_signal_result_t result = {0};
        float32_t expected_first;
        float32_t expected_second;
        float32_t expected_rms;

        result.component_count = 2U;
        result.fundamental_frequency_hz = 250000.0f;
        result.components[0].frequency_hz = 250000.0f;
        result.components[0].amplitude_mv = 50.0f;
        result.components[0].harmonic_order = 1U;
        result.components[1].frequency_hz = 500000.0f;
        result.components[1].amplitude_mv = 50.0f;
        result.components[1].harmonic_order = 2U;
        expected_first = 50.0f * g26_gain_correction(250000.0f);
        expected_second = 50.0f * g26_gain_correction(500000.0f);
        expected_rms = sqrtf(0.5f * expected_first * expected_first +
                             0.5f * expected_second * expected_second);
        if (g26_signal_apply_amplitude_calibration(&result) !=
                G26_SIGNAL_OK ||
            fabsf(result.components[0].amplitude_mv - expected_first) >
                0.001f ||
            fabsf(result.components[1].amplitude_mv - expected_second) >
                0.001f ||
            fabsf(result.rms_mv - expected_rms) > 0.001f ||
            !isfinite(result.upp_mv) || result.upp_mv <= 0.0f) {
            return -90;
        }
    }
    return G26_SIGNAL_OK;
}
