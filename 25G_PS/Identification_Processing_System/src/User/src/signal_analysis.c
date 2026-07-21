/******************************************************************************
 * signal_analysis.c
 *
 * The FFT supplies candidate frequencies. A joint time-domain residual then
 * selects the independent A/B waveform types from the four valid combinations.
 ******************************************************************************/

#include "../include/app_config.h"
#include "../include/signal_processing.h"

#include <math.h>

typedef struct {
    int bin;
    float32_t magnitude;
} signal_peak_t;

typedef struct {
    float32_t frequency_hz;
    float32_t magnitude;
} signal_frequency_candidate_t;

typedef struct {
    float32_t sine;
    float32_t cosine;
    float32_t sine_step;
    float32_t cosine_step;
} signal_oscillator_t;

static void signal_oscillator_init(signal_oscillator_t *oscillator,
                                   float32_t frequency_hz,
                                   float32_t phase_rad)
{
    float32_t phase_step = 2.0f * APP_PI * frequency_hz /
                             APP_SAMPLE_RATE_HZ;

    oscillator->sine = sinf(phase_rad);
    oscillator->cosine = cosf(phase_rad);
    oscillator->sine_step = sinf(phase_step);
    oscillator->cosine_step = cosf(phase_step);
}

static void signal_oscillator_advance(signal_oscillator_t *oscillator,
                                      int sample_index)
{
    float32_t sine = oscillator->sine * oscillator->cosine_step +
                     oscillator->cosine * oscillator->sine_step;
    float32_t cosine = oscillator->cosine * oscillator->cosine_step -
                       oscillator->sine * oscillator->sine_step;

    oscillator->sine = sine;
    oscillator->cosine = cosine;
    if ((sample_index & 63) == 63) {
        float32_t magnitude = sqrtf(sine * sine + cosine * cosine);

        if (magnitude > 1.0e-12f) {
            oscillator->sine /= magnitude;
            oscillator->cosine /= magnitude;
        }
    }
}

static void signal_render_component(const signal_component_t *component,
                                    float32_t *model_workspace, int add)
{
    signal_oscillator_t oscillators[(APP_TRI_MAX_HARMONIC + 1) / 2];
    int harmonic_count = (component->waveform == SIGNAL_WAVE_TRIANGLE) ?
        ((APP_TRI_MAX_HARMONIC + 1) / 2) : 1;
    int harmonic_index;
    int sample_index;

    for (harmonic_index = 0; harmonic_index < harmonic_count;
         harmonic_index++) {
        int harmonic = 2 * harmonic_index + 1;

        signal_oscillator_init(&oscillators[harmonic_index],
                               component->frequency_hz * (float32_t)harmonic,
                               component->measured_phase_rad *
                               (float32_t)harmonic);
    }

    for (sample_index = 0; sample_index < APP_FFT_LEN; sample_index++) {
        float32_t value = 0.0f;

        for (harmonic_index = 0; harmonic_index < harmonic_count;
             harmonic_index++) {
            int harmonic = 2 * harmonic_index + 1;
            float32_t scale = component->fundamental_amplitude;

            if (component->waveform == SIGNAL_WAVE_TRIANGLE) {
                if (harmonic_index & 1) {
                    scale = -scale;
                }
                scale /= (float32_t)(harmonic * harmonic);
            }
            value += scale * oscillators[harmonic_index].sine;
            signal_oscillator_advance(&oscillators[harmonic_index],
                                      sample_index);
        }
        if (add) {
            model_workspace[sample_index] += value;
        } else {
            model_workspace[sample_index] = value;
        }
    }
}

static void signal_prepare_adc_frame(const u16 *raw_samples,
                                     float32_t *time_domain,
                                     float32_t *fft_input)
{
    float32_t sum = 0.0f;
    float32_t mean;
    int index;

    for (index = 0; index < APP_FFT_LEN; index++) {
        time_domain[index] = (float32_t)(raw_samples[index] >> 4);
        sum += time_domain[index];
    }

    mean = sum / (float32_t)APP_FFT_LEN;
    for (index = 0; index < APP_FFT_LEN; index++) {
        float32_t hann = 0.5f - 0.5f * cosf(2.0f * APP_PI *
            (float32_t)index / (float32_t)(APP_FFT_LEN - 1));

        time_domain[index] -= mean;
        fft_input[index] = time_domain[index] * hann;
    }
}

static void signal_compute_magnitudes(const float32_t *spectrum,
                                      float32_t *magnitude)
{
    int bin;

    magnitude[0] = fabsf(spectrum[0]);
    for (bin = 1; bin < APP_SPEC_LEN; bin++) {
        float32_t real = spectrum[2 * bin];
        float32_t imaginary = spectrum[2 * bin + 1];

        magnitude[bin] = sqrtf(real * real + imaginary * imaginary);
    }
}

static void signal_insert_peak(signal_peak_t *peaks, int *peak_count,
                               int bin, float32_t magnitude)
{
    int index;
    int limit = *peak_count;

    if (limit < APP_PEAK_CANDIDATE_COUNT) {
        peaks[limit].bin = bin;
        peaks[limit].magnitude = magnitude;
        (*peak_count)++;
    } else if (magnitude <= peaks[limit - 1].magnitude) {
        return;
    } else {
        peaks[limit - 1].bin = bin;
        peaks[limit - 1].magnitude = magnitude;
    }

    for (index = *peak_count - 1; index > 0; index--) {
        signal_peak_t temporary;

        if (peaks[index].magnitude <= peaks[index - 1].magnitude) {
            break;
        }
        temporary = peaks[index];
        peaks[index] = peaks[index - 1];
        peaks[index - 1] = temporary;
    }
}

static int signal_find_candidate_peaks(const float32_t *magnitude,
                                       signal_peak_t *peaks)
{
    int bin;
    int peak_count = 0;
    int first_bin = (int)(APP_SIGNAL_MIN_HZ / APP_BIN_WIDTH_HZ);
    int last_bin = (int)(APP_SIGNAL_MAX_HZ / APP_BIN_WIDTH_HZ) + 1;

    if (first_bin < 1) {
        first_bin = 1;
    }
    if (last_bin >= APP_SPEC_LEN - 1) {
        last_bin = APP_SPEC_LEN - 2;
    }

    for (bin = first_bin; bin <= last_bin; bin++) {
        if (magnitude[bin] > magnitude[bin - 1] &&
            magnitude[bin] >= magnitude[bin + 1]) {
            signal_insert_peak(peaks, &peak_count, bin, magnitude[bin]);
        }
    }
    return peak_count;
}

static float32_t signal_refine_frequency(const float32_t *magnitude, int bin)
{
    float32_t left = magnitude[bin - 1];
    float32_t center = magnitude[bin];
    float32_t right = magnitude[bin + 1];
    float32_t denominator = left - 2.0f * center + right;
    float32_t offset = 0.0f;

    if (fabsf(denominator) > 1.0e-12f) {
        offset = 0.5f * (left - right) / denominator;
        if (offset > 0.5f) {
            offset = 0.5f;
        } else if (offset < -0.5f) {
            offset = -0.5f;
        }
    }
    return ((float32_t)bin + offset) * APP_BIN_WIDTH_HZ;
}

static void signal_insert_frequency_candidate(
    signal_frequency_candidate_t *candidates, int *candidate_count,
    float32_t frequency_hz, float32_t magnitude)
{
    int index;
    int limit = *candidate_count;

    for (index = 0; index < limit; index++) {
        if (candidates[index].frequency_hz == frequency_hz) {
            if (magnitude > candidates[index].magnitude) {
                candidates[index].magnitude = magnitude;
            }
            return;
        }
    }

    if (limit < APP_PEAK_CANDIDATE_COUNT) {
        candidates[limit].frequency_hz = frequency_hz;
        candidates[limit].magnitude = magnitude;
        (*candidate_count)++;
    } else if (magnitude <= candidates[limit - 1].magnitude) {
        return;
    } else {
        candidates[limit - 1].frequency_hz = frequency_hz;
        candidates[limit - 1].magnitude = magnitude;
    }

    for (index = *candidate_count - 1; index > 0; index--) {
        signal_frequency_candidate_t temporary;

        if (candidates[index].magnitude <= candidates[index - 1].magnitude) {
            break;
        }
        temporary = candidates[index];
        candidates[index] = candidates[index - 1];
        candidates[index - 1] = temporary;
    }
}

static int signal_snap_to_grid(float32_t frequency_hz,
                               float32_t *snapped_frequency_hz)
{
    float32_t snapped_frequency = floorf(frequency_hz /
                                         APP_FREQUENCY_GRID_HZ + 0.5f) *
                                 APP_FREQUENCY_GRID_HZ;

    if (snapped_frequency < APP_SIGNAL_MIN_HZ ||
        snapped_frequency > APP_SIGNAL_MAX_HZ ||
        fabsf(frequency_hz - snapped_frequency) >
            APP_GRID_LOCK_TOLERANCE_HZ) {
        return 0;
    }
    *snapped_frequency_hz = snapped_frequency;
    return 1;
}

static int signal_build_frequency_candidates(
    const float32_t *magnitude, const signal_peak_t *peaks, int peak_count,
    signal_frequency_candidate_t *candidates)
{
    int candidate_count = 0;
    int index;
    int original_count;

    for (index = 0; index < peak_count; index++) {
        float32_t snapped_frequency;
        float32_t frequency = signal_refine_frequency(magnitude,
                                                       peaks[index].bin);

        if (signal_snap_to_grid(frequency, &snapped_frequency)) {
            signal_insert_frequency_candidate(candidates, &candidate_count,
                                              snapped_frequency,
                                              peaks[index].magnitude);
        }
    }

    original_count = candidate_count;
    for (index = 0; index < original_count; index++) {
        float32_t snapped_frequency;
        float32_t frequency = candidates[index].frequency_hz;
        float32_t collision_frequency = frequency / 3.0f;

        if (signal_snap_to_grid(collision_frequency, &snapped_frequency)) {
            signal_insert_frequency_candidate(candidates, &candidate_count,
                                              snapped_frequency,
                                              candidates[index].magnitude);
        }
        collision_frequency = frequency * 3.0f;
        if (signal_snap_to_grid(collision_frequency, &snapped_frequency)) {
            signal_insert_frequency_candidate(candidates, &candidate_count,
                                              snapped_frequency,
                                              candidates[index].magnitude);
        }
    }
    return candidate_count;
}

static void signal_estimate_fundamental(const float32_t *time_domain,
                                        float32_t frequency_hz,
                                        signal_waveform_t waveform,
                                        signal_component_t *component)
{
    float32_t sin_sum = 0.0f;
    float32_t cos_sum = 0.0f;
    signal_oscillator_t oscillator;
    int index;

    signal_oscillator_init(&oscillator, frequency_hz, 0.0f);
    for (index = 0; index < APP_FFT_LEN; index++) {
        sin_sum += time_domain[index] * oscillator.sine;
        cos_sum += time_domain[index] * oscillator.cosine;
        signal_oscillator_advance(&oscillator, index);
    }

    component->frequency_hz = frequency_hz;
    component->fundamental_amplitude = 2.0f * sqrtf(
        sin_sum * sin_sum + cos_sum * cos_sum) / (float32_t)APP_FFT_LEN;
    component->measured_phase_rad = atan2f(cos_sum, sin_sum);
    component->waveform = waveform;
}

static float32_t signal_joint_residual(const float32_t *time_domain,
                                       const signal_component_t *channel_a,
                                       const signal_component_t *channel_b,
                                       float32_t *model_workspace)
{
    float32_t signal_energy = 0.0f;
    float32_t error_energy = 0.0f;
    int index;

    signal_render_component(channel_a, model_workspace, 0);
    signal_render_component(channel_b, model_workspace, 1);
    for (index = 0; index < APP_FFT_LEN; index++) {
        float32_t error;

        error = time_domain[index] - model_workspace[index];
        signal_energy += time_domain[index] * time_domain[index];
        error_energy += error * error;
    }

    if (signal_energy <= 1.0e-12f) {
        return 1.0e9f;
    }
    return sqrtf(error_energy / signal_energy);
}

static void signal_refine_pair(const float32_t *time_domain,
                               signal_component_t *channel_a,
                               signal_component_t *channel_b,
                               float32_t *workspace)
{
    int iteration;
    int index;

    /*
     * Alternate two residual projections. This separates an A triangle third
     * harmonic from a B fundamental when fB = 3*fA before final scoring.
     */
    for (iteration = 0; iteration < 2; iteration++) {
        signal_render_component(channel_b, workspace, 0);
        for (index = 0; index < APP_FFT_LEN; index++) {
            workspace[index] = time_domain[index] - workspace[index];
        }
        signal_estimate_fundamental(workspace, channel_a->frequency_hz,
                                    channel_a->waveform, channel_a);

        signal_render_component(channel_a, workspace, 0);
        for (index = 0; index < APP_FFT_LEN; index++) {
            workspace[index] = time_domain[index] - workspace[index];
        }
        signal_estimate_fundamental(workspace, channel_b->frequency_hz,
                                    channel_b->waveform, channel_b);
    }
}

int signal_analyze_frame(const u16 *raw_samples,
                         arm_rfft_fast_instance_f32 *fft_instance,
                         float32_t *time_domain,
                         float32_t *fft_input,
                         float32_t *fft_spectrum,
                         float32_t *fft_magnitude,
                         float32_t *model_workspace,
                         signal_analysis_result_t *result)
{
    signal_peak_t peaks[APP_PEAK_CANDIDATE_COUNT];
    signal_frequency_candidate_t
        candidates[APP_PEAK_CANDIDATE_COUNT];
    float32_t best_residual = 1.0e9f;
    int peak_count;
    int candidate_count;
    int first;
    int second;
    int waveform_a;
    int waveform_b;

    if (raw_samples == 0 || fft_instance == 0 || time_domain == 0 ||
        fft_input == 0 || fft_spectrum == 0 || fft_magnitude == 0 ||
        model_workspace == 0 || result == 0) {
        return XST_FAILURE;
    }

    signal_prepare_adc_frame(raw_samples, time_domain, fft_input);
    arm_rfft_fast_f32(fft_instance, fft_input, fft_spectrum, 0);
    signal_compute_magnitudes(fft_spectrum, fft_magnitude);
    peak_count = signal_find_candidate_peaks(fft_magnitude, peaks);
    if (peak_count < 2) {
        return XST_FAILURE;
    }
    candidate_count = signal_build_frequency_candidates(fft_magnitude, peaks,
                                                         peak_count, candidates);
    if (candidate_count < 2) {
        return XST_FAILURE;
    }

    for (first = 0; first < candidate_count - 1; first++) {
        for (second = first + 1; second < candidate_count; second++) {
            float32_t frequency_a = candidates[first].frequency_hz;
            float32_t frequency_b = candidates[second].frequency_hz;
            signal_component_t fundamental_a;
            signal_component_t fundamental_b;

            if (frequency_a > frequency_b) {
                float32_t temporary = frequency_a;
                frequency_a = frequency_b;
                frequency_b = temporary;
            }
            if (frequency_b - frequency_a < APP_MIN_COMPONENT_GAP_HZ) {
                continue;
            }

            signal_estimate_fundamental(time_domain, frequency_a,
                                        SIGNAL_WAVE_SINE, &fundamental_a);
            signal_estimate_fundamental(time_domain, frequency_b,
                                        SIGNAL_WAVE_SINE, &fundamental_b);

            for (waveform_a = SIGNAL_WAVE_SINE;
                 waveform_a <= SIGNAL_WAVE_TRIANGLE; waveform_a++) {
                for (waveform_b = SIGNAL_WAVE_SINE;
                     waveform_b <= SIGNAL_WAVE_TRIANGLE; waveform_b++) {
                    signal_component_t candidate_a;
                    signal_component_t candidate_b;
                    float32_t residual;

                    candidate_a = fundamental_a;
                    candidate_b = fundamental_b;
                    candidate_a.waveform = (signal_waveform_t)waveform_a;
                    candidate_b.waveform = (signal_waveform_t)waveform_b;
                    signal_refine_pair(time_domain, &candidate_a, &candidate_b,
                                       model_workspace);
                    residual = signal_joint_residual(time_domain,
                                                      &candidate_a,
                                                      &candidate_b,
                                                      model_workspace);
                    if (residual < best_residual) {
                        best_residual = residual;
                        result->channel_a = candidate_a;
                        result->channel_b = candidate_b;
                    }
                }
            }
        }
    }

    if (best_residual >= 1.0e8f) {
        return XST_FAILURE;
    }
    result->normalized_residual = best_residual;
    return XST_SUCCESS;
}

void signal_track_result(signal_analysis_result_t *tracked,
                         const signal_analysis_result_t *measurement,
                         float32_t frequency_alpha)
{
    signal_component_t *tracked_channels[2];
    const signal_component_t *measured_channels[2];
    int index;

    if (tracked == 0 || measurement == 0) {
        return;
    }
    if (frequency_alpha < 0.0f) {
        frequency_alpha = 0.0f;
    } else if (frequency_alpha > 1.0f) {
        frequency_alpha = 1.0f;
    }

    tracked_channels[0] = &tracked->channel_a;
    tracked_channels[1] = &tracked->channel_b;
    measured_channels[0] = &measurement->channel_a;
    measured_channels[1] = &measurement->channel_b;
    for (index = 0; index < 2; index++) {
        tracked_channels[index]->frequency_hz += frequency_alpha *
            (measured_channels[index]->frequency_hz -
             tracked_channels[index]->frequency_hz);
        tracked_channels[index]->fundamental_amplitude =
            measured_channels[index]->fundamental_amplitude;
        tracked_channels[index]->measured_phase_rad =
            measured_channels[index]->measured_phase_rad;
        tracked_channels[index]->waveform = measured_channels[index]->waveform;
    }
    tracked->normalized_residual = measurement->normalized_residual;
}

const char *signal_waveform_name(signal_waveform_t waveform)
{
    return (waveform == SIGNAL_WAVE_TRIANGLE) ? "triangle" : "sine";
}

static void signal_generate_test_frame(u16 *raw_samples,
                                       const signal_component_t *channel_a,
                                       const signal_component_t *channel_b,
                                       float32_t *model_workspace)
{
    int index;

    signal_render_component(channel_a, model_workspace, 0);
    signal_render_component(channel_b, model_workspace, 1);
    for (index = 0; index < APP_FFT_LEN; index++) {
        int code = (int)(2048.0f + model_workspace[index] + 0.5f);

        if (code < 0) {
            code = 0;
        } else if (code > 4095) {
            code = 4095;
        }
        raw_samples[index] = (u16)(code << 4);
    }
}

static int signal_run_test_case(signal_waveform_t waveform_a,
                                signal_waveform_t waveform_b,
                                float32_t frequency_a,
                                float32_t frequency_b)
{
    static u16 raw_samples[APP_FFT_LEN];
    static float32_t time_domain[APP_FFT_LEN];
    static float32_t fft_input[APP_FFT_LEN];
    static float32_t fft_spectrum[APP_FFT_LEN];
    static float32_t fft_magnitude[APP_SPEC_LEN];
    static float32_t model_workspace[APP_FFT_LEN];
    arm_rfft_fast_instance_f32 fft_instance;
    signal_component_t channel_a;
    signal_component_t channel_b;
    signal_analysis_result_t result;

    if (arm_rfft_fast_init_f32(&fft_instance, APP_FFT_LEN) !=
        ARM_MATH_SUCCESS) {
        return XST_FAILURE;
    }

    channel_a.frequency_hz = frequency_a;
    channel_a.fundamental_amplitude = 220.0f;
    channel_a.measured_phase_rad = 0.20f;
    channel_a.waveform = waveform_a;
    channel_b.frequency_hz = frequency_b;
    channel_b.fundamental_amplitude = 190.0f;
    channel_b.measured_phase_rad = -0.35f;
    channel_b.waveform = waveform_b;
    signal_generate_test_frame(raw_samples, &channel_a, &channel_b,
                               model_workspace);

    if (signal_analyze_frame(raw_samples, &fft_instance, time_domain,
                             fft_input, fft_spectrum, fft_magnitude,
                             model_workspace, &result) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    if (result.channel_a.waveform != waveform_a ||
        result.channel_b.waveform != waveform_b ||
        fabsf(result.channel_a.frequency_hz - frequency_a) > 750.0f ||
        fabsf(result.channel_b.frequency_hz - frequency_b) > 750.0f ||
        result.normalized_residual > 0.30f) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int signal_run_self_tests(void)
{
    if (signal_run_test_case(SIGNAL_WAVE_SINE, SIGNAL_WAVE_SINE,
                             50000.0f, 55000.0f) != XST_SUCCESS ||
        signal_run_test_case(SIGNAL_WAVE_SINE, SIGNAL_WAVE_TRIANGLE,
                             50000.0f, 55000.0f) != XST_SUCCESS ||
        signal_run_test_case(SIGNAL_WAVE_TRIANGLE, SIGNAL_WAVE_SINE,
                             50000.0f, 55000.0f) != XST_SUCCESS ||
        signal_run_test_case(SIGNAL_WAVE_TRIANGLE, SIGNAL_WAVE_TRIANGLE,
                             50000.0f, 55000.0f) != XST_SUCCESS ||
        signal_run_test_case(SIGNAL_WAVE_TRIANGLE, SIGNAL_WAVE_SINE,
                             25000.0f, 75000.0f) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}
