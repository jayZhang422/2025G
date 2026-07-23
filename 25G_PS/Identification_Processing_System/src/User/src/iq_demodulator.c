/******************************************************************************
 * iq_demodulator.c
 *
 * The offsets below match the active iq_demodulator.sv AXI-Lite slave.
 ******************************************************************************/

#include "../include/app_config.h"
#include "../include/iq_demodulator.h"

#include "FreeRTOS.h"
#include "task.h"
#include "xil_io.h"
#include "xstatus.h"

#define IQ_CTRL_OFFSET              0x00U
#define IQ_PINC_OFFSET              0x04U
#define IQ_POFFSET_OFFSET           0x08U
#define IQ_WINDOW_OFFSET            0x0CU
#define IQ_STATUS_OFFSET            0x10U
#define IQ_RESULT_SEQUENCE_OFFSET   0x14U
#define IQ_I_LOW_OFFSET             0x18U
#define IQ_I_HIGH_OFFSET            0x1CU
#define IQ_Q_LOW_OFFSET             0x20U
#define IQ_Q_HIGH_OFFSET            0x24U
#define IQ_SAMPLE_COUNT_OFFSET      0x28U
#define IQ_RESULT_PINC_OFFSET       0x2CU
#define IQ_SCAN_STEP_OFFSET         0x30U
#define IQ_SCAN_COUNT_OFFSET        0x34U

#define IQ_CTRL_ENABLE              0x01U
#define IQ_CTRL_COMMIT              0x02U
#define IQ_STATUS_RESULT_PENDING    0x01U
#define IQ_STATUS_CONFIG_ACK        0x02U
#define IQ_STATUS_CONFIG_BUSY       0x04U

/** 将 IQ 结果超时常量转换为至少一个可调度的 FreeRTOS tick。 */
static TickType_t iq_timeout_ticks(void)
{
    TickType_t ticks = pdMS_TO_TICKS(APP_IQ_RESULT_TIMEOUT_MS);

    return (ticks == 0U) ? 1U : ticks;
}

/** 轮询 IQ 状态寄存器的掩码值，并在规定时间内让出任务调度。 */
static int iq_wait_for_status(iq_demodulator_t *detector, u32 mask,
                              u32 expected)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = iq_timeout_ticks();

    do {
        if ((Xil_In32(detector->base_address + IQ_STATUS_OFFSET) & mask) ==
            expected) {
            return XST_SUCCESS;
        }
        vTaskDelay(1U);
    } while ((xTaskGetTickCount() - start) < timeout);

    return XST_FAILURE;
}

/** 按实际 ADC 时钟把目标解调频率换算为 32 位 DDS PINC。 */
static u32 iq_phase_increment_from_frequency(float32_t frequency_hz)
{
    double increment = ((double)frequency_hz * 4294967296.0) /
                       (double)APP_IQ_ADC_CLOCK_HZ;

    if (increment <= 0.0) {
        return 0U;
    }
    if (increment >= 4294967295.0) {
        return 0xFFFFFFFFU;
    }
    return (u32)(increment + 0.5);
}

/** 从两个 AXI-Lite 字拼接并符号扩展 IQ IP 发布的 48 位累加值。 */
static s64 iq_read_signed_48(UINTPTR base, u32 low_offset, u32 high_offset)
{
    u64 value = (u64)Xil_In32(base + low_offset) |
                ((u64)(Xil_In32(base + high_offset) & 0xFFFFU) << 32);

    if ((value & (1ULL << 47)) != 0U) {
        value |= 0xFFFF000000000000ULL;
    }
    return (s64)value;
}

/** 对 RESULT_PENDING 执行 W1C 确认，允许 PL 开始下一次窗口测量。 */
static int iq_acknowledge_pending_result(iq_demodulator_t *detector)
{
    if ((Xil_In32(detector->base_address + IQ_STATUS_OFFSET) &
         IQ_STATUS_RESULT_PENDING) != 0U) {
        Xil_Out32(detector->base_address + IQ_STATUS_OFFSET,
                  IQ_STATUS_RESULT_PENDING);
    }
    return XST_SUCCESS;
}

/** 绑定 BSP 导出的 IQ AXI-Lite 基址，清除遗留结果并确认初始配置。 */
int iq_demodulator_init(iq_demodulator_t *detector)
{
    if (detector == 0) {
        return XST_FAILURE;
    }

    detector->base_address = (UINTPTR)APP_IQ_BASEADDR;
    (void)iq_acknowledge_pending_result(detector);
    Xil_Out32(detector->base_address + IQ_CTRL_OFFSET, IQ_CTRL_COMMIT);
    return iq_wait_for_status(detector,
                              IQ_STATUS_CONFIG_ACK | IQ_STATUS_CONFIG_BUSY,
                              IQ_STATUS_CONFIG_ACK);
}

/**
 * 对一个目标频率提交单频 IQ 配置、等待结果快照并读取 48 位 I/Q 累加值。
 * 本函数完成读取后的 W1C 确认；调用者只应在返回成功后使用 measurement。
 */
int iq_demodulator_measure(iq_demodulator_t *detector, float32_t frequency_hz,
                           iq_measurement_t *measurement)
{
    UINTPTR base;

    if (detector == 0 || measurement == 0 || frequency_hz <= 0.0f) {
        return XST_FAILURE;
    }

    base = detector->base_address;
    (void)iq_acknowledge_pending_result(detector);
    Xil_Out32(base + IQ_PINC_OFFSET, iq_phase_increment_from_frequency(
                                     frequency_hz));
    Xil_Out32(base + IQ_POFFSET_OFFSET, 0U);
    Xil_Out32(base + IQ_WINDOW_OFFSET, APP_IQ_WINDOW_SAMPLES);
    Xil_Out32(base + IQ_SCAN_STEP_OFFSET, 0U);
    Xil_Out32(base + IQ_SCAN_COUNT_OFFSET, 1U);
    Xil_Out32(base + IQ_CTRL_OFFSET, IQ_CTRL_ENABLE | IQ_CTRL_COMMIT);

    if (iq_wait_for_status(detector,
                           IQ_STATUS_CONFIG_ACK | IQ_STATUS_CONFIG_BUSY,
                           IQ_STATUS_CONFIG_ACK) != XST_SUCCESS ||
        iq_wait_for_status(detector, IQ_STATUS_RESULT_PENDING,
                           IQ_STATUS_RESULT_PENDING) != XST_SUCCESS) {
        return XST_FAILURE;
    }

    measurement->result_sequence = Xil_In32(base + IQ_RESULT_SEQUENCE_OFFSET);
    measurement->i_sum = iq_read_signed_48(base, IQ_I_LOW_OFFSET,
                                            IQ_I_HIGH_OFFSET);
    measurement->q_sum = iq_read_signed_48(base, IQ_Q_LOW_OFFSET,
                                            IQ_Q_HIGH_OFFSET);
    measurement->sample_count = (u16)Xil_In32(base + IQ_SAMPLE_COUNT_OFFSET);
    measurement->phase_increment = Xil_In32(base + IQ_RESULT_PINC_OFFSET);
    (void)iq_acknowledge_pending_result(detector);

    Xil_Out32(base + IQ_CTRL_OFFSET, IQ_CTRL_COMMIT);
    return iq_wait_for_status(detector,
                              IQ_STATUS_CONFIG_ACK | IQ_STATUS_CONFIG_BUSY,
                              IQ_STATUS_CONFIG_ACK);
}
