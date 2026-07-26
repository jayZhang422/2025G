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

/* Application-level button timing policy. */
#define APP_BUTTON_DEBOUNCE_US       20000U
#define APP_BUTTON_POLL_US           1000U

/*
 * Board bring-up diagnostics. Keep DDS test mode at zero for normal use.
 * Setting it to one proves the BRAM -> DDS -> DAC path without ADC or DMA.
 */
#define APP_DIAG_BUILD_TAG           "FREERTOS_SIGNAL_API_20260726"
#define APP_DIAG_FIRST_ATTEMPTS      4U
#define APP_DIAG_REPORT_PERIOD       16U
#define APP_DIAG_FORCE_DDS_TEST      0   //0: normal 1:force
#define APP_DIAG_FORCE_DDS_AMPLITUDE APP_DDS_UNITY_AMPLITUDE
#endif /* USER_INCLUDE_APP_CONFIG_H_ */
