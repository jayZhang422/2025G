/******************************************************************************
 * dma_utils.h
 *
 * Interrupt-driven SG wrapper for the sole PL S2MM DMA channel.
 ******************************************************************************/

#ifndef USER_INCLUDE_DMA_UTILS_H_
#define USER_INCLUDE_DMA_UTILS_H_

#include "xaxidma.h"
#include "xil_types.h"

void dma_dump_s2mm_regs(const char *tag, XAxiDma *dma);
int dma_init_s2mm(XAxiDma *dma, u16 device_id);
int dma_shutdown_s2mm(XAxiDma *dma);
int dma_align_s2mm(XAxiDma *dma, u16 device_id);
int dma_submit_frame(XAxiDma *dma, void *buffer, u32 length_bytes);
int dma_wait_frame(XAxiDma *dma, u32 timeout_ms, u32 *dma_status);
int dma_capture_frame(XAxiDma *dma, u16 device_id, u16 *buffer,
                      u32 length_bytes);
u32 dma_last_s2mm_length_bytes(const XAxiDma *dma);

#endif /* USER_INCLUDE_DMA_UTILS_H_ */
