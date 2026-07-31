#ifndef HMI_PROTOCOL_H
#define HMI_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HMI_EVENT_FRAME_SIZE 5U
#define HMI_EVENT_HEADER0    0xAAU
#define HMI_EVENT_HEADER1    0x55U
#define HMI_EVENT_TAIL       0xFFU
#define HMI_PAGE_FRAME_SIZE  5U
#define HMI_PAGE_HEADER      0x66U

/*
 * Keep the established frame type and parser API. For the 26G display
 * contract, view is the page byte, command is the command byte, and data is
 * reserved as zero.
 */
typedef struct {
    uint8_t command;
    uint8_t view;
    uint32_t data;
} hmi_event_frame_t;

typedef struct {
    uint8_t bytes[HMI_EVENT_FRAME_SIZE];
    uint8_t count;
} hmi_event_parser_t;

typedef struct {
    uint8_t bytes[HMI_PAGE_FRAME_SIZE];
    uint8_t count;
} hmi_page_parser_t;

void hmi_event_parser_init(hmi_event_parser_t *parser);

int hmi_event_parser_feed(hmi_event_parser_t *parser,
                          uint8_t byte,
                          hmi_event_frame_t *frame);

void hmi_page_parser_init(hmi_page_parser_t *parser);

/** Parse the TJC sendme reply: 0x66, page, 0xFF, 0xFF, 0xFF. */
int hmi_page_parser_feed(hmi_page_parser_t *parser,
                         uint8_t byte,
                         uint8_t *page);

#ifdef __cplusplus
}
#endif

#endif /* HMI_PROTOCOL_H */
