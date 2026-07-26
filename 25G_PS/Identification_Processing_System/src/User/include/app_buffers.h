/******************************************************************************
 * app_buffers.h
 *
 * Shared frame buffers used by the PS processing flow.
 ******************************************************************************/

#ifndef USER_INCLUDE_APP_BUFFERS_H_
#define USER_INCLUDE_APP_BUFFERS_H_

#include "../config/algorithm_config.h"

#include "arm_math.h"
#include "xil_types.h"

/*
 * Float buffers are static globals because 4096-point arrays are too large for
 * the stack on the Zynq PS.
 */
extern float32_t g_time_domain_buffer[APP_FFT_LEN]; /**< 去直流时域数据。 */
extern float32_t g_fft_input_buffer[APP_FFT_LEN];   /**< 加 Hann 窗后的 FFT 输入。 */
extern float32_t g_fft_spectrum_buffer[APP_FFT_LEN]; /**< CMSIS 紧凑实数 FFT 输出。 */
extern float32_t g_fft_magnitude_buffer[APP_SPEC_LEN]; /**< 非负频率半谱幅值。 */
extern float32_t g_model_buffer[APP_FFT_LEN]; /**< 双分量重建和残差计算工作区。 */

/*
 * The ADC DMA writes g_adc_raw_buffer. The other buffers are CPU workspaces.
 * The DDS is configured through BRAM words, so no DAC sample stream exists.
 */
extern u16 g_adc_raw_buffer[APP_FFT_LEN]; /**< DMA 写入、下一帧会覆盖的 ADC 原始码。 */

#endif /* USER_INCLUDE_APP_BUFFERS_H_ */
