/******************************************************************************
 * app_config.h
 *
 * Project-wide constants for the PL-aligned signal separation application.
 ******************************************************************************/

#ifndef USER_INCLUDE_APP_CONFIG_H_
#define USER_INCLUDE_APP_CONFIG_H_

#include "xparameters.h"

#ifndef APP_PI
#define APP_PI 3.14159265358979f
#endif

/* The PL emits one 4096-sample, 16-bit AXIS frame per TLAST. */
#define APP_FFT_LEN                 4096
#define APP_SPEC_LEN                (APP_FFT_LEN / 2)
#define APP_SAMPLE_RATE_HZ          5120800.0f
#define APP_BIN_WIDTH_HZ            (APP_SAMPLE_RATE_HZ / APP_FFT_LEN)
#define APP_RX_FRAME_BYTES          (APP_FFT_LEN * sizeof(u16))

/* Contest search domain. The PS always orders the result as fA < fB. */
#define APP_SIGNAL_MIN_HZ           20000.0f
#define APP_SIGNAL_MAX_HZ           100000.0f
#define APP_PEAK_CANDIDATE_COUNT    8
#define APP_MIN_COMPONENT_GAP_HZ    2500.0f

/* Triangle templates retain the fundamental plus odd harmonics through 15f. */
#define APP_TRI_MAX_HARMONIC        15

/*
 * Contest inputs for the required items are on a 5 kHz grid. Locking to that
 * known grid prevents FFT interpolation noise from frequency-modulating DDS.
 */
#define APP_FREQUENCY_GRID_HZ       5000.0f
#define APP_GRID_LOCK_TOLERANCE_HZ  1000.0f
#define APP_LOCK_CONFIRM_FRAMES     3U
#define APP_LOCK_MAX_RESIDUAL       0.30f
#define APP_LOCK_TIMEOUT_SECONDS    18U

/* The exported hardware platform has exactly one simple-mode S2MM DMA. */
#define APP_DMA_RX_DEV_ID           XPAR_AXI_DMA_ADC_DEVICE_ID
#define APP_DMA_CAPTURE_TIMEOUT_MS   1000U
#define APP_DMA_RESET_TIMEOUT        1000000U

/*
 * DMA writes this fixed DDR region. It is not a PL register address; fixed
 * placement is retained because linker-placed buffers failed during bring-up.
 */
#define APP_DMA_BUFFER_BASE         0x01000000U

/* DDS control BRAM is a PL contract and must always come from xparameters.h. */
#define APP_DDS_BRAM_BASEADDR       XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR
#define APP_DDS_CLOCK_HZ            125000000.0f
#define APP_DDS_UNITY_AMPLITUDE     8191U

/* The module's channel B analogue output is inverted after the DAC. */
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

/* B' initial phase is an intentional user setting, not the measured input phase. */
#define APP_B_TO_A_PHASE_DEGREES    0.0f
#define APP_PHASE_STEP_DEGREES       5.0f
#define APP_PHASE_MAX_DEGREES        180.0f

/* Synthetic algorithm regression runs before the first DMA capture. */
#define APP_ENABLE_STARTUP_SELF_TEST 1

/* KEY1 remains MIO50. The three active-low EMIO keys are Bank 2 pins 54..56. */
#define BUTTON_START                 50U
#define BUTTON_RESET                 54U
#define BUTTON_PHASE_INC             55U
#define BUTTON_PHASE_DEC             56U
#define APP_BUTTON_ACTIVE_LEVEL      0U
#define APP_BUTTON_DEBOUNCE_US       20000U
#define APP_BUTTON_POLL_US           1000U

/*
 * Board bring-up diagnostics. Keep DDS test mode at zero for normal use.
 * Setting it to one proves the BRAM -> DDS -> DAC path without ADC or DMA.
 */
#define APP_DIAG_BUILD_TAG           "BTN_LOCK_DIAG_O2_20260721"
#define APP_DIAG_FIRST_ATTEMPTS      4U
#define APP_DIAG_REPORT_PERIOD       16U
#define APP_DIAG_FORCE_DDS_TEST      0   //0: normal 1:force
#define APP_DIAG_FORCE_DDS_AMPLITUDE APP_DDS_UNITY_AMPLITUDE
#endif /* USER_INCLUDE_APP_CONFIG_H_ */
