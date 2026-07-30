/******************************************************************************
 * modulation_analysis.c
 *
 * Minimal one-frame estimators for the fixed 23D candidate grids:
 * F = 1..5 kHz and Rb = 6/8/10 kbps.
 ******************************************************************************/

#include "../include/modulation_analysis.h"

#include <math.h>
#include <string.h>

#include "xstatus.h"

#define MOD_PI                    3.14159265358979f
#define MOD_TWO_PI                (2.0f * MOD_PI)
#define MOD_MIN_POWER             4.0f
#define MOD_PHASE_JUMP_RAD        (0.75f * MOD_PI)
#define MOD_AM_MIN_INDEX          0.15f
#define MOD_AM_TONE_SCORE         0.90f
#define MOD_DIGITAL_TONE_SCORE    0.45f
#define MOD_FSK_MIN_SEPARATION_HZ 8000.0f
#define MOD_FSK_CLUSTER_SCORE     0.90f
#define MOD_FSK_STABLE_FRACTION   0.35f
#define MOD_FSK_SWITCH_GUARD      0.10f
#define MOD_FM_TONE_SCORE         0.72f
#define MOD_PSK_MIN_JUMPS         4U
#define MOD_MAX_SAMPLES           4096U

typedef struct {
    float dc;
    float amplitude;
    float phase;
    float score;
} tone_fit_t;

typedef struct {
    float low;
    float high;
    float score;
} two_cluster_t;

typedef struct {
    float modulation_index;
    float carrier_offset_hz;
    float score;
} phase_fit_t;

/* One application task owns the analyzer, so static workspaces avoid heap and
 * keep 32 KiB off the FreeRTOS task stack. */
static float g_feature[MOD_MAX_SAMPLES];
static float g_scratch[MOD_MAX_SAMPLES];

static float clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static s16 round_s16(float value)
{
    int rounded = (value >= 0.0f) ? (int)(value + 0.5f) :
                                    (int)(value - 0.5f);

    if (rounded > 32767) return 32767;
    if (rounded < -32768) return -32768;
    return (s16)rounded;
}

static int fit_tone(const float *samples, u32 count, float sample_rate_hz,
                    float frequency_hz, tone_fit_t *fit)
{
    double sum_x = 0.0;
    double sum_s = 0.0;
    double sum_c = 0.0;
    double sum_ss = 0.0;
    double sum_cc = 0.0;
    double sum_sc = 0.0;
    double sum_xs = 0.0;
    double sum_xc = 0.0;
    double total_energy = 0.0;
    double residual_energy = 0.0;
    double mean_x;
    double mean_s;
    double mean_c;
    double determinant;
    double sine_coefficient;
    double cosine_coefficient;
    float phase_step;
    u32 index;

    if (samples == NULL || fit == NULL || count < 8U ||
        sample_rate_hz <= 0.0f || frequency_hz <= 0.0f) {
        return XST_FAILURE;
    }

    phase_step = MOD_TWO_PI * frequency_hz / sample_rate_hz;
    for (index = 0U; index < count; ++index) {
        float phase = phase_step * index;
        double sine = sinf(phase);
        double cosine = cosf(phase);
        double value = samples[index];

        sum_x += value;
        sum_s += sine;
        sum_c += cosine;
        sum_ss += sine * sine;
        sum_cc += cosine * cosine;
        sum_sc += sine * cosine;
        sum_xs += value * sine;
        sum_xc += value * cosine;
    }

    mean_x = sum_x / count;
    mean_s = sum_s / count;
    mean_c = sum_c / count;
    sum_ss -= (double)count * mean_s * mean_s;
    sum_cc -= (double)count * mean_c * mean_c;
    sum_sc -= (double)count * mean_s * mean_c;
    sum_xs -= (double)count * mean_x * mean_s;
    sum_xc -= (double)count * mean_x * mean_c;
    determinant = sum_ss * sum_cc - sum_sc * sum_sc;
    if (fabs(determinant) < 1.0e-12) {
        return XST_FAILURE;
    }

    sine_coefficient = (sum_xs * sum_cc - sum_xc * sum_sc) /
                       determinant;
    cosine_coefficient = (sum_xc * sum_ss - sum_xs * sum_sc) /
                         determinant;
    fit->dc = (float)(mean_x - sine_coefficient * mean_s -
                      cosine_coefficient * mean_c);
    fit->amplitude = (float)sqrt(sine_coefficient * sine_coefficient +
                                cosine_coefficient * cosine_coefficient);
    fit->phase = (float)atan2(cosine_coefficient, sine_coefficient);

    for (index = 0U; index < count; ++index) {
        float phase = phase_step * index;
        double centered = samples[index] - mean_x;
        double model = sine_coefficient * (sinf(phase) - mean_s) +
                       cosine_coefficient * (cosf(phase) - mean_c);
        double error = centered - model;

        total_energy += centered * centered;
        residual_energy += error * error;
    }
    fit->score = (total_energy > 1.0e-12) ?
        clamp01((float)(1.0 - residual_energy / total_energy)) : 0.0f;
    return XST_SUCCESS;
}

static tone_fit_t best_tone(const float *samples, u32 count,
                            float sample_rate_hz, u32 first_khz,
                            u32 last_khz, float *frequency_hz)
{
    tone_fit_t best = {0.0f, 0.0f, 0.0f, 0.0f};
    u32 candidate_khz;

    *frequency_hz = 0.0f;
    for (candidate_khz = first_khz; candidate_khz <= last_khz;
         ++candidate_khz) {
        tone_fit_t candidate;

        if (fit_tone(samples, count, sample_rate_hz,
                     1000.0f * candidate_khz, &candidate) == XST_SUCCESS &&
            candidate.score > best.score) {
            best = candidate;
            *frequency_hz = 1000.0f * candidate_khz;
        }
    }
    return best;
}

static two_cluster_t fit_two_clusters(const float *samples, u32 count)
{
    two_cluster_t result = {0.0f, 0.0f, 0.0f};
    float low;
    float high;
    double error_energy = 0.0;
    u32 low_count = 0U;
    u32 high_count = 0U;
    u32 iteration;
    u32 index;

    if (samples == NULL || count < 2U) return result;
    low = samples[0];
    high = samples[0];
    for (index = 1U; index < count; ++index) {
        if (samples[index] < low) low = samples[index];
        if (samples[index] > high) high = samples[index];
    }
    if (high - low < 1.0e-6f) return result;

    for (iteration = 0U; iteration < 8U; ++iteration) {
        double low_sum = 0.0;
        double high_sum = 0.0;

        low_count = 0U;
        high_count = 0U;
        for (index = 0U; index < count; ++index) {
            if (fabsf(samples[index] - low) <=
                fabsf(samples[index] - high)) {
                low_sum += samples[index];
                low_count++;
            } else {
                high_sum += samples[index];
                high_count++;
            }
        }
        if (low_count == 0U || high_count == 0U) return result;
        low = (float)(low_sum / low_count);
        high = (float)(high_sum / high_count);
    }
    if (low > high) {
        float swap = low;
        low = high;
        high = swap;
    }
    for (index = 0U; index < count; ++index) {
        float distance = fminf(fabsf(samples[index] - low),
                               fabsf(samples[index] - high));
        error_energy += (double)distance * distance;
    }

    result.low = low;
    result.high = high;
    result.score = clamp01((high - low) * (high - low) /
        ((high - low) * (high - low) +
         4.0f * (float)(error_energy / count)));
    return result;
}

static int solve_four(float matrix[4][5], float solution[4])
{
    u32 column;

    for (column = 0U; column < 4U; ++column) {
        u32 pivot = column;
        u32 row;

        for (row = column + 1U; row < 4U; ++row) {
            if (fabsf(matrix[row][column]) >
                fabsf(matrix[pivot][column])) {
                pivot = row;
            }
        }
        if (fabsf(matrix[pivot][column]) < 1.0e-7f) return XST_FAILURE;
        if (pivot != column) {
            u32 item;
            for (item = column; item < 5U; ++item) {
                float swap = matrix[column][item];
                matrix[column][item] = matrix[pivot][item];
                matrix[pivot][item] = swap;
            }
        }
        {
            float divisor = matrix[column][column];
            u32 item;
            for (item = column; item < 5U; ++item)
                matrix[column][item] /= divisor;
        }
        for (row = 0U; row < 4U; ++row) {
            float factor;
            u32 item;

            if (row == column) continue;
            factor = matrix[row][column];
            for (item = column; item < 5U; ++item)
                matrix[row][item] -= factor * matrix[column][item];
        }
    }
    for (column = 0U; column < 4U; ++column)
        solution[column] = matrix[column][4];
    return XST_SUCCESS;
}

static int fit_phase_tone(const float *phase, u32 count,
                          float sample_rate_hz, float frequency_hz,
                          phase_fit_t *fit)
{
    float normal[4][5] = {{0.0f}};
    float coefficient[4];
    double model_energy = 0.0;
    double residual_energy = 0.0;
    u32 row;
    u32 index;

    if (phase == NULL || fit == NULL || count < 8U ||
        sample_rate_hz <= 0.0f || frequency_hz <= 0.0f) {
        return XST_FAILURE;
    }

    for (index = 0U; index < count; ++index) {
        float basis[4];
        u32 column;

        basis[0] = 1.0f;
        basis[1] = ((float)index - 0.5f * (float)(count - 1U)) /
                   (float)count;
        basis[2] = sinf(MOD_TWO_PI * frequency_hz * index /
                        sample_rate_hz);
        basis[3] = cosf(MOD_TWO_PI * frequency_hz * index /
                        sample_rate_hz);
        for (row = 0U; row < 4U; ++row) {
            for (column = 0U; column < 4U; ++column)
                normal[row][column] += basis[row] * basis[column];
            normal[row][4] += basis[row] * phase[index];
        }
    }
    if (solve_four(normal, coefficient) != XST_SUCCESS) return XST_FAILURE;

    for (index = 0U; index < count; ++index) {
        float normalized_time =
            ((float)index - 0.5f * (float)(count - 1U)) / (float)count;
        float sine = sinf(MOD_TWO_PI * frequency_hz * index /
                          sample_rate_hz);
        float cosine = cosf(MOD_TWO_PI * frequency_hz * index /
                            sample_rate_hz);
        float modulation = coefficient[2] * sine +
                           coefficient[3] * cosine;
        float predicted = coefficient[0] +
                          coefficient[1] * normalized_time + modulation;
        float error = phase[index] - predicted;

        model_energy += (double)modulation * modulation;
        residual_energy += (double)error * error;
    }
    fit->modulation_index = sqrtf(coefficient[2] * coefficient[2] +
                                  coefficient[3] * coefficient[3]);
    fit->carrier_offset_hz = coefficient[1] * sample_rate_hz /
                             (MOD_TWO_PI * count);
    fit->score = (model_energy + residual_energy > 1.0e-10) ?
        clamp01((float)(model_energy / (model_energy + residual_energy))) :
        0.0f;
    return XST_SUCCESS;
}

static phase_fit_t best_phase_tone(const float *phase, u32 count,
                                   float sample_rate_hz,
                                   float *frequency_hz)
{
    phase_fit_t best = {0.0f, 0.0f, 0.0f};
    u32 candidate_khz;

    *frequency_hz = 0.0f;
    for (candidate_khz = 1U; candidate_khz <= 5U; ++candidate_khz) {
        phase_fit_t candidate;

        if (fit_phase_tone(phase, count, sample_rate_hz,
                           1000.0f * candidate_khz,
                           &candidate) == XST_SUCCESS &&
            candidate.score > best.score) {
            best = candidate;
            *frequency_hz = 1000.0f * candidate_khz;
        }
    }
    return best;
}

static u32 estimate_bit_rate(const float *waveform, u32 count,
                             float sample_rate_hz, float *score,
                             float *symbol_phase_samples)
{
    float frequency_hz;
    tone_fit_t fit = best_tone(waveform, count, sample_rate_hz,
                               3U, 5U, &frequency_hz);
    float samples_per_symbol;
    float transition_sample;

    *score = fit.score;
    *symbol_phase_samples = 0.0f;
    if (frequency_hz == 0.0f) return 0U;
    samples_per_symbol = sample_rate_hz / (2.0f * frequency_hz);
    transition_sample = -fit.phase * sample_rate_hz /
                        (MOD_TWO_PI * frequency_hz);
    *symbol_phase_samples = fmodf(transition_sample +
                                  0.5f * samples_per_symbol,
                                  samples_per_symbol);
    if (*symbol_phase_samples < 0.0f)
        *symbol_phase_samples += samples_per_symbol;
    return (u32)(2.0f * frequency_hz + 0.5f);
}

static float average_frequency(const s16 *iq, u32 count,
                               float sample_rate_hz)
{
    double phase_sum = 0.0;
    u32 valid = 0U;
    u32 index;

    for (index = 1U; index < count; ++index) {
        float previous_i = iq[2U * (index - 1U)];
        float previous_q = iq[2U * (index - 1U) + 1U];
        float current_i = iq[2U * index];
        float current_q = iq[2U * index + 1U];
        float previous_power = previous_i * previous_i +
                               previous_q * previous_q;
        float current_power = current_i * current_i + current_q * current_q;

        if (previous_power >= MOD_MIN_POWER &&
            current_power >= MOD_MIN_POWER) {
            phase_sum += atan2f(previous_i * current_q -
                                previous_q * current_i,
                                previous_i * current_i +
                                previous_q * current_q);
            valid++;
        }
    }
    return (valid != 0U) ?
        (float)(phase_sum / valid) * sample_rate_hz / MOD_TWO_PI : 0.0f;
}

static u32 build_psk_state(const s16 *iq, u32 count, float sample_rate_hz,
                           float carrier_offset_hz, float *state)
{
    float current_state = 1.0f;
    float carrier_step = MOD_TWO_PI * carrier_offset_hz / sample_rate_hz;
    u32 jumps = 0U;
    u32 index;

    state[0] = current_state;
    for (index = 1U; index < count; ++index) {
        float previous_i = iq[2U * (index - 1U)];
        float previous_q = iq[2U * (index - 1U) + 1U];
        float current_i = iq[2U * index];
        float current_q = iq[2U * index + 1U];
        float previous_power = previous_i * previous_i +
                               previous_q * previous_q;
        float current_power = current_i * current_i + current_q * current_q;

        if (previous_power >= MOD_MIN_POWER &&
            current_power >= MOD_MIN_POWER) {
            float step = atan2f(previous_i * current_q -
                                previous_q * current_i,
                                previous_i * current_i +
                                previous_q * current_q);

            step -= carrier_step;
            if (step > MOD_PI) step -= MOD_TWO_PI;
            else if (step < -MOD_PI) step += MOD_TWO_PI;
            if (fabsf(step) >= MOD_PHASE_JUMP_RAD) {
                current_state = -current_state;
                jumps++;
            }
        }
        state[index] = current_state;
    }
    return jumps;
}

static float psk_carrier_offset(const s16 *iq, u32 count,
                                float sample_rate_hz)
{
    double doubled_phase_sum = 0.0;
    u32 valid = 0U;
    u32 index;

    for (index = 1U; index < count; ++index) {
        float pi = iq[2U * (index - 1U)];
        float pq = iq[2U * (index - 1U) + 1U];
        float ci = iq[2U * index];
        float cq = iq[2U * index + 1U];
        float previous_power = pi * pi + pq * pq;
        float current_power = ci * ci + cq * cq;

        if (previous_power >= MOD_MIN_POWER &&
            current_power >= MOD_MIN_POWER) {
            float previous_real = pi * pi - pq * pq;
            float previous_imag = 2.0f * pi * pq;
            float current_real = ci * ci - cq * cq;
            float current_imag = 2.0f * ci * cq;

            doubled_phase_sum += atan2f(
                previous_real * current_imag -
                previous_imag * current_real,
                previous_real * current_real +
                previous_imag * current_imag);
            valid++;
        }
    }
    return (valid != 0U) ?
        0.5f * (float)(doubled_phase_sum / valid) *
        sample_rate_hz / MOD_TWO_PI : 0.0f;
}

static u32 build_frequency_samples(const s16 *iq, u32 count,
                                   float sample_rate_hz, float *frequency)
{
    u32 valid = 0U;
    u32 index;

    for (index = 1U; index < count; ++index) {
        float previous_i = iq[2U * (index - 1U)];
        float previous_q = iq[2U * (index - 1U) + 1U];
        float current_i = iq[2U * index];
        float current_q = iq[2U * index + 1U];
        float previous_power = previous_i * previous_i +
                               previous_q * previous_q;
        float current_power = current_i * current_i + current_q * current_q;

        if (previous_power >= MOD_MIN_POWER &&
            current_power >= MOD_MIN_POWER) {
            float step = atan2f(previous_i * current_q -
                                previous_q * current_i,
                                previous_i * current_i +
                                previous_q * current_q);
            frequency[valid++] = step * sample_rate_hz / MOD_TWO_PI;
        }
    }
    return valid;
}

static void build_fsk_state(const s16 *iq, u32 count, float sample_rate_hz,
                            float low, float high, float *state)
{
    float previous_state = 1.0f;
    float midpoint = 0.5f * (low + high);
    float guard = MOD_FSK_SWITCH_GUARD * (high - low);
    u32 index;

    state[0] = previous_state;
    for (index = 1U; index < count; ++index) {
        float previous_i = iq[2U * (index - 1U)];
        float previous_q = iq[2U * (index - 1U) + 1U];
        float current_i = iq[2U * index];
        float current_q = iq[2U * index + 1U];
        float previous_power = previous_i * previous_i +
                               previous_q * previous_q;
        float current_power = current_i * current_i + current_q * current_q;

        if (previous_power >= MOD_MIN_POWER &&
            current_power >= MOD_MIN_POWER) {
            float frequency = atan2f(previous_i * current_q -
                                     previous_q * current_i,
                                     previous_i * current_i +
                                     previous_q * current_q) *
                              sample_rate_hz / MOD_TWO_PI;
            if (frequency >= midpoint + guard) previous_state = 1.0f;
            else if (frequency <= midpoint - guard) previous_state = -1.0f;
        }
        state[index] = previous_state;
    }
}

int modulation_analyze_frame(const s16 *interleaved_iq,
                             u32 complex_samples,
                             float sample_rate_hz,
                             modulation_result_t *result)
{
    tone_fit_t am_fit;
    two_cluster_t amplitude_clusters;
    two_cluster_t frequency_clusters;
    phase_fit_t fm_fit;
    float am_frequency_hz;
    float fm_frequency_hz;
    float ask_rate_score;
    float ask_symbol_phase;
    float psk_offset_hz;
    float psk_rate_score;
    float psk_symbol_phase;
    float fsk_rate_score;
    float fsk_symbol_phase;
    float amplitude_mean = 0.0f;
    float amplitude_variance = 0.0f;
    float amplitude_cv;
    u32 ask_bit_rate;
    u32 psk_bit_rate;
    u32 fsk_bit_rate;
    u32 psk_jumps;
    u32 frequency_count;
    u32 index;

    if (interleaved_iq == NULL || result == NULL ||
        complex_samples != MOD_MAX_SAMPLES ||
        sample_rate_hz <= 0.0f) {
        return XST_FAILURE;
    }
    memset(result, 0, sizeof(*result));

    for (index = 0U; index < complex_samples; ++index) {
        float i = interleaved_iq[2U * index];
        float q = interleaved_iq[2U * index + 1U];
        g_feature[index] = sqrtf(i * i + q * q);
    }
    for (index = 0U; index < complex_samples; ++index) {
        u32 first = (index > 2U) ? index - 2U : 0U;
        u32 last = (index + 2U < complex_samples) ?
                   index + 2U : complex_samples - 1U;
        float sum = 0.0f;
        u32 sample;

        for (sample = first; sample <= last; ++sample)
            sum += g_feature[sample];
        g_scratch[index] = sum / (last - first + 1U);
        amplitude_mean += g_scratch[index];
    }
    amplitude_mean /= complex_samples;
    if (amplitude_mean < 0.5f) {
        result->type = MODULATION_UNKNOWN;
        return XST_SUCCESS;
    }
    for (index = 0U; index < complex_samples; ++index) {
        float centered = g_scratch[index] - amplitude_mean;
        amplitude_variance += centered * centered;
    }
    amplitude_variance /= complex_samples;
    amplitude_cv = sqrtf(amplitude_variance) / amplitude_mean;
    am_fit = best_tone(g_scratch, complex_samples, sample_rate_hz,
                       1U, 5U, &am_frequency_hz);
    amplitude_clusters = fit_two_clusters(g_scratch, complex_samples);
    ask_bit_rate = estimate_bit_rate(
        g_scratch, complex_samples, sample_rate_hz,
        &ask_rate_score, &ask_symbol_phase);

    psk_offset_hz = psk_carrier_offset(interleaved_iq, complex_samples,
                                       sample_rate_hz);
    psk_jumps = build_psk_state(interleaved_iq, complex_samples,
                                sample_rate_hz, psk_offset_hz, g_scratch);
    psk_bit_rate = estimate_bit_rate(g_scratch, complex_samples,
                                     sample_rate_hz, &psk_rate_score,
                                     &psk_symbol_phase);

    {
        float previous_raw = atan2f(interleaved_iq[1], interleaved_iq[0]);
        float unwrap_offset = 0.0f;

        g_feature[0] = previous_raw;
        for (index = 1U; index < complex_samples; ++index) {
            float raw = atan2f(interleaved_iq[2U * index + 1U],
                               interleaved_iq[2U * index]);
            float delta = raw - previous_raw;

            if (delta > MOD_PI) unwrap_offset -= MOD_TWO_PI;
            else if (delta < -MOD_PI) unwrap_offset += MOD_TWO_PI;
            g_feature[index] = raw + unwrap_offset;
            previous_raw = raw;
        }
    }
    fm_fit = best_phase_tone(g_feature, complex_samples, sample_rate_hz,
                             &fm_frequency_hz);

    frequency_count = build_frequency_samples(
        interleaved_iq, complex_samples, sample_rate_hz, g_feature);
    frequency_clusters = fit_two_clusters(g_feature, frequency_count);
    if (frequency_clusters.high - frequency_clusters.low >=
        MOD_FSK_MIN_SEPARATION_HZ) {
        float stable_radius = MOD_FSK_STABLE_FRACTION *
                              (frequency_clusters.high -
                               frequency_clusters.low);
        u32 stable_count = 0U;

        for (index = 0U; index < frequency_count; ++index) {
            if (fminf(fabsf(g_feature[index] - frequency_clusters.low),
                      fabsf(g_feature[index] - frequency_clusters.high)) <=
                stable_radius) {
                g_feature[stable_count++] = g_feature[index];
            }
        }
        if (stable_count >= 2U) {
            frequency_count = stable_count;
            frequency_clusters = fit_two_clusters(g_feature,
                                                   frequency_count);
        }
    }
    build_fsk_state(interleaved_iq, complex_samples, sample_rate_hz,
                    frequency_clusters.low, frequency_clusters.high,
                    g_scratch);
    fsk_bit_rate = estimate_bit_rate(g_scratch, complex_samples,
                                     sample_rate_hz, &fsk_rate_score,
                                     &fsk_symbol_phase);

    if (psk_jumps >= MOD_PSK_MIN_JUMPS &&
        psk_rate_score >= MOD_DIGITAL_TONE_SCORE && amplitude_cv < 0.45f) {
        result->type = MODULATION_2PSK;
        result->confidence = clamp01(psk_rate_score);
        result->carrier_offset_hz = psk_offset_hz;
        result->bit_rate_bps = psk_bit_rate;
        result->symbol_phase_samples = psk_symbol_phase;
        return XST_SUCCESS;
    }
    if (am_fit.score >= MOD_AM_TONE_SCORE && am_fit.dc > 0.0f &&
        am_fit.amplitude / am_fit.dc >= MOD_AM_MIN_INDEX &&
        am_fit.amplitude / am_fit.dc < 1.1f) {
        result->type = MODULATION_AM;
        result->confidence = am_fit.score;
        result->carrier_offset_hz = average_frequency(
            interleaved_iq, complex_samples, sample_rate_hz);
        result->modulation_frequency_hz = am_frequency_hz;
        result->am_index = am_fit.amplitude / am_fit.dc;
        return XST_SUCCESS;
    }
    if (amplitude_clusters.score >= 0.82f &&
        ask_rate_score >= MOD_DIGITAL_TONE_SCORE) {
        result->type = MODULATION_2ASK;
        result->confidence = clamp01(0.5f * amplitude_clusters.score +
                                     0.5f * ask_rate_score);
        result->carrier_offset_hz = average_frequency(
            interleaved_iq, complex_samples, sample_rate_hz);
        result->bit_rate_bps = ask_bit_rate;
        result->symbol_phase_samples = ask_symbol_phase;
        return XST_SUCCESS;
    }
    if (frequency_count > complex_samples / 2U &&
        frequency_clusters.high - frequency_clusters.low >=
            MOD_FSK_MIN_SEPARATION_HZ &&
        frequency_clusters.score >= MOD_FSK_CLUSTER_SCORE &&
        fsk_rate_score >= MOD_DIGITAL_TONE_SCORE) {
        result->type = MODULATION_2FSK;
        result->confidence = clamp01(0.5f * frequency_clusters.score +
                                     0.5f * fsk_rate_score);
        result->carrier_offset_hz = 0.5f * (frequency_clusters.low +
                                            frequency_clusters.high);
        result->bit_rate_bps = fsk_bit_rate;
        result->symbol_phase_samples = fsk_symbol_phase;
        result->fsk_low_offset_hz = frequency_clusters.low;
        result->fsk_high_offset_hz = frequency_clusters.high;
        result->fsk_index = (result->bit_rate_bps != 0U) ?
            (frequency_clusters.high - frequency_clusters.low) /
            result->bit_rate_bps : 0.0f;
        return XST_SUCCESS;
    }
    if (fm_fit.score >= MOD_FM_TONE_SCORE &&
        fm_fit.modulation_index >= 0.5f &&
        fm_fit.modulation_index <= 6.0f) {
        result->type = MODULATION_FM;
        result->confidence = fm_fit.score;
        result->carrier_offset_hz = fm_fit.carrier_offset_hz;
        result->modulation_frequency_hz = fm_frequency_hz;
        result->fm_index = fm_fit.modulation_index;
        result->frequency_deviation_hz =
            fm_fit.modulation_index * fm_frequency_hz;
        return XST_SUCCESS;
    }

    result->type = MODULATION_CW;
    result->confidence = clamp01(1.0f - fmaxf(amplitude_cv,
                                              fm_fit.modulation_index / 6.0f));
    result->carrier_offset_hz = average_frequency(
        interleaved_iq, complex_samples, sample_rate_hz);
    return XST_SUCCESS;
}

const char *modulation_type_name(modulation_type_t type)
{
    switch (type) {
    case MODULATION_CW: return "CW";
    case MODULATION_AM: return "AM";
    case MODULATION_FM: return "FM";
    case MODULATION_2ASK: return "2ASK";
    case MODULATION_2FSK: return "2FSK";
    case MODULATION_2PSK: return "2PSK";
    default: return "UNKNOWN";
    }
}

static void generate_test_frame(s16 *iq, modulation_type_t type,
                                float parameter_a, float parameter_b)
{
    const float sample_rate_hz = 160001.875f;
    const float carrier_offset_hz =
        (type == MODULATION_2PSK && parameter_b != 0.0f) ?
        parameter_b : 12.0f;
    float phase = 0.0f;
    u32 index;

    for (index = 0U; index < MOD_MAX_SAMPLES; ++index) {
        float time = index / sample_rate_hz;
        float amplitude = 10.0f;
        float sample_phase = MOD_TWO_PI * carrier_offset_hz * time;
        u32 bit = ((u32)(time * parameter_a)) & 1U;

        switch (type) {
        case MODULATION_AM:
            /* Match the bench setup: the generator keeps the modulated
             * waveform peak near 100 mVpp as modulation depth changes. */
            amplitude *= (1.0f + parameter_b *
                          sinf(MOD_TWO_PI * parameter_a * time)) /
                         (1.0f + parameter_b);
            break;
        case MODULATION_FM:
            sample_phase -= parameter_b *
                            cosf(MOD_TWO_PI * parameter_a * time);
            break;
        case MODULATION_2ASK:
            amplitude = bit ? 10.0f : 0.0f;
            break;
        case MODULATION_2FSK: {
            float deviation_hz = parameter_b;
            float frequency_hz = carrier_offset_hz +
                                 (bit ? deviation_hz : -deviation_hz);
            phase += MOD_TWO_PI * frequency_hz / sample_rate_hz;
            sample_phase = phase;
            break;
        }
        case MODULATION_2PSK:
            if (bit) sample_phase += MOD_PI;
            break;
        default:
            break;
        }
        iq[2U * index] = round_s16(amplitude * cosf(sample_phase));
        iq[2U * index + 1U] = round_s16(amplitude * sinf(sample_phase));
    }
}

static int run_self_test_case(modulation_type_t type, float parameter_a,
                              float parameter_b)
{
    static s16 iq[MOD_MAX_SAMPLES * 2U];
    modulation_result_t result;

    generate_test_frame(iq, type, parameter_a, parameter_b);
    if (modulation_analyze_frame(iq, MOD_MAX_SAMPLES,
                                 160001.875f, &result) != XST_SUCCESS ||
        result.type != type) {
        return XST_FAILURE;
    }
    if (type == MODULATION_AM &&
        (fabsf(result.modulation_frequency_hz - parameter_a) > 50.0f ||
         fabsf(result.am_index - parameter_b) > 0.1f)) {
        return XST_FAILURE;
    }
    if (type == MODULATION_FM &&
        (fabsf(result.modulation_frequency_hz - parameter_a) > 50.0f ||
         fabsf(result.fm_index - parameter_b) > 0.3f ||
         fabsf(result.frequency_deviation_hz - parameter_a * parameter_b) >
             300.0f)) {
        return XST_FAILURE;
    }
    if ((type == MODULATION_CW || type == MODULATION_AM ||
         type == MODULATION_FM) && result.bit_rate_bps != 0U) {
        return XST_FAILURE;
    }
    if ((type == MODULATION_2ASK || type == MODULATION_2PSK) &&
        result.bit_rate_bps != (u32)parameter_a) {
        return XST_FAILURE;
    }
    if (type == MODULATION_2FSK &&
        (result.bit_rate_bps != (u32)parameter_a ||
         fabsf(result.fsk_index - 2.0f * parameter_b / parameter_a) > 0.2f)) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int modulation_analysis_self_test(void)
{
    if (run_self_test_case(MODULATION_CW, 0.0f, 0.0f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_AM, 5000.0f, 0.3f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_AM, 1000.0f, 0.9f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_FM, 1000.0f, 1.0f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_FM, 5000.0f, 5.0f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_2ASK, 6000.0f, 0.0f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_2ASK, 8000.0f, 0.0f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_2ASK, 10000.0f, 0.0f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_2FSK, 6000.0f, 6000.0f) !=
            XST_SUCCESS ||
        run_self_test_case(MODULATION_2FSK, 10000.0f, 25000.0f) !=
            XST_SUCCESS ||
        run_self_test_case(MODULATION_2PSK, 6000.0f, 0.0f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_2PSK, 8000.0f, 0.0f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_2PSK, 10000.0f, 0.0f) != XST_SUCCESS ||
        run_self_test_case(MODULATION_2PSK, 10000.0f, -24000.0f) !=
            XST_SUCCESS) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}
