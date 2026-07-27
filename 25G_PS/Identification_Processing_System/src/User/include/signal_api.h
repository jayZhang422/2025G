/******************************************************************************
 * signal_api.h
 *
 * Single public entry for the existing PS capture, identification, IQ, DDS,
 * button, status, and recovery capabilities.
 ******************************************************************************/

#ifndef USER_INCLUDE_SIGNAL_API_H_
#define USER_INCLUDE_SIGNAL_API_H_

#include "../config/signal_profiles.h"
#include "button_input.h"
#include "dds_control.h"
#include "fifo_monitor.h"
#include "iq_demodulator.h"
#include "signal_errors.h"
#include "signal_processing.h"
#include "signal_types.h"

#include "arm_math.h"
#include "xaxidma.h"
#include "xil_types.h"

/** 应用层使用的逻辑按键名称，由 API 转换到具体 PS MIO 按键。 */
typedef enum {
    SIGNAL_BUTTON_START = 0,
    SIGNAL_BUTTON_RESET,
    SIGNAL_BUTTON_PHASE_INCREMENT,
    SIGNAL_BUTTON_PHASE_DECREMENT
} signal_button_t;

/** 一次原子 DDS 提交所需的 A/B 完整配置和控制位。 */
typedef struct {
    dds_channel_config_t channel_a; /**< PL DDS A 通道的完整 shadow 配置。 */
    dds_channel_config_t channel_b; /**< PL DDS B 通道的完整 shadow 配置。 */
    int phase_reload;               /**< 非零时在提交点重新装载两路绝对相位。 */
    int run;                        /**< 非零启动输出；零使 DAC 回到中点。 */
} signal_dds_pair_t;

/** signal_api_get_status() 复制给应用层的只读运行状态快照。 */
typedef struct {
    signal_error_t last_error; /**< 最近一次公共 API 调用记录的错误。 */
    u32 capture_count;         /**< 已完成 DMA 接收的累计次数。 */
    u32 dma_error_count;       /**< DMA 对齐或采集失败的累计次数。 */
    u32 bad_frame_count;       /**< 因长度不完整而拒绝的累计帧数。 */
    u32 alignment_count;       /**< 已完成 S2MM 帧对齐的累计次数。 */
    u32 last_dma_length_bytes; /**< 最近一次 DMA 接收的实际字节数。 */
    int iq_available;          /**< 非零表示 PL IQ 检测器初始化成功。 */
    int monitor_available;     /**< 非零表示 FIFO 监视器仍可读取。 */
    fifo_monitor_snapshot_t monitor_before; /**< 最近操作前的 FIFO 快照。 */
    fifo_monitor_snapshot_t monitor_after;  /**< 最近操作后的 FIFO 快照。 */
} signal_status_t;

/**
 * PS 统一能力层的长期上下文。
 * 由一个应用任务独占，在进入主循环前初始化一次；不要复制或跨任务并发使用。
 */
typedef struct {
    XAxiDma dma;                       /**< Xilinx AXI DMA 驱动实例。 */
    arm_rfft_fast_instance_f32 fft;    /**< 当前 4096 点识别器使用的 CMSIS FFT 配置。 */
    iq_demodulator_t iq;               /**< PL 单频 IQ 检测器访问上下文。 */
    dds_control_t dds;                 /**< PL 双通道 DDS 提交上下文。 */
    button_input_t buttons;            /**< PS MIO 按键驱动上下文。 */
    const signal_profile_t *profile;   /**< 借用的只读题目参数，不负责释放。 */
    signal_mode_t mode;                /**< profile->mode 的缓存；当前不自动分派。 */
    signal_error_t last_error;         /**< 最近一次 API 错误。 */
    u32 capture_count;                 /**< DMA 完成次数。 */
    u32 dma_error_count;               /**< DMA 对齐/采集失败次数。 */
    u32 bad_frame_count;               /**< 不完整帧累计数。 */
    u32 alignment_count;               /**< S2MM 对齐成功次数。 */
    u32 last_dma_length_bytes;         /**< 最近一次 DMA 实际接收长度。 */
    int initialized;                   /**< 非零后其他 signal_api_* 才允许调用。 */
    int iq_available;                  /**< IQ 初始化是否成功；失败不阻止基本采集。 */
    int monitor_available;             /**< FIFO monitor 当前是否可用。 */
    fifo_monitor_snapshot_t monitor_before; /**< 最近硬件操作前的监视快照。 */
    fifo_monitor_snapshot_t monitor_after;  /**< 最近硬件操作后的监视快照。 */
} signal_api_t;

/** 初始化统一 API 上下文；成功前不得调用其他 signal_api_* 能力。 */
signal_error_t signal_api_init(signal_api_t *api,
                               const signal_profile_t *profile);
/** 更换后续算法/DDS 使用的只读 profile，不重新初始化或复位硬件。 */
signal_error_t signal_api_set_profile(signal_api_t *api,
                                      const signal_profile_t *profile);
/** 正式采集前重新对齐 S2MM 到下一完整 AXIS 帧。 */
signal_error_t signal_align_capture(signal_api_t *api);
/** 接收一帧到共享 DMA 缓冲区，并填写只借用该缓冲区的 frame 视图。 */
signal_error_t signal_capture(signal_api_t *api, signal_frame_t *frame);
/** 调用当前旧双分量识别器；该入口尚未按 mode 分派其他算法。 */
signal_error_t signal_identify_components(
    signal_api_t *api, const signal_frame_t *frame,
    signal_analysis_result_t *result);
/** 要求 IQ 可用；对一个已知目标频率执行一次 PL 窗口测量。 */
signal_error_t signal_iq_measure(signal_api_t *api, float32_t frequency_hz,
                                 iq_measurement_t *measurement);

/** 将旧双分量识别结果转换为一对尚未写入 PL 的 DDS shadow 配置。 */
signal_error_t signal_dds_build_pair(
    signal_api_t *api, const signal_analysis_result_t *result,
    float32_t b_to_a_phase_degrees, signal_dds_pair_t *pair);
/** 原子提交 pair 中的 A/B shadow 配置和运行控制位。 */
signal_error_t signal_dds_apply(signal_api_t *api,
                                const signal_dds_pair_t *pair);
/** 原子关闭双 DDS 输出，使 DAC 回到中点。 */
signal_error_t signal_dds_stop(signal_api_t *api);
/** 保持频率和波形，仅对运行中的 B 通道提交相位增量。 */
signal_error_t signal_dds_adjust_b_phase(signal_api_t *api,
                                         const signal_dds_pair_t *pair,
                                         float32_t phase_delta_degrees);

/** 消费一个已消抖的按压事件；同一次事件只会返回一次非零。 */
int signal_api_take_button(signal_api_t *api, signal_button_t button);
/** 读取按键当前原始逻辑电平，不消费按压事件。 */
u32 signal_api_button_level(const signal_api_t *api, signal_button_t button);
/** 将 API 内部计数器和最近 monitor 快照复制给调用者。 */
signal_error_t signal_api_get_status(signal_api_t *api,
                                     signal_status_t *status);
/** 当前恢复动作等价于重新执行一次 DMA 帧对齐。 */
signal_error_t signal_recover(signal_api_t *api);
/** 查询最近一次记录的 API 错误；不访问硬件。 */
signal_error_t signal_api_last_error(const signal_api_t *api);
void signal_api_print_status(signal_api_t *api, const char *tag);
void signal_api_report_dds_snapshot(const signal_api_t *api, const char *tag);

#endif /* USER_INCLUDE_SIGNAL_API_H_ */
