#ifndef HMI_PROTOCOL_H
#define HMI_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HMI_EVENT_FRAME_SIZE 9U
#define HMI_EVENT_HEADER0    0x55U
#define HMI_EVENT_HEADER1    0xAAU
#define HMI_EVENT_TAIL       0xFFU

typedef struct {
    uint8_t command;
    uint8_t view;
    uint32_t data;
} hmi_event_frame_t;

typedef struct {
    uint8_t bytes[HMI_EVENT_FRAME_SIZE];
    uint8_t count;
} hmi_event_parser_t;

void hmi_event_parser_init(hmi_event_parser_t *parser);

int hmi_event_parser_feed(hmi_event_parser_t *parser,
                          uint8_t byte,
                          hmi_event_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif
