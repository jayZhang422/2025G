/******************************************************************************
 * hardware_config.h
 *
 * Fixed PS/PL hardware contracts. Contest modes must not override these.
 ******************************************************************************/

#ifndef USER_CONFIG_HARDWARE_CONFIG_H_
#define USER_CONFIG_HARDWARE_CONFIG_H_

#include "ad_fifo_monitor_axi.h"
#include "xil_types.h"
#include "xparameters.h"

/* The PL emits one 4096-sample, 16-bit AXIS frame per TLAST. */
#define APP_ADC_FRAME_SAMPLES       4096U
#define APP_SAMPLE_RATE_HZ          5120060.0f
#define APP_RX_FRAME_BYTES          (APP_ADC_FRAME_SAMPLES * sizeof(u16))

/* The exported hardware platform has exactly one simple-mode S2MM DMA. */
#define APP_DMA_RX_DEV_ID           XPAR_AXI_DMA_ADC_DEVICE_ID
#define APP_FIFO_MONITOR_BASEADDR \
    XPAR_AD_FIFO_MONITOR_AXI_0_AD_FIFO_MONITOR_AXI_BASEADDR

/* The current DDC bitstream emits one interleaved I/Q frame per TLAST. */
#ifndef XPAR_DDC_STREAM_0_BASEADDR
#error "The active BSP does not contain ddc_stream_0"
#endif
#define APP_DDC_BASEADDR             XPAR_DDC_STREAM_0_BASEADDR
#define APP_DDC_FRAME_COMPLEX_SAMPLES 4096U
#define APP_DDC_FRAME_IQ_WORDS       (APP_DDC_FRAME_COMPLEX_SAMPLES * 2U)
#define APP_DDC_RX_FRAME_BYTES       (APP_DDC_FRAME_IQ_WORDS * sizeof(s16))

/* DDS control BRAM address and ten-word PL register protocol. */
#define APP_DDS_BRAM_BASEADDR       XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR
#define APP_DDS_CLOCK_HZ            125000000.0f
#define APP_DDS_UNITY_AMPLITUDE     8191U
#define APP_DDS_B_PHASE_COMPENSATION_DEGREES 180.0f

#define APP_DDS_A_WAVE_OFFSET       0x00U
#define APP_DDS_A_STEP_OFFSET       0x04U
#define APP_DDS_A_PHASE_OFFSET      0x08U
#define APP_DDS_A_AMPLITUDE_OFFSET  0x0CU
#define APP_DDS_B_WAVE_OFFSET       0x10U
#define APP_DDS_B_STEP_OFFSET       0x14U
#define APP_DDS_B_PHASE_OFFSET      0x18U
#define APP_DDS_B_AMPLITUDE_OFFSET  0x1CU
#define APP_DDS_CONTROL_OFFSET      0x20U
#define APP_DDS_COMMIT_OFFSET       0x24U

#define APP_DDS_CONTROL_RUN         0x01U
#define APP_DDS_CONTROL_PHASE_LOAD  0x02U
#define APP_DDS_CONTROL_B_PHASE_ADJUST 0x04U

/* PL IQ detector address and ADC-domain reference clock. */
#define APP_IQ_BASEADDR             XPAR_IQ_DEMODULATOR_0_BASEADDR
#define APP_IQ_ADC_CLOCK_HZ         APP_SAMPLE_RATE_HZ

/* FIFO monitor exposes an explicit version register in its BSP contract. */
#define APP_FIFO_MONITOR_PROTOCOL_VERSION AD_FIFO_MONITOR_AXI_VERSION

/* KEY1 is MIO50; the remaining active-low keys are EMIO54..56. */
#define BUTTON_START                50U
#define BUTTON_RESET                54U
#define BUTTON_PHASE_INC            55U
#define BUTTON_PHASE_DEC            56U
#define APP_BUTTON_ACTIVE_LEVEL     0U
#define APP_GPIO_DEVICE_ID          XPAR_XGPIOPS_0_DEVICE_ID

#endif /* USER_CONFIG_HARDWARE_CONFIG_H_ */
