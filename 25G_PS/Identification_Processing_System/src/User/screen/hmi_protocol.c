#include "hmi_protocol.h"

void hmi_event_parser_init(hmi_event_parser_t *parser)
{
    if (parser != 0) {
        parser->count = 0U;
    }
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

int hmi_event_parser_feed(hmi_event_parser_t *parser, uint8_t byte,
                          hmi_event_t *event)
{
    if (parser == 0 || event == 0) {
        return 0;
    }
    if (parser->count == 0U) {
        hmi_event_parser_restart(parser, byte);
        return 0;
    }
    if (parser->count == 1U) {
        if (byte != HMI_EVENT_HEADER1) {
            hmi_event_parser_restart(parser, byte);
            return 0;
        }
        parser->bytes[parser->count++] = byte;
        return 0;
    }
    if (parser->count == HMI_EVENT_FRAME_SIZE - 1U) {
        if (byte != HMI_EVENT_TAIL) {
            hmi_event_parser_restart(parser, byte);
            return 0;
        }
        parser->bytes[parser->count++] = byte;
        event->page = parser->bytes[2];
        event->command = parser->bytes[3];
        parser->count = 0U;
        return 1;
    }
    parser->bytes[parser->count++] = byte;
    return 0;
}

void hmi_page_parser_init(hmi_page_parser_t *parser)
{
    if (parser != 0) {
        parser->count = 0U;
    }
}

static void hmi_page_parser_restart(hmi_page_parser_t *parser,
                                    uint8_t byte)
{
    parser->count = 0U;
    if (byte == HMI_PAGE_HEADER) {
        parser->bytes[0] = byte;
        parser->count = 1U;
    }
}

int hmi_page_parser_feed(hmi_page_parser_t *parser, uint8_t byte,
                         uint8_t *page)
{
    if (parser == 0 || page == 0) {
        return 0;
    }
    if (parser->count == 0U) {
        hmi_page_parser_restart(parser, byte);
        return 0;
    }
    if (parser->count == 1U) {
        parser->bytes[parser->count++] = byte;
        return 0;
    }
    if (byte != HMI_EVENT_TAIL) {
        hmi_page_parser_restart(parser, byte);
        return 0;
    }
    parser->bytes[parser->count++] = byte;
    if (parser->count == HMI_PAGE_FRAME_SIZE) {
        *page = parser->bytes[1];
        parser->count = 0U;
        return 1;
    }
    return 0;
}

void hmi_transfer_parser_init(hmi_transfer_parser_t *parser)
{
    if (parser != 0) {
        parser->count = 0U;
    }
}

static int hmi_transfer_is_marker(uint8_t byte)
{
    return byte == HMI_TRANSFER_READY || byte == HMI_TRANSFER_DONE ||
        byte == HMI_SERIAL_BUFFER_OVERFLOW;
}

static void hmi_transfer_parser_restart(hmi_transfer_parser_t *parser,
                                        uint8_t byte)
{
    parser->count = 0U;
    if (hmi_transfer_is_marker(byte)) {
        parser->bytes[0] = byte;
        parser->count = 1U;
    }
}

int hmi_transfer_parser_feed(hmi_transfer_parser_t *parser, uint8_t byte,
                             uint8_t *marker)
{
    if (parser == 0 || marker == 0) {
        return 0;
    }
    if (parser->count == 0U) {
        hmi_transfer_parser_restart(parser, byte);
        return 0;
    }
    if (byte != HMI_EVENT_TAIL) {
        hmi_transfer_parser_restart(parser, byte);
        return 0;
    }
    parser->bytes[parser->count++] = byte;
    if (parser->count == HMI_TRANSFER_FRAME_SIZE) {
        *marker = parser->bytes[0];
        parser->count = 0U;
        return 1;
    }
    return 0;
}
