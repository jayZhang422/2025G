/******************************************************************************
 * ddc_validation.c
 *
 * Validates ddc_stream -> AXIS FIFO -> SG S2MM DMA one frame at a time.
 * This deliberately measures preserved I/Q features; it does not classify a
 * modulation type or estimate contest parameters.
 ******************************************************************************/

#include "../include/ddc_validation.h"

#include "../config/hardware_config.h"
#include "../include/dma_utils.h"
#include "../include/modulation_analysis.h"

#include <math.h>
#include <string.h>

#include "xaxidma_hw.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "xtime_l.h"

#define DDC_CTRL_OFFSET           0x00U
#define DDC_PINC_OFFSET           0x04U
#define DDC_STATUS_OFFSET         0x08U
#define DDC_OUTPUT_COUNT_OFFSET   0x0CU
#define DDC_FRAME_COUNT_OFFSET    0x10U
#define DDC_DECIM_OFFSET          0x14U
#define DDC_SAMPLE_RATE_OFFSET    0x18U
#define DDC_BUILD_ID_OFFSET       0x1CU

#define DDC_CTRL_RUN              0x01U
#define DDC_CTRL_RESTART          0x02U
#define DDC_CTRL_CLEAR_FAULT      0x04U
#define DDC_STATUS_RUNNING        0x01U
#define DDC_STATUS_CFG_BUSY       0x02U
#define DDC_STATUS_FAULT          0x08U

#define DDC_PI                    3.14159265358979f
#define DDC_MILLIDEGREES_PER_RADIAN (180000.0f / DDC_PI)

const ddc_validation_config_t g_ddc_validation_config = {
    .nco_phase_increment = 0x63FFB333U,
    .expected_build_id = 0x20260727U,
    .expected_decimation = 32U,
    .expected_adc_sample_rate_hz = 5120060U,
    .complex_samples_per_frame = APP_DDC_FRAME_COMPLEX_SAMPLES,
    .timeout_ms = 1000U,
    .minimum_phase_power = 4U,
    .frequency_cluster_threshold_hz = 2000U,
    .phase_jump_threshold_degrees = 135U
};

static s16 g_ddc_iq_buffer[APP_DDC_FRAME_IQ_WORDS]
    __attribute__((aligned(64)));

static u32 ddc_read(u32 offset)
{
    return Xil_In32(APP_DDC_BASEADDR + offset);
}

static void ddc_write(u32 offset, u32 value)
{
    Xil_Out32(APP_DDC_BASEADDR + offset, value);
}

static int timeout_expired(XTime start, u32 timeout_ms)
{
    XTime now;

    XTime_GetTime(&now);
    return ((now - start) >=
            ((XTime)timeout_ms * (XTime)COUNTS_PER_SECOND / 1000U));
}

static s32 round_to_s32(float value)
{
    return (value >= 0.0f) ? (s32)(value + 0.5f) : (s32)(value - 0.5f);
}

static u32 round_to_u32(float value)
{
    return (u32)(value + 0.5f);
}

static int ddc_wait_config_idle(const ddc_validation_config_t *config)
{
    XTime start;

    XTime_GetTime(&start);
    while ((ddc_read(DDC_STATUS_OFFSET) & DDC_STATUS_CFG_BUSY) != 0U) {
        if (timeout_expired(start, config->timeout_ms)) {
            xil_printf("[DDC VALIDATE] FAIL: config busy timeout\r\n");
            return XST_FAILURE;
        }
    }
    return XST_SUCCESS;
}

static int ddc_wait_reset_complete(const ddc_validation_config_t *config)
{
    XTime start;
    u32 status;

    XTime_GetTime(&start);
    for (;;) {
        status = ddc_read(DDC_STATUS_OFFSET);
        if ((status & (DDC_STATUS_RUNNING | DDC_STATUS_CFG_BUSY |
                       DDC_STATUS_FAULT)) == 0U &&
            ddc_read(DDC_OUTPUT_COUNT_OFFSET) == 0U &&
            ddc_read(DDC_FRAME_COUNT_OFFSET) == 0U) {
            return XST_SUCCESS;
        }
        if (timeout_expired(start, config->timeout_ms)) {
            xil_printf("[DDC VALIDATE] FAIL: restart timeout, "
                       "STATUS=0x%08x OUT=%u FRAME=%u\r\n",
                       (unsigned int)status,
                       (unsigned int)ddc_read(DDC_OUTPUT_COUNT_OFFSET),
                       (unsigned int)ddc_read(DDC_FRAME_COUNT_OFFSET));
            return XST_FAILURE;
        }
    }
}

static int ddc_wait_first_frame(const ddc_validation_config_t *config)
{
    XTime start;
    u32 status;

    XTime_GetTime(&start);
    while (ddc_read(DDC_FRAME_COUNT_OFFSET) == 0U) {
        status = ddc_read(DDC_STATUS_OFFSET);
        if ((status & DDC_STATUS_FAULT) != 0U) {
            xil_printf("[DDC VALIDATE] FAIL: DDC fault before TLAST, "
                       "STATUS=0x%08x\r\n", (unsigned int)status);
            return XST_FAILURE;
        }
        if (timeout_expired(start, config->timeout_ms)) {
            xil_printf("[DDC VALIDATE] FAIL: first-frame timeout, "
                       "STATUS=0x%08x OUT=%u\r\n",
                       (unsigned int)status,
                       (unsigned int)ddc_read(DDC_OUTPUT_COUNT_OFFSET));
            return XST_FAILURE;
        }
    }
    return XST_SUCCESS;
}

int ddc_validation_analyze_frame(const s16 *interleaved_iq,
                                 u32 complex_samples,
                                 u32 adc_sample_rate_hz,
                                 u32 decimation,
                                 const ddc_validation_config_t *config,
                                 ddc_frame_statistics_t *statistics)
{
    s64 sum_i = 0;
    s64 sum_q = 0;
    u64 power_sum = 0U;
    s64 frequency_sum = 0;
    s64 negative_frequency_sum = 0;
    s64 positive_frequency_sum = 0;
    u32 index;
    u32 phase_jump_threshold_millidegrees;
    float baseband_sample_rate_hz;

    if (interleaved_iq == NULL || config == NULL || statistics == NULL ||
        complex_samples == 0U || decimation == 0U ||
        config->phase_jump_threshold_degrees > 180U) {
        return XST_FAILURE;
    }

    memset(statistics, 0, sizeof(*statistics));
    statistics->power_min = 0xFFFFFFFFU;
    statistics->instantaneous_frequency_min_hz = 0x7FFFFFFF;
    statistics->instantaneous_frequency_max_hz = (-0x7FFFFFFF - 1);
    phase_jump_threshold_millidegrees =
        config->phase_jump_threshold_degrees * 1000U;
    baseband_sample_rate_hz =
        (float)adc_sample_rate_hz / (float)decimation;

    for (index = 0U; index < complex_samples; ++index) {
        s32 i = interleaved_iq[2U * index];
        s32 q = interleaved_iq[2U * index + 1U];
        u32 power = (u32)((s64)i * i + (s64)q * q);

        if (power < statistics->power_min) statistics->power_min = power;
        if (power > statistics->power_max) statistics->power_max = power;
        power_sum += power;
        sum_i += i;
        sum_q += q;

        if (i == 0 && q == 0) statistics->zero_sample_count++;
        if (i == 32767 || i == -32768 || q == 32767 || q == -32768)
            statistics->saturation_count++;

        if (index != 0U) {
            s32 previous_i = interleaved_iq[2U * (index - 1U)];
            s32 previous_q = interleaved_iq[2U * (index - 1U) + 1U];
            u32 previous_power =
                (u32)((s64)previous_i * previous_i +
                      (s64)previous_q * previous_q);

            if (power >= config->minimum_phase_power &&
                previous_power >= config->minimum_phase_power) {
                s64 dot = (s64)previous_i * i + (s64)previous_q * q;
                s64 cross = (s64)previous_i * q - (s64)previous_q * i;

                if (dot != 0 || cross != 0) {
                    float phase_step = atan2f((float)cross, (float)dot);
                    float absolute_phase_step =
                        (phase_step < 0.0f) ? -phase_step : phase_step;
                    s32 frequency_hz = round_to_s32(
                        phase_step * baseband_sample_rate_hz /
                        (2.0f * DDC_PI));
                    u32 phase_step_millidegrees = round_to_u32(
                        absolute_phase_step * DDC_MILLIDEGREES_PER_RADIAN);

                    if (frequency_hz <
                        statistics->instantaneous_frequency_min_hz) {
                        statistics->instantaneous_frequency_min_hz =
                            frequency_hz;
                    }
                    if (frequency_hz >
                        statistics->instantaneous_frequency_max_hz) {
                        statistics->instantaneous_frequency_max_hz =
                            frequency_hz;
                    }
                    frequency_sum += frequency_hz;
                    statistics->valid_frequency_count++;

                    if (frequency_hz >
                        (s32)config->frequency_cluster_threshold_hz) {
                        positive_frequency_sum += frequency_hz;
                        statistics->positive_frequency_count++;
                    } else if (frequency_hz <
                               -(s32)config->frequency_cluster_threshold_hz) {
                        negative_frequency_sum += frequency_hz;
                        statistics->negative_frequency_count++;
                    }

                    if (phase_step_millidegrees >
                        statistics->maximum_phase_step_millidegrees) {
                        statistics->maximum_phase_step_millidegrees =
                            phase_step_millidegrees;
                    }
                    if (phase_step_millidegrees >=
                        phase_jump_threshold_millidegrees) {
                        statistics->phase_jump_count++;
                    }
                }
            }
        }
    }

    statistics->power_mean = (u32)(power_sum / complex_samples);
    if (statistics->valid_frequency_count != 0U) {
        statistics->instantaneous_frequency_mean_hz =
            (s32)(frequency_sum / statistics->valid_frequency_count);
    } else {
        statistics->instantaneous_frequency_min_hz = 0;
        statistics->instantaneous_frequency_max_hz = 0;
    }
    if (statistics->negative_frequency_count != 0U) {
        statistics->negative_frequency_mean_hz =
            (s32)(negative_frequency_sum /
                  statistics->negative_frequency_count);
    }
    if (statistics->positive_frequency_count != 0U) {
        statistics->positive_frequency_mean_hz =
            (s32)(positive_frequency_sum /
                  statistics->positive_frequency_count);
    }

    if (power_sum != 0U) {
        u64 dc_numerator =
            (u64)(sum_i * sum_i + sum_q * sum_q) * 100U;
        u64 dc_denominator = (u64)complex_samples * power_sum;

        statistics->dc_power_percent =
            (u32)(dc_numerator / dc_denominator);
    }

    return XST_SUCCESS;
}

static int ddc_validation_self_test(const ddc_validation_config_t *config)
{
    const s16 iq[] = {10, 0, -10, 0};
    ddc_validation_config_t self_test_config = *config;
    ddc_frame_statistics_t statistics;

    self_test_config.minimum_phase_power = 1U;
    self_test_config.phase_jump_threshold_degrees = 135U;
    if (ddc_validation_analyze_frame(iq, 2U, 160000U, 1U,
                                     &self_test_config,
                                     &statistics) != XST_SUCCESS ||
        statistics.power_min != 100U ||
        statistics.power_max != 100U ||
        statistics.power_mean != 100U ||
        statistics.instantaneous_frequency_min_hz != 80000 ||
        statistics.instantaneous_frequency_max_hz != 80000 ||
        statistics.instantaneous_frequency_mean_hz != 80000 ||
        statistics.maximum_phase_step_millidegrees != 180000U ||
        statistics.phase_jump_count != 1U ||
        statistics.saturation_count != 0U ||
        statistics.zero_sample_count != 0U) {
        xil_printf("[DDC VALIDATE] FAIL: statistics self-test\r\n");
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static void ddc_print_statistics(u32 frame_index,
                                 const ddc_frame_statistics_t *statistics)
{
    xil_printf("[DDC FRAME %u] STATUS=0x%08x OUT=%u FRAME=%u "
               "DMA_LEN=%u DMA_SR=0x%08x\r\n",
               (unsigned int)frame_index,
               (unsigned int)statistics->ddc_status,
               (unsigned int)statistics->ddc_output_count,
               (unsigned int)statistics->ddc_frame_count,
               (unsigned int)statistics->dma_length_bytes,
               (unsigned int)statistics->dma_status);
    xil_printf("[DDC FRAME %u] power min=%u max=%u mean=%u "
               "dc_power=%u%%\r\n",
               (unsigned int)frame_index,
               (unsigned int)statistics->power_min,
               (unsigned int)statistics->power_max,
               (unsigned int)statistics->power_mean,
               (unsigned int)statistics->dc_power_percent);
    xil_printf("[DDC FRAME %u] freq min=%d max=%d mean=%d Hz valid=%u\r\n",
               (unsigned int)frame_index,
               (int)statistics->instantaneous_frequency_min_hz,
               (int)statistics->instantaneous_frequency_max_hz,
               (int)statistics->instantaneous_frequency_mean_hz,
               (unsigned int)statistics->valid_frequency_count);
    xil_printf("[DDC FRAME %u] negative=%d Hz (%u) positive=%d Hz (%u)\r\n",
               (unsigned int)frame_index,
               (int)statistics->negative_frequency_mean_hz,
               (unsigned int)statistics->negative_frequency_count,
               (int)statistics->positive_frequency_mean_hz,
               (unsigned int)statistics->positive_frequency_count);
    xil_printf("[DDC FRAME %u] phase_max=%u mdeg jumps=%u | "
               "sat=%u zero=%u fault=%u dma_error=%u\r\n",
               (unsigned int)frame_index,
               (unsigned int)statistics->maximum_phase_step_millidegrees,
               (unsigned int)statistics->phase_jump_count,
               (unsigned int)statistics->saturation_count,
               (unsigned int)statistics->zero_sample_count,
               (unsigned int)statistics->ddc_fault,
               (unsigned int)statistics->dma_error);
}

static void ddc_print_modulation_result(const modulation_result_t *result)
{
    u32 confidence_milli = round_to_u32(result->confidence * 1000.0f);
    s32 carrier_offset_hz = round_to_s32(result->carrier_offset_hz);

    xil_printf("[MOD] type=%s confidence=%u/1000 carrier_offset=%d Hz\r\n",
               modulation_type_name(result->type),
               (unsigned int)confidence_milli, (int)carrier_offset_hz);
    switch (result->type) {
    case MODULATION_AM:
        xil_printf("[MOD] F=%d Hz ma=%u/1000\r\n",
                   (int)round_to_s32(result->modulation_frequency_hz),
                   (unsigned int)round_to_u32(result->am_index * 1000.0f));
        break;
    case MODULATION_FM:
        xil_printf("[MOD] F=%d Hz deviation=%d Hz mf=%u/1000\r\n",
                   (int)round_to_s32(result->modulation_frequency_hz),
                   (int)round_to_s32(result->frequency_deviation_hz),
                   (unsigned int)round_to_u32(result->fm_index * 1000.0f));
        break;
    case MODULATION_2ASK:
    case MODULATION_2PSK:
        xil_printf("[MOD] Rb=%u bps symbol_phase=%u/1000 samples\r\n",
                   (unsigned int)result->bit_rate_bps,
                   (unsigned int)round_to_u32(
                       result->symbol_phase_samples * 1000.0f));
        break;
    case MODULATION_2FSK:
        xil_printf("[MOD] Rb=%u bps low=%d Hz high=%d Hz h=%u/1000 "
                   "symbol_phase=%u/1000 samples\r\n",
                   (unsigned int)result->bit_rate_bps,
                   (int)round_to_s32(result->fsk_low_offset_hz),
                   (int)round_to_s32(result->fsk_high_offset_hz),
                   (unsigned int)round_to_u32(result->fsk_index * 1000.0f),
                   (unsigned int)round_to_u32(
                       result->symbol_phase_samples * 1000.0f));
        break;
    default:
        break;
    }
}

static int ddc_capture_one_frame(XAxiDma *dma,
                                 const ddc_validation_config_t *config,
                                 u32 adc_sample_rate_hz,
                                 u32 decimation,
                                 ddc_frame_statistics_t *statistics)
{
    u32 dma_status = 0U;
    u32 ddc_status;
    u32 ddc_output_count;
    u32 ddc_frame_count;
    u32 dma_length_bytes;
    modulation_result_t modulation;

    ddc_write(DDC_PINC_OFFSET, config->nco_phase_increment);
    ddc_write(DDC_CTRL_OFFSET, DDC_CTRL_RESTART | DDC_CTRL_CLEAR_FAULT);
    if (ddc_wait_config_idle(config) != XST_SUCCESS ||
        ddc_wait_reset_complete(config) != XST_SUCCESS) {
        return XST_FAILURE;
    }

    memset(g_ddc_iq_buffer, 0xA5, sizeof(g_ddc_iq_buffer));
    Xil_DCacheFlushRange((UINTPTR)g_ddc_iq_buffer,
                         APP_DDC_RX_FRAME_BYTES);
    if (dma_submit_frame(dma, g_ddc_iq_buffer,
                         APP_DDC_RX_FRAME_BYTES) != XST_SUCCESS) {
        xil_printf("[DDC VALIDATE] FAIL: DMA start\r\n");
        return XST_FAILURE;
    }

    ddc_write(DDC_CTRL_OFFSET, DDC_CTRL_RUN);
    if (ddc_wait_config_idle(config) != XST_SUCCESS ||
        ddc_wait_first_frame(config) != XST_SUCCESS) {
        ddc_write(DDC_CTRL_OFFSET, 0U);
        return XST_FAILURE;
    }

    /* The downstream BD AXIS FIFO is not cleared by DDC RESTART. Perform only
     * one capture per PL/peripheral reset so post-TLAST beats cannot pollute a
     * later DMA buffer. */
    ddc_write(DDC_CTRL_OFFSET, 0U);
    if (ddc_wait_config_idle(config) != XST_SUCCESS ||
        dma_wait_frame(dma, config->timeout_ms,
                       &dma_status) != XST_SUCCESS) {
        xil_printf("[DDC VALIDATE] FAIL: DMA completion\r\n");
        return XST_FAILURE;
    }

    Xil_DCacheInvalidateRange((UINTPTR)g_ddc_iq_buffer,
                              APP_DDC_RX_FRAME_BYTES);

    ddc_status = ddc_read(DDC_STATUS_OFFSET);
    ddc_output_count = ddc_read(DDC_OUTPUT_COUNT_OFFSET);
    ddc_frame_count = ddc_read(DDC_FRAME_COUNT_OFFSET);
    dma_length_bytes = dma_last_s2mm_length_bytes(dma);
    if ((ddc_status & (DDC_STATUS_RUNNING | DDC_STATUS_FAULT)) != 0U ||
        (dma_status & XAXIDMA_ERR_ALL_MASK) != 0U ||
        ddc_output_count != config->complex_samples_per_frame ||
        ddc_frame_count != 1U ||
        dma_length_bytes != APP_DDC_RX_FRAME_BYTES) {
        xil_printf("[DDC VALIDATE] FAIL: frame contract STATUS=0x%08x "
                   "OUT=%u FRAME=%u DMA_LEN=%u DMA_SR=0x%08x\r\n",
                   (unsigned int)ddc_status,
                   (unsigned int)ddc_output_count,
                   (unsigned int)ddc_frame_count,
                   (unsigned int)dma_length_bytes,
                   (unsigned int)dma_status);
        return XST_FAILURE;
    }

    if (ddc_validation_analyze_frame(
            g_ddc_iq_buffer, config->complex_samples_per_frame,
            adc_sample_rate_hz, decimation, config,
            statistics) != XST_SUCCESS) {
        xil_printf("[DDC VALIDATE] FAIL: frame analysis\r\n");
        return XST_FAILURE;
    }

    statistics->ddc_status = ddc_status;
    statistics->ddc_output_count = ddc_output_count;
    statistics->ddc_frame_count = ddc_frame_count;
    statistics->dma_length_bytes = dma_length_bytes;
    statistics->dma_status = dma_status;
    statistics->ddc_fault =
        ((statistics->ddc_status & DDC_STATUS_FAULT) != 0U);
    statistics->dma_error =
        ((statistics->dma_status & XAXIDMA_ERR_ALL_MASK) != 0U);
    ddc_print_statistics(1U, statistics);

    if (statistics->saturation_count != 0U) {
        xil_printf("[DDC VALIDATE] FAIL: saturated I/Q frame\r\n");
        return XST_FAILURE;
    }
    if (modulation_analyze_frame(
            g_ddc_iq_buffer, config->complex_samples_per_frame,
            (float)adc_sample_rate_hz / (float)decimation,
            &modulation) != XST_SUCCESS) {
        xil_printf("[DDC VALIDATE] FAIL: modulation analysis\r\n");
        return XST_FAILURE;
    }
    ddc_print_modulation_result(&modulation);

    return XST_SUCCESS;
}

int ddc_validation_run(const ddc_validation_config_t *config)
{
    XAxiDma dma;
    ddc_frame_statistics_t statistics;
    u32 build_id;
    u32 decimation;
    u32 adc_sample_rate_hz;
    int result = XST_FAILURE;

    if (config == NULL || config->expected_decimation == 0U ||
        config->complex_samples_per_frame != APP_DDC_FRAME_COMPLEX_SAMPLES ||
        config->timeout_ms == 0U ||
        config->phase_jump_threshold_degrees > 180U) {
        xil_printf("[DDC VALIDATE] FAIL: invalid configuration\r\n");
        return XST_FAILURE;
    }
    if (ddc_validation_self_test(config) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    xil_printf("\r\n[DDC VALIDATE] one-frame complex-baseband analyzer\r\n");
    xil_printf("[DDC VALIDATE] base=0x%08x samples=%u bytes=%u\r\n",
               (unsigned int)APP_DDC_BASEADDR,
               (unsigned int)config->complex_samples_per_frame,
               (unsigned int)APP_DDC_RX_FRAME_BYTES);

    build_id = ddc_read(DDC_BUILD_ID_OFFSET);
    decimation = ddc_read(DDC_DECIM_OFFSET);
    adc_sample_rate_hz = ddc_read(DDC_SAMPLE_RATE_OFFSET);
    xil_printf("[DDC VALIDATE] BUILD=0x%08x DECIM=%u ADC_FS=%u PINC=0x%08x\r\n",
               (unsigned int)build_id, (unsigned int)decimation,
               (unsigned int)adc_sample_rate_hz,
               (unsigned int)config->nco_phase_increment);
    if (build_id != config->expected_build_id ||
        decimation != config->expected_decimation ||
        adc_sample_rate_hz != config->expected_adc_sample_rate_hz) {
        xil_printf("[DDC VALIDATE] FAIL: bitstream/BSP contract mismatch\r\n");
        return XST_FAILURE;
    }

    if (dma_init_s2mm(&dma, APP_DMA_RX_DEV_ID) != XST_SUCCESS) {
        xil_printf("[DDC VALIDATE] FAIL: DMA init\r\n");
        return XST_FAILURE;
    }

    if (ddc_capture_one_frame(&dma, config, adc_sample_rate_hz,
                              decimation, &statistics) != XST_SUCCESS) {
        dma_dump_s2mm_regs("[DDC VALIDATE] capture failed:", &dma);
        goto stop_ddc;
    }

    xil_printf("[DDC VALIDATE] PASS: transport and frame contracts passed\r\n");
    result = XST_SUCCESS;

stop_ddc:
    ddc_write(DDC_CTRL_OFFSET, 0U);
    (void)ddc_wait_config_idle(config);
    if (dma_shutdown_s2mm(&dma) != XST_SUCCESS) {
        xil_printf("[DDC VALIDATE] FAIL: DMA shutdown\r\n");
        result = XST_FAILURE;
    }
    return result;
}
