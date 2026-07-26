/******************************************************************************
 * signal_profiles.c
 *
 * PROFILE_OLD_SEPARATOR preserves the active contest behavior. Other entries
 * intentionally contain no guessed signal parameters.
 ******************************************************************************/

#include "algorithm_config.h"
#include "signal_profiles.h"

/* 静态存储期保证返回的指针在程序整个运行期间有效；const 防止调用者修改
 * 共享的题目参数。 */
static const signal_profile_t g_signal_profiles[SIGNAL_PROFILE_COUNT] = {
    {
        .name = "old_separator",
        .mode = SIGNAL_MODE_SEPARATOR,
        .frequency_min_hz = 20000.0f,
        .frequency_max_hz = 100000.0f,
        .frequency_grid_hz = 5000.0f,
        .grid_lock_tolerance_hz = 1000.0f,
        .lock_max_residual = 0.30f,
        .confirm_frames = 3U,
        .lock_timeout_seconds = 18U,
        .allowed_waveforms = SIGNAL_WAVEFORM_MASK_ALL,
        .dds_amplitude_code = APP_DDS_UNITY_AMPLITUDE,
        .initial_phase_degrees = 0.0f,
        .phase_step_degrees = 5.0f,
        .phase_max_degrees = 180.0f
    },
    { .name = "general_measure", .mode = SIGNAL_MODE_MEASURE },
    { .name = "weak_signal", .mode = SIGNAL_MODE_LOCKIN },
    { .name = "am", .mode = SIGNAL_MODE_AM },
    { .name = "ask", .mode = SIGNAL_MODE_ASK },
    { .name = "2fsk", .mode = SIGNAL_MODE_FSK },
    { .name = "fm", .mode = SIGNAL_MODE_FM }
};

/** 按固定 ID 返回只读题目配置；越界 ID 不返回默认值。 */
const signal_profile_t *signal_profile_get(signal_profile_id_t id)
{
    if ((u32)id >= (u32)SIGNAL_PROFILE_COUNT) {
        return 0;
    }
    return &g_signal_profiles[id];
}

/** 返回保持当前功能和参数的旧双分量分离 profile。 */
const signal_profile_t *signal_profile_default(void)
{
    /* 默认选择集中在此处；调用者得到的仍是 signal_profile_get() 返回的同一个
     * 只读表项。 */
    return signal_profile_get(PROFILE_OLD_SEPARATOR);
}

/** 返回已登记 profile 的数量，包含尚未配置参数的预留项。 */
u32 signal_profile_count(void)
{
    return (u32)SIGNAL_PROFILE_COUNT;
}

/** 验证当前已实现模式运行所需的题目参数是否完整。 */
int signal_profile_is_configured(const signal_profile_t *profile)
{
    return profile != 0 && profile->name != 0 &&
           profile->frequency_min_hz > 0.0f &&
           profile->frequency_max_hz > profile->frequency_min_hz &&
           profile->frequency_grid_hz > 0.0f &&
           profile->grid_lock_tolerance_hz > 0.0f &&
           profile->lock_max_residual > 0.0f &&
           profile->confirm_frames > 0U &&
           profile->lock_timeout_seconds > 0U &&
           profile->allowed_waveforms != 0U &&
           profile->dds_amplitude_code <= APP_DDS_UNITY_AMPLITUDE &&
           profile->phase_step_degrees > 0.0f &&
           profile->phase_max_degrees >= profile->phase_step_degrees;
}
