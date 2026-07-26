/******************************************************************************
 * signal_profiles.h
 *
 * Contest-level parameters. Zero-valued future profiles are placeholders and
 * must be configured before a matching mode is implemented or selected.
 ******************************************************************************/

#ifndef USER_CONFIG_SIGNAL_PROFILES_H_
#define USER_CONFIG_SIGNAL_PROFILES_H_

#include "arm_math.h"
#include "xil_types.h"

typedef enum {
    SIGNAL_MODE_SEPARATOR = 0,
    SIGNAL_MODE_MEASURE,
    SIGNAL_MODE_LOCKIN,
    SIGNAL_MODE_AM,
    SIGNAL_MODE_ASK,
    SIGNAL_MODE_FSK,
    SIGNAL_MODE_FM,
    SIGNAL_MODE_SELF_TEST
} signal_mode_t;

typedef enum {
    PROFILE_OLD_SEPARATOR = 0,
    PROFILE_GENERAL_MEASURE,
    PROFILE_WEAK_SIGNAL,
    PROFILE_AM,
    PROFILE_ASK,
    PROFILE_2FSK,
    PROFILE_FM,
    SIGNAL_PROFILE_COUNT
} signal_profile_id_t;

#define SIGNAL_WAVEFORM_MASK_SINE      0x01U
#define SIGNAL_WAVEFORM_MASK_TRIANGLE  0x02U
#define SIGNAL_WAVEFORM_MASK_ALL \
    (SIGNAL_WAVEFORM_MASK_SINE | SIGNAL_WAVEFORM_MASK_TRIANGLE)

typedef struct {
    const char *name;
    signal_mode_t mode;
    float32_t frequency_min_hz;
    float32_t frequency_max_hz;
    float32_t frequency_grid_hz;
    float32_t expected_carrier_hz;
    float32_t expected_deviation_hz;
    float32_t expected_symbol_rate;
    float32_t grid_lock_tolerance_hz;
    float32_t lock_max_residual;
    u32 confirm_frames;
    u32 lock_timeout_seconds;
    u32 allowed_waveforms;
    u16 dds_amplitude_code;
    float32_t initial_phase_degrees;
    float32_t phase_step_degrees;
    float32_t phase_max_degrees;
} signal_profile_t;

const signal_profile_t *signal_profile_get(signal_profile_id_t id);
const signal_profile_t *signal_profile_default(void);
u32 signal_profile_count(void);
int signal_profile_is_configured(const signal_profile_t *profile);

#endif /* USER_CONFIG_SIGNAL_PROFILES_H_ */
