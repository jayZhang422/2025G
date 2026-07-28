#include "../include/hmi_protocol.h"

void hmi_protocol_parser_init(hmi_protocol_parser_t *parser)
{
    if (parser == 0) {
        return;
    }

    parser->count = 0U;
}

static void hmi_protocol_restart(hmi_protocol_parser_t *parser, uint8_t byte)
{
    parser->count = 0U;
    if (byte == HMI_PROTOCOL_HEADER0) {
        parser->bytes[0] = byte;
        parser->count = 1U;
    }
}

int hmi_protocol_parser_feed(hmi_protocol_parser_t *parser,
                             uint8_t byte,
                             hmi_protocol_frame_t *frame)
{
    if (parser == 0 || frame == 0) {
        return 0;
    }

    if (parser->count == 0U) {
        hmi_protocol_restart(parser, byte);
        return 0;
    }

    if (parser->count == 1U && byte != HMI_PROTOCOL_HEADER1) {
        hmi_protocol_restart(parser, byte);
        return 0;
    }

    if (parser->count == (HMI_PROTOCOL_FRAME_SIZE - 1U) &&
        byte != HMI_PROTOCOL_TAIL) {
        hmi_protocol_restart(parser, byte);
        return 0;
    }

    parser->bytes[parser->count] = byte;
    parser->count++;
    if (parser->count != HMI_PROTOCOL_FRAME_SIZE) {
        return 0;
    }

    frame->command = parser->bytes[2];
    frame->mode = parser->bytes[3];
    frame->data = (uint32_t)parser->bytes[4] |
                  ((uint32_t)parser->bytes[5] << 8) |
                  ((uint32_t)parser->bytes[6] << 16) |
                  ((uint32_t)parser->bytes[7] << 24);
    parser->count = 0U;
    return 1;
}

int hmi_protocol_mode_is_valid(uint8_t mode)
{
    return mode >= HMI_PROTOCOL_MODE_SIGNAL_SOURCE &&
           mode <= HMI_PROTOCOL_MODE_LEARNING;
}
