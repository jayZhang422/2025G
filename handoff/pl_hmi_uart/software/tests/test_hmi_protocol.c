#include "hmi_protocol.h"

#include <stdint.h>
#include <stdio.h>

static int feed_bytes(hmi_event_parser_t *parser,
                      const uint8_t *bytes,
                      uint32_t count,
                      hmi_event_frame_t *frame)
{
    uint32_t index;
    int parsed = 0;

    for (index = 0U; index < count; ++index)
        parsed += hmi_event_parser_feed(parser, bytes[index], frame);
    return parsed;
}

int main(void)
{
    const uint8_t noise[] = {0x01U, 0x55U, 0x10U, 0x55U};
    const uint8_t frame_bytes[] = {
        0xAAU, 0x21U, 0x03U, 0x78U, 0x56U, 0x34U, 0x12U, 0xFFU
    };
    const uint8_t malformed[] = {
        0x55U, 0xAAU, 0x20U, 0x01U, 0U, 0U, 0U, 0U, 0x00U
    };
    hmi_event_parser_t parser;
    hmi_event_frame_t frame;

    hmi_event_parser_init(&parser);
    if (feed_bytes(&parser, noise, sizeof(noise), &frame) != 0)
        return 1;
    if (feed_bytes(&parser, frame_bytes, sizeof(frame_bytes), &frame) != 1)
        return 2;
    if (frame.command != 0x21U || frame.view != 0x03U ||
        frame.data != 0x12345678U)
        return 3;
    if (feed_bytes(&parser, malformed, sizeof(malformed), &frame) != 0)
        return 4;
    puts("HMI_PROTOCOL_C_TEST_PASSED");
    return 0;
}
