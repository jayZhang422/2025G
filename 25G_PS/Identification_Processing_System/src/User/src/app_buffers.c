/******************************************************************************
 * app_buffers.c
 *
 * Shared buffers for one 4096-sample processing frame.
 ******************************************************************************/

#include "../include/app_buffers.h"

float32_t g_time_domain_buffer[APP_FFT_LEN];
float32_t g_fft_input_buffer[APP_FFT_LEN];
float32_t g_fft_spectrum_buffer[APP_FFT_LEN];
float32_t g_fft_magnitude_buffer[APP_SPEC_LEN];
float32_t g_model_buffer[APP_FFT_LEN];

u16 *g_adc_raw_buffer = (u16 *)APP_DMA_BUFFER_BASE;
