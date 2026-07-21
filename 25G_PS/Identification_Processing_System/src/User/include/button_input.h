/******************************************************************************
 * button_input.h
 *
 * Debounced PS MIO button access for the pre-start control state.
 ******************************************************************************/

#ifndef USER_INCLUDE_BUTTON_INPUT_H_
#define USER_INCLUDE_BUTTON_INPUT_H_

#include "xgpiops.h"
#include "xstatus.h"

typedef struct {
    XGpioPs instance;
} button_input_t;

int button_input_init(button_input_t *buttons);
int button_input_take_start_press(button_input_t *buttons);
int button_input_take_reset_press(button_input_t *buttons);
int button_input_take_phase_increment_press(button_input_t *buttons);
int button_input_take_phase_decrement_press(button_input_t *buttons);
u32 button_input_read_start_level(const button_input_t *buttons);
u32 button_input_read_reset_level(const button_input_t *buttons);
u32 button_input_read_phase_increment_level(const button_input_t *buttons);
u32 button_input_read_phase_decrement_level(const button_input_t *buttons);

#endif /* USER_INCLUDE_BUTTON_INPUT_H_ */
