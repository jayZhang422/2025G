/******************************************************************************
 * dma_utils.h
 *
 * Polling-mode wrapper for the sole PL S2MM DMA channel.
 ******************************************************************************/

#ifndef USER_INCLUDE_DMA_UTILS_H_
#define USER_INCLUDE_DMA_UTILS_H_

#include "xaxidma.h"
#include "xil_types.h"

void dma_dump_s2mm_regs(const char *tag, XAxiDma *dma);
int dma_init_s2mm(XAxiDma *dma, u16 device_id);
int dma_capture_frame(XAxiDma *dma, u16 device_id, u16 *buffer,
                      u32 length_bytes);

#endif /* USER_INCLUDE_DMA_UTILS_H_ */
