/******************************************************************************
 * diagnostics.c
 *
 * Bounded diagnostic output: first few attempts and then periodic snapshots.
 ******************************************************************************/

#include "../include/app_config.h"
#include "../include/diagnostics.h"

#include <math.h>

#include "xil_io.h"
#include "xil_printf.h"

/** 判断当前尝试次数是否应输出受限频率的诊断日志。 */
int diagnostics_should_report(u32 attempt)
{
    return attempt <= APP_DIAG_FIRST_ATTEMPTS ||
           (attempt % APP_DIAG_REPORT_PERIOD) == 0U;
}

/** 统计一帧高位对齐 ADC 码的范围、均值、变化和饱和情况并输出日志。 */
void diagnostics_report_adc_frame(const u16 *raw_samples, u32 attempt)
{
    u16 minimum = 0xFFFFU;
    u16 maximum = 0U;
    u32 sum = 0U;
    u32 transitions = 0U;
    u32 saturated_low = 0U;
    u32 saturated_high = 0U;
    u16 previous = 0U;
    int index;

    for (index = 0; index < APP_FFT_LEN; index++) {
        u16 code = raw_samples[index] >> 4;

        if (code < minimum) {
            minimum = code;
        }
        if (code > maximum) {
            maximum = code;
        }
        if (code == 0U) {
            saturated_low++;
        }
        if (code == 4095U) {
            saturated_high++;
        }
        if (index > 0 && code != previous) {
            transitions++;
        }
        previous = code;
        sum += code;
    }

    xil_printf("[ADC] DBG frame[%d]: min=%d max=%d mean=%d change=%d sat_lo=%d sat_hi=%d\r\n",
               (int)attempt, (int)minimum, (int)maximum,
               (int)(sum / APP_FFT_LEN), (int)transitions,
               (int)saturated_low, (int)saturated_high);
    xil_printf("[ADC] DBG first8: %d %d %d %d %d %d %d %d\r\n",
               (int)(raw_samples[0] >> 4), (int)(raw_samples[1] >> 4),
               (int)(raw_samples[2] >> 4), (int)(raw_samples[3] >> 4),
               (int)(raw_samples[4] >> 4), (int)(raw_samples[5] >> 4),
               (int)(raw_samples[6] >> 4), (int)(raw_samples[7] >> 4));
}

/** 输出一次信号分析与锁定判定的摘要；分析失败时只打印失败原因。 */
void diagnostics_report_analysis(u32 attempt, int analysis_status,
                                 int lock_status,
                                 const signal_analysis_result_t *result)
{
    if (analysis_status != XST_SUCCESS) {
        xil_printf("[FFT] DBG frame[%d]: signal_analyze_frame failed\r\n",
                   (int)attempt);
        return;
    }

    xil_printf("[FFT] DBG frame[%d]: A=%s/%d B=%s/%d residual_ppm=%d lock=%s\r\n",
               (int)attempt,
               signal_waveform_name(result->channel_a.waveform),
               (int)result->channel_a.frequency_hz,
               signal_waveform_name(result->channel_b.waveform),
               (int)result->channel_b.frequency_hz,
               (int)(result->normalized_residual * 1000000.0f),
               (lock_status == XST_SUCCESS) ? "accept" : "reject");
}

/** 输出最终锁定的 A/B 分量及拟合残差。 */
void diagnostics_report_signal_result(const signal_analysis_result_t *result)
{
    xil_printf("[APP] A: %s f=%d Hz amp=%d phase_mrad=%d\r\n",
               signal_waveform_name(result->channel_a.waveform),
               (int)result->channel_a.frequency_hz,
               (int)result->channel_a.fundamental_amplitude,
               (int)(result->channel_a.measured_phase_rad * 1000.0f));
    xil_printf("[APP] B: %s f=%d Hz amp=%d phase_mrad=%d residual_ppm=%d\r\n",
               signal_waveform_name(result->channel_b.waveform),
               (int)result->channel_b.frequency_hz,
               (int)result->channel_b.fundamental_amplitude,
               (int)(result->channel_b.measured_phase_rad * 1000.0f),
               (int)(result->normalized_residual * 1000000.0f));
}

/** 输出一次 PL IQ 测量的归一化幅值、相位、序号和积分点数。 */
void diagnostics_report_iq_measurement(u32 channel_index,
                                       float32_t frequency_hz,
                                       const iq_measurement_t *measurement)
{
    double magnitude = sqrt((double)measurement->i_sum * measurement->i_sum +
                            (double)measurement->q_sum * measurement->q_sum) /
                       (double)measurement->sample_count;
    double phase = atan2((double)measurement->q_sum,
                         (double)measurement->i_sum);

    xil_printf("[IQ] channel=%d f=%d seq=%d amp=%d phase_mrad=%d n=%d\r\n",
               (int)channel_index, (int)frequency_hz,
               (int)measurement->result_sequence, (int)magnitude,
               (int)(phase * 1000.0), (int)measurement->sample_count);
}

/** 读取并输出 DDS 控制 BRAM 当前快照，用于确认 PS 写入结果。 */
void diagnostics_report_dds_snapshot(const char *tag,
                                     const dds_control_t *control)
{
    UINTPTR base = control->base_address;

    xil_printf("[DDS] DBG %s: A[w=%d step=%d phase=%d amp=%d] ", tag,
               (int)Xil_In32(base + APP_DDS_A_WAVE_OFFSET),
               (int)Xil_In32(base + APP_DDS_A_STEP_OFFSET),
               (int)Xil_In32(base + APP_DDS_A_PHASE_OFFSET),
               (int)Xil_In32(base + APP_DDS_A_AMPLITUDE_OFFSET));
    xil_printf("B[w=%d step=%d phase=%d amp=%d] ctrl=%d seq=%d\r\n",
               (int)Xil_In32(base + APP_DDS_B_WAVE_OFFSET),
               (int)Xil_In32(base + APP_DDS_B_STEP_OFFSET),
               (int)Xil_In32(base + APP_DDS_B_PHASE_OFFSET),
               (int)Xil_In32(base + APP_DDS_B_AMPLITUDE_OFFSET),
               (int)Xil_In32(base + APP_DDS_CONTROL_OFFSET),
               (int)Xil_In32(base + APP_DDS_COMMIT_OFFSET));
}
