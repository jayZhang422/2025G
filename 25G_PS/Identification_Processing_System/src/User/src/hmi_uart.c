#include "../include/app_config.h"
#include "../include/hmi_uart.h"

#include "xstatus.h"

int hmi_uart_init(hmi_uart_t *uart)
{
    XUartPs_Config *config;

    if (uart == 0) {
        return XST_FAILURE;
    }

    uart->initialized = 0;
    config = XUartPs_LookupConfig(APP_HMI_UART_DEVICE_ID);
    if (config == 0 ||
        XUartPs_CfgInitialize(&uart->instance, config, config->BaseAddress) !=
        XST_SUCCESS) {
        return XST_FAILURE;
    }
    if (XUartPs_SetBaudRate(&uart->instance, APP_HMI_UART_BAUD_RATE) !=
        XST_SUCCESS) {
        return XST_FAILURE;
    }

    hmi_protocol_parser_init(&uart->parser);
    uart->initialized = 1;
    return XST_SUCCESS;
}

int hmi_uart_poll(hmi_uart_t *uart, hmi_protocol_frame_t *frame)
{
    u8 byte;

    if (uart == 0 || frame == 0 || uart->initialized == 0) {
        return XST_FAILURE;
    }

    while (XUartPs_IsReceiveData(uart->instance.Config.BaseAddress)) {
        if (XUartPs_Recv(&uart->instance, &byte, 1U) != 1U) {
            return 0;
        }
        if (hmi_protocol_parser_feed(&uart->parser, byte, frame) != 0) {
            return 1;
        }
    }

    return 0;
}
