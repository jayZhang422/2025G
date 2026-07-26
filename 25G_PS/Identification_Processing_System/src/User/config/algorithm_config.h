/******************************************************************************
 * algorithm_config.h
 *
 * Defaults for the existing FFT, lock, polling, and diagnostic algorithms.
 ******************************************************************************/

#ifndef USER_CONFIG_ALGORITHM_CONFIG_H_
#define USER_CONFIG_ALGORITHM_CONFIG_H_

#include "hardware_config.h"

#ifndef APP_PI
#define APP_PI 3.14159265358979f
#endif

/* The FFT currently consumes exactly one hardware DMA frame. */
#define APP_FFT_LEN                 APP_ADC_FRAME_SAMPLES
#define APP_SPEC_LEN                (APP_FFT_LEN / 2U)
#define APP_BIN_WIDTH_HZ            (APP_SAMPLE_RATE_HZ / APP_FFT_LEN)
#define APP_FFT_WINDOW_HANN         1U
#define APP_FFT_WINDOW              APP_FFT_WINDOW_HANN

#define APP_PEAK_CANDIDATE_COUNT    8
#define APP_MIN_COMPONENT_GAP_HZ    2500.0f
#define APP_TRI_MAX_HARMONIC        15

#define APP_DMA_CAPTURE_TIMEOUT_MS  1000U
#define APP_DMA_RESET_TIMEOUT       1000000U
#define APP_IQ_WINDOW_SAMPLES       APP_ADC_FRAME_SAMPLES
#define APP_IQ_RESULT_TIMEOUT_MS    100U

/* Synthetic algorithm regression runs before the first DMA capture. */
#define APP_ENABLE_STARTUP_SELF_TEST 1

#endif /* USER_CONFIG_ALGORITHM_CONFIG_H_ */
