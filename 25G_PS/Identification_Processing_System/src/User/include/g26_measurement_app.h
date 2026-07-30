/******************************************************************************
 * g26_measurement_app.h
 *
 * KEY1-triggered one-shot measurement application for the 2026 G question.
 ******************************************************************************/

#ifndef USER_INCLUDE_G26_MEASUREMENT_APP_H_
#define USER_INCLUDE_G26_MEASUREMENT_APP_H_

#include "../config/algorithm_config.h"
#include "g26_signal_analysis.h"

typedef struct {
    int valid;
    g26_signal_result_t result;
    float32_t one_period_mv[APP_G26_WAVEFORM_POINTS];
    float32_t three_period_mv[APP_G26_WAVEFORM_POINTS];
} g26_measurement_output_t;

/** FreeRTOS task: KEY1 measures once; KEY2 clears and rearms KEY1. */
void g26_measurement_task(void *parameters);

/** Read-only result storage for a later display task. Check valid first. */
const g26_measurement_output_t *g26_measurement_output(void);

#endif /* USER_INCLUDE_G26_MEASUREMENT_APP_H_ */
