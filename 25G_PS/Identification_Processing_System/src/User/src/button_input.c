/******************************************************************************
 * button_input.c
 *
 * Each API consumes one active-low press after debounce and release.
 ******************************************************************************/

#include "../include/app_config.h"
#include "../include/button_input.h"

#include "sleep.h"

/*Debouncing Func*/
static int button_input_take_press(button_input_t *buttons, u32 pin)
{
    if (XGpioPs_ReadPin(&buttons->instance, pin) !=
        APP_BUTTON_ACTIVE_LEVEL) {
        return 0;
    }

    usleep(APP_BUTTON_DEBOUNCE_US);
    if (XGpioPs_ReadPin(&buttons->instance, pin) !=
        APP_BUTTON_ACTIVE_LEVEL) {
        return 0;
    }

    while (XGpioPs_ReadPin(&buttons->instance, pin) ==
           APP_BUTTON_ACTIVE_LEVEL) {
        usleep(APP_BUTTON_POLL_US);
    }
    return 1;
}

/*init Func*/
int button_input_init(button_input_t *buttons)
{
    XGpioPs_Config *config;

    if (buttons == 0) {
        return XST_FAILURE;
    }

    config = XGpioPs_LookupConfig(XPAR_XGPIOPS_0_DEVICE_ID);
    if (config == 0 || XGpioPs_CfgInitialize(&buttons->instance, config,
                                              config->BaseAddr) != XST_SUCCESS) {
        return XST_FAILURE;
    }

    XGpioPs_SetDirectionPin(&buttons->instance, BUTTON_START, 0U);
    XGpioPs_SetDirectionPin(&buttons->instance, BUTTON_RESET, 0U);
    XGpioPs_SetDirectionPin(&buttons->instance, BUTTON_PHASE_INC, 0U);
    XGpioPs_SetDirectionPin(&buttons->instance, BUTTON_PHASE_DEC, 0U);
    XGpioPs_SetOutputEnablePin(&buttons->instance, BUTTON_START, 0U);
    XGpioPs_SetOutputEnablePin(&buttons->instance, BUTTON_RESET, 0U);
    XGpioPs_SetOutputEnablePin(&buttons->instance, BUTTON_PHASE_INC, 0U);
    XGpioPs_SetOutputEnablePin(&buttons->instance, BUTTON_PHASE_DEC, 0U);
    return XST_SUCCESS;
}

/*start Func*/
int button_input_take_start_press(button_input_t *buttons)
{
    return button_input_take_press(buttons, BUTTON_START);
}

/*reset Func*/
int button_input_take_reset_press(button_input_t *buttons)
{
    return button_input_take_press(buttons, BUTTON_RESET);
}

/*phase increment Func*/
int button_input_take_phase_increment_press(button_input_t *buttons)
{
    return button_input_take_press(buttons, BUTTON_PHASE_INC);
}

/*phase decrement Func*/
int button_input_take_phase_decrement_press(button_input_t *buttons)
{
    return button_input_take_press(buttons, BUTTON_PHASE_DEC);
}

/*read level*/
u32 button_input_read_start_level(const button_input_t *buttons)
{
    return XGpioPs_ReadPin(&buttons->instance, BUTTON_START);
}

u32 button_input_read_reset_level(const button_input_t *buttons)
{
    return XGpioPs_ReadPin(&buttons->instance, BUTTON_RESET);
}

u32 button_input_read_phase_increment_level(const button_input_t *buttons)
{
    return XGpioPs_ReadPin(&buttons->instance, BUTTON_PHASE_INC);
}

u32 button_input_read_phase_decrement_level(const button_input_t *buttons)
{
    return XGpioPs_ReadPin(&buttons->instance, BUTTON_PHASE_DEC);
}
