#include "../include/basic_output_ui.h"

static void clamp_request(open_loop_output_request_t *request)
{
    if (request->frequency_hz < BASIC_OUTPUT_MIN_FREQUENCY_HZ) {
        request->frequency_hz = BASIC_OUTPUT_MIN_FREQUENCY_HZ;
    } else if (request->frequency_hz > BASIC_OUTPUT_MAX_FREQUENCY_HZ) {
        request->frequency_hz = BASIC_OUTPUT_MAX_FREQUENCY_HZ;
    }

    if (request->target_output_vpp < BASIC_OUTPUT_MIN_TARGET_VPP) {
        request->target_output_vpp = BASIC_OUTPUT_MIN_TARGET_VPP;
    } else if (request->target_output_vpp > BASIC_OUTPUT_MAX_TARGET_VPP) {
        request->target_output_vpp = BASIC_OUTPUT_MAX_TARGET_VPP;
    }
}

int basic_output_ui_init(basic_output_ui_t *ui,
                         float initial_frequency_hz,
                         float initial_target_output_vpp)
{
    open_loop_output_request_t request;

    if (ui == 0) {
        return -1;
    }

    request.frequency_hz = initial_frequency_hz;
    request.target_output_vpp = initial_target_output_vpp;
    if (basic_output_validate_request(&request) != 0) {
        return -1;
    }

    ui->request = request;
    ui->selected_field = BASIC_OUTPUT_FIELD_FREQUENCY;
    ui->running = 0;
    return 0;
}

int basic_output_ui_handle(basic_output_ui_t *ui,
                           basic_output_ui_event_t event)
{
    if (ui == 0) {
        return -1;
    }

    if (event == BASIC_OUTPUT_UI_SELECT) {
        if (!ui->running) {
            ui->selected_field =
                (ui->selected_field == BASIC_OUTPUT_FIELD_FREQUENCY) ?
                BASIC_OUTPUT_FIELD_TARGET_VPP : BASIC_OUTPUT_FIELD_FREQUENCY;
        }
        return 0;
    }

    if (event == BASIC_OUTPUT_UI_START) {
        ui->running = 1;
        return 0;
    }

    if (event == BASIC_OUTPUT_UI_RESET) {
        ui->running = 0;
        return 0;
    }

    if (ui->running) {
        return -1;
    }

    if (ui->selected_field == BASIC_OUTPUT_FIELD_FREQUENCY) {
        if (event == BASIC_OUTPUT_UI_INCREMENT) {
            ui->request.frequency_hz += BASIC_OUTPUT_FREQUENCY_STEP_HZ;
        } else if (event == BASIC_OUTPUT_UI_DECREMENT) {
            ui->request.frequency_hz -= BASIC_OUTPUT_FREQUENCY_STEP_HZ;
        } else {
            return -1;
        }
    } else {
        if (event == BASIC_OUTPUT_UI_INCREMENT) {
            ui->request.target_output_vpp += BASIC_OUTPUT_TARGET_VPP_STEP;
        } else if (event == BASIC_OUTPUT_UI_DECREMENT) {
            ui->request.target_output_vpp -= BASIC_OUTPUT_TARGET_VPP_STEP;
        } else {
            return -1;
        }
    }

    clamp_request(&ui->request);
    return basic_output_validate_request(&ui->request);
}

const char *basic_output_ui_field_name(basic_output_field_t field)
{
    return (field == BASIC_OUTPUT_FIELD_TARGET_VPP) ? "target_vpp" :
           "frequency";
}
