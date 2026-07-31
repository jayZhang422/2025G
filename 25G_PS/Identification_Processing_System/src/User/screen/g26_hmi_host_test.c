/*
 * Host-only regression. Target builds leave this translation unit empty.
 * Example:
 * gcc -DG26_HMI_HOST_TEST hmi_protocol.c g26_hmi_render.c \
 *     g26_hmi_host_test.c -o g26_hmi_host_test
 */
#ifdef G26_HMI_HOST_TEST

#include "g26_hmi_render.h"
#include "hmi_protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n",                \
                    __FILE__, __LINE__, #condition);                         \
            return 1;                                                        \
        }                                                                    \
    } while (0)

static int test_event_parser(void)
{
    static const uint8_t stream[] = {
        0x00U, 0xAAU, 0xAAU, 0x55U, 0x02U, 0x33U, 0xFFU,
        0xAAU, 0x55U, 0x03U, 0x11U, 0x00U,
        0xAAU, 0x55U, 0x03U, 0x11U, 0xFFU
    };
    static const uint8_t pages[] = {0x02U, 0x03U};
    static const uint8_t commands[] = {0x33U, 0x11U};
    hmi_event_parser_t parser;
    hmi_event_t event;
    size_t index;
    size_t count = 0U;

    hmi_event_parser_init(&parser);
    for (index = 0U; index < sizeof(stream); index++) {
        if (hmi_event_parser_feed(&parser, stream[index], &event) != 0) {
            CHECK(count < sizeof(pages));
            CHECK(event.page == pages[count]);
            CHECK(event.command == commands[count]);
            count++;
        }
    }
    CHECK(count == sizeof(pages));
    return 0;
}

static int test_page_and_transfer_parsers(void)
{
    static const uint8_t page_stream[] = {
        0x66U, 0x03U, 0xFFU, 0x00U,
        0x66U, 0x02U, 0xFFU, 0xFFU, 0xFFU
    };
    static const uint8_t marker_stream[] = {
        HMI_TRANSFER_READY, 0xFFU, 0xFFU, 0xFFU,
        HMI_TRANSFER_DONE, 0xFFU, 0xFFU, 0xFFU
    };
    hmi_page_parser_t page_parser;
    hmi_transfer_parser_t transfer_parser;
    uint8_t page = 0U;
    uint8_t marker = 0U;
    size_t index;
    int page_count = 0;
    int marker_count = 0;

    hmi_page_parser_init(&page_parser);
    for (index = 0U; index < sizeof(page_stream); index++) {
        page_count += hmi_page_parser_feed(
            &page_parser, page_stream[index], &page);
    }
    CHECK(page_count == 1);
    CHECK(page == G26_HMI_PAGE_TIME_DOMAIN);

    hmi_transfer_parser_init(&transfer_parser);
    for (index = 0U; index < sizeof(marker_stream); index++) {
        if (hmi_transfer_parser_feed(
                &transfer_parser, marker_stream[index], &marker) != 0) {
            marker_count++;
            CHECK(marker == ((marker_count == 1) ?
                  HMI_TRANSFER_READY : HMI_TRANSFER_DONE));
        }
    }
    CHECK(marker_count == 2);
    return 0;
}

static int test_session_generation_gate(void)
{
    g26_hmi_session_t session;

    g26_hmi_session_init(&session);
    CHECK(!g26_hmi_session_is_pending(&session));
    g26_hmi_session_start(
        &session, 7U, 0U, G26_HMI_PAGE_TIME_DOMAIN);
    CHECK(g26_hmi_session_is_pending(&session));
    CHECK(g26_hmi_session_accept_event(&session, 7U, 0U));
    CHECK(!g26_hmi_session_accept_event(&session, 8U, 0U));
    CHECK(!g26_hmi_session_accept_event(
        &session, 7U, G26_HMI_PAGE_TIME_DOMAIN));
    g26_hmi_session_publish_snapshot(&session);
    CHECK(!g26_hmi_session_is_pending(&session));
    CHECK(session.snapshot_valid == 1);
    CHECK(session.active_page == G26_HMI_PAGE_TIME_DOMAIN);

    session.period_count = 3U;
    session.show_time_parameters = 1;
    session.show_amplitudes = 1;
    g26_hmi_session_continue(
        &session, 8U, G26_HMI_PAGE_TIME_DOMAIN);
    CHECK(g26_hmi_session_is_pending(&session));
    CHECK(g26_hmi_session_accept_event(
        &session, 8U, G26_HMI_PAGE_TIME_DOMAIN));
    CHECK(session.snapshot_valid == 1);
    CHECK(session.period_count == 3U);
    CHECK(session.show_time_parameters == 1);
    CHECK(session.show_amplitudes == 1);

    g26_hmi_session_publish_invalid(&session);
    CHECK(!g26_hmi_session_is_pending(&session));
    CHECK(session.snapshot_valid == 0);
    CHECK(session.active_page == G26_HMI_PAGE_TIME_DOMAIN);
    CHECK(session.period_count == 3U);
    CHECK(session.show_time_parameters == 1);
    CHECK(session.show_amplitudes == 1);
    g26_hmi_session_stop(&session);
    CHECK(session.snapshot_valid == 0);
    return 0;
}

static int test_render_mapping(void)
{
    float source[640];
    uint8_t destination[601];
    size_t index;
    size_t count;

    CHECK(g26_hmi_frequency_tenths_khz(10500.0f) == 105U);
    CHECK(g26_hmi_frequency_integer_khz(10500.0f) == 11U);
    CHECK(g26_hmi_millivolts_thousandths(49.8714f) == 49871U);
    CHECK(g26_hmi_spectrum_x(100000.0f) == G26_HMI_SPECTRUM_X100);
    CHECK(g26_hmi_spectrum_x(500000.0f) == G26_HMI_SPECTRUM_X500);
    CHECK(g26_hmi_spectrum_top(50.0f, 100.0f) ==
          G26_HMI_SPECTRUM_Y050);

    for (index = 0U; index < 640U; index++) {
        source[index] = -1.0f + 2.0f * (float)index / 639.0f;
    }
    count = g26_hmi_waveform_resample(
        source, 640U, destination, sizeof(destination), 601U);
    CHECK(count == 601U);
    CHECK(destination[0] == 24U);
    CHECK(destination[count - 1U] == 254U);
    CHECK(destination[count / 2U] == 139U);
    return 0;
}

static int test_measurement_change_gate(void)
{
    g26_hmi_measurement_signature_t previous;
    g26_hmi_measurement_signature_t current;

    memset(&previous, 0, sizeof(previous));
    memset(&current, 0, sizeof(current));

    previous.component_count = 2U;
    previous.harmonic_order[0] = 1U;
    previous.harmonic_order[1] = 2U;
    previous.frequency_hz[0] = 100000.0f;
    previous.frequency_hz[1] = 200000.0f;
    previous.amplitude_mv[0] = 50.0f;
    previous.amplitude_mv[1] = 25.0f;
    previous.relative_phase_rad[1] = 3.13f;
    previous.fundamental_frequency_hz = 100000.0f;
    previous.rms_mv = 39.528f;
    previous.upp_mv = 140.0f;
    current = previous;

    CHECK(!g26_hmi_signature_changed(&previous, &current));
    current.frequency_hz[1] += G26_HMI_CHANGE_FREQUENCY_HZ;
    CHECK(!g26_hmi_signature_changed(&previous, &current));
    current.frequency_hz[1] += 1.0f;
    CHECK(g26_hmi_signature_changed(&previous, &current));

    current = previous;
    current.amplitude_mv[1] += G26_HMI_CHANGE_VOLTAGE_MV + 0.01f;
    CHECK(g26_hmi_signature_changed(&previous, &current));

    current = previous;
    current.relative_phase_rad[1] = -3.13f;
    CHECK(!g26_hmi_signature_changed(&previous, &current));
    current.relative_phase_rad[1] = -3.0f;
    CHECK(g26_hmi_signature_changed(&previous, &current));

    current = previous;
    current.component_count = 3U;
    CHECK(g26_hmi_signature_changed(&previous, &current));
    return 0;
}

int main(void)
{
    CHECK(test_event_parser() == 0);
    CHECK(test_page_and_transfer_parsers() == 0);
    CHECK(test_session_generation_gate() == 0);
    CHECK(test_render_mapping() == 0);
    CHECK(test_measurement_change_gate() == 0);
    puts("G26 HMI host tests passed");
    return 0;
}

#endif /* G26_HMI_HOST_TEST */
