#include "pl_hmi_uart.h"

#include "xstatus.h"
#include "xtime_l.h"

#define PL_HMI_UART_FIFO_BYTES        16U
#define PL_HMI_UART_TX_TIMEOUT_COUNTS ((XTime)COUNTS_PER_SECOND)

static int pl_hmi_uart_send_bytes(pl_hmi_uart_t *uart,
                                  const uint8_t *bytes,
                                  size_t byte_count)
{
    XTime start;
    XTime now;
    size_t sent = 0U;

    if (uart == 0 || bytes == 0 || uart->initialized == 0)
        return XST_FAILURE;

    XTime_GetTime(&start);
    while (sent < byte_count) {
        size_t remaining = byte_count - sent;
        unsigned int chunk = (remaining > PL_HMI_UART_FIFO_BYTES) ?
                             PL_HMI_UART_FIFO_BYTES :
                             (unsigned int)remaining;
        unsigned int accepted = XUartLite_Send(
            &uart->instance,
            (uint8_t *)(uintptr_t)(bytes + sent),
            chunk);

        sent += accepted;
        while (XUartLite_IsSending(&uart->instance)) {
            XTime_GetTime(&now);
            if (now - start > PL_HMI_UART_TX_TIMEOUT_COUNTS)
                return XST_FAILURE;
        }
        if (accepted == 0U) {
            XTime_GetTime(&now);
            if (now - start > PL_HMI_UART_TX_TIMEOUT_COUNTS)
                return XST_FAILURE;
        }
    }
    return XST_SUCCESS;
}

int pl_hmi_uart_init(pl_hmi_uart_t *uart,
                     uint16_t device_id,
                     UINTPTR expected_base_address)
{
    static const uint8_t resync[] = {0x00U, 0xFFU, 0xFFU, 0xFFU};
    XUartLite_Config *config;

    if (uart == 0)
        return XST_FAILURE;

    uart->initialized = 0;
    config = XUartLite_LookupConfig(device_id);
    if (config == 0 || config->RegBaseAddr != expected_base_address)
        return XST_FAILURE;
    if (XUartLite_CfgInitialize(&uart->instance, config,
                                config->RegBaseAddr) != XST_SUCCESS)
        return XST_FAILURE;
    if (XUartLite_SelfTest(&uart->instance) != XST_SUCCESS)
        return XST_FAILURE;

    XUartLite_ResetFifos(&uart->instance);
    hmi_event_parser_init(&uart->parser);
    uart->initialized = 1;
    if (pl_hmi_uart_send_bytes(uart, resync, sizeof(resync)) != XST_SUCCESS) {
        uart->initialized = 0;
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int pl_hmi_uart_send_command(pl_hmi_uart_t *uart,
                             const char *command,
                             size_t command_length)
{
    static const uint8_t terminator[] = {0xFFU, 0xFFU, 0xFFU};

    if (command == 0 || command_length == 0U)
        return XST_FAILURE;
    if (pl_hmi_uart_send_bytes(uart, (const uint8_t *)command,
                               command_length) != XST_SUCCESS)
        return XST_FAILURE;
    return pl_hmi_uart_send_bytes(uart, terminator, sizeof(terminator));
}

int pl_hmi_uart_poll_event(pl_hmi_uart_t *uart,
                           hmi_event_frame_t *frame)
{
    uint8_t byte;

    if (uart == 0 || frame == 0 || uart->initialized == 0)
        return XST_FAILURE;

    while (XUartLite_Recv(&uart->instance, &byte, 1U) == 1U) {
        if (hmi_event_parser_feed(&uart->parser, byte, frame) != 0)
            return 1;
    }
    return 0;
}
