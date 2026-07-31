#ifndef PL_HMI_UART_H
#define PL_HMI_UART_H

#include <stddef.h>
#include <stdint.h>

#include "hmi_protocol.h"
#include "xuartlite.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PL_HMI_UART_PENDING_EVENTS 4U

typedef struct {
    XUartLite instance;
    hmi_event_parser_t parser;
    hmi_page_parser_t page_parser;
    hmi_event_frame_t pending_events[PL_HMI_UART_PENDING_EVENTS];
    uint8_t pending_head;
    uint8_t pending_count;
    int initialized;
} pl_hmi_uart_t;

int pl_hmi_uart_init(pl_hmi_uart_t *uart,
                     uint16_t device_id,
                     UINTPTR expected_base_address);

int pl_hmi_uart_send_raw(pl_hmi_uart_t *uart,
                         const uint8_t *bytes,
                         size_t byte_count);

int pl_hmi_uart_send_command(pl_hmi_uart_t *uart,
                             const char *command,
                             size_t command_length);

int pl_hmi_uart_poll_event(pl_hmi_uart_t *uart,
                           hmi_event_frame_t *frame);

/** Query the currently displayed TJC page without dropping touch events. */
int pl_hmi_uart_get_current_page(pl_hmi_uart_t *uart, uint8_t *page);

#ifdef __cplusplus
}
#endif

#endif /* PL_HMI_UART_H */
