#include <stdint.h>
#include <stdio.h>

#include "hmi_protocol.h"

static int feed_bytes(hmi_protocol_parser_t *parser,
                      const uint8_t *bytes,
                      size_t count,
                      hmi_protocol_frame_t *frame)
{
    size_t index;
    int ready = 0;

    for (index = 0U; index < count; ++index) {
        ready = hmi_protocol_parser_feed(parser, bytes[index], frame);
    }
    return ready;
}

int main(void)
{
    const uint8_t source_start[] = {
        0x55U, 0xAAU, 0x02U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0xFFU
    };
    const uint8_t frequency[] = {
        0x55U, 0xAAU, 0x10U, 0x02U, 0xE8U, 0x03U, 0x00U, 0x00U, 0xFFU
    };
    const uint8_t malformed[] = {
        0x55U, 0xAAU, 0x02U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x55U
    };
    hmi_protocol_parser_t parser;
    hmi_protocol_frame_t frame;

    hmi_protocol_parser_init(&parser);
    if (feed_bytes(&parser, source_start, 4U, &frame) != 0 ||
        feed_bytes(&parser, &source_start[4], 5U, &frame) != 1 ||
        frame.command != HMI_PROTOCOL_CMD_START ||
        frame.mode != HMI_PROTOCOL_MODE_SIGNAL_SOURCE || frame.data != 0U) {
        return 1;
    }

    hmi_protocol_parser_init(&parser);
    if (feed_bytes(&parser, malformed, sizeof(malformed), &frame) != 0 ||
        feed_bytes(&parser, &frequency[1], sizeof(frequency) - 1U, &frame) != 1 ||
        frame.command != HMI_PROTOCOL_CMD_FREQUENCY_HZ ||
        frame.mode != HMI_PROTOCOL_MODE_AMPLITUDE_CONTROL ||
        frame.data != 1000U) {
        return 2;
    }

    if (!hmi_protocol_mode_is_valid(HMI_PROTOCOL_MODE_LEARNING) ||
        hmi_protocol_mode_is_valid(0U) || hmi_protocol_mode_is_valid(5U)) {
        return 3;
    }

    puts("HMI_PROTOCOL_SELF_TEST_PASSED");
    return 0;
}
