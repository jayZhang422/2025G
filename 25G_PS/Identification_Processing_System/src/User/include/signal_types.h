/******************************************************************************
 * signal_types.h
 *
 * Shared application-facing signal descriptors. No storage is allocated here.
 ******************************************************************************/

#ifndef USER_INCLUDE_SIGNAL_TYPES_H_
#define USER_INCLUDE_SIGNAL_TYPES_H_

#include "arm_math.h"
#include "xil_types.h"

/** 当前双分量识别器和 PL DDS 共同支持的波形编码。 */
typedef enum {
    SIGNAL_WAVE_SINE = 0,
    SIGNAL_WAVE_TRIANGLE = 1
} signal_waveform_t;

/** 已识别出的一个稳态信号分量；由算法填写，应用和 DDS 转换层读取。 */
typedef struct {
    float32_t frequency_hz;          /**< 基波频率，单位 Hz。 */
    float32_t fundamental_amplitude; /**< 基波幅度，单位为去直流后的 ADC 码。 */
    float32_t measured_phase_rad;    /**< 相对当前采样帧起点的估计相位，单位弧度。 */
    signal_waveform_t waveform;      /**< 当前判定的正弦波或三角波类型。 */
} signal_component_t;

/** 当前双分量识别算法的一帧输出；不适合作为所有新解调算法的通用结果。 */
typedef struct {
    signal_component_t channel_a; /**< 第一分量；当前锁定流程最终要求其频率较低。 */
    signal_component_t channel_b; /**< 第二分量；当前锁定流程最终要求其频率较高。 */
    float32_t normalized_residual; /**< 重建误差能量比的平方根，越小越可信。 */
} signal_analysis_result_t;

/* signal_frame_t::quality_flags 的逐位诊断标志，可同时置多个标志。 */
#define SIGNAL_FRAME_QUALITY_SHORT          0x00000001U
#define SIGNAL_FRAME_QUALITY_FIFO_BLOCKED   0x00000002U
#define SIGNAL_FRAME_QUALITY_RESET_BLOCKED  0x00000004U

/**
 * signal_capture() 生成的一帧 ADC 数据视图。
 * samples 不拥有内存，指向共享 DMA 缓冲区；下一次采集会覆盖其内容。
 */
typedef struct {
    const u16 *samples;       /**< 只读 ADC 原始码首地址，不得释放或长期保存。 */
    u32 sample_count;         /**< 本帧实际收到的 u16 样本数。 */
    float32_t sample_rate_hz; /**< 采集本帧所用采样率，单位 Hz。 */
    u32 frame_sequence;       /**< 从 1 递增的成功 DMA 采集序号。 */
    u32 quality_flags;        /**< SIGNAL_FRAME_QUALITY_* 的按位组合。 */
    int frame_valid;          /**< 非零表示长度满足当前算法的基本处理条件。 */
} signal_frame_t;

#endif /* USER_INCLUDE_SIGNAL_TYPES_H_ */
