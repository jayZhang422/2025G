#ifndef USER_INCLUDE_HMI_UART_H_
#define USER_INCLUDE_HMI_UART_H_

#include "hmi_protocol.h"
#include "xuartps.h"

typedef struct {
    XUartPs instance;
    hmi_protocol_parser_t parser;
    int initialized;
} hmi_uart_t;

int hmi_uart_init(hmi_uart_t *uart);
int hmi_uart_poll(hmi_uart_t *uart, hmi_protocol_frame_t *frame);

#endif /* USER_INCLUDE_HMI_UART_H_ */
