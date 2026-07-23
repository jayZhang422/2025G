#include "signal_separator_task.h"

#include "User/include/iq_demodulator.h"
#include "User/include/signal_processing.h"

#include <math.h>

#include "xil_printf.h"
#include "xstatus.h"

static iq_demodulator_t g_iq_detector;

/** 为归档裸机主流程提供基于 FreeRTOS tick 的 usleep 兼容延时。 */
static void app_delay_us(unsigned long delay_us)
{
    TickType_t ticks = pdMS_TO_TICKS((delay_us + 999U) / 1000U);

    vTaskDelay((ticks == 0U) ? 1U : ticks);
}

/** 对已锁定的 A/B 频点分别触发 PL IQ 测量，并输出归一化幅值和相位日志。 */
static void app_report_pl_iq(const signal_analysis_result_t *result)
{
    const signal_component_t *channels[2] = {
        &result->channel_a,
        &result->channel_b
    };
    int index;

    for (index = 0; index < 2; ++index) {
        iq_measurement_t measurement;
        double magnitude;
        double phase;

        if (iq_demodulator_measure(&g_iq_detector,
                                   channels[index]->frequency_hz,
                                   &measurement) != XST_SUCCESS) {
            xil_printf("WARN: PL IQ measurement failed for %d Hz\r\n",
                       (int)channels[index]->frequency_hz);
            continue;
        }

        magnitude = sqrt((double)measurement.i_sum * measurement.i_sum +
                         (double)measurement.q_sum * measurement.q_sum) /
                    (double)measurement.sample_count;
        phase = atan2((double)measurement.q_sum, (double)measurement.i_sum);
        xil_printf("IQ[%d] f=%d seq=%d amp=%d phase_mrad=%d n=%d\r\n",
                   index,
                   (int)channels[index]->frequency_hz,
                   (int)measurement.result_sequence,
                   (int)magnitude,
                   (int)(phase * 1000.0),
                   (int)measurement.sample_count);
    }
}

/* 将归档裸机入口重命名并以 FreeRTOS 延时替代其 usleep 调用。 */
#define main signal_separator_bare_metal_main
#define usleep app_delay_us
#include "The main function from a past problem.txt"
#undef usleep
#undef main

/** FreeRTOS 应用任务：初始化 PL IQ 驱动后运行信号分离状态机并在退出时自删除。 */
void signal_separator_task(void *parameters)
{
    (void)parameters;

    if (iq_demodulator_init(&g_iq_detector) != XST_SUCCESS) {
        xil_printf("WARN: PL IQ detector initialization failed\r\n");
    }
    if (signal_separator_bare_metal_main() != XST_SUCCESS) {
        xil_printf("ERROR: signal separator task stopped\r\n");
    }
    vTaskDelete(NULL);
}
