#include <stdio.h>

#include "app_state_machine.h"

int main(void)
{
    app_state_machine_t machine;

    app_state_machine_init(&machine);
    if (machine.state != APP_STATE_BOOT ||
        app_state_machine_dispatch(&machine, APP_EVENT_INIT_OK, 0) != 0 ||
        machine.state != APP_STATE_MENU ||
        app_state_machine_dispatch(&machine, APP_EVENT_START_BASIC, 0) != 0 ||
        machine.state != APP_STATE_BASIC ||
        app_state_machine_dispatch(&machine, APP_EVENT_COMPLETE, 0) != 0 ||
        machine.state != APP_STATE_MENU ||
        app_state_machine_dispatch(&machine, APP_EVENT_START_LEARN, 0) != 0 ||
        machine.state != APP_STATE_LEARN ||
        app_state_machine_dispatch(&machine, APP_EVENT_FAILURE, 17) != 0 ||
        machine.state != APP_STATE_ERROR || machine.error_code != 17 ||
        app_state_machine_dispatch(&machine, APP_EVENT_RESET, 0) != 0 ||
        machine.state != APP_STATE_BOOT ||
        app_state_machine_state_name(APP_STATE_INFER)[0] != 'I') {
        return 1;
    }

    puts("APP_STATE_MACHINE_SELF_TEST_PASSED");
    return 0;
}