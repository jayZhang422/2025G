/******************************************************************************
 * signal_profiles.c
 *
 * PROFILE_OLD_SEPARATOR preserves the active contest behavior. Other entries
 * intentionally contain no guessed signal parameters.
 ******************************************************************************/

#include "algorithm_config.h"
#include "signal_profiles.h"

static const signal_profile_t g_signal_profiles[SIGNAL_PROFILE_COUNT] = {
    {
        "old_separator",
        SIGNAL_MODE_SEPARATOR,
        20000.0f,
        100000.0f,
        5000.0f,
        0.0f,
        0.0f,
        0.0f,
        APP_DEFAULT_CONFIRM_FRAMES
    },
    { "general_measure", SIGNAL_MODE_MEASURE, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0U },
    { "weak_signal", SIGNAL_MODE_LOCKIN, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0U },
    { "am", SIGNAL_MODE_AM, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0U },
    { "ask", SIGNAL_MODE_ASK, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0U },
    { "2fsk", SIGNAL_MODE_FSK, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0U },
    { "fm", SIGNAL_MODE_FM, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0U }
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
    return signal_profile_get(PROFILE_OLD_SEPARATOR);
}

/** 返回已登记 profile 的数量，包含尚未配置参数的预留项。 */
u32 signal_profile_count(void)
{
    return (u32)SIGNAL_PROFILE_COUNT;
}
