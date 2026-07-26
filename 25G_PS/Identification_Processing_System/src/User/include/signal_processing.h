/******************************************************************************
 * signal_processing.h
 *
 * Two-component signal identification for the AD9226 DMA frame.
 ******************************************************************************/

#ifndef USER_INCLUDE_SIGNAL_PROCESSING_H_
#define USER_INCLUDE_SIGNAL_PROCESSING_H_

#include "../config/signal_profiles.h"
#include "signal_types.h"

#include "arm_math.h"
#include "xstatus.h"
#include "xil_types.h"

/**
 * 将一帧原始 ADC 码转换为时域值，执行加窗 FFT 粗搜索，并选择联合残差最小
 * 的正弦/三角波组合。
 *
 * raw_samples、profile 和 result 分别是只读输入、只读参数和输出；其余数组
 * 均为调用者提供的可写工作区，长度必须与 APP_FFT_LEN/APP_SPEC_LEN 匹配。
 * fft_instance 必须提前按 APP_FFT_LEN 初始化。本函数不访问 DMA 或 DDS。
 */
int signal_analyze_frame(const u16 *raw_samples,
                         arm_rfft_fast_instance_f32 *fft_instance,
                         float32_t *time_domain,
                         float32_t *fft_input,
                         float32_t *fft_spectrum,
                         float32_t *fft_magnitude,
                         float32_t *model_workspace,
                         const signal_profile_t *profile,
                         signal_analysis_result_t *result);

/** 低通更新相位连续跟踪中允许变化的测量值，不改变离散波形判定。 */
void signal_track_result(signal_analysis_result_t *tracked,
                         const signal_analysis_result_t *measurement,
                         float32_t frequency_alpha);

const char *signal_waveform_name(signal_waveform_t waveform);

/** 运行仅依赖合成样本的算法回归，不访问 DMA 或 DDS 控制 BRAM。 */
int signal_run_self_tests(void);

#endif /* USER_INCLUDE_SIGNAL_PROCESSING_H_ */
