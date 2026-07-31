/******************************************************************************
 * g26_measurement_app.h
 *
 * START-triggered continuous measurement application for the 2026 G question.
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

typedef enum {
    G26_MEASUREMENT_STARTED = 1,
    G26_MEASUREMENT_COMPLETED,
    G26_MEASUREMENT_CLEARED
} g26_measurement_event_type_t;

typedef struct {
    g26_measurement_event_type_t type;
    u32 generation;
    u32 started_tick;
    u32 completed_tick;
    u8 source_page;
    int status;
} g26_measurement_event_t;

/** Create the screen request/event queues and protected result store. */
int g26_measurement_app_init(void);

/** Queue one measurement requested by a screen page. */
int g26_measurement_request(u32 generation, u8 source_page);

/** Nonblocking measurement-event poll for the screen task. */
int g26_measurement_poll_event(g26_measurement_event_t *event);

/** Copy one complete published generation under the result mutex. */
int g26_measurement_snapshot(g26_measurement_output_t *destination,
                             u32 *generation);

/** FreeRTOS task: START requests are measured; KEY2 clears and stops the loop. */
void g26_measurement_task(void *parameters);

/** Legacy single-task accessor. Concurrent consumers use snapshot(). */
const g26_measurement_output_t *g26_measurement_output(void);

#endif /* USER_INCLUDE_G26_MEASUREMENT_APP_H_ */
