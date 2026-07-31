#ifndef USER_SCREEN_PL_HMI_UART_H_
#define USER_SCREEN_PL_HMI_UART_H_

#include "hmi_protocol.h"

#include "xuartlite.h"

#include <stddef.h>
#include <stdint.h>

#define PL_HMI_UART_PENDING_EVENTS 4U

typedef struct {
    XUartLite instance;
    hmi_event_parser_t event_parser;
    hmi_page_parser_t page_parser;
    hmi_transfer_parser_t transfer_parser;
    hmi_event_t pending_events[PL_HMI_UART_PENDING_EVENTS];
    uint8_t pending_head;
    uint8_t pending_count;
    uint8_t pending_transfer_markers;
    int serial_buffer_overflow;
    int pending_event_overflow;
    int transfer_desynchronized;
    int initialized;
} pl_hmi_uart_t;

int pl_hmi_uart_init(pl_hmi_uart_t *uart, uint16_t device_id,
                     UINTPTR expected_base_address);
int pl_hmi_uart_send_command(pl_hmi_uart_t *uart, const char *command,
                             size_t command_length);
int pl_hmi_uart_send_waveform(pl_hmi_uart_t *uart, uint8_t component_id,
                              uint8_t channel, const uint8_t *samples,
                              size_t sample_count);
int pl_hmi_uart_poll_event(pl_hmi_uart_t *uart, hmi_event_t *event);
int pl_hmi_uart_get_current_page(pl_hmi_uart_t *uart, uint8_t *page);

#endif /* USER_SCREEN_PL_HMI_UART_H_ */
