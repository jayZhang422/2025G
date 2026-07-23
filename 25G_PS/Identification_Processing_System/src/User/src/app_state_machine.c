#include "app_state_machine.h"

static void enter_error(app_state_machine_t *machine, int error_code)
{
    machine->state = APP_STATE_ERROR;
    machine->error_code = error_code;
}

void app_state_machine_init(app_state_machine_t *machine)
{
    if (machine == 0) {
        return;
    }
    machine->state = APP_STATE_BOOT;
    machine->error_code = 0;
}

int app_state_machine_dispatch(app_state_machine_t *machine,
                               app_event_t event,
                               int error_code)
{
    if (machine == 0) {
        return -1;
    }

    if (event == APP_EVENT_FAILURE) {
        enter_error(machine, error_code);
        return 0;
    }

    switch (machine->state) {
    case APP_STATE_BOOT:
        if (event == APP_EVENT_INIT_OK) {
            machine->state = APP_STATE_MENU;
            machine->error_code = 0;
        } else if (event != APP_EVENT_RESET) {
            return -1;
        }
        break;

    case APP_STATE_MENU:
        if (event == APP_EVENT_START_BASIC) {
            machine->state = APP_STATE_BASIC;
        } else if (event == APP_EVENT_START_LEARN) {
            machine->state = APP_STATE_LEARN;
        } else if (event == APP_EVENT_START_INFER) {
            machine->state = APP_STATE_INFER;
        } else if (event == APP_EVENT_RESET) {
            machine->state = APP_STATE_BOOT;
        } else {
            return -1;
        }
        break;

    case APP_STATE_BASIC:
    case APP_STATE_LEARN:
    case APP_STATE_INFER:
        if (event == APP_EVENT_COMPLETE) {
            machine->state = APP_STATE_MENU;
        } else if (event == APP_EVENT_RESET) {
            machine->state = APP_STATE_MENU;
        } else {
            return -1;
        }
        break;

    case APP_STATE_ERROR:
        if (event == APP_EVENT_RESET) {
            machine->state = APP_STATE_BOOT;
            machine->error_code = 0;
        } else {
            return -1;
        }
        break;

    default:
        return -1;
    }
    return 0;
}

const char *app_state_machine_state_name(app_state_t state)
{
    switch (state) {
    case APP_STATE_BOOT:
        return "BOOT";
    case APP_STATE_MENU:
        return "MENU";
    case APP_STATE_BASIC:
        return "BASIC";
    case APP_STATE_LEARN:
        return "LEARN";
    case APP_STATE_INFER:
        return "INFER";
    case APP_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}