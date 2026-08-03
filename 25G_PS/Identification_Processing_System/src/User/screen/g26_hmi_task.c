#include "g26_hmi_task.h"

#include "g26_hmi_render.h"
#include "pl_hmi_uart.h"
#include "../include/g26_measurement_app.h"

#include "FreeRTOS.h"
#include "task.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xstatus.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef XPAR_UARTLITE_0_DEVICE_ID
#error "The active BSP does not contain the J11 AXI UARTLite device ID"
#endif
#ifndef XPAR_UARTLITE_0_BASEADDR
#error "The active BSP does not contain the J11 AXI UARTLite base address"
#endif
#ifndef XPAR_UARTLITE_0_BAUDRATE
#error "The active BSP does not describe the J11 AXI UARTLite baud rate"
#endif
#ifndef XPAR_UARTLITE_0_DATA_BITS
#error "The active BSP does not describe the J11 AXI UARTLite data width"
#endif
#ifndef XPAR_UARTLITE_0_USE_PARITY
#error "The active BSP does not describe the J11 AXI UARTLite parity"
#endif

#if XPAR_UARTLITE_0_BAUDRATE != 230400U
#error "The J11 screen requires a 230400-baud UARTLite bitstream and BSP"
#endif
#if XPAR_UARTLITE_0_DATA_BITS != 8U
#error "The J11 screen requires eight UART data bits"
#endif
#if XPAR_UARTLITE_0_USE_PARITY != 0U
#error "The J11 screen requires parity disabled"
#endif
#if G26_SIGNAL_MAX_COMPONENTS != 3U
#error "The screen layout exposes exactly three spectrum component rows"
#endif

#define G26_HMI_TASK_POLL_TICKS 1U
#define G26_HMI_COMMAND_BYTES   64U
#define G26_HMI_SPECTRUM_COLOR  65504U
#define G26_HMI_INVALID_CLEAR_COUNT 2U

typedef struct {
    pl_hmi_uart_t uart;
    g26_hmi_session_t session;
    g26_hmi_measurement_signature_t last_sent_signature;
    u32 next_generation;
    u32 request_tick;
    u8 navigation_lock_page;
    u8 observed_page;
    u8 invalid_generation_count;
    int last_sent_valid;
} g26_hmi_state_t;

static g26_measurement_output_t g26_hmi_snapshot;
static uint8_t g26_hmi_waveform_bytes[G26_HMI_WAVEFORM_MAX_POINTS];

static float32_t g26_hmi_wrap_phase(float32_t phase)
{
    while (phase > APP_PI) {
        phase -= 2.0f * APP_PI;
    }
    while (phase < -APP_PI) {
        phase += 2.0f * APP_PI;
    }
    return phase;
}

static void g26_hmi_build_signature(
    const g26_signal_result_t *result,
    g26_hmi_measurement_signature_t *signature)
{
    float32_t fundamental_phase = 0.0f;
    u32 component;

    memset(signature, 0, sizeof(*signature));
    signature->component_count = result->component_count;
    signature->fundamental_frequency_hz =
        result->fundamental_frequency_hz;
    signature->dc_mv = result->dc_mv;
    signature->rms_mv = result->rms_mv;
    signature->upp_mv = result->upp_mv;

    for (component = 0U;
         component < result->component_count &&
         component < G26_HMI_SIGNATURE_COMPONENTS; component++) {
        if (result->components[component].harmonic_order == 1U) {
            fundamental_phase = result->components[component].phase_rad;
            break;
        }
    }
    for (component = 0U;
         component < result->component_count &&
         component < G26_HMI_SIGNATURE_COMPONENTS; component++) {
        const g26_signal_component_t *line = &result->components[component];

        signature->harmonic_order[component] = line->harmonic_order;
        signature->frequency_hz[component] = line->frequency_hz;
        signature->amplitude_mv[component] = line->amplitude_mv;
        signature->relative_phase_rad[component] = g26_hmi_wrap_phase(
            line->phase_rad -
            (float32_t)line->harmonic_order * fundamental_phase);
    }
}

static void g26_hmi_reset_display_tracking(g26_hmi_state_t *state)
{
    state->last_sent_valid = 0;
    state->invalid_generation_count = 0U;
}

static int g26_hmi_send_format(pl_hmi_uart_t *uart,
                               const char *format, ...)
{
    char command[G26_HMI_COMMAND_BYTES];
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = vsnprintf(command, sizeof(command), format, arguments);
    va_end(arguments);
    if (length <= 0 || (size_t)length >= sizeof(command)) {
        return XST_FAILURE;
    }
    return pl_hmi_uart_send_command(uart, command, (size_t)length);
}

static int g26_hmi_set_visible(pl_hmi_uart_t *uart,
                               const char *object_name, int visible)
{
    return g26_hmi_send_format(
        uart, "vis %s,%u", object_name, visible ? 1U : 0U);
}

static int g26_hmi_set_value(pl_hmi_uart_t *uart,
                             const char *object_name, u32 value)
{
    return g26_hmi_send_format(
        uart, "%s.val=%u", object_name, (unsigned int)value);
}

static int g26_hmi_set_navigation_enabled(g26_hmi_state_t *state,
                                          u8 page, int enabled)
{
    static const char *const page2_buttons[] = {"b2", "b4", "b6"};
    static const char *const page3_buttons[] = {"b4", "b6"};
    const char *const *buttons;
    size_t button_count;
    size_t index;
    int status = XST_SUCCESS;

    if (page == G26_HMI_PAGE_TIME_DOMAIN) {
        buttons = page2_buttons;
        button_count = sizeof(page2_buttons) / sizeof(page2_buttons[0]);
    } else if (page == G26_HMI_PAGE_SPECTRUM) {
        buttons = page3_buttons;
        button_count = sizeof(page3_buttons) / sizeof(page3_buttons[0]);
    } else {
        return XST_FAILURE;
    }
    for (index = 0U; index < button_count; index++) {
        if (g26_hmi_send_format(
                &state->uart, "tsw %s,%u", buttons[index],
                enabled ? 1U : 0U) != XST_SUCCESS) {
            status = XST_FAILURE;
        }
    }
    return status;
}

static int g26_hmi_unlock_navigation(g26_hmi_state_t *state)
{
    int status;

    if (state->navigation_lock_page == 0U) {
        return XST_SUCCESS;
    }
    status = g26_hmi_set_navigation_enabled(
        state, state->navigation_lock_page, 1);
    if (status == XST_SUCCESS) {
        state->navigation_lock_page = 0U;
    }
    return status;
}

static int g26_hmi_set_page_touch_enabled(g26_hmi_state_t *state,
                                           int enabled)
{
    return g26_hmi_send_format(
        &state->uart, "tsw 255,%u", enabled ? 1U : 0U);
}

static int g26_hmi_prepare_page2(g26_hmi_state_t *state)
{
    if (g26_hmi_send_format(
            &state->uart, "%s.pco%u=%u",
            G26_HMI_PAGE2_WAVEFORM_OBJECT,
            G26_HMI_PAGE2_WAVEFORM_CHANNEL,
            G26_HMI_WAVEFORM_COLOR) != XST_SUCCESS ||
        g26_hmi_send_format(
            &state->uart, "cle %u,%u",
            G26_HMI_PAGE2_WAVEFORM_COMPONENT_ID,
            G26_HMI_PAGE2_WAVEFORM_CHANNEL) != XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE2_UPP_OBJECT, 0) != XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE2_RMS_OBJECT, 0) != XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE2_FUNDAMENTAL_OBJECT, 0) !=
            XST_SUCCESS) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static int g26_hmi_prepare_page3(g26_hmi_state_t *state)
{
    if (g26_hmi_send_format(
            &state->uart, "ref %s", G26_HMI_PAGE3_PLOT_OBJECT) !=
            XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE3_FREQUENCY0_OBJECT, 0) !=
            XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE3_FREQUENCY1_OBJECT, 0) !=
            XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE3_FREQUENCY2_OBJECT, 0) !=
            XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE3_AMPLITUDE0_OBJECT, 0) !=
            XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE3_AMPLITUDE1_OBJECT, 0) !=
            XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE3_AMPLITUDE2_OBJECT, 0) !=
            XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE3_THIRD_LABEL_OBJECT, 0) !=
            XST_SUCCESS) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static int g26_hmi_prepare_page(g26_hmi_state_t *state, u8 page)
{
    if (page == G26_HMI_PAGE_TIME_DOMAIN) {
        return g26_hmi_prepare_page2(state);
    }
    if (page == G26_HMI_PAGE_SPECTRUM) {
        return g26_hmi_prepare_page3(state);
    }
    return XST_FAILURE;
}

/* Return 1 for a result page, 0 for another page, and -1 on link failure. */
static int g26_hmi_sync_active_result_page(g26_hmi_state_t *state,
                                           u8 *page, int *page_changed)
{
    uint8_t current_page;

    if (pl_hmi_uart_get_current_page(
            &state->uart, &current_page) != XST_SUCCESS) {
        return -1;
    }
    *page_changed = (current_page != state->observed_page);
    state->observed_page = current_page;
    if (current_page != G26_HMI_PAGE_TIME_DOMAIN &&
        current_page != G26_HMI_PAGE_SPECTRUM) {
        return 0;
    }
    if (current_page != state->session.active_page) {
        *page_changed = 1;
        state->session.active_page = current_page;
        if (g26_hmi_prepare_page(state, current_page) != XST_SUCCESS) {
            return -1;
        }
    }
    *page = current_page;
    return 1;
}

static int g26_hmi_confirm_page(g26_hmi_state_t *state, u8 expected_page)
{
    uint8_t page;

    return pl_hmi_uart_get_current_page(&state->uart, &page) ==
            XST_SUCCESS && page == expected_page ?
        XST_SUCCESS : XST_FAILURE;
}

static int g26_hmi_render_page2_waveform(g26_hmi_state_t *state)
{
    const float32_t *source = (state->session.period_count == 3U) ?
        g26_hmi_snapshot.three_period_mv :
        g26_hmi_snapshot.one_period_mv;
    size_t sample_count = g26_hmi_waveform_resample(
        source, APP_G26_WAVEFORM_POINTS, g26_hmi_waveform_bytes,
        sizeof(g26_hmi_waveform_bytes), G26_HMI_WAVEFORM_MAX_POINTS);

    if (sample_count == 0U ||
        g26_hmi_send_format(
            &state->uart, "cle %u,%u",
            G26_HMI_PAGE2_WAVEFORM_COMPONENT_ID,
            G26_HMI_PAGE2_WAVEFORM_CHANNEL) != XST_SUCCESS ||
        pl_hmi_uart_send_waveform(
            &state->uart, G26_HMI_PAGE2_WAVEFORM_COMPONENT_ID,
            G26_HMI_PAGE2_WAVEFORM_CHANNEL, g26_hmi_waveform_bytes,
            sample_count) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    xil_printf("[HMI] WAVEFORM_TX periods=%u points=%u\r\n",
               (unsigned int)state->session.period_count,
               (unsigned int)sample_count);
    return XST_SUCCESS;
}

static int g26_hmi_render_page2_parameters(g26_hmi_state_t *state)
{
    const g26_signal_result_t *result = &g26_hmi_snapshot.result;

    if (g26_hmi_set_value(
            &state->uart, G26_HMI_PAGE2_UPP_OBJECT,
            g26_hmi_millivolts_thousandths(result->upp_mv)) !=
            XST_SUCCESS ||
        g26_hmi_set_value(
            &state->uart, G26_HMI_PAGE2_RMS_OBJECT,
            g26_hmi_millivolts_thousandths(result->rms_mv)) !=
            XST_SUCCESS ||
        g26_hmi_set_value(
            &state->uart, G26_HMI_PAGE2_FUNDAMENTAL_OBJECT,
            g26_hmi_frequency_integer_khz(
                result->fundamental_frequency_hz)) != XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE2_UPP_OBJECT, 1) != XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE2_RMS_OBJECT, 1) != XST_SUCCESS ||
        g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE2_FUNDAMENTAL_OBJECT, 1) !=
            XST_SUCCESS) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static const char *g26_hmi_page3_frequency_object(u32 component)
{
    static const char *const objects[G26_SIGNAL_MAX_COMPONENTS] = {
        G26_HMI_PAGE3_FREQUENCY0_OBJECT,
        G26_HMI_PAGE3_FREQUENCY1_OBJECT,
        G26_HMI_PAGE3_FREQUENCY2_OBJECT
    };

    return (component < G26_SIGNAL_MAX_COMPONENTS) ?
        objects[component] : NULL;
}

static const char *g26_hmi_page3_amplitude_object(u32 component)
{
    static const char *const objects[G26_SIGNAL_MAX_COMPONENTS] = {
        G26_HMI_PAGE3_AMPLITUDE0_OBJECT,
        G26_HMI_PAGE3_AMPLITUDE1_OBJECT,
        G26_HMI_PAGE3_AMPLITUDE2_OBJECT
    };

    return (component < G26_SIGNAL_MAX_COMPONENTS) ?
        objects[component] : NULL;
}

static float32_t g26_hmi_maximum_component_amplitude(
    const g26_signal_result_t *result)
{
    float32_t maximum = 0.0f;
    u32 component;

    for (component = 0U; component < result->component_count &&
         component < G26_SIGNAL_MAX_COMPONENTS; component++) {
        if (result->components[component].amplitude_mv > maximum) {
            maximum = result->components[component].amplitude_mv;
        }
    }
    return maximum;
}

static int g26_hmi_draw_spectrum_lines(g26_hmi_state_t *state)
{
    const g26_signal_result_t *result = &g26_hmi_snapshot.result;
    float32_t maximum_amplitude =
        g26_hmi_maximum_component_amplitude(result);
    u32 component;

    if (result->component_count < 1U ||
        result->component_count > G26_SIGNAL_MAX_COMPONENTS) {
        return XST_FAILURE;
    }
    for (component = 0U; component < result->component_count; component++) {
        const g26_signal_component_t *line = &result->components[component];
        int x = g26_hmi_spectrum_x(line->frequency_hz);
        int top = g26_hmi_spectrum_top(
            line->amplitude_mv, maximum_amplitude);
        int height = G26_HMI_SPECTRUM_Y0 - top;

        if (height > 0 && g26_hmi_send_format(
                &state->uart, "fill %d,%d,3,%d,%u", x - 1, top, height,
                (unsigned int)G26_HMI_SPECTRUM_COLOR) != XST_SUCCESS) {
            return XST_FAILURE;
        }
    }
    xil_printf("[HMI] SPECTRUM_TX lines=%u amplitudes=%u\r\n",
               (unsigned int)result->component_count,
               state->session.show_amplitudes ? 1U : 0U);
    return XST_SUCCESS;
}

static int g26_hmi_flush_updates(g26_hmi_state_t *state)
{
    static const char doevents_command[] = "doevents";

    return pl_hmi_uart_send_command(
        &state->uart, doevents_command, sizeof(doevents_command) - 1U);
}

static int g26_hmi_render_page3(g26_hmi_state_t *state)
{
    const g26_signal_result_t *result = &g26_hmi_snapshot.result;
    u32 component;

    if (result->component_count < 1U ||
        result->component_count > G26_SIGNAL_MAX_COMPONENTS ||
        g26_hmi_send_format(
            &state->uart, "ref %s", G26_HMI_PAGE3_PLOT_OBJECT) !=
            XST_SUCCESS) {
        return XST_FAILURE;
    }
    for (component = 0U; component < G26_SIGNAL_MAX_COMPONENTS;
         component++) {
        const char *frequency_object =
            g26_hmi_page3_frequency_object(component);
        const char *amplitude_object =
            g26_hmi_page3_amplitude_object(component);

        if (component < result->component_count) {
            const g26_signal_component_t *line =
                &result->components[component];

            if (g26_hmi_set_value(
                    &state->uart, frequency_object,
                    g26_hmi_frequency_tenths_khz(
                        line->frequency_hz)) != XST_SUCCESS ||
                g26_hmi_set_value(
                    &state->uart, amplitude_object,
                    g26_hmi_millivolts_thousandths(
                        line->amplitude_mv)) != XST_SUCCESS ||
                g26_hmi_set_visible(
                    &state->uart, frequency_object, 1) != XST_SUCCESS ||
                g26_hmi_set_visible(
                    &state->uart, amplitude_object,
                    state->session.show_amplitudes) != XST_SUCCESS) {
                return XST_FAILURE;
            }
        } else if (g26_hmi_set_visible(
                       &state->uart, frequency_object, 0) != XST_SUCCESS ||
                   g26_hmi_set_visible(
                       &state->uart, amplitude_object, 0) != XST_SUCCESS) {
            return XST_FAILURE;
        }
    }
    if (g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE3_THIRD_LABEL_OBJECT,
            result->component_count == 3U) != XST_SUCCESS ||
        g26_hmi_flush_updates(state) != XST_SUCCESS ||
        g26_hmi_draw_spectrum_lines(state) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    return g26_hmi_confirm_page(state, G26_HMI_PAGE_SPECTRUM);
}

static int g26_hmi_show_page3_amplitudes(g26_hmi_state_t *state)
{
    const g26_signal_result_t *result = &g26_hmi_snapshot.result;
    u32 component;

    if (result->component_count < 1U ||
        result->component_count > G26_SIGNAL_MAX_COMPONENTS) {
        return XST_FAILURE;
    }
    for (component = 0U; component < G26_SIGNAL_MAX_COMPONENTS;
         component++) {
        const char *amplitude_object =
            g26_hmi_page3_amplitude_object(component);

        if (component < result->component_count) {
            u32 amplitude_thousandths = g26_hmi_millivolts_thousandths(
                result->components[component].amplitude_mv);

            if (g26_hmi_set_value(
                    &state->uart, amplitude_object,
                    amplitude_thousandths) != XST_SUCCESS ||
                g26_hmi_set_visible(
                    &state->uart, amplitude_object, 1) != XST_SUCCESS) {
                return XST_FAILURE;
            }
            xil_printf("[HMI] AMPLITUDE_TX %s=%u.%03u mVpeak\r\n",
                       amplitude_object,
                       (unsigned int)(amplitude_thousandths / 1000U),
                       (unsigned int)(amplitude_thousandths % 1000U));
        } else if (g26_hmi_set_visible(
                       &state->uart, amplitude_object, 0) != XST_SUCCESS) {
            return XST_FAILURE;
        }
    }
    if (g26_hmi_flush_updates(state) != XST_SUCCESS ||
        g26_hmi_draw_spectrum_lines(state) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    return g26_hmi_confirm_page(state, G26_HMI_PAGE_SPECTRUM);
}

static int g26_hmi_render_active_page(g26_hmi_state_t *state)
{
    if (!state->session.snapshot_valid) {
        return XST_SUCCESS;
    }
    if (state->session.active_page == G26_HMI_PAGE_TIME_DOMAIN) {
        if (g26_hmi_render_page2_waveform(state) != XST_SUCCESS ||
            (state->session.show_time_parameters &&
             g26_hmi_render_page2_parameters(state) != XST_SUCCESS)) {
            return XST_FAILURE;
        }
        return g26_hmi_confirm_page(state, G26_HMI_PAGE_TIME_DOMAIN);
    }
    if (state->session.active_page == G26_HMI_PAGE_SPECTRUM) {
        return g26_hmi_render_page3(state);
    }
    return XST_FAILURE;
}

static int g26_hmi_with_touch_lock(g26_hmi_state_t *state,
                                   int (*operation)(g26_hmi_state_t *))
{
    int status;

    if (g26_hmi_set_page_touch_enabled(state, 0) != XST_SUCCESS) {
        (void)g26_hmi_set_page_touch_enabled(state, 1);
        return XST_FAILURE;
    }
    status = operation(state);
    if (g26_hmi_set_page_touch_enabled(state, 1) != XST_SUCCESS) {
        status = XST_FAILURE;
    }
    return status;
}

static u32 g26_hmi_allocate_generation(g26_hmi_state_t *state)
{
    u32 generation = state->next_generation++;

    if (generation == 0U || generation >= 0x80000000U) {
        state->next_generation = 2U;
        generation = 1U;
    }
    return generation;
}

static int g26_hmi_continue_measurement(g26_hmi_state_t *state)
{
    u8 page = state->session.active_page;
    u32 generation;

    if (page != G26_HMI_PAGE_TIME_DOMAIN &&
        page != G26_HMI_PAGE_SPECTRUM) {
        return XST_FAILURE;
    }
    generation = g26_hmi_allocate_generation(state);
    g26_hmi_session_continue(&state->session, generation, page);
    state->request_tick = (u32)xTaskGetTickCount();
    if (g26_measurement_request(generation, page) != XST_SUCCESS) {
        state->session.pending_generation = 0U;
        state->session.source_page = 0U;
        state->request_tick = 0U;
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static int g26_hmi_start_measurement(g26_hmi_state_t *state, u8 page)
{
    u32 generation;

    if (g26_hmi_session_is_pending(&state->session)) {
        xil_printf("[HMI] START_IGNORED generation=%u\r\n",
                   (unsigned int)state->session.pending_generation);
        return XST_SUCCESS;
    }
    generation = g26_hmi_allocate_generation(state);
    g26_hmi_reset_display_tracking(state);
    g26_hmi_session_start(
        &state->session, generation, page, page);
    state->request_tick = (u32)xTaskGetTickCount();

    state->navigation_lock_page = page;
    if (g26_hmi_set_navigation_enabled(state, page, 0) != XST_SUCCESS) {
        g26_hmi_session_stop(&state->session);
        state->request_tick = 0U;
        (void)g26_hmi_unlock_navigation(state);
        return XST_FAILURE;
    }
    if (g26_hmi_prepare_page(state, page) != XST_SUCCESS ||
        g26_measurement_request(generation, page) != XST_SUCCESS) {
        g26_hmi_session_stop(&state->session);
        state->request_tick = 0U;
        (void)g26_hmi_unlock_navigation(state);
        return XST_FAILURE;
    }
    xil_printf("[HMI] MEASUREMENT_REQUEST generation=%u page=%u\r\n",
               (unsigned int)generation, (unsigned int)page);
    return XST_SUCCESS;
}

static int g26_hmi_stop_page(g26_hmi_state_t *state, u8 page)
{
    int status;

    g26_hmi_reset_display_tracking(state);
    g26_hmi_session_stop(&state->session);
    state->request_tick = 0U;
    status = g26_hmi_prepare_page(state, page);
    if (g26_hmi_unlock_navigation(state) != XST_SUCCESS) {
        status = XST_FAILURE;
    }
    return status;
}

static int g26_hmi_handle_page2_event(g26_hmi_state_t *state, u8 command)
{
    if (command == G26_HMI_COMMAND_START) {
        return g26_hmi_start_measurement(
            state, G26_HMI_PAGE_TIME_DOMAIN);
    }
    if (command == G26_HMI_COMMAND_STOP) {
        return g26_hmi_stop_page(state, G26_HMI_PAGE_TIME_DOMAIN);
    }

    state->session.active_page = G26_HMI_PAGE_TIME_DOMAIN;
    if (command == G26_HMI_COMMAND_ONE_PERIOD) {
        if (state->session.period_count == 1U) {
            return XST_SUCCESS;
        }
        state->session.period_count = 1U;
        return state->session.snapshot_valid ?
            g26_hmi_with_touch_lock(
                state, g26_hmi_render_page2_waveform) : XST_SUCCESS;
    }
    if (command == G26_HMI_COMMAND_THREE_PERIODS) {
        if (state->session.period_count == 3U) {
            return XST_SUCCESS;
        }
        state->session.period_count = 3U;
        return state->session.snapshot_valid ?
            g26_hmi_with_touch_lock(
                state, g26_hmi_render_page2_waveform) : XST_SUCCESS;
    }
    if (command == G26_HMI_COMMAND_TIME_PARAMETERS) {
        if (state->session.show_time_parameters) {
            return XST_SUCCESS;
        }
        state->session.show_time_parameters = 1;
        return state->session.snapshot_valid ?
            g26_hmi_render_page2_parameters(state) : XST_SUCCESS;
    }
    return XST_SUCCESS;
}

static int g26_hmi_handle_page3_event(g26_hmi_state_t *state, u8 command)
{
    if (command == G26_HMI_COMMAND_START) {
        return g26_hmi_start_measurement(state, G26_HMI_PAGE_SPECTRUM);
    }
    if (command == G26_HMI_COMMAND_STOP) {
        return g26_hmi_stop_page(state, G26_HMI_PAGE_SPECTRUM);
    }
    if (command == G26_HMI_COMMAND_SHOW_AMPLITUDE) {
        int status = XST_SUCCESS;

        state->session.active_page = G26_HMI_PAGE_SPECTRUM;
        state->session.show_amplitudes = 1;
        if (state->session.snapshot_valid) {
            status = g26_hmi_with_touch_lock(
                state, g26_hmi_show_page3_amplitudes);
            if (status != XST_SUCCESS) {
                state->session.show_amplitudes = 0;
            }
        }
        return status;
    }
    return XST_SUCCESS;
}

static int g26_hmi_handle_touch(g26_hmi_state_t *state,
                                const hmi_event_t *event)
{
    xil_printf("[HMI] EVENT page=%u command=0x%02x\r\n",
               (unsigned int)event->page,
               (unsigned int)event->command);
    if (event->page == G26_HMI_PAGE_TIME_DOMAIN) {
        return g26_hmi_handle_page2_event(state, event->command);
    }
    if (event->page == G26_HMI_PAGE_SPECTRUM) {
        return g26_hmi_handle_page3_event(state, event->command);
    }
    return XST_SUCCESS;
}

static int g26_hmi_sync_key_start(g26_hmi_state_t *state,
                                  const g26_measurement_event_t *event)
{
    uint8_t page;

    if (g26_hmi_session_is_pending(&state->session)) {
        xil_printf("[HMI] KEY1_SYNC_SKIPPED pending_generation=%u\r\n",
                   (unsigned int)state->session.pending_generation);
        return XST_SUCCESS;
    }
    if (pl_hmi_uart_get_current_page(&state->uart, &page) != XST_SUCCESS) {
        xil_printf("[HMI] WARN: KEY1 cannot identify active result page\r\n");
        return XST_FAILURE;
    }
    if (page != G26_HMI_PAGE_TIME_DOMAIN &&
        page != G26_HMI_PAGE_SPECTRUM) {
        xil_printf("[HMI] KEY1_SYNC_SKIPPED page=%u\r\n",
                   (unsigned int)page);
        return XST_SUCCESS;
    }
    g26_hmi_session_start(
        &state->session, event->generation, 0U, page);
    g26_hmi_reset_display_tracking(state);
    state->request_tick = event->started_tick;
    state->navigation_lock_page = page;
    if (g26_hmi_set_navigation_enabled(state, page, 0) != XST_SUCCESS) {
        g26_hmi_session_stop(&state->session);
        (void)g26_hmi_unlock_navigation(state);
        return XST_FAILURE;
    }
    if (g26_hmi_prepare_page(state, page) != XST_SUCCESS) {
        g26_hmi_session_stop(&state->session);
        (void)g26_hmi_unlock_navigation(state);
        return XST_FAILURE;
    }
    xil_printf("[HMI] KEY1_SYNC_START generation=%u page=%u\r\n",
               (unsigned int)event->generation, (unsigned int)page);
    return XST_SUCCESS;
}

static u32 g26_hmi_elapsed_ms(u32 from_tick, u32 to_tick)
{
    return (u32)(((u64)(to_tick - from_tick) * 1000U) /
                 configTICK_RATE_HZ);
}

static int g26_hmi_process_completion(
    g26_hmi_state_t *state, const g26_measurement_event_t *event)
{
    u32 generation;
    u32 display_tick;
    u8 page = state->session.active_page;
    g26_hmi_measurement_signature_t current_signature;
    int page_changed = 0;
    int page_status;
    int rendered = 0;
    int render_status = XST_SUCCESS;
    int status = XST_SUCCESS;

    if (!g26_hmi_session_accept_event(
            &state->session, event->generation, event->source_page)) {
        return XST_SUCCESS;
    }
    if (event->status != XST_SUCCESS ||
        g26_measurement_snapshot(
            &g26_hmi_snapshot, &generation) != XST_SUCCESS ||
        generation != event->generation) {
        g26_hmi_session_publish_invalid(&state->session);
        if (state->invalid_generation_count < 0xFFU) {
            state->invalid_generation_count++;
        }
        page_status = g26_hmi_sync_active_result_page(
            state, &page, &page_changed);
        if (page_status < 0) {
            status = XST_FAILURE;
        }
        if (state->invalid_generation_count ==
                G26_HMI_INVALID_CLEAR_COUNT) {
            memset(&g26_hmi_snapshot, 0, sizeof(g26_hmi_snapshot));
            state->last_sent_valid = 0;
        } else if (page_status == 1 && page_changed) {
            state->last_sent_valid = 0;
        }
        if (page_status == 1 &&
            (state->invalid_generation_count ==
                 G26_HMI_INVALID_CLEAR_COUNT ||
             (state->invalid_generation_count >
                  G26_HMI_INVALID_CLEAR_COUNT && page_changed)) &&
            g26_hmi_prepare_page(state, page) != XST_SUCCESS) {
            status = XST_FAILURE;
        }
        if (g26_hmi_unlock_navigation(state) != XST_SUCCESS) {
            status = XST_FAILURE;
        }
        xil_printf("[HMI] LIVE_INVALID generation=%u status=%d count=%u\r\n",
                   (unsigned int)event->generation, event->status,
                   (unsigned int)state->invalid_generation_count);
        state->request_tick = 0U;
        if (g26_hmi_continue_measurement(state) != XST_SUCCESS) {
            status = XST_FAILURE;
        }
        return status;
    }

    state->invalid_generation_count = 0U;
    g26_hmi_build_signature(
        &g26_hmi_snapshot.result, &current_signature);
    g26_hmi_session_publish_snapshot(&state->session);
    page_status = g26_hmi_sync_active_result_page(
        state, &page, &page_changed);
    if (page_status < 0) {
        status = XST_FAILURE;
    } else if (page_status == 1 &&
               (!state->last_sent_valid || page_changed ||
                g26_hmi_signature_changed(
                    &state->last_sent_signature, &current_signature))) {
        rendered = 1;
        render_status = g26_hmi_with_touch_lock(
            state, g26_hmi_render_active_page);
        if (render_status != XST_SUCCESS) {
            status = XST_FAILURE;
        } else {
            state->last_sent_signature = current_signature;
            state->last_sent_valid = 1;
        }
    }
    if (g26_hmi_unlock_navigation(state) != XST_SUCCESS) {
        status = XST_FAILURE;
    }
    display_tick = (u32)xTaskGetTickCount();
    if (status == XST_SUCCESS && rendered &&
        render_status == XST_SUCCESS) {
        xil_printf("[HMI] TIMING origin=%s generation=%u page=%u compute_ms=%u display_tx_ms=%u total_ms=%u request_to_display_ms=%u\r\n",
                   (event->source_page == 0U) ? "KEY1" : "HMI",
                   (unsigned int)event->generation,
                   (unsigned int)page,
                   (unsigned int)g26_hmi_elapsed_ms(
                       event->started_tick, event->completed_tick),
                   (unsigned int)g26_hmi_elapsed_ms(
                       event->completed_tick, display_tick),
                   (unsigned int)g26_hmi_elapsed_ms(
                       event->started_tick, display_tick),
                   (unsigned int)g26_hmi_elapsed_ms(
                       state->request_tick, display_tick));
    }
    state->request_tick = 0U;
    if (g26_hmi_continue_measurement(state) != XST_SUCCESS) {
        status = XST_FAILURE;
    }
    return status;
}

static int g26_hmi_process_clear(g26_hmi_state_t *state)
{
    uint8_t page;
    int status;

    g26_hmi_reset_display_tracking(state);
    g26_hmi_session_stop(&state->session);
    memset(&g26_hmi_snapshot, 0, sizeof(g26_hmi_snapshot));
    state->request_tick = 0U;
    if (pl_hmi_uart_get_current_page(&state->uart, &page) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    if (page != G26_HMI_PAGE_TIME_DOMAIN &&
        page != G26_HMI_PAGE_SPECTRUM) {
        xil_printf("[HMI] KEY2_SYNC_CLEAR_SKIPPED page=%u\r\n",
                   (unsigned int)page);
        return XST_SUCCESS;
    }
    status = g26_hmi_prepare_page(state, page);
    if (g26_hmi_unlock_navigation(state) != XST_SUCCESS) {
        status = XST_FAILURE;
    }
    if (status == XST_SUCCESS) {
        xil_printf("[HMI] KEY2_SYNC_CLEAR page=%u\r\n",
                   (unsigned int)page);
    }
    return status;
}

static int g26_hmi_handle_measurement_event(
    g26_hmi_state_t *state, const g26_measurement_event_t *event)
{
    if (event->type == G26_MEASUREMENT_STARTED) {
        if (event->source_page == 0U) {
            return g26_hmi_sync_key_start(state, event);
        }
        if (!g26_hmi_session_accept_event(
                &state->session, event->generation, event->source_page)) {
            xil_printf("[HMI] STARTED_IGNORED generation=%u page=%u\r\n",
                       (unsigned int)event->generation,
                       (unsigned int)event->source_page);
        }
        return XST_SUCCESS;
    }
    if (event->type == G26_MEASUREMENT_COMPLETED) {
        return g26_hmi_process_completion(state, event);
    }
    if (event->type == G26_MEASUREMENT_CLEARED) {
        return g26_hmi_process_clear(state);
    }
    return XST_FAILURE;
}

void g26_hmi_task(void *parameters)
{
    g26_hmi_state_t state;

    (void)parameters;
    portTASK_USES_FLOATING_POINT();
    memset(&state, 0, sizeof(state));
    memset(&g26_hmi_snapshot, 0, sizeof(g26_hmi_snapshot));
    state.next_generation = 1U;
    g26_hmi_session_init(&state.session);

    if (pl_hmi_uart_init(
            &state.uart, XPAR_UARTLITE_0_DEVICE_ID,
            XPAR_UARTLITE_0_BASEADDR) != XST_SUCCESS) {
        xil_printf("[HMI] FATAL: J11 UARTLite initialization failed\r\n");
        vTaskDelete(NULL);
        return;
    }
    xil_printf("[HMI] READY: J11 UARTLite %u 8N1, KEY1/KEY2 sync enabled\r\n",
               (unsigned int)XPAR_UARTLITE_0_BAUDRATE);

    for (;;) {
        hmi_event_t touch;
        g26_measurement_event_t measurement_event;
        int poll_status;

        do {
            poll_status = pl_hmi_uart_poll_event(&state.uart, &touch);
            if (poll_status == 1 &&
                g26_hmi_handle_touch(&state, &touch) != XST_SUCCESS) {
                xil_printf("[HMI] ERROR: touch page=%u command=0x%02x\r\n",
                           (unsigned int)touch.page,
                           (unsigned int)touch.command);
            }
        } while (poll_status == 1);
        if (poll_status < 0) {
            xil_printf("[HMI] ERROR: UART receive poll failed\r\n");
        }

        while (g26_measurement_poll_event(&measurement_event) == 1) {
            if (g26_hmi_handle_measurement_event(
                    &state, &measurement_event) != XST_SUCCESS) {
                xil_printf("[HMI] ERROR: measurement event=%u generation=%u status=%d\r\n",
                           (unsigned int)measurement_event.type,
                           (unsigned int)measurement_event.generation,
                           measurement_event.status);
            }
        }
        vTaskDelay(G26_HMI_TASK_POLL_TICKS);
    }
}
