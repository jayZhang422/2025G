/******************************************************************************
 * app_buffers.h
 *
 * Shared frame buffers used by the PS processing flow.
 ******************************************************************************/

#ifndef USER_INCLUDE_APP_BUFFERS_H_
#define USER_INCLUDE_APP_BUFFERS_H_

#include "arm_math.h"
#include "xil_types.h"
#include "app_config.h"

/*
 * Float buffers are static globals because 4096-point arrays are too large for
 * the stack on the Zynq PS.
 */
extern float32_t g_time_domain_buffer[APP_FFT_LEN];
extern float32_t g_fft_input_buffer[APP_FFT_LEN];
extern float32_t g_fft_spectrum_buffer[APP_FFT_LEN];
extern float32_t g_fft_magnitude_buffer[APP_SPEC_LEN];
extern float32_t g_model_buffer[APP_FFT_LEN];

/*
 * The ADC DMA writes g_adc_raw_buffer. The other buffers are CPU workspaces.
 * The DDS is configured through BRAM words, so no DAC sample stream exists.
 */
extern u16 *g_adc_raw_buffer;

#endif /* USER_INCLUDE_APP_BUFFERS_H_ */
