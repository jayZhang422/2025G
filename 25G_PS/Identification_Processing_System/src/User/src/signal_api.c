/******************************************************************************
 * signal_api.c
 *
 * Ordering-only facade over the existing PS drivers and identification code.
 * It intentionally adds no signal-processing algorithm.
 ******************************************************************************/

#include "../config/algorithm_config.h"
#include "../config/hardware_config.h"
#include "../include/app_buffers.h"
#include "../include/diagnostics.h"
#include "../include/dma_utils.h"
#include "../include/signal_api.h"

#include <math.h>
#include <string.h>

#include "xil_cache.h"
#include "xstatus.h"

static signal_error_t signal_api_fail(signal_api_t *api,
                                      signal_error_t error)
{
    if (api != NULL) {
        api->last_error = error;
    }
    return error;
}

static int signal_api_snapshot(signal_api_t *api,
                               fifo_monitor_snapshot_t *snapshot)
{
    if (!api->monitor_available) {
        return XST_FAILURE;
    }
    if (fifo_monitor_snapshot(snapshot) != XST_SUCCESS) {
        api->monitor_available = 0;
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

/** 初始化当前 PS 已有硬件对象并用停止提交将 DDS 置于中点。 */
signal_error_t signal_api_init(signal_api_t *api,
                               const signal_profile_t *profile)
{
    dds_channel_config_t stopped;

    if (api == NULL) {
        return SIGNAL_ERR_ARGUMENT;
    }
    if (!signal_profile_is_configured(profile)) {
        return signal_api_fail(api, SIGNAL_ERR_PROFILE_INVALID);
    }

    memset(api, 0, sizeof(*api));
    api->profile = profile;
    api->mode = profile->mode;

    api->iq_available =
        (iq_demodulator_init(&api->iq) == XST_SUCCESS) ? 1 : 0;
    if (dma_init_s2mm(&api->dma, APP_DMA_RX_DEV_ID) != XST_SUCCESS) {
        return signal_api_fail(api, SIGNAL_ERR_DMA_INIT);
    }
    if (arm_rfft_fast_init_f32(&api->fft, APP_FFT_LEN) != ARM_MATH_SUCCESS) {
        return signal_api_fail(api, SIGNAL_ERR_FFT_INIT);
    }
    if (button_input_init(&api->buttons) != XST_SUCCESS) {
        return signal_api_fail(api, SIGNAL_ERR_BUTTON_INIT);
    }

    api->monitor_available =
        (fifo_monitor_init() == XST_SUCCESS) ? 1 : 0;
    if (api->monitor_available) {
        (void)signal_api_snapshot(api, &api->monitor_after);
        if (fifo_monitor_clear_sticky() != XST_SUCCESS) {
            api->monitor_available = 0;
        }
    }

    dds_control_init(&api->dds);
    stopped.waveform = SIGNAL_WAVE_SINE;
    stopped.phase_step = 0U;
    stopped.phase_word = 0U;
    stopped.amplitude_code = 0U;
    if (dds_control_commit(&api->dds, &stopped, &stopped, 0, 0) !=
        XST_SUCCESS) {
        return signal_api_fail(api, SIGNAL_ERR_DDS_COMMIT);
    }

    api->initialized = 1;
    api->last_error = SIGNAL_OK;
    return SIGNAL_OK;
}

/** 切换到一个已完整配置的 profile；不隐式重置硬件。 */
signal_error_t signal_api_set_profile(signal_api_t *api,
                                      const signal_profile_t *profile)
{
    if (api == NULL || !api->initialized) {
        return signal_api_fail(api, SIGNAL_ERR_ARGUMENT);
    }
    if (!signal_profile_is_configured(profile)) {
        return signal_api_fail(api, SIGNAL_ERR_PROFILE_INVALID);
    }
    api->profile = profile;
    api->mode = profile->mode;
    api->last_error = SIGNAL_OK;
    return SIGNAL_OK;
}

/** 在正式采集前重置 S2MM 并丢弃当前不完整 AXIS 帧。 */
signal_error_t signal_align_capture(signal_api_t *api)
{
    if (api == NULL || !api->initialized) {
        return signal_api_fail(api, SIGNAL_ERR_ARGUMENT);
    }

    (void)signal_api_snapshot(api, &api->monitor_before);
    if (dma_align_s2mm(&api->dma, APP_DMA_RX_DEV_ID) != XST_SUCCESS) {
        api->dma_error_count++;
        return signal_api_fail(api, SIGNAL_ERR_FRAME_ALIGN);
    }
    (void)signal_api_snapshot(api, &api->monitor_after);
    api->alignment_count++;
    api->last_error = SIGNAL_OK;
    return SIGNAL_OK;
}

/** 完成 monitor 前快照、Cache 处理、DMA 接收和 monitor 后快照。 */
signal_error_t signal_capture(signal_api_t *api, signal_frame_t *frame)
{
    int have_before;
    int have_after;

    if (api == NULL || frame == NULL || !api->initialized) {
        return signal_api_fail(api, SIGNAL_ERR_ARGUMENT);
    }

    memset(frame, 0, sizeof(*frame));
    have_before = (signal_api_snapshot(api, &api->monitor_before) ==
                   XST_SUCCESS);
    Xil_DCacheFlushRange((UINTPTR)g_adc_raw_buffer, APP_RX_FRAME_BYTES);
    if (dma_capture_frame(&api->dma, APP_DMA_RX_DEV_ID, g_adc_raw_buffer,
                          APP_RX_FRAME_BYTES) != XST_SUCCESS) {
        api->dma_error_count++;
        (void)signal_api_snapshot(api, &api->monitor_after);
        return signal_api_fail(api, SIGNAL_ERR_DMA_CAPTURE);
    }
    Xil_DCacheInvalidateRange((UINTPTR)g_adc_raw_buffer,
                              APP_RX_FRAME_BYTES);
    have_after = (signal_api_snapshot(api, &api->monitor_after) ==
                  XST_SUCCESS);

    api->last_dma_length_bytes = dma_last_s2mm_length_bytes(&api->dma);
    api->capture_count++;
    frame->samples = g_adc_raw_buffer;
    frame->sample_count = api->last_dma_length_bytes / sizeof(u16);
    frame->sample_rate_hz = APP_SAMPLE_RATE_HZ;
    frame->frame_sequence = api->capture_count;

    if (api->last_dma_length_bytes != APP_RX_FRAME_BYTES) {
        frame->quality_flags |= SIGNAL_FRAME_QUALITY_SHORT;
    }
    if (have_before && have_after &&
        api->monitor_after.blocked_high_watermark_count >
            api->monitor_before.blocked_high_watermark_count) {
        frame->quality_flags |= SIGNAL_FRAME_QUALITY_FIFO_BLOCKED;
    }
    if (have_before && have_after &&
        api->monitor_after.blocked_reset_count >
            api->monitor_before.blocked_reset_count) {
        frame->quality_flags |= SIGNAL_FRAME_QUALITY_RESET_BLOCKED;
    }

    frame->frame_valid =
        ((frame->quality_flags & SIGNAL_FRAME_QUALITY_SHORT) == 0U);
    if (!frame->frame_valid) {
        api->bad_frame_count++;
        return signal_api_fail(api, SIGNAL_ERR_FRAME_INVALID);
    }

    api->last_error = SIGNAL_OK;
    return SIGNAL_OK;
}

/** 用现有 4096 点双分量识别器处理一帧有效采样。 */
signal_error_t signal_identify_components(
    signal_api_t *api, const signal_frame_t *frame,
    signal_analysis_result_t *result)
{
    if (api == NULL || frame == NULL || result == NULL ||
        !api->initialized || !frame->frame_valid ||
        frame->sample_count != APP_ADC_FRAME_SAMPLES) {
        return signal_api_fail(api, SIGNAL_ERR_FRAME_INVALID);
    }

    if (signal_analyze_frame(frame->samples, &api->fft,
                             g_time_domain_buffer, g_fft_input_buffer,
                             g_fft_spectrum_buffer, g_fft_magnitude_buffer,
                             g_model_buffer, api->profile, result) !=
        XST_SUCCESS) {
        return signal_api_fail(api, SIGNAL_ERR_ANALYSIS);
    }
    api->last_error = SIGNAL_OK;
    return SIGNAL_OK;
}

/** 用现有 PL IQ 驱动测量一个已知频点。 */
signal_error_t signal_iq_measure(signal_api_t *api, float32_t frequency_hz,
                                 iq_measurement_t *measurement)
{
    if (api == NULL || measurement == NULL || frequency_hz <= 0.0f ||
        !api->initialized) {
        return signal_api_fail(api, SIGNAL_ERR_ARGUMENT);
    }
    if (!api->iq_available) {
        return signal_api_fail(api, SIGNAL_ERR_IQ_UNAVAILABLE);
    }
    if (iq_demodulator_measure(&api->iq, frequency_hz, measurement) !=
        XST_SUCCESS) {
        return signal_api_fail(api, SIGNAL_ERR_IQ_MEASURE);
    }
    api->last_error = SIGNAL_OK;
    return SIGNAL_OK;
}

/** 从识别结果和 profile 构造保持现有频率、幅度和相位语义的 DDS 对。 */
signal_error_t signal_dds_build_pair(
    signal_api_t *api, const signal_analysis_result_t *result,
    float32_t b_to_a_phase_degrees, signal_dds_pair_t *pair)
{
    float32_t ratio;
    u32 integer_ratio;

    if (api == NULL || result == NULL || pair == NULL ||
        !api->initialized || api->profile == NULL ||
        result->channel_a.frequency_hz <= 0.0f ||
        result->channel_b.frequency_hz <= 0.0f) {
        return signal_api_fail(api, SIGNAL_ERR_ARGUMENT);
    }

    dds_control_from_component(&result->channel_a, 0.0f,
                               api->profile->dds_amplitude_code,
                               &pair->channel_a);
    dds_control_from_component(
        &result->channel_b,
        b_to_a_phase_degrees + APP_DDS_B_PHASE_COMPENSATION_DEGREES,
        api->profile->dds_amplitude_code, &pair->channel_b);

    ratio = result->channel_b.frequency_hz /
            result->channel_a.frequency_hz;
    integer_ratio = (u32)(ratio + 0.5f);
    if (integer_ratio > 1U &&
        fabsf(ratio - (float32_t)integer_ratio) < 0.001f) {
        pair->channel_b.phase_step =
            pair->channel_a.phase_step * integer_ratio;
    }
    pair->phase_reload = 1;
    pair->run = 1;
    api->last_error = SIGNAL_OK;
    return SIGNAL_OK;
}

/** 原子提交已构造的 A/B DDS 配置。 */
signal_error_t signal_dds_apply(signal_api_t *api,
                                const signal_dds_pair_t *pair)
{
    if (api == NULL || pair == NULL || !api->initialized) {
        return signal_api_fail(api, SIGNAL_ERR_ARGUMENT);
    }
    if (dds_control_commit(&api->dds, &pair->channel_a, &pair->channel_b,
                           pair->phase_reload, pair->run) != XST_SUCCESS) {
        return signal_api_fail(api, SIGNAL_ERR_DDS_COMMIT);
    }
    api->last_error = SIGNAL_OK;
    return SIGNAL_OK;
}

/** 原子停止双 DDS，使 PL 输出中点。 */
signal_error_t signal_dds_stop(signal_api_t *api)
{
    dds_channel_config_t stopped;

    if (api == NULL || !api->initialized) {
        return signal_api_fail(api, SIGNAL_ERR_ARGUMENT);
    }
    stopped.waveform = SIGNAL_WAVE_SINE;
    stopped.phase_step = 0U;
    stopped.phase_word = 0U;
    stopped.amplitude_code = 0U;
    if (dds_control_commit(&api->dds, &stopped, &stopped, 0, 0) !=
        XST_SUCCESS) {
        return signal_api_fail(api, SIGNAL_ERR_DDS_COMMIT);
    }
    api->last_error = SIGNAL_OK;
    return SIGNAL_OK;
}

/** 复用现有 PL B 相位增量提交，不重载 A/B 相位。 */
signal_error_t signal_dds_adjust_b_phase(signal_api_t *api,
                                         const signal_dds_pair_t *pair,
                                         float32_t phase_delta_degrees)
{
    if (api == NULL || pair == NULL || !api->initialized) {
        return signal_api_fail(api, SIGNAL_ERR_ARGUMENT);
    }
    if (dds_control_adjust_b_phase(&api->dds, &pair->channel_a,
                                   &pair->channel_b,
                                   phase_delta_degrees) != XST_SUCCESS) {
        return signal_api_fail(api, SIGNAL_ERR_DDS_COMMIT);
    }
    api->last_error = SIGNAL_OK;
    return SIGNAL_OK;
}

int signal_api_take_button(signal_api_t *api, signal_button_t button)
{
    if (api == NULL || !api->initialized) {
        return 0;
    }
    switch (button) {
    case SIGNAL_BUTTON_START:
        return button_input_take_start_press(&api->buttons);
    case SIGNAL_BUTTON_RESET:
        return button_input_take_reset_press(&api->buttons);
    case SIGNAL_BUTTON_PHASE_INCREMENT:
        return button_input_take_phase_increment_press(&api->buttons);
    case SIGNAL_BUTTON_PHASE_DECREMENT:
        return button_input_take_phase_decrement_press(&api->buttons);
    default:
        return 0;
    }
}

u32 signal_api_button_level(const signal_api_t *api, signal_button_t button)
{
    if (api == NULL || !api->initialized) {
        return 0U;
    }
    switch (button) {
    case SIGNAL_BUTTON_START:
        return button_input_read_start_level(&api->buttons);
    case SIGNAL_BUTTON_RESET:
        return button_input_read_reset_level(&api->buttons);
    case SIGNAL_BUTTON_PHASE_INCREMENT:
        return button_input_read_phase_increment_level(&api->buttons);
    case SIGNAL_BUTTON_PHASE_DECREMENT:
        return button_input_read_phase_decrement_level(&api->buttons);
    default:
        return 0U;
    }
}

signal_error_t signal_api_get_status(signal_api_t *api,
                                     signal_status_t *status)
{
    if (api == NULL || status == NULL || !api->initialized) {
        return signal_api_fail(api, SIGNAL_ERR_ARGUMENT);
    }
    (void)signal_api_snapshot(api, &api->monitor_after);
    status->last_error = api->last_error;
    status->capture_count = api->capture_count;
    status->dma_error_count = api->dma_error_count;
    status->bad_frame_count = api->bad_frame_count;
    status->alignment_count = api->alignment_count;
    status->last_dma_length_bytes = api->last_dma_length_bytes;
    status->iq_available = api->iq_available;
    status->monitor_available = api->monitor_available;
    status->monitor_before = api->monitor_before;
    status->monitor_after = api->monitor_after;
    return SIGNAL_OK;
}

signal_error_t signal_recover(signal_api_t *api)
{
    return signal_align_capture(api);
}

signal_error_t signal_api_last_error(const signal_api_t *api)
{
    return (api == NULL) ? SIGNAL_ERR_ARGUMENT : api->last_error;
}

void signal_api_print_status(signal_api_t *api, const char *tag)
{
    if (api == NULL || !api->initialized) {
        return;
    }
    dma_dump_s2mm_regs((tag != NULL) ? tag : "[DMA] S2MM:", &api->dma);
    if (signal_api_snapshot(api, &api->monitor_after) == XST_SUCCESS) {
        fifo_monitor_print((tag != NULL) ? tag : "snapshot",
                           &api->monitor_after);
    }
}

void signal_api_report_dds_snapshot(const signal_api_t *api, const char *tag)
{
    if (api != NULL && api->initialized) {
        diagnostics_report_dds_snapshot(tag, &api->dds);
    }
}

const char *signal_error_string(signal_error_t error)
{
    switch (error) {
    case SIGNAL_OK: return "ok";
    case SIGNAL_ERR_ARGUMENT: return "invalid argument or API state";
    case SIGNAL_ERR_PROFILE_INVALID: return "profile is not configured";
    case SIGNAL_ERR_DMA_INIT: return "DMA initialization failed";
    case SIGNAL_ERR_FFT_INIT: return "FFT initialization failed";
    case SIGNAL_ERR_BUTTON_INIT: return "button initialization failed";
    case SIGNAL_ERR_FRAME_ALIGN: return "frame alignment failed";
    case SIGNAL_ERR_DMA_CAPTURE: return "DMA capture failed";
    case SIGNAL_ERR_FRAME_INVALID: return "captured frame is incomplete";
    case SIGNAL_ERR_ANALYSIS: return "signal identification failed";
    case SIGNAL_ERR_IQ_UNAVAILABLE: return "PL IQ detector is unavailable";
    case SIGNAL_ERR_IQ_MEASURE: return "PL IQ measurement failed";
    case SIGNAL_ERR_DDS_COMMIT: return "DDS atomic commit failed";
    default: return "unknown signal error";
    }
}
