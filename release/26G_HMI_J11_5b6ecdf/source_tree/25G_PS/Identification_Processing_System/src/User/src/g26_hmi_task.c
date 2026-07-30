/******************************************************************************
 * g26_hmi_task.c
 *
 * Sole owner of the polled J11 AXI UARTLite and the final 26G TJC contract.
 ******************************************************************************/

#include "../include/g26_hmi_task.h"

#include "../include/g26_hmi_render.h"
#include "../include/g26_measurement_app.h"
#include "../include/pl_hmi_uart.h"

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

#if XPAR_UARTLITE_0_DATA_BITS != 8U
#error "The final HMI requires eight UART data bits"
#endif
#if XPAR_UARTLITE_0_USE_PARITY != 0U
#error "The final HMI requires parity disabled"
#endif
#if G26_SIGNAL_MAX_COMPONENTS != 3U
#error "The final HMI exposes exactly three spectrum component rows"
#endif

#define G26_HMI_TASK_POLL_TICKS 1U
#define G26_HMI_COMMAND_BYTES   64U
#define G26_HMI_SPECTRUM_COLOR  65504U

typedef struct {
    pl_hmi_uart_t uart;
    u32 next_generation;
    TickType_t measurement_started_at;
    g26_hmi_session_t session;
} g26_hmi_state_t;

static g26_measurement_output_t g26_hmi_snapshot;
static uint8_t g26_hmi_waveform_bytes[G26_HMI_WAVEFORM_MAX_POINTS];

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
                               const char *object_name,
                               int visible)
{
    return g26_hmi_send_format(
        uart, "vis %s,%u", object_name, visible ? 1U : 0U);
}

static int g26_hmi_set_value(pl_hmi_uart_t *uart,
                             const char *object_name,
                             u32 value)
{
    return g26_hmi_send_format(
        uart, "%s.val=%u", object_name, (unsigned int)value);
}

static int g26_hmi_set_text_tenths(pl_hmi_uart_t *uart,
                                   const char *object_name,
                                   u32 value_tenths)
{
    return g26_hmi_send_format(
        uart, "%s.txt=\"%u.%u\"", object_name,
        (unsigned int)(value_tenths / 10U),
        (unsigned int)(value_tenths % 10U));
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

static int g26_hmi_prepare_page2(g26_hmi_state_t *state)
{
    if (g26_hmi_send_format(
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

static int g26_hmi_render_page2_waveform(g26_hmi_state_t *state)
{
    const float32_t *source = (state->session.period_count == 3U) ?
        g26_hmi_snapshot.three_period_mv :
        g26_hmi_snapshot.one_period_mv;
    size_t sample_count;
    size_t index;

    sample_count = g26_hmi_waveform_resample(
        source, APP_G26_WAVEFORM_POINTS,
        g26_hmi_waveform_bytes, sizeof(g26_hmi_waveform_bytes),
        G26_HMI_WAVEFORM_MAX_POINTS);
    if (sample_count == 0U ||
        g26_hmi_send_format(
            &state->uart, "cle %u,%u",
            G26_HMI_PAGE2_WAVEFORM_COMPONENT_ID,
            G26_HMI_PAGE2_WAVEFORM_CHANNEL) != XST_SUCCESS) {
        return XST_FAILURE;
    }

    for (index = 0U; index < sample_count; index++) {
        if (g26_hmi_send_format(
                &state->uart, "add %u,%u,%u",
                G26_HMI_PAGE2_WAVEFORM_COMPONENT_ID,
                G26_HMI_PAGE2_WAVEFORM_CHANNEL,
                (unsigned int)g26_hmi_waveform_bytes[index]) !=
            XST_SUCCESS) {
            return XST_FAILURE;
        }
    }
    return XST_SUCCESS;
}

static int g26_hmi_render_page2_parameters(g26_hmi_state_t *state)
{
    const g26_signal_result_t *result = &g26_hmi_snapshot.result;

    if (g26_hmi_set_value(
            &state->uart, G26_HMI_PAGE2_UPP_OBJECT,
            g26_hmi_millivolts_integer(result->upp_mv)) != XST_SUCCESS ||
        g26_hmi_set_value(
            &state->uart, G26_HMI_PAGE2_RMS_OBJECT,
            g26_hmi_millivolts_integer(result->rms_mv)) != XST_SUCCESS ||
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

static int g26_hmi_draw_spectrum_line(g26_hmi_state_t *state,
                                      int x, int top)
{
    int height = G26_HMI_SPECTRUM_Y0 - top;

    if (height <= 0) {
        return XST_SUCCESS;
    }

    return g26_hmi_send_format(
        &state->uart, "fill %d,%d,3,%d,%u", x - 1, top, height,
        (unsigned int)G26_HMI_SPECTRUM_COLOR);
}

static int g26_hmi_render_page3(g26_hmi_state_t *state)
{
    const g26_signal_result_t *result = &g26_hmi_snapshot.result;
    float32_t maximum_amplitude =
        g26_hmi_maximum_component_amplitude(result);
    u32 component;

    if (result->component_count < 2U ||
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
            int x = g26_hmi_spectrum_x(line->frequency_hz);
            int top = g26_hmi_spectrum_top(
                line->amplitude_mv, maximum_amplitude);

            /* amplitude_mv is the analyzer's sinusoidal peak amplitude. */
            if (g26_hmi_set_value(
                    &state->uart, frequency_object,
                    g26_hmi_frequency_tenths_khz(
                        line->frequency_hz)) != XST_SUCCESS ||
                g26_hmi_set_text_tenths(
                    &state->uart, amplitude_object,
                    g26_hmi_amplitude_tenths_mv(
                        line->amplitude_mv)) != XST_SUCCESS ||
                g26_hmi_set_visible(
                    &state->uart, frequency_object, 1) != XST_SUCCESS ||
                g26_hmi_set_visible(
                    &state->uart, amplitude_object,
                    state->session.show_amplitudes) != XST_SUCCESS ||
                g26_hmi_draw_spectrum_line(state, x, top) !=
                    XST_SUCCESS) {
                return XST_FAILURE;
            }
        } else {
            if (g26_hmi_set_visible(
                    &state->uart, frequency_object, 0) != XST_SUCCESS ||
                g26_hmi_set_visible(
                    &state->uart, amplitude_object, 0) != XST_SUCCESS) {
                return XST_FAILURE;
            }
        }
    }

    if (g26_hmi_set_visible(
            &state->uart, G26_HMI_PAGE3_THIRD_LABEL_OBJECT,
            result->component_count == 3U) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

static int g26_hmi_render_active_page(g26_hmi_state_t *state)
{
    if (!state->session.snapshot_valid) {
        return XST_SUCCESS;
    }
    if (state->session.active_page == G26_HMI_PAGE_TIME_DOMAIN) {
        if (g26_hmi_render_page2_waveform(state) != XST_SUCCESS) {
            return XST_FAILURE;
        }
        if (state->session.show_time_parameters &&
            g26_hmi_render_page2_parameters(state) != XST_SUCCESS) {
            return XST_FAILURE;
        }
    } else if (state->session.active_page == G26_HMI_PAGE_SPECTRUM) {
        return g26_hmi_render_page3(state);
    }
    return XST_SUCCESS;
}

static int g26_hmi_render_page2_waveform_locked(g26_hmi_state_t *state)
{
    int status;

    if (g26_hmi_set_navigation_enabled(
            state, G26_HMI_PAGE_TIME_DOMAIN, 0) != XST_SUCCESS) {
        (void)g26_hmi_set_navigation_enabled(
            state, G26_HMI_PAGE_TIME_DOMAIN, 1);
        return XST_FAILURE;
    }
    status = g26_hmi_render_page2_waveform(state);
    if (g26_hmi_set_navigation_enabled(
            state, G26_HMI_PAGE_TIME_DOMAIN, 1) != XST_SUCCESS) {
        status = XST_FAILURE;
    }
    return status;
}

static int g26_hmi_render_page3_locked(g26_hmi_state_t *state)
{
    int status;

    if (g26_hmi_set_navigation_enabled(
            state, G26_HMI_PAGE_SPECTRUM, 0) != XST_SUCCESS) {
        (void)g26_hmi_set_navigation_enabled(
            state, G26_HMI_PAGE_SPECTRUM, 1);
        return XST_FAILURE;
    }
    status = g26_hmi_render_page3(state);
    if (g26_hmi_set_navigation_enabled(
            state, G26_HMI_PAGE_SPECTRUM, 1) != XST_SUCCESS) {
        status = XST_FAILURE;
    }
    return status;
}

static u32 g26_hmi_allocate_generation(g26_hmi_state_t *state)
{
    u32 generation = state->next_generation++;

    if (generation == 0U) {
        generation = state->next_generation++;
    }
    return generation;
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

    g26_hmi_session_start(&state->session, generation, page);
    state->measurement_started_at = xTaskGetTickCount();

    if (g26_hmi_set_navigation_enabled(state, page, 0) != XST_SUCCESS ||
        (page == G26_HMI_PAGE_TIME_DOMAIN &&
         g26_hmi_prepare_page2(state) != XST_SUCCESS) ||
        (page == G26_HMI_PAGE_SPECTRUM &&
         g26_hmi_prepare_page3(state) != XST_SUCCESS) ||
        g26_measurement_request(generation, page) != XST_SUCCESS) {
        g26_hmi_session_stop(&state->session);
        state->measurement_started_at = 0U;
        (void)g26_hmi_set_navigation_enabled(state, page, 1);
        return XST_FAILURE;
    }
    xil_printf("[HMI] MEASUREMENT_REQUEST generation=%u page=%u\r\n",
               (unsigned int)generation, (unsigned int)page);
    return XST_SUCCESS;
}

static int g26_hmi_handle_page2_event(g26_hmi_state_t *state, u8 command)
{
    if (command == G26_HMI_COMMAND_START) {
        return g26_hmi_start_measurement(
            state, G26_HMI_PAGE_TIME_DOMAIN);
    }
    if (command == G26_HMI_COMMAND_STOP) {
        int status;

        g26_hmi_session_stop(&state->session);
        state->measurement_started_at = 0U;
        status = g26_hmi_prepare_page2(state);
        if (g26_hmi_set_navigation_enabled(
                state, G26_HMI_PAGE_TIME_DOMAIN, 1) != XST_SUCCESS) {
            status = XST_FAILURE;
        }
        return status;
    }

    state->session.active_page = G26_HMI_PAGE_TIME_DOMAIN;
    if (command == G26_HMI_COMMAND_ONE_PERIOD) {
        state->session.period_count = 1U;
        return state->session.snapshot_valid ?
            g26_hmi_render_page2_waveform_locked(state) : XST_SUCCESS;
    }
    if (command == G26_HMI_COMMAND_THREE_PERIODS) {
        state->session.period_count = 3U;
        return state->session.snapshot_valid ?
            g26_hmi_render_page2_waveform_locked(state) : XST_SUCCESS;
    }
    if (command == G26_HMI_COMMAND_TIME_PARAMETERS) {
        state->session.show_time_parameters = 1;
        return state->session.snapshot_valid ?
            g26_hmi_render_page2_parameters(state) : XST_SUCCESS;
    }
    return XST_SUCCESS;
}

static int g26_hmi_handle_page3_event(g26_hmi_state_t *state, u8 command)
{
    if (command == G26_HMI_COMMAND_START) {
        return g26_hmi_start_measurement(
            state, G26_HMI_PAGE_SPECTRUM);
    }
    if (command == G26_HMI_COMMAND_STOP) {
        int status;

        g26_hmi_session_stop(&state->session);
        state->measurement_started_at = 0U;
        status = g26_hmi_prepare_page3(state);
        if (g26_hmi_set_navigation_enabled(
                state, G26_HMI_PAGE_SPECTRUM, 1) != XST_SUCCESS) {
            status = XST_FAILURE;
        }
        return status;
    }
    if (command == G26_HMI_COMMAND_SHOW_AMPLITUDE) {
        state->session.active_page = G26_HMI_PAGE_SPECTRUM;
        state->session.show_amplitudes = 1;
        return state->session.snapshot_valid ?
            g26_hmi_render_page3_locked(state) : XST_SUCCESS;
    }
    return XST_SUCCESS;
}

static int g26_hmi_handle_event(g26_hmi_state_t *state,
                                const hmi_event_frame_t *frame)
{
    xil_printf("[HMI] EVENT page=%u command=0x%02x\r\n",
               (unsigned int)frame->view,
               (unsigned int)frame->command);
    if (frame->view == G26_HMI_PAGE_TIME_DOMAIN) {
        return g26_hmi_handle_page2_event(state, frame->command);
    }
    if (frame->view == G26_HMI_PAGE_SPECTRUM) {
        return g26_hmi_handle_page3_event(state, frame->command);
    }
    return XST_SUCCESS;
}

static int g26_hmi_process_completion(
    g26_hmi_state_t *state,
    const g26_measurement_completion_t *completion)
{
    u32 generation;
    u8 page;
    int status;

    if (!g26_hmi_session_accept_completion(
            &state->session, completion->generation,
            completion->source_page)) {
        return XST_SUCCESS;
    }
    if (completion->status != XST_SUCCESS) {
        page = state->session.request_page;
        g26_hmi_session_stop(&state->session);
        state->measurement_started_at = 0U;
        (void)g26_hmi_set_navigation_enabled(state, page, 1);
        return XST_FAILURE;
    }
    page = state->session.active_page;
    if (g26_measurement_snapshot(
            &g26_hmi_snapshot, &generation) != XST_SUCCESS ||
        generation != completion->generation) {
        g26_hmi_session_stop(&state->session);
        state->measurement_started_at = 0U;
        (void)g26_hmi_set_navigation_enabled(state, page, 1);
        return XST_FAILURE;
    }

    g26_hmi_session_publish_snapshot(&state->session);
    status = g26_hmi_render_active_page(state);
    if (g26_hmi_set_navigation_enabled(state, page, 1) != XST_SUCCESS) {
        status = XST_FAILURE;
    }
    if (status == XST_SUCCESS) {
        u32 elapsed_ms = (u32)(
            ((u64)(xTaskGetTickCount() - state->measurement_started_at) *
             1000U) / configTICK_RATE_HZ);

        xil_printf("[HMI] DISPLAY_TX_COMPLETE generation=%u page=%u elapsed=%u ms\r\n",
                   (unsigned int)completion->generation,
                   (unsigned int)page,
                   (unsigned int)elapsed_ms);
    }
    state->measurement_started_at = 0U;
    return status;
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
    xil_printf("[HMI] READY: J11 UARTLite %u 8N1\r\n",
               (unsigned int)XPAR_UARTLITE_0_BAUDRATE);

    for (;;) {
        hmi_event_frame_t frame;
        g26_measurement_completion_t completion;
        int poll_status;

        do {
            poll_status = pl_hmi_uart_poll_event(&state.uart, &frame);
            if (poll_status == 1 &&
                g26_hmi_handle_event(&state, &frame) != XST_SUCCESS) {
                xil_printf("[HMI] ERROR: event page=%u command=0x%02x\r\n",
                           (unsigned int)frame.view,
                           (unsigned int)frame.command);
            }
        } while (poll_status == 1);
        if (poll_status < 0) {
            xil_printf("[HMI] ERROR: UART receive poll failed\r\n");
        }

        while (g26_measurement_poll_completion(&completion) == 1) {
            if (g26_hmi_process_completion(
                    &state, &completion) != XST_SUCCESS) {
                xil_printf("[HMI] ERROR: measurement generation=%u status=%d\r\n",
                           (unsigned int)completion.generation,
                           completion.status);
            }
        }
        vTaskDelay(G26_HMI_TASK_POLL_TICKS);
    }
}
