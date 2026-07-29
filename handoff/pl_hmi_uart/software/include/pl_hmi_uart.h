#ifndef PL_HMI_UART_H
#define PL_HMI_UART_H

#include <stddef.h>
#include <stdint.h>

#include "hmi_protocol.h"
#include "xuartlite.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    XUartLite instance;
    hmi_event_parser_t parser;
    int initialized;
} pl_hmi_uart_t;

int pl_hmi_uart_init(pl_hmi_uart_t *uart,
                     uint16_t device_id,
                     UINTPTR expected_base_address);

int pl_hmi_uart_send_command(pl_hmi_uart_t *uart,
                             const char *command,
                             size_t command_length);

int pl_hmi_uart_poll_event(pl_hmi_uart_t *uart,
                           hmi_event_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif
