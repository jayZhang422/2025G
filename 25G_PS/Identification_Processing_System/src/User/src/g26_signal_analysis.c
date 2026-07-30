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
#define G26_GRID_HZ                    500.0f
#define G26_MIN_FREQUENCY_HZ           10000.0f
#define G26_MAX_FREQUENCY_HZ           500000.0f
#define G26_PEAK_CAPACITY              16
#define G26_MAX_LS_COLUMNS             7
/* Allow 10 percent estimator margin below the specified 5 mVpeak floor. */
#define G26_MIN_COMPONENT_MV           4.5f
#define G26_MIN_CANDIDATE_MV           0.5f
#define G26_UPP_POINTS                 4096U
#define G26_EPSILON                    1.0e-12f

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
    /* The snapped candidate, not its bin center, enforces the final limits. */
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
            float32_t snapped_hz = floorf(refined_hz / G26_GRID_HZ + 0.5f) *
                                   G26_GRID_HZ;

            if (snapped_hz >= G26_MIN_FREQUENCY_HZ &&
                snapped_hz <= max_frequency_hz + 0.5f * G26_GRID_HZ) {
                g26_insert_candidate(candidates, &candidate_count,
                                     snapped_hz, amplitude_mv);
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
        result->normalized_residual = sqrtf(error_energy / signal_energy);
    }
    return G26_SIGNAL_OK;
}

static int g26_is_harmonic(float32_t base_hz, float32_t frequency_hz)
{
    float32_t order = floorf(frequency_hz / base_hz + 0.5f);

    return order >= 2.0f &&
           fabsf(frequency_hz - order * base_hz) < 0.1f;
}

static int g26_model_is_usable(const g26_signal_result_t *result)
{
    u32 component;

    for (component = 0U; component < result->component_count; component++) {
        if (result->components[component].amplitude_mv <
            G26_MIN_COMPONENT_MV) {
            return 0;
        }
    }
    return 1;
}

static void g26_consider_model(const float32_t *frequencies_hz,
                               u32 component_count,
                               float32_t sample_rate_hz,
                               g26_signal_result_t *best,
                               float32_t *best_residual,
                               int *found)
{
    g26_signal_result_t candidate;

    if (g26_fit_model(frequencies_hz, component_count, sample_rate_hz,
                      &candidate) != G26_SIGNAL_OK ||
        !g26_model_is_usable(&candidate)) {
        return;
    }
    if (!*found || candidate.normalized_residual < *best_residual) {
        *best = candidate;
        *best_residual = candidate.normalized_residual;
        *found = 1;
    }
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
    g26_signal_result_t best;
    float32_t best_residual = 1.0e30f;
    float32_t window_sum;
    int candidate_count;
    int base;
    int second;
    int third;
    int found = 0;

    if (samples == 0 || result == 0 || !isfinite(sample_rate_hz) ||
        !isfinite(mv_per_code) || sample_rate_hz <= 2.0f *
        G26_MIN_FREQUENCY_HZ || fabsf(mv_per_code) <= G26_EPSILON) {
        return G26_SIGNAL_ERROR;
    }
    if (!g26_fft_initialized &&
        g26_signal_analysis_init() != G26_SIGNAL_OK) {
        return G26_SIGNAL_ERROR;
    }

    g26_prepare_fft(samples, mv_per_code, &window_sum);
    arm_rfft_fast_f32(&g26_fft, g_fft_input_buffer,
                      g_fft_spectrum_buffer, 0);
    g26_compute_magnitudes(window_sum);
    candidate_count = g26_find_candidates(sample_rate_hz, candidates);
    if (candidate_count < 2) {
        return G26_SIGNAL_ERROR;
    }

    for (base = 0; base < candidate_count; base++) {
        for (second = 0; second < candidate_count; second++) {
            float32_t pair[2];

            if (!g26_is_harmonic(candidates[base].frequency_hz,
                                 candidates[second].frequency_hz)) {
                continue;
            }
            pair[0] = candidates[base].frequency_hz;
            pair[1] = candidates[second].frequency_hz;
            g26_consider_model(pair, 2U, sample_rate_hz, &best,
                               &best_residual, &found);

            for (third = second + 1; third < candidate_count; third++) {
                float32_t triple[3];

                if (!g26_is_harmonic(candidates[base].frequency_hz,
                                     candidates[third].frequency_hz)) {
                    continue;
                }
                triple[0] = candidates[base].frequency_hz;
                if (candidates[second].frequency_hz <
                    candidates[third].frequency_hz) {
                    triple[1] = candidates[second].frequency_hz;
                    triple[2] = candidates[third].frequency_hz;
                } else {
                    triple[1] = candidates[third].frequency_hz;
                    triple[2] = candidates[second].frequency_hz;
                }
                g26_consider_model(triple, 3U, sample_rate_hz, &best,
                                   &best_residual, &found);
            }
        }
    }
    if (!found) {
        return G26_SIGNAL_ERROR;
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
    s16 *samples = (s16 *)(void *)g_adc_raw_buffer;
    g26_signal_result_t result;
    float32_t expected_rms_square = 0.0f;
    float32_t expected_upp;
    u32 component;

    g26_synthesize_case(test_case, samples);
    if (g26_signal_analyze(samples, test_case->sample_rate_hz,
                           test_case->mv_per_code, &result) !=
        G26_SIGNAL_OK ||
        result.component_count != test_case->expected_component_count ||
        fabsf(result.fundamental_frequency_hz -
              test_case->fundamental_hz) > 0.1f) {
        return G26_SIGNAL_ERROR;
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
            return G26_SIGNAL_ERROR;
        }
    }
    expected_upp = g26_test_expected_upp(test_case, 32768U);
    if (fabsf(result.rms_mv - sqrtf(expected_rms_square)) > 0.5f ||
        fabsf(result.upp_mv - expected_upp) > 0.8f ||
        fabsf(result.dc_mv - test_case->dc_mv) > 0.5f ||
        result.normalized_residual > 0.02f) {
        return G26_SIGNAL_ERROR;
    }
    if (require_sparse_upp_failure &&
        expected_upp - g26_test_sampled_upp(test_case) < 30.0f) {
        return G26_SIGNAL_ERROR;
    }
    if (g26_signal_generate_waveform(&result, 1U, g_model_buffer, 640U) !=
            G26_SIGNAL_OK ||
        fabsf(g_model_buffer[0] - g_model_buffer[639]) > 0.01f) {
        return G26_SIGNAL_ERROR;
    }
    if (g26_signal_generate_waveform(&result, 3U, g_model_buffer, 640U) !=
            G26_SIGNAL_OK ||
        fabsf(g_model_buffer[0] - g_model_buffer[639]) > 0.01f) {
        return G26_SIGNAL_ERROR;
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
            5120060.0f / 3.0f, 0.25f, 50500.0f, -1.0f, 3U, 2U,
            {{1U, 160.0f, 0.45f},
             {2U, 150.0f, -0.70f},
             {5U, 4.0f, 1.20f}}
        },
        {
            1280000.0f, 0.25f, 80000.0f, 1.0f, 2U, 2U,
            {{1U, 5.0f, 0.15f},
             {4U, 125.0f, G26_PI / 4.0f},
             {0U, 0.0f, 0.0f}}
        }
    };
    u32 index;

    if (g26_signal_analysis_init() != G26_SIGNAL_OK) {
        return G26_SIGNAL_ERROR;
    }
    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++) {
        if (g26_test_one_case(&cases[index], index == 3U) != G26_SIGNAL_OK) {
            return G26_SIGNAL_ERROR;
        }
    }
    return G26_SIGNAL_OK;
}
