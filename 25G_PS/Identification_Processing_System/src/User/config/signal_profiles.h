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

/** 应用/算法模式标签；当前仅保存于 signal_api_t，尚未自动分派算法。 */
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

/** 静态 profile 表的索引；SIGNAL_PROFILE_COUNT 只用于表示表项数量。 */
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

/**
 * 一道题或一种工作模式的只读参数集合。
 * profile 只描述“参数是多少”，不保存运行状态，也不实现算法。
 */
typedef struct {
    const char *name;                 /**< 诊断输出使用的静态名称。 */
    signal_mode_t mode;               /**< 该配置期望使用的算法/应用模式。 */
    float32_t frequency_min_hz;       /**< 允许搜索的最低频率，单位 Hz。 */
    float32_t frequency_max_hz;       /**< 允许搜索的最高频率，单位 Hz。 */
    float32_t frequency_grid_hz;      /**< 离散候选频率间隔，单位 Hz。 */
    float32_t expected_carrier_hz;    /**< 调制题预期载波，单位 Hz；当前预留。 */
    float32_t expected_deviation_hz;  /**< FM/FSK 预期频偏，单位 Hz；当前预留。 */
    float32_t expected_symbol_rate;   /**< 数字调制预期符号率，单位 Baud；当前预留。 */
    float32_t grid_lock_tolerance_hz; /**< 实测频率到栅格点的最大偏差，单位 Hz。 */
    float32_t lock_max_residual;      /**< 锁定允许的最大归一化模型残差。 */
    u32 confirm_frames;               /**< 判定锁定前要求连续一致的帧数。 */
    u32 lock_timeout_seconds;         /**< 单轮锁定允许的最长时间，单位秒。 */
    u32 allowed_waveforms;            /**< SIGNAL_WAVEFORM_MASK_* 位掩码。 */
    u16 dds_amplitude_code;           /**< 提交给 PL DDS 的无符号幅度码。 */
    float32_t initial_phase_degrees;  /**< B 相对 A 的初始相位，单位度。 */
    float32_t phase_step_degrees;     /**< 每次按键调整的相位步长，单位度。 */
    float32_t phase_max_degrees;      /**< 用户相位设置的环绕上限，单位度。 */
} signal_profile_t;

const signal_profile_t *signal_profile_get(signal_profile_id_t id);
const signal_profile_t *signal_profile_default(void);
u32 signal_profile_count(void);
int signal_profile_is_configured(const signal_profile_t *profile);

#endif /* USER_CONFIG_SIGNAL_PROFILES_H_ */
