/******************************************************************************
 * dma_utils.c
 *
 * AXI DMA helper functions. The design uses simple mode and polling, which is
 * easier to debug during bring-up than interrupt-driven transfers.
 ******************************************************************************/

#include "../config/algorithm_config.h"
#include "../config/hardware_config.h"
#include "../include/dma_utils.h"

#include "xil_cache.h"
#include "xil_printf.h"
#include "xtime_l.h"

static u16 g_dma_discard_buffer[APP_ADC_FRAME_SAMPLES]
    __attribute__((aligned(64)));
static int g_dma_needs_realign;

/** 轮询 S2MM 完成，同时区分超时和 DMA 状态错误。 */
static int dma_wait_s2mm(XAxiDma *dma)
{
    XTime start_time;
    XTime current_time;
    u32 status;

    XTime_GetTime(&start_time);
    while (XAxiDma_Busy(dma, XAXIDMA_DEVICE_TO_DMA)) {
        XTime_GetTime(&current_time);
        if ((current_time - start_time) >=
            ((XTime)APP_DMA_CAPTURE_TIMEOUT_MS *
             (XTime)COUNTS_PER_SECOND / 1000U)) {
            return XST_TIMEOUT;
        }
    }

    status = XAxiDma_ReadReg(dma->RegBase + XAXIDMA_RX_OFFSET,
                             XAXIDMA_SR_OFFSET);
    return ((status & XAXIDMA_ERR_ALL_MASK) == 0U) ?
        XST_SUCCESS : XST_FAILURE;
}

/** 读取并打印 S2MM 控制、状态、目的地址和剩余长度寄存器。 */
void dma_dump_s2mm_regs(const char *tag, XAxiDma *dma)
{
    u32 cr = XAxiDma_ReadReg(dma->RegBase + XAXIDMA_RX_OFFSET,
                             XAXIDMA_CR_OFFSET);
    u32 sr = XAxiDma_ReadReg(dma->RegBase + XAXIDMA_RX_OFFSET,
                             XAXIDMA_SR_OFFSET);
    u32 destination = XAxiDma_ReadReg(dma->RegBase + XAXIDMA_RX_OFFSET,
                                      XAXIDMA_DESTADDR_OFFSET);
    u32 remaining = XAxiDma_ReadReg(dma->RegBase + XAXIDMA_RX_OFFSET,
                                    XAXIDMA_BUFFLEN_OFFSET);

    xil_printf("%s CR=0x%08x SR=0x%08x | halted=%d idle=%d "
               "interr=%d slverr=%d decerr=%d | busy=%d "
               "da=0x%08x btt=0x%08x\r\n",
               tag,
               (unsigned int)cr, (unsigned int)sr,
               (int)((sr >> 0) & 1),
               (int)((sr >> 1) & 1),
               (int)((sr >> 4) & 1),
               (int)((sr >> 5) & 1),
               (int)((sr >> 6) & 1),
               XAxiDma_Busy(dma, XAXIDMA_DEVICE_TO_DMA),
               (unsigned int)destination, (unsigned int)remaining);
}

/** 初始化并复位只支持 simple-mode S2MM 的 ADC DMA，关闭其所有中断。 */
int dma_init_s2mm(XAxiDma *dma, u16 device_id)
{
    XAxiDma_Config *config;
    int status;
    u32 timeout = APP_DMA_RESET_TIMEOUT;

    config = XAxiDma_LookupConfig(device_id);
    if (config == NULL) {
        xil_printf("[DMA] ERROR: no config for device %d\r\n", device_id);
        return XST_FAILURE;
    }

    status = XAxiDma_CfgInitialize(dma, config);
    if (status != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: device %d init failed\r\n", device_id);
        return XST_FAILURE;
    }

    if (XAxiDma_HasSg(dma) || !dma->HasS2Mm || dma->HasMm2S) {
        xil_printf("[DMA] ERROR: device does not match simple S2MM-only PL design\r\n");
        return XST_FAILURE;
    }

    /* An ELF restart does not reset PL DMA registers. Start from a known state. */
    XAxiDma_Reset(dma);
    while (!XAxiDma_ResetIsDone(dma)) {
        if (--timeout == 0U) {
            xil_printf("[DMA] ERROR: ADC reset timeout\r\n");
            return XST_FAILURE;
        }
    }

    status = XAxiDma_CfgInitialize(dma, config);
    if (status != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: device %d reinit failed\r\n", device_id);
        return XST_FAILURE;
    }
    XAxiDma_IntrDisable(dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

    g_dma_needs_realign = 0;

    return XST_SUCCESS;
}

/** 复位 DMA 后丢弃到下一个 TLAST，使后续正式采集从帧边界开始。 */
static int dma_recover_s2mm(XAxiDma *dma, u16 device_id)
{
    int status;

    dma_dump_s2mm_regs("[DMA] S2MM before reset:", dma);
    if (dma_init_s2mm(dma, device_id) != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: receive reinitialization failed\r\n");
        g_dma_needs_realign = 1;
        return XST_FAILURE;
    }

    g_dma_needs_realign = 1;
    Xil_DCacheFlushRange((UINTPTR)g_dma_discard_buffer,
                         APP_RX_FRAME_BYTES);
    status = XAxiDma_SimpleTransfer(dma, (UINTPTR)g_dma_discard_buffer,
                                    APP_RX_FRAME_BYTES,
                                    XAXIDMA_DEVICE_TO_DMA);
    if (status == XST_SUCCESS) {
        status = dma_wait_s2mm(dma);
    }
    Xil_DCacheInvalidateRange((UINTPTR)g_dma_discard_buffer,
                              APP_RX_FRAME_BYTES);

    if (status != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: frame realignment failed (%d)\r\n",
                   status);
        dma_dump_s2mm_regs("[DMA] S2MM realignment failed:", dma);
        (void)dma_init_s2mm(dma, device_id);
        g_dma_needs_realign = 1;
        return XST_FAILURE;
    }

    g_dma_needs_realign = 0;
    dma_dump_s2mm_regs("[DMA] S2MM realigned:", dma);
    return XST_SUCCESS;
}

/** 显式重置接收通道并丢弃至下一个 TLAST，供首次正式采集前建立边界。 */
int dma_align_s2mm(XAxiDma *dma, u16 device_id)
{
    if (dma == NULL) {
        return XST_FAILURE;
    }
    return dma_recover_s2mm(dma, device_id);
}

/** 启动一帧 ADC S2MM 传输并轮询完成；失败或超时后尝试恢复 DMA。 */
int dma_capture_frame(XAxiDma *dma, u16 device_id, u16 *buffer,
                      u32 length_bytes)
{
    int status;

    if (g_dma_needs_realign &&
        dma_recover_s2mm(dma, device_id) != XST_SUCCESS) {
        return XST_FAILURE;
    }

    if (XAxiDma_SimpleTransfer(dma, (UINTPTR)buffer, length_bytes,
                               XAXIDMA_DEVICE_TO_DMA) != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: ADC start failed\r\n");
        g_dma_needs_realign = 1;
        (void)dma_recover_s2mm(dma, device_id);
        return XST_FAILURE;
    }

    status = dma_wait_s2mm(dma);
    if (status != XST_SUCCESS) {
        xil_printf((status == XST_TIMEOUT) ?
                   "[DMA] ERROR: ADC timeout\r\n" :
                   "[DMA] ERROR: ADC status error\r\n");
        g_dma_needs_realign = 1;
        (void)dma_recover_s2mm(dma, device_id);
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

/** 返回最近一次 S2MM 完成后 DMA 报告的实际接收字节数。 */
u32 dma_last_s2mm_length_bytes(const XAxiDma *dma)
{
    if (dma == NULL) {
        return 0U;
    }
    return XAxiDma_ReadReg(dma->RegBase + XAXIDMA_RX_OFFSET,
                           XAXIDMA_BUFFLEN_OFFSET);
}
