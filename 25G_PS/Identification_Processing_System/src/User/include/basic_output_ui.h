#ifndef USER_INCLUDE_BASIC_OUTPUT_UI_H_
#define USER_INCLUDE_BASIC_OUTPUT_UI_H_

#include "../algorithms/basic_output.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BASIC_OUTPUT_FIELD_FREQUENCY = 0,
    BASIC_OUTPUT_FIELD_TARGET_VPP
} basic_output_field_t;

typedef enum {
    BASIC_OUTPUT_UI_SELECT = 0,
    BASIC_OUTPUT_UI_INCREMENT,
    BASIC_OUTPUT_UI_DECREMENT,
    BASIC_OUTPUT_UI_START,
    BASIC_OUTPUT_UI_RESET
} basic_output_ui_event_t;

typedef struct {
    open_loop_output_request_t request;
    basic_output_field_t selected_field;
    int running;
} basic_output_ui_t;

int basic_output_ui_init(basic_output_ui_t *ui,
                         float initial_frequency_hz,
                         float initial_target_output_vpp);
int basic_output_ui_handle(basic_output_ui_t *ui,
                           basic_output_ui_event_t event);
const char *basic_output_ui_field_name(basic_output_field_t field);

#ifdef __cplusplus
}
#endif

#endif
