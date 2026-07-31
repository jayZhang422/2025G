#ifndef USER_SCREEN_HMI_PROTOCOL_H_
#define USER_SCREEN_HMI_PROTOCOL_H_

#include <stdint.h>

#define HMI_EVENT_FRAME_SIZE       5U
#define HMI_EVENT_HEADER0          0xAAU
#define HMI_EVENT_HEADER1          0x55U
#define HMI_EVENT_TAIL             0xFFU
#define HMI_PAGE_FRAME_SIZE        5U
#define HMI_PAGE_HEADER            0x66U
#define HMI_TRANSFER_FRAME_SIZE    4U
#define HMI_TRANSFER_READY         0xFEU
#define HMI_TRANSFER_DONE          0xFDU
#define HMI_SERIAL_BUFFER_OVERFLOW 0x24U

typedef struct {
    uint8_t command;
    uint8_t page;
} hmi_event_t;

typedef struct {
    uint8_t bytes[HMI_EVENT_FRAME_SIZE];
    uint8_t count;
} hmi_event_parser_t;

typedef struct {
    uint8_t bytes[HMI_PAGE_FRAME_SIZE];
    uint8_t count;
} hmi_page_parser_t;

typedef struct {
    uint8_t bytes[HMI_TRANSFER_FRAME_SIZE];
    uint8_t count;
} hmi_transfer_parser_t;

void hmi_event_parser_init(hmi_event_parser_t *parser);
int hmi_event_parser_feed(hmi_event_parser_t *parser, uint8_t byte,
                          hmi_event_t *event);

void hmi_page_parser_init(hmi_page_parser_t *parser);
int hmi_page_parser_feed(hmi_page_parser_t *parser, uint8_t byte,
                         uint8_t *page);

void hmi_transfer_parser_init(hmi_transfer_parser_t *parser);
int hmi_transfer_parser_feed(hmi_transfer_parser_t *parser, uint8_t byte,
                             uint8_t *marker);

#endif /* USER_SCREEN_HMI_PROTOCOL_H_ */
