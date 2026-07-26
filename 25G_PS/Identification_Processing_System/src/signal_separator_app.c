/******************************************************************************
 * signal_separator_app.c
 *
 * One-button contest flow:
 *   ARMED   - KEY2 selects B'-to-A' phase; DDS is stopped at midscale.
 *   LOCKING - one debounced KEY1 press acquires stable source descriptors.
 *   RUNNING - one atomic DDS start commit; no noise-driven frequency updates.
 ******************************************************************************/

#include "signal_separator_app.h"

#include "User/include/app_config.h"
#include "User/include/diagnostics.h"
#include "User/include/signal_api.h"

#include <math.h>

#include "FreeRTOS.h"
#include "task.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "xtime_l.h"

/** 将微秒级轮询间隔换算为至少一个 FreeRTOS tick。 */
static void app_delay_us(unsigned long delay_us)
{
    TickType_t ticks = pdMS_TO_TICKS((delay_us + 999U) / 1000U);

    vTaskDelay((ticks == 0U) ? 1U : ticks);
}

/** 对已锁定的 A/B 频点分别触发 PL IQ 测量并输出诊断。 */
static void app_report_pl_iq(signal_api_t *api,
                             const signal_analysis_result_t *result)
{
    const signal_component_t *channels[2] = {
        &result->channel_a,
        &result->channel_b
    };
    int index;

    for (index = 0; index < 2; ++index) {
        iq_measurement_t measurement;

        signal_error_t error = signal_iq_measure(
            api, channels[index]->frequency_hz, &measurement);

        if (error != SIGNAL_OK) {
            xil_printf("[IQ] WARN: %s for %d Hz\r\n",
                       signal_error_string(error),
                       (int)channels[index]->frequency_hz);
            continue;
        }
        diagnostics_report_iq_measurement((u32)index,
                                          channels[index]->frequency_hz,
                                          &measurement);
    }
}

/** 采集一帧 DMA ADC 数据、完成缓存失效和信号分析，并按需输出耗时诊断。 */
static int app_capture_measurement(signal_api_t *api,
                                   signal_analysis_result_t *measurement,
                                   u32 attempt)
{
    signal_frame_t frame;
    XTime capture_start_time;
    XTime capture_end_time;
    XTime analysis_end_time;
    int analysis_status;

    XTime_GetTime(&capture_start_time);
    if (signal_capture(api, &frame) != SIGNAL_OK) {
        xil_printf("[APP] WARN: capture rejected: %s\r\n",
                   signal_error_string(signal_api_last_error(api)));
        signal_api_print_status(api, "[DMA] S2MM capture failed:");
        return XST_FAILURE;
    }
    XTime_GetTime(&capture_end_time);

    if (diagnostics_should_report(attempt)) {
        diagnostics_report_adc_frame(frame.samples, attempt);
        signal_api_print_status(api, "[DMA] DBG S2MM:");
    }
    analysis_status = (signal_identify_components(api, &frame, measurement) ==
                       SIGNAL_OK) ? XST_SUCCESS : XST_FAILURE;
    XTime_GetTime(&analysis_end_time);
    if (diagnostics_should_report(attempt)) {
        xil_printf("[APP] DBG time[%d]: dma_ms=%d analysis_ms=%d total_ms=%d\r\n",
                   (int)attempt,
                   (int)((capture_end_time - capture_start_time) * 1000U /
                         COUNTS_PER_SECOND),
                   (int)((analysis_end_time - capture_end_time) * 1000U /
                         COUNTS_PER_SECOND),
                   (int)((analysis_end_time - capture_start_time) * 1000U /
                         COUNTS_PER_SECOND));
    }
    return analysis_status;
}

/** 构造固定 50 kHz/100 kHz 正弦配置，用于绕过 ADC 分析的 DDS 诊断模式。 */
static int app_start_forced_dds_test(signal_api_t *api)
{
    signal_analysis_result_t result;
    signal_dds_pair_t pair;

    result.channel_a.frequency_hz = 50000.0f;
    result.channel_a.fundamental_amplitude = 0.0f;
    result.channel_a.measured_phase_rad = 0.0f;
    result.channel_a.waveform = SIGNAL_WAVE_SINE;
    result.channel_b.frequency_hz = 100000.0f;
    result.channel_b.fundamental_amplitude = 0.0f;
    result.channel_b.measured_phase_rad = 0.0f;
    result.channel_b.waveform = SIGNAL_WAVE_SINE;
    result.normalized_residual = 0.0f;
    if (signal_dds_build_pair(api, &result, 0.0f, &pair) != SIGNAL_OK) {
        return XST_FAILURE;
    }
    pair.channel_a.amplitude_code = APP_DIAG_FORCE_DDS_AMPLITUDE;
    pair.channel_b.amplitude_code = APP_DIAG_FORCE_DDS_AMPLITUDE;
    if (signal_dds_apply(api, &pair) != SIGNAL_OK) {
        return XST_FAILURE;
    }
    signal_api_report_dds_snapshot(api, "forced 50k/100k sine");
    return XST_SUCCESS;
}

/** 将频率吸附到应用定义的最近离散频率栅格。 */
static float32_t app_snap_to_frequency_grid(
    float32_t frequency_hz, const signal_profile_t *profile)
{
    return floorf(frequency_hz / profile->frequency_grid_hz + 0.5f) *
           profile->frequency_grid_hz;
}

/** 验证残差和频率容差，规范化锁定候选并要求 A 频率小于 B。 */
static int app_normalize_lock_measurement(
    signal_analysis_result_t *measurement,
    const signal_profile_t *profile)
{
    signal_component_t *channels[2];
    int index;

    if (measurement->normalized_residual > profile->lock_max_residual) {
        return XST_FAILURE;
    }

    channels[0] = &measurement->channel_a;
    channels[1] = &measurement->channel_b;
    for (index = 0; index < 2; index++) {
        float32_t snapped_frequency = app_snap_to_frequency_grid(
            channels[index]->frequency_hz, profile);

        if (snapped_frequency < profile->frequency_min_hz ||
            snapped_frequency > profile->frequency_max_hz ||
            fabsf(channels[index]->frequency_hz - snapped_frequency) >
                profile->grid_lock_tolerance_hz) {
            return XST_FAILURE;
        }
        channels[index]->frequency_hz = snapped_frequency;
    }

    return (measurement->channel_a.frequency_hz <
            measurement->channel_b.frequency_hz) ? XST_SUCCESS : XST_FAILURE;
}

/** 判断两帧锁定候选的 A/B 波形和栅格频率是否完全一致。 */
static int app_same_lock_solution(const signal_analysis_result_t *left,
                                  const signal_analysis_result_t *right)
{
    return left->channel_a.waveform == right->channel_a.waveform &&
           left->channel_b.waveform == right->channel_b.waveform &&
           left->channel_a.frequency_hz == right->channel_a.frequency_hz &&
           left->channel_b.frequency_hz == right->channel_b.frequency_hz;
}

/** 以环绕方式更新用户选择的 B 相对 A 相位，并输出当前设置。 */
static void app_update_phase_setting(float32_t *phase_degrees,
                                     float32_t phase_delta_degrees,
                                     const signal_profile_t *profile)
{
    *phase_degrees += phase_delta_degrees;
    if (*phase_degrees > profile->phase_max_degrees) {
        *phase_degrees = 0.0f;
    } else if (*phase_degrees < 0.0f) {
        *phase_degrees = profile->phase_max_degrees;
    }
    xil_printf("[APP] B-to-A phase setting: %d degrees\r\n",
               (int)*phase_degrees);
}

/** 在 RUNNING 状态处理复位和 B 相位微调，并提交对应 DDS BRAM 配置。 */
static int app_process_running_buttons(signal_api_t *api,
                                       const signal_dds_pair_t *pair,
                                       float32_t *phase_degrees,
                                       const signal_profile_t *profile)
{
    float32_t phase_delta_degrees = 0.0f;

    if (signal_api_take_button(api, SIGNAL_BUTTON_RESET)) {
        if (signal_dds_stop(api) != SIGNAL_OK) {
            return XST_FAILURE;
        }
        *phase_degrees = profile->initial_phase_degrees;
        xil_printf("[APP] RESET: DDS stopped; return to ARMED\r\n");
        return 2;
    }
    if (signal_api_take_button(api, SIGNAL_BUTTON_PHASE_INCREMENT)) {
        phase_delta_degrees = profile->phase_step_degrees;
    } else if (signal_api_take_button(api, SIGNAL_BUTTON_PHASE_DECREMENT)) {
        phase_delta_degrees = -profile->phase_step_degrees;
    }

    if (phase_delta_degrees == 0.0f) {
        return 0;
    }
    if (signal_dds_adjust_b_phase(api, pair, phase_delta_degrees) !=
        SIGNAL_OK) {
        return XST_FAILURE;
    }
    app_update_phase_setting(phase_degrees, phase_delta_degrees, profile);
    return 0;
}

/** 判断从锁定开始到当前时刻是否超过允许的获取时限。 */
static int app_lock_timed_out(XTime start_time,
                              const signal_profile_t *profile)
{
    XTime current_time;

    XTime_GetTime(&current_time);
    return (current_time - start_time) >=
        ((XTime)profile->lock_timeout_seconds * (XTime)COUNTS_PER_SECOND);
}

/** 在 ARMED 状态轮询按键，允许预设相位并等待一次启动按压。 */
static int app_wait_for_start(signal_api_t *api, float32_t *phase_degrees,
                              const signal_profile_t *profile)
{
    while (1) {
        if (signal_api_take_button(api, SIGNAL_BUTTON_RESET)) {
            *phase_degrees = profile->initial_phase_degrees;
            xil_printf("[APP] RESET: DDS is already stopped\r\n");
        } else if (signal_api_take_button(api,
                                          SIGNAL_BUTTON_PHASE_INCREMENT)) {
            app_update_phase_setting(phase_degrees,
                                     profile->phase_step_degrees, profile);
        } else if (signal_api_take_button(api,
                                          SIGNAL_BUTTON_PHASE_DECREMENT)) {
            app_update_phase_setting(phase_degrees,
                                     -profile->phase_step_degrees, profile);
        }
        if (signal_api_take_button(api, SIGNAL_BUTTON_START)) {
            return XST_SUCCESS;
        }
        app_delay_us(APP_BUTTON_POLL_US);
    }
}

/** 运行 ARMED、LOCKING、RUNNING 三态信号分离应用。 */
int signal_separator_run(void)
{
    /* 这些对象跨越下方多轮 ARMED -> LOCKING -> RUNNING 流程，保存硬件/API
     * 状态以及本轮识别、DDS 配置结果。 */
    signal_api_t api;
    signal_status_t status;
    signal_dds_pair_t dds_pair;
    signal_analysis_result_t measurement;
    signal_analysis_result_t locked_result;
    const signal_profile_t *profile = signal_profile_default();
    float32_t phase_degrees;
    signal_error_t error;

    /* 一次性启动阶段：选择参数、按配置执行算法自测并初始化所有硬件服务，
     * 完成后才进入长期状态循环。 */
    xil_printf("\r\n[APP] === PL-aligned dual-channel signal separator ===\r\n");
    if (!signal_profile_is_configured(profile)) {
        xil_printf("[APP] ERROR: default signal profile is not configured\r\n");
        return XST_FAILURE;
    }
    phase_degrees = profile->initial_phase_degrees;
    xil_printf("[APP] DBG build=%s profile=%s\r\n",
               APP_DIAG_BUILD_TAG, profile->name);
    if (APP_ENABLE_STARTUP_SELF_TEST &&
        signal_run_self_tests() != XST_SUCCESS) {
        xil_printf("[APP] ERROR: signal algorithm self-test failed\r\n");
        return XST_FAILURE;
    }
    error = signal_api_init(&api, profile);
    if (error != SIGNAL_OK) {
        xil_printf("[APP] ERROR: initialization failed: %s\r\n",
                   signal_error_string(error));
        return XST_FAILURE;
    }
    (void)signal_api_get_status(&api, &status);
    if (!status.iq_available) {
        xil_printf("[IQ] WARN: PL detector initialization failed\r\n");
    }
    if (!status.monitor_available) {
        xil_printf("[FIFO] WARN: monitor initialization failed; continuing\r\n");
    }
    signal_api_print_status(&api, "[DMA] S2MM initialized:");

    xil_printf("[APP] Init OK. Fs=%d Hz, bin width=%d Hz\r\n",
               (int)APP_SAMPLE_RATE_HZ, (int)APP_BIN_WIDTH_HZ);
    xil_printf("[APP] Self-test passed. DDS is stopped until KEY1.\r\n");
    xil_printf("[APP] DBG key levels: KEY1=%d RESET=%d INC=%d DEC=%d active=%d\r\n",
               (int)signal_api_button_level(&api, SIGNAL_BUTTON_START),
               (int)signal_api_button_level(&api, SIGNAL_BUTTON_RESET),
               (int)signal_api_button_level(
                   &api, SIGNAL_BUTTON_PHASE_INCREMENT),
               (int)signal_api_button_level(
                   &api, SIGNAL_BUTTON_PHASE_DECREMENT),
               (int)APP_BUTTON_ACTIVE_LEVEL);
    signal_api_report_dds_snapshot(&api, "stopped");

    /* 外层循环的每次迭代是一轮完整分离流程；RUNNING 中复位后回到这里，
     * 不重复初始化硬件。 */
    while (1) {
        /* 锁定候选状态属于单轮流程：每次重新启动都会重置超时起点和连续帧
         * 确认历史。 */
        XTime lock_start_time;
        u32 lock_attempt = 0U;
        u32 confirmed_frames = 0U;
        int have_candidate = 0;

        /* ARMED：保持 DDS 停止，轮询相位设置并等待启动按键。 */
        xil_printf("[APP] ARMED: T17/R17 set phase, KEY1 starts one separation run\r\n");
        app_wait_for_start(&api, &phase_degrees, profile);
        XTime_GetTime(&lock_start_time);
        xil_printf("[APP] START accepted: acquiring stable input descriptors\r\n");

        if (APP_DIAG_FORCE_DDS_TEST) {
            xil_printf("[DDS] DBG forced test enabled; ADC/DMA is bypassed\r\n");
            if (app_start_forced_dds_test(&api) != XST_SUCCESS) {
                xil_printf("[DDS] ERROR: forced test commit failed\r\n");
                return XST_FAILURE;
            }
            while (1) {
                app_delay_us(APP_BUTTON_POLL_US);
            }
        }

        error = signal_align_capture(&api);
        if (error != SIGNAL_OK) {
            xil_printf("[DMA] WARN: startup frame alignment failed: %s\r\n",
                       signal_error_string(error));
            signal_api_print_status(&api, "[DMA] alignment failed:");
            continue;
        }
        signal_api_print_status(&api, "[DMA] startup frame aligned:");

        /* LOCKING：重复采集和识别，直到归一化后的频率/波形解连续出现足够
         * 帧数。 */
        while (confirmed_frames < profile->confirm_frames) {
            int analysis_status;
            int lock_status;

            if (app_lock_timed_out(lock_start_time, profile)) {
                xil_printf("[APP] WARN: lock timeout; DDS remains stopped\r\n");
                break;
            }
            if (signal_api_take_button(&api, SIGNAL_BUTTON_RESET)) {
                phase_degrees = profile->initial_phase_degrees;
                xil_printf("[APP] RESET: lock cancelled; return to ARMED\r\n");
                break;
            }
            lock_attempt++;
            analysis_status = app_capture_measurement(&api, &measurement,
                                                      lock_attempt);
            lock_status = (analysis_status == XST_SUCCESS) ?
                app_normalize_lock_measurement(&measurement, profile) :
                XST_FAILURE;
            if (diagnostics_should_report(lock_attempt)) {
                diagnostics_report_analysis(lock_attempt, analysis_status,
                                            lock_status, &measurement);
            }
            if (analysis_status != XST_SUCCESS || lock_status != XST_SUCCESS) {
                have_candidate = 0;
                confirmed_frames = 0U;
                continue;
            }
            if (!have_candidate || !app_same_lock_solution(&locked_result,
                                                            &measurement)) {
                locked_result = measurement;
                have_candidate = 1;
                confirmed_frames = 1U;
            } else {
                locked_result = measurement;
                confirmed_frames++;
            }
        }

        if (confirmed_frames < profile->confirm_frames) {
            continue;
        }

        /* 锁定结果只转换并提交一次；RUNNING 仅处理相位按键和复位，不根据
         * 后续噪声连续重调 DDS。 */
        if (signal_dds_build_pair(&api, &locked_result, phase_degrees,
                                  &dds_pair) != SIGNAL_OK ||
            signal_dds_apply(&api, &dds_pair) != SIGNAL_OK) {
            xil_printf("[DDS] ERROR: initial commit failed\r\n");
            return XST_FAILURE;
        }

        signal_api_report_dds_snapshot(&api, "locked start");
        diagnostics_report_signal_result(&locked_result);
        app_report_pl_iq(&api, &locked_result);
        xil_printf("[APP] RUNNING: T17/R17 adjust B phase; N16 stops and rearms\r\n");
        while (1) {
            int button_status = app_process_running_buttons(
                &api, &dds_pair, &phase_degrees, profile);

            if (button_status == XST_FAILURE) {
                xil_printf("[DDS] ERROR: running button commit failed\r\n");
                return XST_FAILURE;
            }
            if (button_status != 0) {
                break;
            }
            app_delay_us(APP_BUTTON_POLL_US);
        }
    }
}
