/******************************************************************************
 * signal_separator_app.c
 *
 * One-button contest flow:
 *   ARMED   - KEY2 selects B'-to-A' phase; DDS is stopped at midscale.
 *   LOCKING - one debounced KEY1 press acquires stable source descriptors.
 *   RUNNING - one atomic DDS start commit; no noise-driven frequency updates.
 ******************************************************************************/

#include "signal_separator_app.h"

#include "User/include/app_buffers.h"
#include "User/include/app_config.h"
#include "User/include/button_input.h"
#include "User/include/diagnostics.h"
#include "User/include/dds_control.h"
#include "User/include/dma_utils.h"
#include "User/include/fifo_monitor.h"
#include "User/include/iq_demodulator.h"
#include "User/include/signal_processing.h"

#include <math.h>

#include "FreeRTOS.h"
#include "arm_math.h"
#include "task.h"
#include "xaxidma.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "xtime_l.h"

static XAxiDma g_dma_rx;
static arm_rfft_fast_instance_f32 g_fft_instance;
static iq_demodulator_t g_iq_detector;

/** 将微秒级轮询间隔换算为至少一个 FreeRTOS tick。 */
static void app_delay_us(unsigned long delay_us)
{
    TickType_t ticks = pdMS_TO_TICKS((delay_us + 999U) / 1000U);

    vTaskDelay((ticks == 0U) ? 1U : ticks);
}

/** 请求并打印一次 FIFO/AXIS 计数器快照。 */
static void app_report_fifo_monitor(const char *tag)
{
    fifo_monitor_snapshot_t snapshot;
    int status = fifo_monitor_snapshot(&snapshot);

    if (status == XST_SUCCESS) {
        fifo_monitor_print(tag, &snapshot);
    } else {
        xil_printf("[FIFO] WARN: monitor snapshot failed (%d)\r\n", status);
    }
}

/** 对已锁定的 A/B 频点分别触发 PL IQ 测量并输出诊断。 */
static void app_report_pl_iq(const signal_analysis_result_t *result)
{
    const signal_component_t *channels[2] = {
        &result->channel_a,
        &result->channel_b
    };
    int index;

    for (index = 0; index < 2; ++index) {
        iq_measurement_t measurement;

        if (iq_demodulator_measure(&g_iq_detector,
                                   channels[index]->frequency_hz,
                                   &measurement) != XST_SUCCESS) {
            xil_printf("[IQ] WARN: PL measurement failed for %d Hz\r\n",
                       (int)channels[index]->frequency_hz);
            continue;
        }
        diagnostics_report_iq_measurement((u32)index,
                                          channels[index]->frequency_hz,
                                          &measurement);
    }
}

/** 采集一帧 DMA ADC 数据、完成缓存失效和信号分析，并按需输出耗时诊断。 */
static int app_capture_measurement(signal_analysis_result_t *measurement,
                                   u32 attempt)
{
    XTime capture_start_time;
    XTime capture_end_time;
    XTime analysis_end_time;
    int analysis_status;

    XTime_GetTime(&capture_start_time);
    Xil_DCacheFlushRange((UINTPTR)g_adc_raw_buffer,
                         APP_RX_FRAME_BYTES);
    if (dma_capture_frame(&g_dma_rx, APP_DMA_RX_DEV_ID, g_adc_raw_buffer,
                          APP_RX_FRAME_BYTES) != XST_SUCCESS) {
        dma_dump_s2mm_regs("[DMA] S2MM capture failed:", &g_dma_rx);
        app_report_fifo_monitor("after DMA failure");
        return XST_FAILURE;
    }
    XTime_GetTime(&capture_end_time);

    Xil_DCacheInvalidateRange((UINTPTR)g_adc_raw_buffer,
                              APP_RX_FRAME_BYTES);
    if (diagnostics_should_report(attempt)) {
        diagnostics_report_adc_frame(g_adc_raw_buffer, attempt);
        dma_dump_s2mm_regs("[DMA] DBG S2MM:", &g_dma_rx);
    }
    analysis_status = signal_analyze_frame(g_adc_raw_buffer, &g_fft_instance,
                                           g_time_domain_buffer,
                                           g_fft_input_buffer,
                                           g_fft_spectrum_buffer,
                                           g_fft_magnitude_buffer,
                                           g_model_buffer, measurement);
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
static int app_start_forced_dds_test(dds_control_t *dds_control)
{
    signal_component_t channel_a;
    signal_component_t channel_b;
    dds_channel_config_t dds_a;
    dds_channel_config_t dds_b;

    channel_a.frequency_hz = 50000.0f;
    channel_a.fundamental_amplitude = 0.0f;
    channel_a.measured_phase_rad = 0.0f;
    channel_a.waveform = SIGNAL_WAVE_SINE;
    channel_b.frequency_hz = 100000.0f;
    channel_b.fundamental_amplitude = 0.0f;
    channel_b.measured_phase_rad = 0.0f;
    channel_b.waveform = SIGNAL_WAVE_SINE;
    dds_control_from_component(&channel_a, 0.0f, &dds_a);
    dds_control_from_component(&channel_b,
                               APP_DDS_B_PHASE_COMPENSATION_DEGREES,
                               &dds_b);
    dds_a.amplitude_code = APP_DIAG_FORCE_DDS_AMPLITUDE;
    dds_b.amplitude_code = APP_DIAG_FORCE_DDS_AMPLITUDE;

    if (dds_control_commit(dds_control, &dds_a, &dds_b, 1, 1) !=
        XST_SUCCESS) {
        return XST_FAILURE;
    }
    diagnostics_report_dds_snapshot("forced 50k/100k sine", dds_control);
    return XST_SUCCESS;
}

/** 将频率吸附到应用定义的最近离散频率栅格。 */
static float32_t app_snap_to_frequency_grid(float32_t frequency_hz)
{
    return floorf(frequency_hz / APP_FREQUENCY_GRID_HZ + 0.5f) *
           APP_FREQUENCY_GRID_HZ;
}

/** 验证残差和频率容差，规范化锁定候选并要求 A 频率小于 B。 */
static int app_normalize_lock_measurement(signal_analysis_result_t *measurement)
{
    signal_component_t *channels[2];
    int index;

    if (measurement->normalized_residual > APP_LOCK_MAX_RESIDUAL) {
        return XST_FAILURE;
    }

    channels[0] = &measurement->channel_a;
    channels[1] = &measurement->channel_b;
    for (index = 0; index < 2; index++) {
        float32_t snapped_frequency = app_snap_to_frequency_grid(
            channels[index]->frequency_hz);

        if (snapped_frequency < APP_SIGNAL_MIN_HZ ||
            snapped_frequency > APP_SIGNAL_MAX_HZ ||
            fabsf(channels[index]->frequency_hz - snapped_frequency) >
                APP_GRID_LOCK_TOLERANCE_HZ) {
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

/** 当 B/A 是精确整数比时，由 A 步进推导 B 步进以保持数字频率比例。 */
static void app_preserve_integer_frequency_ratio(
    const signal_analysis_result_t *result, dds_channel_config_t *dds_a,
    dds_channel_config_t *dds_b)
{
    float32_t ratio = result->channel_b.frequency_hz /
                      result->channel_a.frequency_hz;
    u32 integer_ratio = (u32)(ratio + 0.5f);

    if (integer_ratio > 1U &&
        fabsf(ratio - (float32_t)integer_ratio) < 0.001f) {
        dds_b->phase_step = dds_a->phase_step * integer_ratio;
    }
}

/** 以环绕方式更新用户选择的 B 相对 A 相位，并输出当前设置。 */
static void app_update_phase_setting(float32_t *phase_degrees,
                                     float32_t phase_delta_degrees)
{
    *phase_degrees += phase_delta_degrees;
    if (*phase_degrees > APP_PHASE_MAX_DEGREES) {
        *phase_degrees = 0.0f;
    } else if (*phase_degrees < 0.0f) {
        *phase_degrees = APP_PHASE_MAX_DEGREES;
    }
    xil_printf("[APP] B-to-A phase setting: %d degrees\r\n",
               (int)*phase_degrees);
}

/** 在 RUNNING 状态处理复位和 B 相位微调，并提交对应 DDS BRAM 配置。 */
static int app_process_running_buttons(button_input_t *buttons,
                                       dds_control_t *dds_control,
                                       const dds_channel_config_t *dds_a,
                                       const dds_channel_config_t *dds_b,
                                       const dds_channel_config_t *stopped,
                                       float32_t *phase_degrees)
{
    float32_t phase_delta_degrees = 0.0f;

    if (button_input_take_reset_press(buttons)) {
        if (dds_control_commit(dds_control, stopped, stopped, 0, 0) !=
            XST_SUCCESS) {
            return XST_FAILURE;
        }
        *phase_degrees = APP_B_TO_A_PHASE_DEGREES;
        xil_printf("[APP] RESET: DDS stopped; return to ARMED\r\n");
        return 2;
    }
    if (button_input_take_phase_increment_press(buttons)) {
        phase_delta_degrees = APP_PHASE_STEP_DEGREES;
    } else if (button_input_take_phase_decrement_press(buttons)) {
        phase_delta_degrees = -APP_PHASE_STEP_DEGREES;
    }

    if (phase_delta_degrees == 0.0f) {
        return 0;
    }
    if (dds_control_adjust_b_phase(dds_control, dds_a, dds_b,
                                   phase_delta_degrees) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    app_update_phase_setting(phase_degrees, phase_delta_degrees);
    return 0;
}

/** 判断从锁定开始到当前时刻是否超过允许的获取时限。 */
static int app_lock_timed_out(XTime start_time)
{
    XTime current_time;

    XTime_GetTime(&current_time);
    return (current_time - start_time) >=
        ((XTime)APP_LOCK_TIMEOUT_SECONDS * (XTime)COUNTS_PER_SECOND);
}

/** 在 ARMED 状态轮询按键，允许预设相位并等待一次启动按压。 */
static int app_wait_for_start(button_input_t *buttons,
                              float32_t *phase_degrees)
{
    while (1) {
        if (button_input_take_reset_press(buttons)) {
            *phase_degrees = APP_B_TO_A_PHASE_DEGREES;
            xil_printf("[APP] RESET: DDS is already stopped\r\n");
        } else if (button_input_take_phase_increment_press(buttons)) {
            app_update_phase_setting(phase_degrees, APP_PHASE_STEP_DEGREES);
        } else if (button_input_take_phase_decrement_press(buttons)) {
            app_update_phase_setting(phase_degrees, -APP_PHASE_STEP_DEGREES);
        }
        if (button_input_take_start_press(buttons)) {
            return XST_SUCCESS;
        }
        app_delay_us(APP_BUTTON_POLL_US);
    }
}

/** 运行 ARMED、LOCKING、RUNNING 三态信号分离应用。 */
int signal_separator_run(void)
{
    dds_control_t dds_control;
    dds_channel_config_t dds_a;
    dds_channel_config_t dds_b;
    dds_channel_config_t stopped_channel;
    signal_analysis_result_t measurement;
    signal_analysis_result_t locked_result;
    button_input_t buttons;
    float32_t phase_degrees = APP_B_TO_A_PHASE_DEGREES;

    if (iq_demodulator_init(&g_iq_detector) != XST_SUCCESS) {
        xil_printf("[IQ] WARN: PL detector initialization failed\r\n");
    }
    xil_printf("\r\n[APP] === PL-aligned dual-channel signal separator ===\r\n");
    xil_printf("[APP] DBG build=%s\r\n", APP_DIAG_BUILD_TAG);
    if (APP_ENABLE_STARTUP_SELF_TEST &&
        signal_run_self_tests() != XST_SUCCESS) {
        xil_printf("[APP] ERROR: signal algorithm self-test failed\r\n");
        return XST_FAILURE;
    }
    if (dma_init_s2mm(&g_dma_rx, APP_DMA_RX_DEV_ID) != XST_SUCCESS ||
        arm_rfft_fast_init_f32(&g_fft_instance, APP_FFT_LEN) !=
            ARM_MATH_SUCCESS ||
        button_input_init(&buttons) != XST_SUCCESS) {
        xil_printf("[APP] ERROR: application initialization failed\r\n");
        return XST_FAILURE;
    }
    dma_dump_s2mm_regs("[DMA] S2MM initialized:", &g_dma_rx);
    if (fifo_monitor_init() == XST_SUCCESS) {
        app_report_fifo_monitor("initialized");
        if (fifo_monitor_clear_sticky() != XST_SUCCESS) {
            xil_printf("[FIFO] WARN: monitor sticky clear failed\r\n");
        }
    } else {
        xil_printf("[FIFO] WARN: monitor initialization failed; continuing\r\n");
    }

    dds_control_init(&dds_control);
    stopped_channel.waveform = SIGNAL_WAVE_SINE;
    stopped_channel.phase_step = 0U;
    stopped_channel.phase_word = 0U;
    stopped_channel.amplitude_code = 0U;
    if (dds_control_commit(&dds_control, &stopped_channel, &stopped_channel,
                           0, 0) != XST_SUCCESS) {
        xil_printf("[DDS] ERROR: initial stop commit failed\r\n");
        return XST_FAILURE;
    }

    xil_printf("[APP] Init OK. Fs=%d Hz, bin width=%d Hz\r\n",
               (int)APP_SAMPLE_RATE_HZ, (int)APP_BIN_WIDTH_HZ);
    xil_printf("[APP] Self-test passed. DDS is stopped until KEY1.\r\n");
    xil_printf("[APP] DBG key levels: KEY1=%d RESET=%d INC=%d DEC=%d active=%d\r\n",
               (int)button_input_read_start_level(&buttons),
               (int)button_input_read_reset_level(&buttons),
               (int)button_input_read_phase_increment_level(&buttons),
               (int)button_input_read_phase_decrement_level(&buttons),
               (int)APP_BUTTON_ACTIVE_LEVEL);
    diagnostics_report_dds_snapshot("stopped", &dds_control);

    while (1) {
        XTime lock_start_time;
        u32 lock_attempt = 0U;
        u32 confirmed_frames = 0U;
        int have_candidate = 0;

        xil_printf("[APP] ARMED: T17/R17 set phase, KEY1 starts one separation run\r\n");
        app_wait_for_start(&buttons, &phase_degrees);
        XTime_GetTime(&lock_start_time);
        xil_printf("[APP] START accepted: acquiring stable input descriptors\r\n");

        if (APP_DIAG_FORCE_DDS_TEST) {
            xil_printf("[DDS] DBG forced test enabled; ADC/DMA is bypassed\r\n");
            if (app_start_forced_dds_test(&dds_control) != XST_SUCCESS) {
                xil_printf("[DDS] ERROR: forced test commit failed\r\n");
                return XST_FAILURE;
            }
            while (1) {
                app_delay_us(APP_BUTTON_POLL_US);
            }
        }

        while (confirmed_frames < APP_LOCK_CONFIRM_FRAMES) {
            int analysis_status;
            int lock_status;

            if (app_lock_timed_out(lock_start_time)) {
                xil_printf("[APP] WARN: lock timeout; DDS remains stopped\r\n");
                break;
            }
            if (button_input_take_reset_press(&buttons)) {
                phase_degrees = APP_B_TO_A_PHASE_DEGREES;
                xil_printf("[APP] RESET: lock cancelled; return to ARMED\r\n");
                break;
            }
            lock_attempt++;
            analysis_status = app_capture_measurement(&measurement,
                                                      lock_attempt);
            lock_status = (analysis_status == XST_SUCCESS) ?
                app_normalize_lock_measurement(&measurement) : XST_FAILURE;
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

        if (confirmed_frames < APP_LOCK_CONFIRM_FRAMES) {
            continue;
        }

        dds_control_from_component(&locked_result.channel_a, 0.0f, &dds_a);
        dds_control_from_component(&locked_result.channel_b,
                                   phase_degrees +
                                   APP_DDS_B_PHASE_COMPENSATION_DEGREES,
                                   &dds_b);
        app_preserve_integer_frequency_ratio(&locked_result, &dds_a, &dds_b);
        if (dds_control_commit(&dds_control, &dds_a, &dds_b, 1, 1) !=
            XST_SUCCESS) {
            xil_printf("[DDS] ERROR: initial commit failed\r\n");
            return XST_FAILURE;
        }

        diagnostics_report_dds_snapshot("locked start", &dds_control);
        diagnostics_report_signal_result(&locked_result);
        app_report_pl_iq(&locked_result);
        xil_printf("[APP] RUNNING: T17/R17 adjust B phase; N16 stops and rearms\r\n");
        while (1) {
            int button_status = app_process_running_buttons(
                &buttons, &dds_control, &dds_a, &dds_b, &stopped_channel,
                &phase_degrees);

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
