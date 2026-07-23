#ifndef USER_INCLUDE_APP_STATE_MACHINE_H_
#define USER_INCLUDE_APP_STATE_MACHINE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_MENU,
    APP_STATE_BASIC,
    APP_STATE_LEARN,
    APP_STATE_INFER,
    APP_STATE_ERROR
} app_state_t;

typedef enum {
    APP_EVENT_INIT_OK = 0,
    APP_EVENT_START_BASIC,
    APP_EVENT_START_LEARN,
    APP_EVENT_START_INFER,
    APP_EVENT_COMPLETE,
    APP_EVENT_RESET,
    APP_EVENT_FAILURE
} app_event_t;

typedef struct {
    app_state_t state;
    int error_code;
} app_state_machine_t;

void app_state_machine_init(app_state_machine_t *machine);
int app_state_machine_dispatch(app_state_machine_t *machine,
                               app_event_t event,
                               int error_code);
const char *app_state_machine_state_name(app_state_t state);

#ifdef __cplusplus
}
#endif

#endif