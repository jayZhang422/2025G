#ifndef USER_SCREEN_G26_HMI_RENDER_H_
#define USER_SCREEN_G26_HMI_RENDER_H_

#include <stddef.h>
#include <stdint.h>

#define G26_HMI_PAGE_TIME_DOMAIN 2U
#define G26_HMI_PAGE_SPECTRUM    3U

#define G26_HMI_COMMAND_STOP            0x00U
#define G26_HMI_COMMAND_START           0x01U
#define G26_HMI_COMMAND_ONE_PERIOD      0x11U
#define G26_HMI_COMMAND_TIME_PARAMETERS 0x12U
#define G26_HMI_COMMAND_THREE_PERIODS   0x33U
#define G26_HMI_COMMAND_SHOW_AMPLITUDE  0x11U

#define G26_HMI_WAVEFORM_MAX_POINTS          601U
#define G26_HMI_PAGE2_WAVEFORM_OBJECT        "s0"
#define G26_HMI_PAGE2_WAVEFORM_COMPONENT_ID  1U
#define G26_HMI_PAGE2_WAVEFORM_CHANNEL       0U
#define G26_HMI_WAVEFORM_COLOR               65504U
#define G26_HMI_WAVEFORM_CENTER_CODE         139.0f
#define G26_HMI_WAVEFORM_HALF_RANGE_CODE     115.0f

#define G26_HMI_PAGE2_UPP_OBJECT         "x0"
#define G26_HMI_PAGE2_RMS_OBJECT         "x1"
#define G26_HMI_PAGE2_FUNDAMENTAL_OBJECT "n0"

#define G26_HMI_PAGE3_FREQUENCY0_OBJECT "x0"
#define G26_HMI_PAGE3_FREQUENCY1_OBJECT "x1"
#define G26_HMI_PAGE3_FREQUENCY2_OBJECT "x2"
#define G26_HMI_PAGE3_AMPLITUDE0_OBJECT "x3"
#define G26_HMI_PAGE3_AMPLITUDE1_OBJECT "x4"
#define G26_HMI_PAGE3_AMPLITUDE2_OBJECT "x5"
#define G26_HMI_PAGE3_THIRD_LABEL_OBJECT "t7"
#define G26_HMI_PAGE3_PLOT_OBJECT        "s0"

/* Coordinates calibrated for the current 1616x976 page-3 background. */
#define G26_HMI_SPECTRUM_X0   71
#define G26_HMI_SPECTRUM_X100 152
#define G26_HMI_SPECTRUM_X200 233
#define G26_HMI_SPECTRUM_X300 314
#define G26_HMI_SPECTRUM_X400 395
#define G26_HMI_SPECTRUM_X500 476
#define G26_HMI_SPECTRUM_Y0   274
#define G26_HMI_SPECTRUM_Y025 217
#define G26_HMI_SPECTRUM_Y050 161
#define G26_HMI_SPECTRUM_Y075 105
#define G26_HMI_SPECTRUM_Y1   48

typedef struct {
    uint32_t pending_generation;
    uint8_t source_page;
    uint8_t active_page;
    uint8_t period_count;
    int show_time_parameters;
    int show_amplitudes;
    int snapshot_valid;
} g26_hmi_session_t;

void g26_hmi_session_init(g26_hmi_session_t *session);
void g26_hmi_session_start(g26_hmi_session_t *session, uint32_t generation,
                           uint8_t source_page, uint8_t active_page);
void g26_hmi_session_continue(g26_hmi_session_t *session,
                              uint32_t generation, uint8_t source_page);
void g26_hmi_session_stop(g26_hmi_session_t *session);
int g26_hmi_session_is_pending(const g26_hmi_session_t *session);
int g26_hmi_session_accept_event(const g26_hmi_session_t *session,
                                 uint32_t generation, uint8_t source_page);
void g26_hmi_session_publish_snapshot(g26_hmi_session_t *session);
void g26_hmi_session_publish_invalid(g26_hmi_session_t *session);

uint32_t g26_hmi_frequency_tenths_khz(float frequency_hz);
uint32_t g26_hmi_frequency_integer_khz(float frequency_hz);
uint32_t g26_hmi_millivolts_thousandths(float value_mv);
int g26_hmi_spectrum_x(float frequency_hz);
int g26_hmi_spectrum_top(float amplitude_mv, float maximum_amplitude_mv);

size_t g26_hmi_waveform_resample(const float *source, size_t source_count,
                                 uint8_t *destination,
                                 size_t destination_capacity,
                                 size_t requested_count);

#endif /* USER_SCREEN_G26_HMI_RENDER_H_ */
