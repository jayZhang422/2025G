/******************************************************************************
 * g26_measurement_app.c
 ******************************************************************************/

#include "../include/g26_measurement_app.h"

#include "../include/app_buffers.h"
#include "../include/button_input.h"
#include "../include/dma_utils.h"
#include "../include/fifo_monitor.h"

#include "FreeRTOS.h"
#include "task.h"
#include "xaxidma.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xstatus.h"

#include <string.h>

#define G26_APP_POLL_MS 5U

static g26_measurement_output_t g26_output;

const g26_measurement_output_t *g26_measurement_output(void)
{
    return &g26_output;
}

static void g26_print_fixed_3(float32_t value)
{
    s32 scaled = (s32)(value * 1000.0f +
                       ((value >= 0.0f) ? 0.5f : -0.5f));
    u32 magnitude = (scaled < 0) ? (u32)(-scaled) : (u32)scaled;

    xil_printf("%s%u.%03u", (scaled < 0) ? "-" : "",
               (unsigned int)(magnitude / 1000U),
               (unsigned int)(magnitude % 1000U));
}

static void g26_print_result(const g26_signal_result_t *result)
{
    u32 component;

    xil_printf("[G26] RESULT components=%u fundamental=%u Hz\r\n",
               (unsigned int)result->component_count,
               (unsigned int)(result->fundamental_frequency_hz + 0.5f));
    xil_printf("[G26] Upp=");
    g26_print_fixed_3(result->upp_mv);
    xil_printf(" mV Urms=");
    g26_print_fixed_3(result->rms_mv);
    xil_printf(" mV DC=");
    g26_print_fixed_3(result->dc_mv);
    xil_printf(" mV residual_ppm=%u bic=",
               (unsigned int)(result->normalized_residual * 1000000.0f +
                               0.5f));
    g26_print_fixed_3(result->model_bic);
    xil_printf(" delta_bic=");
    if (result->delta_bic < 1.0e29f) {
        g26_print_fixed_3(result->delta_bic);
    } else {
        xil_printf("n/a");
    }
    xil_printf("\r\n");

    for (component = 0U; component < result->component_count; component++) {
        const g26_signal_component_t *line = &result->components[component];

        xil_printf("[G26] H%u f=%u Hz amplitude=",
                   (unsigned int)line->harmonic_order,
                   (unsigned int)(line->frequency_hz + 0.5f));
        g26_print_fixed_3(line->amplitude_mv);
        xil_printf(" mVpeak phase=");
        g26_print_fixed_3(line->phase_rad);
        xil_printf(" rad\r\n");
    }
}

static int g26_capture_frame(XAxiDma *dma)
{
    int status;

    Xil_DCacheFlushRange((UINTPTR)g_adc_raw_buffer, APP_RX_FRAME_BYTES);
    status = dma_capture_frame(dma, APP_DMA_RX_DEV_ID, g_adc_raw_buffer,
                               APP_RX_FRAME_BYTES);
    Xil_DCacheInvalidateRange((UINTPTR)g_adc_raw_buffer,
                              APP_RX_FRAME_BYTES);
    if (status != XST_SUCCESS ||
        dma_last_s2mm_length_bytes(dma) != APP_RX_FRAME_BYTES) {
        xil_printf("[G26] ERROR: DMA frame status=%d bytes=%u expected=%u\r\n",
                   status,
                   (unsigned int)dma_last_s2mm_length_bytes(dma),
                   (unsigned int)APP_RX_FRAME_BYTES);
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static int g26_capture_measurement(XAxiDma *dma, int monitor_available,
                                   g26_signal_result_t *result)
{
    fifo_monitor_snapshot_t before;
    fifo_monitor_snapshot_t after;
    int analysis_status;
    u32 warmup;

    if (dma_align_s2mm(dma, APP_DMA_RX_DEV_ID) != XST_SUCCESS) {
        xil_printf("[G26] ERROR: DMA frame alignment failed\r\n");
        return XST_FAILURE;
    }
    for (warmup = 0U; warmup < APP_G26_WARMUP_FRAMES; warmup++) {
        if (g26_capture_frame(dma) != XST_SUCCESS) {
            xil_printf("[G26] ERROR: warm-up frame %u failed\r\n",
                       (unsigned int)(warmup + 1U));
            return XST_FAILURE;
        }
    }

    if (monitor_available &&
        fifo_monitor_snapshot(&before) != XST_SUCCESS) {
        xil_printf("[G26] ERROR: pre-capture FIFO snapshot failed\r\n");
        return XST_FAILURE;
    }
    if (g26_capture_frame(dma) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    if (monitor_available) {
        if (fifo_monitor_snapshot(&after) != XST_SUCCESS) {
            xil_printf("[G26] ERROR: post-capture FIFO snapshot failed\r\n");
            return XST_FAILURE;
        }
        if (after.blocked_reset_count != before.blocked_reset_count) {
            xil_printf("[G26] ERROR: FIFO reset blocked the analysis frame\r\n");
            fifo_monitor_print("before", &before);
            fifo_monitor_print("after", &after);
            return XST_FAILURE;
        }
        if (after.blocked_high_watermark_count !=
            before.blocked_high_watermark_count) {
            /* ponytail: one-BD snapshot capture can back-pressure between BDs;
             * make this fatal only after board data defines a safe threshold. */
            xil_printf("[G26] WARN: FIFO high-watermark count increased\r\n");
        }
    }

    analysis_status = g26_signal_analyze(
        (const s16 *)(const void *)g_adc_raw_buffer,
        APP_ANALYSIS_SAMPLE_RATE_HZ, APP_G26_INPUT_MV_PER_CODE, result);
    if (analysis_status != G26_SIGNAL_OK) {
        xil_printf("[G26] ERROR: analysis failed status=%d\r\n",
                   analysis_status);
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static int g26_measure_once(XAxiDma *dma, int monitor_available)
{
    g26_signal_result_t result;

    g26_output.valid = 0;
    if (g26_capture_measurement(dma, monitor_available, &result) !=
        XST_SUCCESS) {
        return XST_FAILURE;
    }
    if (g26_signal_generate_waveform(
            &result, 1U, g26_output.one_period_mv,
            APP_G26_WAVEFORM_POINTS) != G26_SIGNAL_OK ||
        g26_signal_generate_waveform(
            &result, 3U, g26_output.three_period_mv,
            APP_G26_WAVEFORM_POINTS) != G26_SIGNAL_OK) {
        xil_printf("[G26] ERROR: waveform reconstruction failed\r\n");
        return XST_FAILURE;
    }

    g26_output.result = result;
    g26_output.valid = 1;
    g26_print_result(&g26_output.result);
    xil_printf("[G26] HOLD: 1/3-period waveforms ready; press KEY2 to clear\r\n");
    return XST_SUCCESS;
}

static void g26_clear_result(int monitor_available)
{
    g26_output.valid = 0;
    memset(&g26_output, 0, sizeof(g26_output));
    if (monitor_available && fifo_monitor_clear_sticky() != XST_SUCCESS) {
        xil_printf("[G26] WARN: FIFO sticky clear failed\r\n");
    }
    xil_printf("[G26] ARMED: result cleared; press KEY1 to measure\r\n");
}

void g26_measurement_task(void *parameters)
{
    XAxiDma dma;
    button_input_t buttons;
    int self_test_status;
    int monitor_available;
    int armed = 1;

    (void)parameters;
    portTASK_USES_FLOATING_POINT();
    memset(&g26_output, 0, sizeof(g26_output));

    xil_printf("\r\n[G26] 2026 periodic-signal analyzer, 4096-point PL-FIR input\r\n");
    self_test_status = g26_signal_analysis_self_test();
    if (self_test_status != G26_SIGNAL_OK) {
        xil_printf("[G26] WARN: algorithm self-test failed code=%d; measurement remains enabled\r\n",
                   -self_test_status);
    } else {
        xil_printf("[G26] PASS: algorithm self-test\r\n");
    }

    if (button_input_init(&buttons) != XST_SUCCESS ||
        dma_init_s2mm(&dma, APP_DMA_RX_DEV_ID) != XST_SUCCESS) {
        xil_printf("[G26] FATAL: button/DMA initialization failed\r\n");
        vTaskDelete(NULL);
        return;
    }
    monitor_available = (fifo_monitor_init() == XST_SUCCESS);
    if (!monitor_available) {
        xil_printf("[G26] WARN: FIFO monitor unavailable\r\n");
    } else if (fifo_monitor_clear_sticky() != XST_SUCCESS) {
        xil_printf("[G26] WARN: initial FIFO sticky clear failed\r\n");
    }

    xil_printf("[G26] Fs=%u Hz frame=%u bytes scale=",
               (unsigned int)(APP_ANALYSIS_SAMPLE_RATE_HZ + 0.5f),
               (unsigned int)APP_RX_FRAME_BYTES);
    g26_print_fixed_3(APP_G26_INPUT_MV_PER_CODE);
    xil_printf(" mV/code\r\n[G26] ARMED: KEY1=start KEY2=clear/rearm\r\n");

    for (;;) {
        if (button_input_take_reset_press(&buttons)) {
            g26_clear_result(monitor_available);
            armed = 1;
        } else if (armed && button_input_take_start_press(&buttons)) {
            TickType_t started_at = xTaskGetTickCount();
            int status;
            u32 elapsed_ms;

            armed = 0;
            xil_printf("[G26] MEASURING: align + %u warm-up + 1 analysis frame\r\n",
                       (unsigned int)APP_G26_WARMUP_FRAMES);
            status = g26_measure_once(&dma, monitor_available);
            elapsed_ms = (u32)(((u64)(xTaskGetTickCount() - started_at) *
                                1000U) / configTICK_RATE_HZ);
            xil_printf("[G26] measurement_time=%u ms\r\n",
                       (unsigned int)elapsed_ms);
            if (status != XST_SUCCESS) {
                xil_printf("[G26] FAILED: press KEY1 to retry or KEY2 to clear\r\n");
                armed = 1;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(G26_APP_POLL_MS));
    }
}
