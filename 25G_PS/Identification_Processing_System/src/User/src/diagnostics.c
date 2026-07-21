/******************************************************************************
 * diagnostics.c
 *
 * Bounded diagnostic output: first few attempts and then periodic snapshots.
 ******************************************************************************/

#include "../include/app_config.h"
#include "../include/diagnostics.h"

#include "xil_io.h"
#include "xil_printf.h"

int diagnostics_should_report(u32 attempt)
{
    return attempt <= APP_DIAG_FIRST_ATTEMPTS ||
           (attempt % APP_DIAG_REPORT_PERIOD) == 0U;
}

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

    xil_printf("DBG ADC[%d]: min=%d max=%d mean=%d change=%d sat_lo=%d sat_hi=%d\r\n",
               (int)attempt, (int)minimum, (int)maximum,
               (int)(sum / APP_FFT_LEN), (int)transitions,
               (int)saturated_low, (int)saturated_high);
    xil_printf("DBG ADC first8: %d %d %d %d %d %d %d %d\r\n",
               (int)(raw_samples[0] >> 4), (int)(raw_samples[1] >> 4),
               (int)(raw_samples[2] >> 4), (int)(raw_samples[3] >> 4),
               (int)(raw_samples[4] >> 4), (int)(raw_samples[5] >> 4),
               (int)(raw_samples[6] >> 4), (int)(raw_samples[7] >> 4));
}

void diagnostics_report_analysis(u32 attempt, int analysis_status,
                                 int lock_status,
                                 const signal_analysis_result_t *result)
{
    if (analysis_status != XST_SUCCESS) {
        xil_printf("DBG ANA[%d]: signal_analyze_frame failed\r\n",
                   (int)attempt);
        return;
    }

    xil_printf("DBG ANA[%d]: A=%s/%d B=%s/%d residual_ppm=%d lock=%s\r\n",
               (int)attempt,
               signal_waveform_name(result->channel_a.waveform),
               (int)result->channel_a.frequency_hz,
               signal_waveform_name(result->channel_b.waveform),
               (int)result->channel_b.frequency_hz,
               (int)(result->normalized_residual * 1000000.0f),
               (lock_status == XST_SUCCESS) ? "accept" : "reject");
}

void diagnostics_report_dds_snapshot(const char *tag,
                                     const dds_control_t *control)
{
    UINTPTR base = control->base_address;

    xil_printf("DBG DDS %s: A[w=%d step=%d phase=%d amp=%d] ", tag,
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
