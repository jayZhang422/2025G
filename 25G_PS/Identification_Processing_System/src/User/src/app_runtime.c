#include "../include/app_buffers.h"
#include "../include/app_config.h"
#include "../include/app_runtime.h"

#include "../algorithms/two_channel_signal_analyzer.h"
#include "xil_cache.h"
#include "xstatus.h"

int app_runtime_init(app_runtime_t *runtime)
{
    if (runtime == 0) {
        return XST_FAILURE;
    }

    runtime->initialized = 0;
    dds_control_init(&runtime->dds);

    if (button_input_init(&runtime->buttons) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    if (dma_init_s2mm(&runtime->dma, APP_DMA_RX_DEV_ID) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    if (arm_rfft_fast_init_f32(&runtime->fft, APP_FFT_LEN) != ARM_MATH_SUCCESS) {
        return XST_FAILURE;
    }

    runtime->initialized = 1;
    return XST_SUCCESS;
}

int app_runtime_run_algorithm_self_tests(void)
{
    return signal_run_self_tests();
}

int app_runtime_capture_and_analyze(app_runtime_t *runtime,
                                    signal_analysis_result_t *result)
{
    int status;

    if (runtime == 0 || result == 0 || runtime->initialized == 0) {
        return XST_FAILURE;
    }

    /* The capture buffer belongs to S2MM; discard stale CPU cache lines first. */
    Xil_DCacheInvalidateRange((INTPTR)g_adc_raw_buffer, APP_RX_FRAME_BYTES);

    status = dma_capture_frame(&runtime->dma, APP_DMA_RX_DEV_ID,
                               g_adc_raw_buffer, APP_RX_FRAME_BYTES);
    if (status != XST_SUCCESS) {
        return status;
    }

    /*
     * The DMA writes DDR while the Cortex-A9 cache may retain old lines.
     * Invalidate only after DMA completion and before signal analysis.
     */
    Xil_DCacheInvalidateRange((INTPTR)g_adc_raw_buffer, APP_RX_FRAME_BYTES);

    return signal_analyze_frame(g_adc_raw_buffer,
                                &runtime->fft,
                                g_time_domain_buffer,
                                g_fft_input_buffer,
                                g_fft_spectrum_buffer,
                                g_fft_magnitude_buffer,
                                g_model_buffer,
                                result);
}