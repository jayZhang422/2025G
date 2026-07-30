/******************************************************************************
 * app_buffers.c
 *
 * Shared buffers for one 4096-sample processing frame. The DMA buffer keeps
 * its legacy u16 declaration while the active 26G path interprets it as s16.
 ******************************************************************************/

#include "../include/app_buffers.h"

float32_t g_time_domain_buffer[APP_FFT_LEN];
float32_t g_fft_input_buffer[APP_FFT_LEN];
float32_t g_fft_spectrum_buffer[APP_FFT_LEN];
float32_t g_fft_magnitude_buffer[APP_SPEC_LEN];
float32_t g_model_buffer[APP_FFT_LEN];

u16 g_adc_raw_buffer[APP_FFT_LEN]
    __attribute__((section(".dma_buffer"), aligned(64)));
