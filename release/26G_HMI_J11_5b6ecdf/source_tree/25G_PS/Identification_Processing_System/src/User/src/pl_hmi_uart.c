#include "../include/pl_hmi_uart.h"

#include "xstatus.h"
#include "xtime_l.h"

#define PL_HMI_UART_FIFO_BYTES        16U
#define PL_HMI_UART_TX_TIMEOUT_COUNTS ((XTime)COUNTS_PER_SECOND)
#define PL_HMI_UART_PAGE_TIMEOUT_COUNTS ((XTime)COUNTS_PER_SECOND / 10U)

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
            &uart->instance,
            (uint8_t *)(uintptr_t)(bytes + sent),
            chunk);

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

int pl_hmi_uart_init(pl_hmi_uart_t *uart,
                     uint16_t device_id,
                     UINTPTR expected_base_address)
{
    static const char disable_command_reply[] = "bkcmd=0";
    XUartLite_Config *config;

    if (uart == 0) {
        return XST_FAILURE;
    }

    uart->initialized = 0;
    config = XUartLite_LookupConfig(device_id);
    if (config == 0 || config->RegBaseAddr != expected_base_address) {
        return XST_FAILURE;
    }
    if (XUartLite_CfgInitialize(&uart->instance, config,
                                config->RegBaseAddr) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    if (XUartLite_SelfTest(&uart->instance) != XST_SUCCESS) {
        return XST_FAILURE;
    }

    XUartLite_ResetFifos(&uart->instance);
    hmi_event_parser_init(&uart->parser);
    hmi_page_parser_init(&uart->page_parser);
    uart->pending_head = 0U;
    uart->pending_count = 0U;
    uart->initialized = 1;
    if (pl_hmi_uart_send_command(
            uart, disable_command_reply,
            sizeof(disable_command_reply) - 1U) != XST_SUCCESS) {
        uart->initialized = 0;
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int pl_hmi_uart_send_raw(pl_hmi_uart_t *uart,
                         const uint8_t *bytes,
                         size_t byte_count)
{
    return pl_hmi_uart_send_bytes(uart, bytes, byte_count);
}

int pl_hmi_uart_send_command(pl_hmi_uart_t *uart,
                             const char *command,
                             size_t command_length)
{
    static const uint8_t terminator[] = {0xFFU, 0xFFU, 0xFFU};

    if (command == 0 || command_length == 0U) {
        return XST_FAILURE;
    }
    if (pl_hmi_uart_send_bytes(uart, (const uint8_t *)command,
                               command_length) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    return pl_hmi_uart_send_bytes(uart, terminator, sizeof(terminator));
}

int pl_hmi_uart_poll_event(pl_hmi_uart_t *uart,
                           hmi_event_frame_t *frame)
{
    uint8_t byte;

    if (uart == 0 || frame == 0 || uart->initialized == 0) {
        return -1;
    }

    if (uart->pending_count != 0U) {
        *frame = uart->pending_events[uart->pending_head];
        uart->pending_head = (uint8_t)(
            (uart->pending_head + 1U) % PL_HMI_UART_PENDING_EVENTS);
        uart->pending_count--;
        return 1;
    }

    while (XUartLite_Recv(&uart->instance, &byte, 1U) == 1U) {
        if (hmi_event_parser_feed(&uart->parser, byte, frame) != 0) {
            return 1;
        }
    }
    return 0;
}

static int pl_hmi_uart_queue_event(pl_hmi_uart_t *uart,
                                   const hmi_event_frame_t *frame)
{
    uint8_t tail;

    if (uart->pending_count >= PL_HMI_UART_PENDING_EVENTS) {
        return XST_FAILURE;
    }
    tail = (uint8_t)((uart->pending_head + uart->pending_count) %
                     PL_HMI_UART_PENDING_EVENTS);
    uart->pending_events[tail] = *frame;
    uart->pending_count++;
    return XST_SUCCESS;
}

int pl_hmi_uart_get_current_page(pl_hmi_uart_t *uart, uint8_t *page)
{
    static const char sendme_command[] = "sendme";
    hmi_event_frame_t event;
    XTime start;
    XTime now;
    uint8_t byte;

    if (uart == 0 || page == 0 || uart->initialized == 0) {
        return XST_FAILURE;
    }

    while (XUartLite_Recv(&uart->instance, &byte, 1U) == 1U) {
        if (hmi_event_parser_feed(&uart->parser, byte, &event) != 0 &&
            pl_hmi_uart_queue_event(uart, &event) != XST_SUCCESS) {
            return XST_FAILURE;
        }
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
            if (hmi_event_parser_feed(
                    &uart->parser, byte, &event) != 0 &&
                pl_hmi_uart_queue_event(uart, &event) != XST_SUCCESS) {
                return XST_FAILURE;
            }
            if (hmi_page_parser_feed(
                    &uart->page_parser, byte, page) != 0) {
                return XST_SUCCESS;
            }
        }
        XTime_GetTime(&now);
        if (now - start > PL_HMI_UART_PAGE_TIMEOUT_COUNTS) {
            return XST_FAILURE;
        }
    }
}
