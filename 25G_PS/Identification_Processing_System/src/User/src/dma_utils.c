/******************************************************************************
 * dma_utils.c
 *
 * AXI DMA helper functions for the interrupt-driven SG S2MM channel.
 ******************************************************************************/

#include "../config/algorithm_config.h"
#include "../config/hardware_config.h"
#include "../include/dma_utils.h"

#include "FreeRTOS.h"
#include "task.h"
#include "xaxidma_hw.h"
#include "xil_cache.h"
#include "xil_printf.h"

#define DMA_RX_BD_COUNT 2U

static u16 g_dma_discard_buffer[APP_ADC_FRAME_SAMPLES]
    __attribute__((aligned(64)));
/* Alternate descriptors so hardware never refetches the BD it just completed. */
static u8 g_dma_bd_space[XAXIDMA_BD_MINIMUM_ALIGNMENT * DMA_RX_BD_COUNT]
    __attribute__((aligned(XAXIDMA_BD_MINIMUM_ALIGNMENT)));
static XAxiDma *g_dma_instance;
static TaskHandle_t g_dma_wait_task;
static volatile u32 g_dma_irq_status;
static volatile int g_dma_active;
static u32 g_dma_last_length_bytes;
static int g_dma_needs_realign;

static void dma_s2mm_isr(void *callback)
{
    XAxiDma_BdRing *ring;
    BaseType_t higher_priority_task_woken = pdFALSE;
    u32 irq_status;

    (void)callback;
    if (g_dma_instance == NULL) {
        return;
    }

    ring = XAxiDma_GetRxRing(g_dma_instance);
    irq_status = XAxiDma_BdRingGetIrq(ring);
    XAxiDma_BdRingAckIrq(ring, irq_status);
    if ((irq_status & XAXIDMA_IRQ_ALL_MASK) == 0U) {
        return;
    }

    g_dma_irq_status |= irq_status;
    if (g_dma_wait_task != NULL) {
        vTaskNotifyGiveFromISR(g_dma_wait_task,
                               &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

static int dma_wait_reset(XAxiDma *dma)
{
    u32 timeout = APP_DMA_RESET_TIMEOUT;

    while (!XAxiDma_ResetIsDone(dma)) {
        if (--timeout == 0U) {
            return XST_TIMEOUT;
        }
    }
    return XST_SUCCESS;
}

/** 读取并打印 SG S2MM 控制、状态和描述符 ring 状态。 */
void dma_dump_s2mm_regs(const char *tag, XAxiDma *dma)
{
    XAxiDma_BdRing *ring;
    u32 cr;
    u32 sr;
    u32 current_descriptor;
    u32 tail_descriptor;

    if (dma == NULL) {
        return;
    }

    ring = XAxiDma_GetRxRing(dma);
    cr = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_CR_OFFSET);
    sr = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET);
    current_descriptor = XAxiDma_ReadReg(ring->ChanBase,
                                         XAXIDMA_CDESC_OFFSET);
    tail_descriptor = XAxiDma_ReadReg(ring->ChanBase,
                                      XAXIDMA_TDESC_OFFSET);

    xil_printf("%s CR=0x%08x SR=0x%08x | halted=%d idle=%d "
               "interr=%d slverr=%d decerr=%d "
               "sginterr=%d sgslverr=%d sgdecerr=%d | active=%d "
               "free=%d hw=%d post=%d cdesc=0x%08x tdesc=0x%08x\r\n",
               tag,
               (unsigned int)cr, (unsigned int)sr,
               (int)((sr >> 0) & 1U),
               (int)((sr >> 1) & 1U),
               (int)((sr >> 4) & 1U),
               (int)((sr >> 5) & 1U),
               (int)((sr >> 6) & 1U),
               (int)((sr >> 8) & 1U),
               (int)((sr >> 9) & 1U),
               (int)((sr >> 10) & 1U),
               (int)g_dma_active,
               ring->FreeCnt, ring->HwCnt, ring->PostCnt,
               (unsigned int)current_descriptor,
               (unsigned int)tail_descriptor);
}

/** 初始化、复位并挂接只支持 SG S2MM 的 ADC DMA 中断。 */
int dma_init_s2mm(XAxiDma *dma, u16 device_id)
{
    XAxiDma_BdRing *ring;
    XAxiDma_Bd template_bd;
    XAxiDma_Config *config;
    int status;

    if (dma == NULL) {
        return XST_FAILURE;
    }

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
    if (!XAxiDma_HasSg(dma) || !dma->HasS2Mm || dma->HasMm2S) {
        xil_printf("[DMA] ERROR: device does not match SG S2MM-only PL design\r\n");
        return XST_FAILURE;
    }

    vPortDisableInterrupt((uint8_t)APP_DMA_RX_INTR_ID);
    ring = XAxiDma_GetRxRing(dma);
    XAxiDma_BdRingIntDisable(ring, XAXIDMA_IRQ_ALL_MASK);

    /* An ELF restart does not reset PL DMA registers. Start from a known state. */
    XAxiDma_Reset(dma);
    if (dma_wait_reset(dma) != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: ADC reset timeout\r\n");
        return XST_FAILURE;
    }

    status = XAxiDma_CfgInitialize(dma, config);
    if (status != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: device %d reinit failed\r\n", device_id);
        return XST_FAILURE;
    }

    ring = XAxiDma_GetRxRing(dma);
    XAxiDma_BdRingIntDisable(ring, XAXIDMA_IRQ_ALL_MASK);
    status = XAxiDma_BdRingCreate(
        ring, (UINTPTR)g_dma_bd_space, (UINTPTR)g_dma_bd_space,
        XAXIDMA_BD_MINIMUM_ALIGNMENT, DMA_RX_BD_COUNT);
    if (status != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: RX BD ring create failed (%d)\r\n", status);
        return XST_FAILURE;
    }

    XAxiDma_BdClear(&template_bd);
    status = XAxiDma_BdRingClone(ring, &template_bd);
    if (status != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: RX BD clone failed (%d)\r\n", status);
        return XST_FAILURE;
    }
    status = XAxiDma_BdRingSetCoalesce(ring, 1U, 0U);
    if (status != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: RX IRQ coalesce setup failed (%d)\r\n",
                   status);
        return XST_FAILURE;
    }

    g_dma_instance = dma;
    g_dma_wait_task = NULL;
    g_dma_irq_status = 0U;
    g_dma_active = 0;
    g_dma_last_length_bytes = 0U;
    g_dma_needs_realign = 0;

    XAxiDma_BdRingAckIrq(ring, XAXIDMA_IRQ_ALL_MASK);
    if (xPortInstallInterruptHandler(
            (uint8_t)APP_DMA_RX_INTR_ID,
            (XInterruptHandler)dma_s2mm_isr, NULL) != pdPASS) {
        xil_printf("[DMA] ERROR: S2MM IRQ install failed\r\n");
        g_dma_instance = NULL;
        return XST_FAILURE;
    }
    XAxiDma_BdRingIntEnable(ring, XAXIDMA_IRQ_ALL_MASK);
    vPortEnableInterrupt((uint8_t)APP_DMA_RX_INTR_ID);
    return XST_SUCCESS;
}

int dma_shutdown_s2mm(XAxiDma *dma)
{
    int status;

    if (dma == NULL || dma != g_dma_instance) {
        return XST_FAILURE;
    }

    vPortDisableInterrupt((uint8_t)APP_DMA_RX_INTR_ID);
    XAxiDma_BdRingIntDisable(XAxiDma_GetRxRing(dma),
                             XAXIDMA_IRQ_ALL_MASK);
    XAxiDma_BdRingAckIrq(XAxiDma_GetRxRing(dma),
                         XAXIDMA_IRQ_ALL_MASK);
    XAxiDma_Reset(dma);
    status = dma_wait_reset(dma);

    g_dma_instance = NULL;
    g_dma_wait_task = NULL;
    g_dma_irq_status = 0U;
    g_dma_active = 0;
    g_dma_last_length_bytes = 0U;
    g_dma_needs_realign = 0;
    return status;
}

/** 将一个接收 BD 交给硬件；完成由 S2MM IRQ 通知当前任务。 */
int dma_submit_frame(XAxiDma *dma, void *buffer, u32 length_bytes)
{
    XAxiDma_BdRing *ring;
    XAxiDma_Bd *bd;
    int status;

    if (dma == NULL || buffer == NULL || length_bytes == 0U ||
        dma != g_dma_instance || g_dma_active) {
        return XST_FAILURE;
    }

    ring = XAxiDma_GetRxRing(dma);
    while (ulTaskNotifyTake(pdTRUE, 0U) != 0U) {
    }
    g_dma_wait_task = xTaskGetCurrentTaskHandle();
    g_dma_irq_status = 0U;
    g_dma_last_length_bytes = 0U;
    XAxiDma_BdRingAckIrq(ring, XAXIDMA_IRQ_ALL_MASK);

    status = XAxiDma_BdRingAlloc(ring, 1, &bd);
    if (status != XST_SUCCESS) {
        g_dma_wait_task = NULL;
        return status;
    }

    status = XAxiDma_BdSetBufAddr(bd, (UINTPTR)buffer);
    if (status == XST_SUCCESS) {
        status = XAxiDma_BdSetLength(bd, length_bytes,
                                     ring->MaxTransferLen);
    }
    if (status != XST_SUCCESS) {
        (void)XAxiDma_BdRingUnAlloc(ring, 1, bd);
        g_dma_wait_task = NULL;
        return status;
    }

    XAxiDma_BdSetCtrl(bd, 0U);
    XAxiDma_BdSetId(bd, buffer);
    XAxiDma_BdWrite(bd, XAXIDMA_BD_STS_OFFSET, 0U);
    g_dma_active = 1;
    status = XAxiDma_BdRingToHw(ring, 1, bd);
    if (status != XST_SUCCESS) {
        g_dma_active = 0;
        g_dma_wait_task = NULL;
        (void)XAxiDma_BdRingUnAlloc(ring, 1, bd);
        return status;
    }

    if (ring->RunState == AXIDMA_CHANNEL_HALTED) {
        status = XAxiDma_BdRingStart(ring);
        if (status != XST_SUCCESS) {
            xil_printf("[DMA] ERROR: RX BD ring start failed (%d)\r\n",
                       status);
        }
    }
    return status;
}

/** 阻塞当前任务直到 S2MM IRQ 到达，并回收完成的接收 BD。 */
int dma_wait_frame(XAxiDma *dma, u32 timeout_ms, u32 *dma_status)
{
    XAxiDma_BdRing *ring;
    XAxiDma_Bd *bd;
    TickType_t timeout_ticks;
    u32 bd_status;
    u32 status_register;
    int bd_count;
    int free_status;

    if (dma == NULL || timeout_ms == 0U || dma != g_dma_instance ||
        !g_dma_active) {
        return XST_FAILURE;
    }

    timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    if (timeout_ticks == 0U) {
        timeout_ticks = 1U;
    }
    if (ulTaskNotifyTake(pdTRUE, timeout_ticks) == 0U) {
        return XST_TIMEOUT;
    }

    ring = XAxiDma_GetRxRing(dma);
    status_register = XAxiDma_ReadReg(ring->ChanBase,
                                      XAXIDMA_SR_OFFSET);
    if (dma_status != NULL) {
        *dma_status = status_register;
    }
    if ((g_dma_irq_status & XAXIDMA_IRQ_ERROR_MASK) != 0U ||
        (status_register & XAXIDMA_ERR_ALL_MASK) != 0U) {
        return XST_FAILURE;
    }

    bd_count = XAxiDma_BdRingFromHw(ring, 1, &bd);
    if (bd_count != 1) {
        return XST_FAILURE;
    }

    bd_status = XAxiDma_BdGetSts(bd);
    g_dma_last_length_bytes =
        XAxiDma_BdGetActualLength(bd, ring->MaxTransferLen);
    free_status = XAxiDma_BdRingFree(ring, 1, bd);
    g_dma_active = 0;
    g_dma_wait_task = NULL;

    if (free_status != XST_SUCCESS ||
        (bd_status & XAXIDMA_BD_STS_ALL_ERR_MASK) != 0U ||
        (bd_status & XAXIDMA_BD_STS_COMPLETE_MASK) == 0U) {
        return XST_FAILURE;
    }
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
    status = dma_submit_frame(dma, g_dma_discard_buffer,
                              APP_RX_FRAME_BYTES);
    if (status == XST_SUCCESS) {
        status = dma_wait_frame(dma, APP_DMA_CAPTURE_TIMEOUT_MS, NULL);
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

    g_dma_last_length_bytes = 0U;
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

/** 提交一帧 ADC SG S2MM 传输并等待中断；失败后尝试恢复 DMA。 */
int dma_capture_frame(XAxiDma *dma, u16 device_id, u16 *buffer,
                      u32 length_bytes)
{
    int status;

    if (g_dma_needs_realign &&
        dma_recover_s2mm(dma, device_id) != XST_SUCCESS) {
        return XST_FAILURE;
    }

    if (dma_submit_frame(dma, buffer, length_bytes) != XST_SUCCESS) {
        xil_printf("[DMA] ERROR: ADC SG submit failed\r\n");
        g_dma_needs_realign = 1;
        (void)dma_recover_s2mm(dma, device_id);
        return XST_FAILURE;
    }

    status = dma_wait_frame(dma, APP_DMA_CAPTURE_TIMEOUT_MS, NULL);
    if (status != XST_SUCCESS) {
        xil_printf((status == XST_TIMEOUT) ?
                   "[DMA] ERROR: ADC IRQ timeout\r\n" :
                   "[DMA] ERROR: ADC SG status error\r\n");
        g_dma_needs_realign = 1;
        (void)dma_recover_s2mm(dma, device_id);
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

/** 返回最近一个完成 BD 报告的实际接收字节数。 */
u32 dma_last_s2mm_length_bytes(const XAxiDma *dma)
{
    return (dma == g_dma_instance) ? g_dma_last_length_bytes : 0U;
}
