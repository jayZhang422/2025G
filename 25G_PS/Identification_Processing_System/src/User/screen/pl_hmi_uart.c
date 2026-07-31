#include "pl_hmi_uart.h"

#include "xil_printf.h"
#include "xstatus.h"
#include "xtime_l.h"

#include <stdio.h>

#define PL_HMI_UART_FIFO_BYTES              16U
#define PL_HMI_UART_TX_TIMEOUT_COUNTS       ((XTime)COUNTS_PER_SECOND)
#define PL_HMI_UART_PAGE_TIMEOUT_COUNTS     ((XTime)COUNTS_PER_SECOND / 10U)
#define PL_HMI_UART_TRANSFER_TIMEOUT_COUNTS ((XTime)COUNTS_PER_SECOND)
#define PL_HMI_UART_MAX_TRANSFER_BYTES      1024U
#define PL_HMI_UART_MARKER_READY_BIT        0x01U
#define PL_HMI_UART_MARKER_DONE_BIT         0x02U

static int pl_hmi_uart_queue_event(pl_hmi_uart_t *uart,
                                   const hmi_event_t *event);

static int pl_hmi_uart_send_bytes(pl_hmi_uart_t *uart,
                                  const uint8_t *bytes,
                                  size_t byte_count)
{
    XTime start;
    XTime now;
    size_t sent = 0U;

    if (uart == 0 || bytes == 0 || byte_count == 0U ||
        uart->initialized == 0) {
        return XST_FAILURE;
    }
    XTime_GetTime(&start);
    while (sent < byte_count) {
        size_t remaining = byte_count - sent;
        unsigned int chunk = (remaining > PL_HMI_UART_FIFO_BYTES) ?
            PL_HMI_UART_FIFO_BYTES : (unsigned int)remaining;
        unsigned int accepted = XUartLite_Send(
            &uart->instance, (uint8_t *)(uintptr_t)(bytes + sent), chunk);

        sent += accepted;
        while (XUartLite_IsSending(&uart->instance)) {
            XTime_GetTime(&now);
            if (now - start > PL_HMI_UART_TX_TIMEOUT_COUNTS) {
                return XST_FAILURE;
            }
        }
        if (accepted == 0U) {
            XTime_GetTime(&now);
            if (now - start > PL_HMI_UART_TX_TIMEOUT_COUNTS) {
                return XST_FAILURE;
            }
        }
    }
    return XST_SUCCESS;
}

int pl_hmi_uart_init(pl_hmi_uart_t *uart, uint16_t device_id,
                     UINTPTR expected_base_address)
{
    static const char disable_command_reply[] = "bkcmd=0";
    XUartLite_Config *config;

    if (uart == 0) {
        return XST_FAILURE;
    }
    uart->initialized = 0;
    config = XUartLite_LookupConfig(device_id);
    if (config == 0 || config->RegBaseAddr != expected_base_address ||
        XUartLite_CfgInitialize(&uart->instance, config,
                                config->RegBaseAddr) != XST_SUCCESS ||
        XUartLite_SelfTest(&uart->instance) != XST_SUCCESS) {
        return XST_FAILURE;
    }

    XUartLite_ResetFifos(&uart->instance);
    XUartLite_ClearStats(&uart->instance);
    hmi_event_parser_init(&uart->event_parser);
    hmi_page_parser_init(&uart->page_parser);
    hmi_transfer_parser_init(&uart->transfer_parser);
    uart->pending_head = 0U;
    uart->pending_count = 0U;
    uart->pending_transfer_markers = 0U;
    uart->serial_buffer_overflow = 0;
    uart->pending_event_overflow = 0;
    uart->transfer_desynchronized = 0;
    uart->initialized = 1;
    if (pl_hmi_uart_send_command(
            uart, disable_command_reply,
            sizeof(disable_command_reply) - 1U) != XST_SUCCESS) {
        uart->initialized = 0;
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int pl_hmi_uart_send_command(pl_hmi_uart_t *uart, const char *command,
                             size_t command_length)
{
    static const uint8_t terminator[] = {0xFFU, 0xFFU, 0xFFU};

    if (uart == 0 || command == 0 || command_length == 0U ||
        uart->initialized == 0 || uart->transfer_desynchronized != 0 ||
        pl_hmi_uart_send_bytes(uart, (const uint8_t *)command,
                               command_length) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    return pl_hmi_uart_send_bytes(uart, terminator, sizeof(terminator));
}

static uint8_t pl_hmi_uart_marker_bit(uint8_t marker)
{
    if (marker == HMI_TRANSFER_READY) {
        return PL_HMI_UART_MARKER_READY_BIT;
    }
    if (marker == HMI_TRANSFER_DONE) {
        return PL_HMI_UART_MARKER_DONE_BIT;
    }
    return 0U;
}

static void pl_hmi_uart_process_received_byte(pl_hmi_uart_t *uart,
                                               uint8_t byte)
{
    hmi_event_t event;
    uint8_t marker;

    if (hmi_event_parser_feed(
            &uart->event_parser, byte, &event) != 0 &&
        pl_hmi_uart_queue_event(uart, &event) != XST_SUCCESS) {
        uart->pending_event_overflow = 1;
    }
    if (hmi_transfer_parser_feed(
            &uart->transfer_parser, byte, &marker) != 0) {
        if (marker == HMI_SERIAL_BUFFER_OVERFLOW) {
            uart->serial_buffer_overflow = 1;
        } else {
            uart->pending_transfer_markers |=
                pl_hmi_uart_marker_bit(marker);
        }
    }
}

static void pl_hmi_uart_drain_receive(pl_hmi_uart_t *uart)
{
    uint8_t byte;

    while (XUartLite_Recv(&uart->instance, &byte, 1U) == 1U) {
        pl_hmi_uart_process_received_byte(uart, byte);
    }
}

static int pl_hmi_uart_take_receive_fault(pl_hmi_uart_t *uart)
{
    int fault = 0;

    if (uart->serial_buffer_overflow != 0) {
        xil_printf("[HMI] ERROR: display serial buffer overflow\r\n");
        uart->serial_buffer_overflow = 0;
        fault = 1;
    }
    if (uart->pending_event_overflow != 0) {
        xil_printf("[HMI] ERROR: pending touch-event queue overflow\r\n");
        uart->pending_event_overflow = 0;
        fault = 1;
    }
    return fault;
}

static int pl_hmi_uart_wait_transfer_marker(pl_hmi_uart_t *uart,
                                             uint8_t expected_marker)
{
    uint8_t expected_bit = pl_hmi_uart_marker_bit(expected_marker);
    XTime start;
    XTime now;

    if (expected_bit == 0U) {
        return XST_FAILURE;
    }
    XTime_GetTime(&start);
    for (;;) {
        uint8_t byte;

        uart->pending_transfer_markers &= expected_bit;
        if ((uart->pending_transfer_markers & expected_bit) != 0U) {
            uart->pending_transfer_markers &= (uint8_t)~expected_bit;
            return XST_SUCCESS;
        }
        while (XUartLite_Recv(&uart->instance, &byte, 1U) == 1U) {
            pl_hmi_uart_process_received_byte(uart, byte);
            uart->pending_transfer_markers &= expected_bit;
            if ((uart->pending_transfer_markers & expected_bit) != 0U) {
                uart->pending_transfer_markers &= (uint8_t)~expected_bit;
                return XST_SUCCESS;
            }
        }
        XTime_GetTime(&now);
        if (now - start > PL_HMI_UART_TRANSFER_TIMEOUT_COUNTS) {
            return XST_FAILURE;
        }
    }
}

int pl_hmi_uart_send_waveform(pl_hmi_uart_t *uart, uint8_t component_id,
                              uint8_t channel, const uint8_t *samples,
                              size_t sample_count)
{
    char command[32];
    int command_length;
    XUartLite_Stats stats_before;
    XUartLite_Stats stats_after;
    int receive_fault;

    if (uart == 0 || samples == 0 || sample_count == 0U ||
        sample_count > PL_HMI_UART_MAX_TRANSFER_BYTES || channel > 3U ||
        uart->initialized == 0 || uart->transfer_desynchronized != 0) {
        return XST_FAILURE;
    }
    command_length = snprintf(
        command, sizeof(command), "addt %u,%u,%u",
        (unsigned int)component_id, (unsigned int)channel,
        (unsigned int)sample_count);
    if (command_length <= 0 || (size_t)command_length >= sizeof(command)) {
        return XST_FAILURE;
    }
    pl_hmi_uart_drain_receive(uart);
    if (pl_hmi_uart_take_receive_fault(uart) != 0) {
        return XST_FAILURE;
    }

    uart->pending_transfer_markers = 0U;
    XUartLite_GetStats(&uart->instance, &stats_before);
    if (pl_hmi_uart_send_command(
            uart, command, (size_t)command_length) != XST_SUCCESS ||
        pl_hmi_uart_wait_transfer_marker(
            uart, HMI_TRANSFER_READY) != XST_SUCCESS) {
        uart->transfer_desynchronized = 1;
        xil_printf("[HMI] ERROR: addt ready handshake failed; reset required\r\n");
        return XST_FAILURE;
    }
    if (pl_hmi_uart_send_bytes(uart, samples, sample_count) != XST_SUCCESS ||
        pl_hmi_uart_wait_transfer_marker(
            uart, HMI_TRANSFER_DONE) != XST_SUCCESS) {
        uart->transfer_desynchronized = 1;
        xil_printf("[HMI] ERROR: addt completion handshake failed; reset required\r\n");
        return XST_FAILURE;
    }

    XUartLite_GetStats(&uart->instance, &stats_after);
    receive_fault = pl_hmi_uart_take_receive_fault(uart);
    if (stats_after.ReceiveOverrunErrors !=
            stats_before.ReceiveOverrunErrors ||
        stats_after.ReceiveFramingErrors !=
            stats_before.ReceiveFramingErrors ||
        stats_after.ReceiveParityErrors !=
            stats_before.ReceiveParityErrors) {
        xil_printf("[HMI] ERROR: UART RX fault overrun=%u framing=%u parity=%u\r\n",
                   (unsigned int)(stats_after.ReceiveOverrunErrors -
                                  stats_before.ReceiveOverrunErrors),
                   (unsigned int)(stats_after.ReceiveFramingErrors -
                                  stats_before.ReceiveFramingErrors),
                   (unsigned int)(stats_after.ReceiveParityErrors -
                                  stats_before.ReceiveParityErrors));
        receive_fault = 1;
    }
    uart->pending_transfer_markers = 0U;
    return receive_fault ? XST_FAILURE : XST_SUCCESS;
}

static int pl_hmi_uart_queue_event(pl_hmi_uart_t *uart,
                                   const hmi_event_t *event)
{
    uint8_t tail;

    if (uart->pending_count >= PL_HMI_UART_PENDING_EVENTS) {
        return XST_FAILURE;
    }
    tail = (uint8_t)((uart->pending_head + uart->pending_count) %
                     PL_HMI_UART_PENDING_EVENTS);
    uart->pending_events[tail] = *event;
    uart->pending_count++;
    return XST_SUCCESS;
}

int pl_hmi_uart_poll_event(pl_hmi_uart_t *uart, hmi_event_t *event)
{
    if (uart == 0 || event == 0 || uart->initialized == 0) {
        return -1;
    }
    if (uart->pending_count == 0U) {
        pl_hmi_uart_drain_receive(uart);
    }
    if (uart->pending_count != 0U) {
        *event = uart->pending_events[uart->pending_head];
        uart->pending_head = (uint8_t)(
            (uart->pending_head + 1U) % PL_HMI_UART_PENDING_EVENTS);
        uart->pending_count--;
        return 1;
    }
    return (pl_hmi_uart_take_receive_fault(uart) == 0) ? 0 : -1;
}

int pl_hmi_uart_get_current_page(pl_hmi_uart_t *uart, uint8_t *page)
{
    static const char sendme_command[] = "sendme";
    XTime start;
    XTime now;
    uint8_t byte;

    if (uart == 0 || page == 0 || uart->initialized == 0) {
        return XST_FAILURE;
    }
    pl_hmi_uart_drain_receive(uart);
    if (pl_hmi_uart_take_receive_fault(uart) != 0) {
        return XST_FAILURE;
    }
    hmi_page_parser_init(&uart->page_parser);
    if (pl_hmi_uart_send_command(
            uart, sendme_command, sizeof(sendme_command) - 1U) !=
        XST_SUCCESS) {
        return XST_FAILURE;
    }

    XTime_GetTime(&start);
    for (;;) {
        while (XUartLite_Recv(&uart->instance, &byte, 1U) == 1U) {
            pl_hmi_uart_process_received_byte(uart, byte);
            if (hmi_page_parser_feed(
                    &uart->page_parser, byte, page) != 0) {
                return (pl_hmi_uart_take_receive_fault(uart) == 0) ?
                    XST_SUCCESS : XST_FAILURE;
            }
        }
        XTime_GetTime(&now);
        if (now - start > PL_HMI_UART_PAGE_TIMEOUT_COUNTS) {
            return XST_FAILURE;
        }
    }
}
