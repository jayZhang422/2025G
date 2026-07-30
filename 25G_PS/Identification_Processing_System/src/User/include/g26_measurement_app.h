/******************************************************************************
 * g26_measurement_app.h
 *
 * KEY1-triggered one-shot measurement application for the 2026 G question.
 ******************************************************************************/

#ifndef USER_INCLUDE_G26_MEASUREMENT_APP_H_
#define USER_INCLUDE_G26_MEASUREMENT_APP_H_

#include "../config/algorithm_config.h"
#include "g26_signal_analysis.h"
#include "xil_types.h"

typedef struct {
    int valid;
    g26_signal_result_t result;
    float32_t one_period_mv[APP_G26_WAVEFORM_POINTS];
    float32_t three_period_mv[APP_G26_WAVEFORM_POINTS];
} g26_measurement_output_t;

typedef struct {
    u32 generation;
    u8 source_page;
    int status;
} g26_measurement_completion_t;

/** Create the HMI request/result queues and protected result store. */
int g26_measurement_app_init(void);

/** Queue one HMI-triggered measurement without using task notifications. */
int g26_measurement_request(u32 generation, u8 source_page);

/** Nonblocking completion poll: 1=message, 0=none, negative=not initialized. */
int g26_measurement_poll_completion(
    g26_measurement_completion_t *completion);

/** Copy one complete published generation under the result mutex. */
int g26_measurement_snapshot(g26_measurement_output_t *destination,
                             u32 *generation);

/** FreeRTOS task: KEY1 measures once; KEY2 clears and rearms KEY1. */
void g26_measurement_task(void *parameters);

/**
 * Legacy compatibility accessor. New concurrent consumers must use
 * g26_measurement_snapshot().
 */
const g26_measurement_output_t *g26_measurement_output(void);

#endif /* USER_INCLUDE_G26_MEASUREMENT_APP_H_ */
