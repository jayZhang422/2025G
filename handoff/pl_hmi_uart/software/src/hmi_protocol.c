#include "hmi_protocol.h"

void hmi_event_parser_init(hmi_event_parser_t *parser)
{
    if (parser != 0)
        parser->count = 0U;
}

static void hmi_event_parser_restart(hmi_event_parser_t *parser,
                                     uint8_t byte)
{
    parser->count = 0U;
    if (byte == HMI_EVENT_HEADER0) {
        parser->bytes[0] = byte;
        parser->count = 1U;
    }
}

int hmi_event_parser_feed(hmi_event_parser_t *parser,
                          uint8_t byte,
                          hmi_event_frame_t *frame)
{
    if (parser == 0 || frame == 0)
        return 0;

    if (parser->count == 0U) {
        hmi_event_parser_restart(parser, byte);
        return 0;
    }
    if (parser->count == 1U && byte != HMI_EVENT_HEADER1) {
        hmi_event_parser_restart(parser, byte);
        return 0;
    }
    if (parser->count == HMI_EVENT_FRAME_SIZE - 1U &&
        byte != HMI_EVENT_TAIL) {
        hmi_event_parser_restart(parser, byte);
        return 0;
    }

    parser->bytes[parser->count++] = byte;
    if (parser->count != HMI_EVENT_FRAME_SIZE)
        return 0;

    frame->command = parser->bytes[2];
    frame->view = parser->bytes[3];
    frame->data = (uint32_t)parser->bytes[4] |
                  ((uint32_t)parser->bytes[5] << 8U) |
                  ((uint32_t)parser->bytes[6] << 16U) |
                  ((uint32_t)parser->bytes[7] << 24U);
    parser->count = 0U;
    return 1;
}
