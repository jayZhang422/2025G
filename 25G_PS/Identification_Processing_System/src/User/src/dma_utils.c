/******************************************************************************
 * dma_utils.c
 *
 * AXI DMA helper functions. The design uses simple mode and polling, which is
 * easier to debug during bring-up than interrupt-driven transfers.
 ******************************************************************************/

#include "../include/app_config.h"
#include "../include/dma_utils.h"

#include "xil_printf.h"
#include "xtime_l.h"

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

int dma_init_s2mm(XAxiDma *dma, u16 device_id)
{
    XAxiDma_Config *config;
    int status;
    u32 timeout = APP_DMA_RESET_TIMEOUT;

    config = XAxiDma_LookupConfig(device_id);
    if (config == NULL) {
        xil_printf("ERROR: no config for DMA %d\r\n", device_id);
        return XST_FAILURE;
    }

    status = XAxiDma_CfgInitialize(dma, config);
    if (status != XST_SUCCESS) {
        xil_printf("ERROR: DMA %d init failed\r\n", device_id);
        return XST_FAILURE;
    }

    if (XAxiDma_HasSg(dma) || !dma->HasS2Mm || dma->HasMm2S) {
        xil_printf("ERROR: DMA does not match simple S2MM-only PL design\r\n");
        return XST_FAILURE;
    }

    /* An ELF restart does not reset PL DMA registers. Start from a known state. */
    XAxiDma_Reset(dma);
    while (!XAxiDma_ResetIsDone(dma)) {
        if (--timeout == 0U) {
            xil_printf("ERROR: ADC DMA reset timeout\r\n");
            return XST_FAILURE;
        }
    }

    status = XAxiDma_CfgInitialize(dma, config);
    if (status != XST_SUCCESS) {
        xil_printf("ERROR: DMA %d reinit failed\r\n", device_id);
        return XST_FAILURE;
    }
    XAxiDma_IntrDisable(dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

    return XST_SUCCESS;
}

static int dma_recover_s2mm(XAxiDma *dma, u16 device_id)
{
    dma_dump_s2mm_regs("S2MM before reset:", dma);
    if (dma_init_s2mm(dma, device_id) != XST_SUCCESS) {
        xil_printf("ERROR: ADC DMA receive reinitialization failed\r\n");
        return XST_FAILURE;
    }
    dma_dump_s2mm_regs("S2MM recovered:", dma);
    return XST_SUCCESS;
}

int dma_capture_frame(XAxiDma *dma, u16 device_id, u16 *buffer,
                      u32 length_bytes)
{
    XTime start_time;
    XTime current_time;

    if (XAxiDma_SimpleTransfer(dma, (UINTPTR)buffer, length_bytes,
                               XAXIDMA_DEVICE_TO_DMA) != XST_SUCCESS) {
        xil_printf("ERROR: ADC DMA start failed\r\n");
        (void)dma_recover_s2mm(dma, device_id);
        return XST_FAILURE;
    }

    XTime_GetTime(&start_time);
    while (XAxiDma_Busy(dma, XAXIDMA_DEVICE_TO_DMA)) {
        XTime_GetTime(&current_time);
        if ((current_time - start_time) >=
            ((XTime)APP_DMA_CAPTURE_TIMEOUT_MS *
             (XTime)COUNTS_PER_SECOND / 1000U)) {
            xil_printf("ERROR: ADC DMA timeout\r\n");
            (void)dma_recover_s2mm(dma, device_id);
            return XST_FAILURE;
        }
    }

    return XST_SUCCESS;
}
