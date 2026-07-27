/******************************************************************************
 * ddc_board_test.c
 *
 * One-shot board diagnostic for ddc_stream -> AXIS FIFO -> Simple S2MM DMA.
 ******************************************************************************/

#include "ddc_board_test.h"

#include "../config/hardware_config.h"
#include "../include/dma_utils.h"

#include <math.h>
#include <string.h>

#include "xaxidma_hw.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "xtime_l.h"

#ifndef XPAR_DDC_STREAM_0_BASEADDR
#error "The active BSP does not contain ddc_stream_0"
#endif

#define DDC_BASEADDR              XPAR_DDC_STREAM_0_BASEADDR
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

#define DDC_DEFAULT_PINC          0x63FFB333U
#define DDC_EXPECTED_BUILD_ID     0x20260727U
#define DDC_EXPECTED_DECIM        32U
#define DDC_EXPECTED_SAMPLE_RATE  5120060U
#define DDC_COMPLEX_SAMPLES       4096U
#define DDC_IQ_WORDS              (DDC_COMPLEX_SAMPLES * 2U)
#define DDC_DMA_BYTES             (DDC_IQ_WORDS * sizeof(s16))
#define DDC_TIMEOUT_MS            1000U
#define DDC_CLUSTER_THRESHOLD_HZ  2000
#define DDC_PI                    3.14159265358979f

static s16 g_ddc_iq_buffer[DDC_IQ_WORDS] __attribute__((aligned(64)));

static u32 ddc_read(u32 offset)
{
    return Xil_In32(DDC_BASEADDR + offset);
}

static void ddc_write(u32 offset, u32 value)
{
    Xil_Out32(DDC_BASEADDR + offset, value);
}

static int timeout_expired(XTime start, u32 timeout_ms)
{
    XTime now;

    XTime_GetTime(&now);
    return ((now - start) >=
            ((XTime)timeout_ms * (XTime)COUNTS_PER_SECOND / 1000U));
}

static int ddc_wait_config_idle(void)
{
    XTime start;

    XTime_GetTime(&start);
    while ((ddc_read(DDC_STATUS_OFFSET) & DDC_STATUS_CFG_BUSY) != 0U) {
        if (timeout_expired(start, DDC_TIMEOUT_MS)) {
            xil_printf("[DDC TEST] FAIL: config busy timeout\r\n");
            return XST_FAILURE;
        }
    }
    return XST_SUCCESS;
}

static int ddc_wait_reset_complete(void)
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
        if (timeout_expired(start, DDC_TIMEOUT_MS)) {
            xil_printf("[DDC TEST] FAIL: restart did not reach idle state, "
                       "STATUS=0x%08x OUT=%u FRAME=%u\r\n",
                       (unsigned int)status,
                       (unsigned int)ddc_read(DDC_OUTPUT_COUNT_OFFSET),
                       (unsigned int)ddc_read(DDC_FRAME_COUNT_OFFSET));
            return XST_FAILURE;
        }
    }
}

static int ddc_wait_first_frame(void)
{
    XTime start;
    u32 status;

    XTime_GetTime(&start);
    while (ddc_read(DDC_FRAME_COUNT_OFFSET) == 0U) {
        status = ddc_read(DDC_STATUS_OFFSET);
        if ((status & DDC_STATUS_FAULT) != 0U) {
            xil_printf("[DDC TEST] FAIL: DDC fault before first frame, "
                       "STATUS=0x%08x\r\n", (unsigned int)status);
            return XST_FAILURE;
        }
        if (timeout_expired(start, DDC_TIMEOUT_MS)) {
            xil_printf("[DDC TEST] FAIL: first-frame timeout, "
                       "STATUS=0x%08x OUT=%u\r\n",
                       (unsigned int)status,
                       (unsigned int)ddc_read(DDC_OUTPUT_COUNT_OFFSET));
            return XST_FAILURE;
        }
    }
    return XST_SUCCESS;
}

static int dma_wait_complete(XAxiDma *dma)
{
    XTime start;
    u32 status;

    XTime_GetTime(&start);
    while (XAxiDma_Busy(dma, XAXIDMA_DEVICE_TO_DMA)) {
        if (timeout_expired(start, DDC_TIMEOUT_MS)) {
            xil_printf("[DDC TEST] FAIL: DMA timeout\r\n");
            dma_dump_s2mm_regs("[DDC TEST] DMA timeout:", dma);
            return XST_FAILURE;
        }
    }

    status = XAxiDma_ReadReg(dma->RegBase + XAXIDMA_RX_OFFSET,
                             XAXIDMA_SR_OFFSET);
    if ((status & XAXIDMA_ERR_ALL_MASK) != 0U) {
        xil_printf("[DDC TEST] FAIL: DMA status error, SR=0x%08x\r\n",
                   (unsigned int)status);
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static s32 round_to_s32(float value)
{
    return (value >= 0.0f) ? (s32)(value + 0.5f) : (s32)(value - 0.5f);
}

static void ddc_report_samples(u32 adc_sample_rate_hz, u32 decim)
{
    s64 sum_i = 0;
    s64 sum_q = 0;
    s64 energy = 0;
    s64 frequency_sum = 0;
    s64 positive_sum = 0;
    s64 negative_sum = 0;
    s16 min_i = 32767;
    s16 max_i = -32768;
    s16 min_q = 32767;
    s16 max_q = -32768;
    u32 saturation_count = 0U;
    u32 frequency_count = 0U;
    u32 positive_count = 0U;
    u32 negative_count = 0U;
    u32 dc_percent = 0U;
    u32 index;
    float baseband_sample_rate = (float)adc_sample_rate_hz / (float)decim;

    for (index = 0U; index < DDC_COMPLEX_SAMPLES; ++index) {
        s32 i = g_ddc_iq_buffer[2U * index];
        s32 q = g_ddc_iq_buffer[2U * index + 1U];

        if (i < min_i) min_i = (s16)i;
        if (i > max_i) max_i = (s16)i;
        if (q < min_q) min_q = (s16)q;
        if (q > max_q) max_q = (s16)q;
        if (i == 32767 || i == -32768 || q == 32767 || q == -32768)
            saturation_count++;
        sum_i += i;
        sum_q += q;
        energy += (s64)i * i + (s64)q * q;

        if (index != 0U) {
            s32 previous_i = g_ddc_iq_buffer[2U * (index - 1U)];
            s32 previous_q = g_ddc_iq_buffer[2U * (index - 1U) + 1U];
            s64 dot = (s64)previous_i * i + (s64)previous_q * q;
            s64 cross = (s64)previous_i * q - (s64)previous_q * i;

            if (dot != 0 || cross != 0) {
                s32 frequency_hz = round_to_s32(
                    atan2f((float)cross, (float)dot) *
                    baseband_sample_rate / (2.0f * DDC_PI));

                frequency_sum += frequency_hz;
                frequency_count++;
                if (frequency_hz > DDC_CLUSTER_THRESHOLD_HZ) {
                    positive_sum += frequency_hz;
                    positive_count++;
                } else if (frequency_hz < -DDC_CLUSTER_THRESHOLD_HZ) {
                    negative_sum += frequency_hz;
                    negative_count++;
                }
            }
        }
    }

    if (energy != 0) {
        s64 dc_numerator = (sum_i * sum_i + sum_q * sum_q) * 100;
        s64 dc_denominator = (s64)DDC_COMPLEX_SAMPLES * energy;
        dc_percent = (u32)(dc_numerator / dc_denominator);
    }

    xil_printf("[DDC TEST] I min=%d max=%d | Q min=%d max=%d | sat=%u\r\n",
               (int)min_i, (int)max_i, (int)min_q, (int)max_q,
               (unsigned int)saturation_count);
    xil_printf("[DDC TEST] mean_freq=%d Hz dc_power=%u%%\r\n",
               (frequency_count != 0U) ?
                   (int)(frequency_sum / (s64)frequency_count) : 0,
               (unsigned int)dc_percent);
    xil_printf("[DDC TEST] negative=%d Hz (%u) positive=%d Hz (%u)\r\n",
               (negative_count != 0U) ?
                   (int)(negative_sum / (s64)negative_count) : 0,
               (unsigned int)negative_count,
               (positive_count != 0U) ?
                   (int)(positive_sum / (s64)positive_count) : 0,
               (unsigned int)positive_count);

    for (index = 0U; index < 8U; ++index) {
        xil_printf("[DDC TEST] IQ[%u]=%d,%d\r\n", (unsigned int)index,
                   (int)g_ddc_iq_buffer[2U * index],
                   (int)g_ddc_iq_buffer[2U * index + 1U]);
    }
}

int ddc_board_test_run(void)
{
    XAxiDma dma;
    u32 build_id;
    u32 decim;
    u32 sample_rate_hz;
    u32 status;
    u32 output_count;
    u32 frame_count;
    u32 dma_length;
    int result = XST_FAILURE;

    xil_printf("\r\n[DDC TEST] one-frame board diagnostic\r\n");
    xil_printf("[DDC TEST] DDC base=0x%08x DMA bytes=%u\r\n",
               (unsigned int)DDC_BASEADDR, (unsigned int)DDC_DMA_BYTES);

    build_id = ddc_read(DDC_BUILD_ID_OFFSET);
    decim = ddc_read(DDC_DECIM_OFFSET);
    sample_rate_hz = ddc_read(DDC_SAMPLE_RATE_OFFSET);
    xil_printf("[DDC TEST] BUILD=0x%08x DECIM=%u ADC_FS=%u\r\n",
               (unsigned int)build_id, (unsigned int)decim,
               (unsigned int)sample_rate_hz);
    if (build_id != DDC_EXPECTED_BUILD_ID ||
        decim != DDC_EXPECTED_DECIM ||
        sample_rate_hz != DDC_EXPECTED_SAMPLE_RATE) {
        xil_printf("[DDC TEST] FAIL: bitstream/BSP contract mismatch\r\n");
        return XST_FAILURE;
    }

    if (dma_init_s2mm(&dma, APP_DMA_RX_DEV_ID) != XST_SUCCESS) {
        xil_printf("[DDC TEST] FAIL: DMA init\r\n");
        return XST_FAILURE;
    }

    ddc_write(DDC_PINC_OFFSET, DDC_DEFAULT_PINC);
    ddc_write(DDC_CTRL_OFFSET, DDC_CTRL_RESTART | DDC_CTRL_CLEAR_FAULT);
    if (ddc_wait_config_idle() != XST_SUCCESS ||
        ddc_wait_reset_complete() != XST_SUCCESS) {
        return XST_FAILURE;
    }

    memset(g_ddc_iq_buffer, 0xA5, sizeof(g_ddc_iq_buffer));
    Xil_DCacheFlushRange((UINTPTR)g_ddc_iq_buffer, DDC_DMA_BYTES);
    if (XAxiDma_SimpleTransfer(&dma, (UINTPTR)g_ddc_iq_buffer,
                               DDC_DMA_BYTES,
                               XAXIDMA_DEVICE_TO_DMA) != XST_SUCCESS) {
        xil_printf("[DDC TEST] FAIL: DMA start\r\n");
        return XST_FAILURE;
    }

    ddc_write(DDC_CTRL_OFFSET, DDC_CTRL_RUN);
    if (ddc_wait_config_idle() != XST_SUCCESS ||
        ddc_wait_first_frame() != XST_SUCCESS) {
        goto stop_ddc;
    }

    /* Stop at exactly one generated frame so no partial second frame remains. */
    ddc_write(DDC_CTRL_OFFSET, 0U);
    if (ddc_wait_config_idle() != XST_SUCCESS ||
        dma_wait_complete(&dma) != XST_SUCCESS) {
        goto stop_ddc;
    }

    Xil_DCacheInvalidateRange((UINTPTR)g_ddc_iq_buffer, DDC_DMA_BYTES);
    status = ddc_read(DDC_STATUS_OFFSET);
    output_count = ddc_read(DDC_OUTPUT_COUNT_OFFSET);
    frame_count = ddc_read(DDC_FRAME_COUNT_OFFSET);
    dma_length = dma_last_s2mm_length_bytes(&dma);
    dma_dump_s2mm_regs("[DDC TEST] DMA complete:", &dma);
    xil_printf("[DDC TEST] STATUS=0x%08x OUT=%u FRAME=%u DMA_LEN=%u\r\n",
               (unsigned int)status, (unsigned int)output_count,
               (unsigned int)frame_count, (unsigned int)dma_length);

    if ((status & (DDC_STATUS_RUNNING | DDC_STATUS_FAULT)) != 0U ||
        output_count != DDC_COMPLEX_SAMPLES || frame_count != 1U ||
        dma_length != DDC_DMA_BYTES) {
        xil_printf("[DDC TEST] FAIL: one-frame contract mismatch\r\n");
        goto stop_ddc;
    }

    ddc_report_samples(sample_rate_hz, decim);
    xil_printf("[DDC TEST] PASS: return this complete UART log\r\n");
    result = XST_SUCCESS;

stop_ddc:
    ddc_write(DDC_CTRL_OFFSET, 0U);
    (void)ddc_wait_config_idle();
    return result;
}
