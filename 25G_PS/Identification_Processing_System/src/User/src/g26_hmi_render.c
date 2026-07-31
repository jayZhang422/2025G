#include "../include/g26_hmi_render.h"

#include <limits.h>

#define G26_HMI_MAX_FREQUENCY_HZ 500000.0f
#define G26_HMI_RENDER_EPSILON   1.0e-12f

void g26_hmi_session_init(g26_hmi_session_t *session)
{
    if (session == 0) {
        return;
    }
    session->pending_generation = 0U;
    session->request_page = 0U;
    session->active_page = 0U;
    session->period_count = 1U;
    session->show_time_parameters = 0;
    session->show_amplitudes = 0;
    session->snapshot_valid = 0;
}

void g26_hmi_session_start(g26_hmi_session_t *session,
                           uint32_t generation,
                           uint8_t page)
{
    if (session == 0) {
        return;
    }
    session->pending_generation = generation;
    session->request_page = page;
    session->active_page = page;
    session->period_count = 1U;
    session->show_time_parameters = 0;
    session->show_amplitudes = 0;
    session->snapshot_valid = 0;
}

void g26_hmi_session_stop(g26_hmi_session_t *session)
{
    if (session == 0) {
        return;
    }
    session->pending_generation = 0U;
    session->request_page = 0U;
    session->active_page = 0U;
    session->period_count = 1U;
    session->show_time_parameters = 0;
    session->show_amplitudes = 0;
    session->snapshot_valid = 0;
}

int g26_hmi_session_is_pending(const g26_hmi_session_t *session)
{
    return session != 0 && session->pending_generation != 0U;
}

int g26_hmi_session_accept_completion(const g26_hmi_session_t *session,
                                      uint32_t generation,
                                      uint8_t source_page)
{
    return session != 0 && session->pending_generation != 0U &&
        generation == session->pending_generation &&
        source_page == session->request_page;
}

void g26_hmi_session_publish_snapshot(g26_hmi_session_t *session)
{
    if (session == 0) {
        return;
    }
    session->pending_generation = 0U;
    session->request_page = 0U;
    session->snapshot_valid = 1;
}

static float g26_hmi_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static uint32_t g26_hmi_round_nonnegative(float value)
{
    if (value <= 0.0f) {
        return 0U;
    }
    if (value >= (float)UINT_MAX) {
        return UINT_MAX;
    }
    return (uint32_t)(value + 0.5f);
}

uint32_t g26_hmi_frequency_tenths_khz(float frequency_hz)
{
    return g26_hmi_round_nonnegative(
        g26_hmi_clamp(frequency_hz, 0.0f, G26_HMI_MAX_FREQUENCY_HZ) /
        100.0f);
}

uint32_t g26_hmi_frequency_integer_khz(float frequency_hz)
{
    return g26_hmi_round_nonnegative(
        g26_hmi_clamp(frequency_hz, 0.0f, G26_HMI_MAX_FREQUENCY_HZ) /
        1000.0f);
}

uint32_t g26_hmi_amplitude_tenths_mv(float amplitude_mv)
{
    return g26_hmi_round_nonnegative(
        g26_hmi_clamp(amplitude_mv, 0.0f, (float)UINT_MAX / 10.0f) *
        10.0f);
}

uint32_t g26_hmi_millivolts_thousandths(float value_mv)
{
    return g26_hmi_round_nonnegative(
        g26_hmi_clamp(value_mv, 0.0f, (float)UINT_MAX / 1000.0f) *
        1000.0f);
}

uint32_t g26_hmi_millivolts_integer(float value_mv)
{
    return g26_hmi_round_nonnegative(
        g26_hmi_clamp(value_mv, 0.0f, (float)UINT_MAX));
}

int g26_hmi_spectrum_x(float frequency_hz)
{
    float normalized = g26_hmi_clamp(
        frequency_hz, 0.0f, G26_HMI_MAX_FREQUENCY_HZ) /
        G26_HMI_MAX_FREQUENCY_HZ;
    float position = (float)G26_HMI_SPECTRUM_X0 +
        normalized *
        (float)(G26_HMI_SPECTRUM_X500 - G26_HMI_SPECTRUM_X0);

    return (int)(position + 0.5f);
}

int g26_hmi_spectrum_top(float amplitude_mv, float maximum_amplitude_mv)
{
    static const int calibrated_ticks[] = {
        G26_HMI_SPECTRUM_Y0,
        G26_HMI_SPECTRUM_Y025,
        G26_HMI_SPECTRUM_Y050,
        G26_HMI_SPECTRUM_Y075,
        G26_HMI_SPECTRUM_Y1
    };
    float normalized;
    float tick_position;
    int segment;
    float position;
    int rounded;

    if (maximum_amplitude_mv <= G26_HMI_RENDER_EPSILON) {
        return G26_HMI_SPECTRUM_Y0;
    }
    normalized = g26_hmi_clamp(
        amplitude_mv / maximum_amplitude_mv, 0.0f, 1.0f);
    tick_position = normalized * 4.0f;
    segment = (tick_position >= 4.0f) ? 3 : (int)tick_position;
    position = (float)calibrated_ticks[segment] +
        (tick_position - (float)segment) *
        (float)(calibrated_ticks[segment + 1] -
                calibrated_ticks[segment]);
    rounded = (int)(position + 0.5f);
    if (amplitude_mv > G26_HMI_RENDER_EPSILON &&
        rounded >= G26_HMI_SPECTRUM_Y0) {
        return G26_HMI_SPECTRUM_Y0 - 1;
    }
    return rounded;
}

size_t g26_hmi_waveform_resample(const float *source,
                                 size_t source_count,
                                 uint8_t *destination,
                                 size_t destination_capacity,
                                 size_t requested_count)
{
    float maximum_absolute = 0.0f;
    size_t output_count = requested_count;
    size_t index;

    if (source == 0 || destination == 0 || source_count < 2U ||
        destination_capacity == 0U || requested_count < 2U) {
        return 0U;
    }
    if (output_count > destination_capacity) {
        output_count = destination_capacity;
    }
    if (output_count > G26_HMI_WAVEFORM_MAX_POINTS) {
        output_count = G26_HMI_WAVEFORM_MAX_POINTS;
    }

    for (index = 0U; index < source_count; index++) {
        float magnitude = (source[index] < 0.0f) ?
            -source[index] : source[index];

        if (magnitude > maximum_absolute) {
            maximum_absolute = magnitude;
        }
    }
    if (maximum_absolute <= G26_HMI_RENDER_EPSILON) {
        for (index = 0U; index < output_count; index++) {
            destination[index] = 128U;
        }
        return output_count;
    }

    for (index = 0U; index < output_count; index++) {
        size_t numerator = index * (source_count - 1U);
        size_t denominator = output_count - 1U;
        size_t left = numerator / denominator;
        size_t remainder = numerator % denominator;
        size_t right = (left + 1U < source_count) ? left + 1U : left;
        float fraction = (float)remainder / (float)denominator;
        float value = source[left] +
            (source[right] - source[left]) * fraction;
        float scaled = 127.5f + 127.5f *
            g26_hmi_clamp(value / maximum_absolute, -1.0f, 1.0f);

        destination[index] = (uint8_t)(scaled + 0.5f);
    }
    return output_count;
}
