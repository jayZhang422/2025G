#include "../Identification_Processing_System/src/User/include/g26_hmi_render.h"
#include "../Identification_Processing_System/src/User/include/hmi_protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                  \
                    __FILE__, __LINE__, #condition);                         \
            return 1;                                                        \
        }                                                                    \
    } while (0)

static int test_parser_noise_and_fragmentation(void)
{
    static const uint8_t boot_noise[] = {
        0x00U, 0x00U, 0x00U, 0xFFU, 0xFFU,
        0xFFU, 0x88U, 0xFFU, 0xFFU, 0xFFU
    };
    static const uint8_t frame_bytes[] = {
        0xAAU, 0x55U, 0x02U, 0x01U, 0xFFU
    };
    hmi_event_parser_t parser;
    hmi_event_frame_t frame;
    size_t index;

    hmi_event_parser_init(&parser);
    for (index = 0U; index < sizeof(boot_noise); index++) {
        CHECK(hmi_event_parser_feed(
                  &parser, boot_noise[index], &frame) == 0);
    }
    CHECK(hmi_event_parser_feed(&parser, frame_bytes[0], &frame) == 0);
    CHECK(hmi_event_parser_feed(&parser, frame_bytes[1], &frame) == 0);
    for (index = 2U; index + 1U < sizeof(frame_bytes); index++) {
        CHECK(hmi_event_parser_feed(
                  &parser, frame_bytes[index], &frame) == 0);
    }
    CHECK(hmi_event_parser_feed(
              &parser, frame_bytes[sizeof(frame_bytes) - 1U], &frame) == 1);
    CHECK(frame.view == 0x02U);
    CHECK(frame.command == 0x01U);
    CHECK(frame.data == 0U);
    return 0;
}

static int test_parser_back_to_back_and_resync(void)
{
    static const uint8_t stream[] = {
        0xAAU, 0xAAU, 0x55U, 0x02U, 0x33U, 0xFFU,
        0xAAU, 0x55U, 0x03U, 0x01U, 0x00U,
        0xAAU, 0x55U, 0x03U, 0x11U, 0xFFU,
        0xAAU, 0x55U, 0x02U, 0x12U, 0xAAU,
        0x55U, 0x03U, 0x00U, 0xFFU
    };
    static const uint8_t expected_page[] = {0x02U, 0x03U, 0x03U};
    static const uint8_t expected_command[] = {0x33U, 0x11U, 0x00U};
    hmi_event_parser_t parser;
    hmi_event_frame_t frame;
    size_t index;
    size_t event_count = 0U;

    hmi_event_parser_init(&parser);
    for (index = 0U; index < sizeof(stream); index++) {
        if (hmi_event_parser_feed(&parser, stream[index], &frame) != 0) {
            CHECK(event_count <
                  sizeof(expected_page) / sizeof(expected_page[0]));
            CHECK(frame.view == expected_page[event_count]);
            CHECK(frame.command == expected_command[event_count]);
            event_count++;
        }
    }
    CHECK(event_count == sizeof(expected_page) / sizeof(expected_page[0]));
    return 0;
}

static int test_final_event_matrix(void)
{
    static const uint8_t events[][HMI_EVENT_FRAME_SIZE] = {
        {0xAAU, 0x55U, 0x02U, 0x01U, 0xFFU},
        {0xAAU, 0x55U, 0x02U, 0x00U, 0xFFU},
        {0xAAU, 0x55U, 0x02U, 0x11U, 0xFFU},
        {0xAAU, 0x55U, 0x02U, 0x12U, 0xFFU},
        {0xAAU, 0x55U, 0x02U, 0x33U, 0xFFU},
        {0xAAU, 0x55U, 0x03U, 0x01U, 0xFFU},
        {0xAAU, 0x55U, 0x03U, 0x00U, 0xFFU},
        {0xAAU, 0x55U, 0x03U, 0x11U, 0xFFU}
    };
    hmi_event_parser_t parser;
    hmi_event_frame_t frame;
    size_t event;
    size_t byte;

    hmi_event_parser_init(&parser);
    for (event = 0U; event < sizeof(events) / sizeof(events[0]); event++) {
        for (byte = 0U; byte < HMI_EVENT_FRAME_SIZE; byte++) {
            int complete = hmi_event_parser_feed(
                &parser, events[event][byte], &frame);

            CHECK(complete == (byte + 1U == HMI_EVENT_FRAME_SIZE));
        }
        CHECK(frame.view == events[event][2]);
        CHECK(frame.command == events[event][3]);
        CHECK(frame.data == 0U);
    }
    return 0;
}

static int test_sendme_page_parser(void)
{
    static const uint8_t stream[] = {
        0x01U, 0xFFU, 0xFFU, 0xFFU,
        0x66U, 0x03U, 0xFFU, 0x00U,
        0x66U, 0x02U, 0xFFU, 0xFFU, 0xFFU
    };
    hmi_page_parser_t parser;
    uint8_t page = 0xFFU;
    size_t index;
    int completions = 0;

    hmi_page_parser_init(&parser);
    for (index = 0U; index < sizeof(stream); index++) {
        if (hmi_page_parser_feed(&parser, stream[index], &page) != 0) {
            completions++;
        }
    }
    CHECK(completions == 1);
    CHECK(page == G26_HMI_PAGE_TIME_DOMAIN);
    return 0;
}

static int test_session_generation_gate(void)
{
    g26_hmi_session_t session;

    g26_hmi_session_init(&session);
    CHECK(session.pending_generation == 0U);
    CHECK(!g26_hmi_session_is_pending(&session));
    CHECK(session.period_count == 1U);
    CHECK(session.snapshot_valid == 0);

    g26_hmi_session_start(&session, 7U, G26_HMI_PAGE_SPECTRUM);
    CHECK(session.pending_generation == 7U);
    CHECK(g26_hmi_session_is_pending(&session));
    CHECK(session.request_page == G26_HMI_PAGE_SPECTRUM);
    CHECK(session.active_page == G26_HMI_PAGE_SPECTRUM);
    CHECK(g26_hmi_session_accept_completion(
              &session, 7U, G26_HMI_PAGE_SPECTRUM));
    CHECK(!g26_hmi_session_accept_completion(
              &session, 6U, G26_HMI_PAGE_SPECTRUM));
    CHECK(!g26_hmi_session_accept_completion(
              &session, 7U, G26_HMI_PAGE_TIME_DOMAIN));

    g26_hmi_session_stop(&session);
    CHECK(session.pending_generation == 0U);
    CHECK(!g26_hmi_session_is_pending(&session));
    CHECK(session.snapshot_valid == 0);
    CHECK(!g26_hmi_session_accept_completion(
              &session, 7U, G26_HMI_PAGE_SPECTRUM));

    g26_hmi_session_start(&session, 8U, G26_HMI_PAGE_TIME_DOMAIN);
    CHECK(g26_hmi_session_is_pending(&session));
    g26_hmi_session_publish_snapshot(&session);
    CHECK(session.pending_generation == 0U);
    CHECK(!g26_hmi_session_is_pending(&session));
    CHECK(session.request_page == 0U);
    CHECK(session.snapshot_valid == 1);
    CHECK(session.active_page == G26_HMI_PAGE_TIME_DOMAIN);
    CHECK(!g26_hmi_session_accept_completion(
              &session, 8U, G26_HMI_PAGE_TIME_DOMAIN));
    return 0;
}

static int test_contract_and_numeric_mapping(void)
{
    CHECK(G26_HMI_PAGE_TIME_DOMAIN == 2U);
    CHECK(G26_HMI_PAGE_SPECTRUM == 3U);
    CHECK(G26_HMI_PAGE2_WAVEFORM_COMPONENT_ID == 1U);
    CHECK(G26_HMI_PAGE2_WAVEFORM_CHANNEL == 0U);
    CHECK(strcmp(G26_HMI_PAGE2_UPP_OBJECT, "x0") == 0);
    CHECK(strcmp(G26_HMI_PAGE2_RMS_OBJECT, "x1") == 0);
    CHECK(strcmp(G26_HMI_PAGE2_FUNDAMENTAL_OBJECT, "n0") == 0);
    CHECK(strcmp(G26_HMI_PAGE2_STATUS_OBJECT, "status") == 0);
    CHECK(strcmp(G26_HMI_PAGE3_FREQUENCY2_OBJECT, "x2") == 0);
    CHECK(strcmp(G26_HMI_PAGE3_AMPLITUDE2_OBJECT, "t2") == 0);
    CHECK(strcmp(G26_HMI_PAGE3_THIRD_LABEL_OBJECT, "t7") == 0);
    CHECK(strcmp(G26_HMI_PAGE3_STATUS_OBJECT, "status1") == 0);

    CHECK(g26_hmi_frequency_tenths_khz(10000.0f) == 100U);
    CHECK(g26_hmi_frequency_tenths_khz(10500.0f) == 105U);
    CHECK(g26_hmi_frequency_tenths_khz(500000.0f) == 5000U);
    CHECK(g26_hmi_frequency_integer_khz(10500.0f) == 11U);
    CHECK(g26_hmi_millivolts_integer(100.49f) == 100U);
    CHECK(g26_hmi_millivolts_integer(100.50f) == 101U);
    CHECK(g26_hmi_amplitude_tenths_mv(5.0f) == 50U);
    CHECK(g26_hmi_amplitude_tenths_mv(125.4f) == 1254U);
    return 0;
}

static int test_spectrum_boundaries_and_component_counts(void)
{
    static const float two_component_amplitudes[] = {5.0f, 100.0f};
    static const float three_component_amplitudes[] = {40.0f, 20.0f, 10.0f};
    float maximum;
    size_t index;

    CHECK(g26_hmi_spectrum_x(0.0f) == G26_HMI_SPECTRUM_X0);
    CHECK(g26_hmi_spectrum_x(10000.0f) == 79);
    CHECK(g26_hmi_spectrum_x(100000.0f) == G26_HMI_SPECTRUM_X100);
    CHECK(g26_hmi_spectrum_x(200000.0f) == G26_HMI_SPECTRUM_X200);
    CHECK(g26_hmi_spectrum_x(300000.0f) == G26_HMI_SPECTRUM_X300);
    CHECK(g26_hmi_spectrum_x(400000.0f) == G26_HMI_SPECTRUM_X400);
    CHECK(g26_hmi_spectrum_x(500000.0f) == G26_HMI_SPECTRUM_X500);
    CHECK(g26_hmi_spectrum_x(700000.0f) == G26_HMI_SPECTRUM_X500);
    CHECK(g26_hmi_spectrum_top(0.0f, 100.0f) == G26_HMI_SPECTRUM_Y0);
    CHECK(g26_hmi_spectrum_top(0.01f, 100.0f) ==
          G26_HMI_SPECTRUM_Y0 - 1);
    CHECK(g26_hmi_spectrum_top(25.0f, 100.0f) ==
          G26_HMI_SPECTRUM_Y025);
    CHECK(g26_hmi_spectrum_top(50.0f, 100.0f) ==
          G26_HMI_SPECTRUM_Y050);
    CHECK(g26_hmi_spectrum_top(75.0f, 100.0f) ==
          G26_HMI_SPECTRUM_Y075);
    CHECK(g26_hmi_spectrum_top(100.0f, 100.0f) ==
          G26_HMI_SPECTRUM_Y1);

    maximum = 0.0f;
    for (index = 0U;
         index < sizeof(two_component_amplitudes) /
                     sizeof(two_component_amplitudes[0]);
         index++) {
        if (two_component_amplitudes[index] > maximum) {
            maximum = two_component_amplitudes[index];
        }
    }
    CHECK(g26_hmi_spectrum_top(two_component_amplitudes[0], maximum) ==
          263);
    CHECK(g26_hmi_spectrum_top(two_component_amplitudes[1], maximum) ==
          G26_HMI_SPECTRUM_Y1);

    maximum = three_component_amplitudes[0];
    CHECK(g26_hmi_spectrum_top(three_component_amplitudes[0], maximum) ==
          G26_HMI_SPECTRUM_Y1);
    CHECK(g26_hmi_spectrum_top(three_component_amplitudes[1], maximum) ==
          G26_HMI_SPECTRUM_Y050);
    CHECK(g26_hmi_spectrum_top(three_component_amplitudes[2], maximum) ==
          G26_HMI_SPECTRUM_Y025);
    return 0;
}

static int test_waveform_resample_640_to_601(void)
{
    float source[640];
    uint8_t destination[700];
    size_t index;
    size_t count;

    for (index = 0U; index < 640U; index++) {
        source[index] = -1.0f + 2.0f * (float)index / 639.0f;
    }
    count = g26_hmi_waveform_resample(
        source, 640U, destination, sizeof(destination), 700U);
    CHECK(count == G26_HMI_WAVEFORM_MAX_POINTS);
    CHECK(destination[0] == 0U);
    CHECK(destination[count - 1U] == 255U);
    CHECK(destination[count / 2U] >= 127U);
    CHECK(destination[count / 2U] <= 128U);

    count = g26_hmi_waveform_resample(
        source, 640U, destination, 300U, 601U);
    CHECK(count == 300U);
    CHECK(destination[0] == 0U);
    CHECK(destination[count - 1U] == 255U);

    for (index = 0U; index < 640U; index++) {
        source[index] = 0.0f;
    }
    count = g26_hmi_waveform_resample(
        source, 640U, destination, sizeof(destination), 601U);
    CHECK(count == 601U);
    for (index = 0U; index < count; index++) {
        CHECK(destination[index] == 128U);
    }
    return 0;
}

int main(void)
{
    CHECK(test_parser_noise_and_fragmentation() == 0);
    CHECK(test_parser_back_to_back_and_resync() == 0);
    CHECK(test_final_event_matrix() == 0);
    CHECK(test_sendme_page_parser() == 0);
    CHECK(test_session_generation_gate() == 0);
    CHECK(test_contract_and_numeric_mapping() == 0);
    CHECK(test_spectrum_boundaries_and_component_counts() == 0);
    CHECK(test_waveform_resample_640_to_601() == 0);
    puts("G26 HMI host tests passed");
    return 0;
}
