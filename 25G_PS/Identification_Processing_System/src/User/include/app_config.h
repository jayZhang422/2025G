/******************************************************************************
 * app_config.h
 *
 * Application policy and compatibility includes for the signal separator.
 ******************************************************************************/

#ifndef USER_INCLUDE_APP_CONFIG_H_
#define USER_INCLUDE_APP_CONFIG_H_

#include "../config/algorithm_config.h"
#include "../config/hardware_config.h"
#include "../config/signal_profiles.h"

/* B' initial phase is an intentional user setting, not the measured input phase. */
#define APP_B_TO_A_PHASE_DEGREES    0.0f
#define APP_PHASE_STEP_DEGREES       5.0f
#define APP_PHASE_MAX_DEGREES        180.0f

/* Application-level button timing policy. */
#define APP_BUTTON_DEBOUNCE_US       20000U
#define APP_BUTTON_POLL_US           1000U

/*
 * Board bring-up diagnostics. Keep DDS test mode at zero for normal use.
 * Setting it to one proves the BRAM -> DDS -> DAC path without ADC or DMA.
 */
#define APP_DIAG_BUILD_TAG           "FREERTOS_PROFILE_CONFIG_20260725"
#define APP_DIAG_FIRST_ATTEMPTS      4U
#define APP_DIAG_REPORT_PERIOD       16U
#define APP_DIAG_FORCE_DDS_TEST      0   //0: normal 1:force
#define APP_DIAG_FORCE_DDS_AMPLITUDE APP_DDS_UNITY_AMPLITUDE
#endif /* USER_INCLUDE_APP_CONFIG_H_ */
