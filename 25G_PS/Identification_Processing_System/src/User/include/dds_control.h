/******************************************************************************
 * dds_control.h
 *
 * PS writer for the ten-word DDS control snapshot consumed by ad9767.sv.
 ******************************************************************************/

#ifndef USER_INCLUDE_DDS_CONTROL_H_
#define USER_INCLUDE_DDS_CONTROL_H_

#include "signal_processing.h"
#include "xil_types.h"

/** PL DDS BRAM 控制端的长期写入上下文。 */
typedef struct {
    UINTPTR base_address;      /**< DDS 控制 BRAM 的 PS 映射基址。 */
    u32 next_commit_sequence;  /**< 下次原子提交序号；零保留不用。 */
} dds_control_t;

/** 单个 PL DDS 通道的一次完整 shadow 配置。 */
typedef struct {
    signal_waveform_t waveform; /**< PL 支持的波形编码。 */
    u32 phase_step;             /**< 32 位频率调谐字。 */
    u32 phase_word;             /**< 32 位绝对相位字或相位增量字。 */
    u16 amplitude_code;         /**< 输出幅度码，上限由配置限定。 */
} dds_channel_config_t;

void dds_control_init(dds_control_t *control);
void dds_control_from_component(const signal_component_t *component,
                                float32_t initial_phase_degrees,
                                u16 amplitude_code,
                                dds_channel_config_t *config);
int dds_control_commit(dds_control_t *control,
                       const dds_channel_config_t *channel_a,
                       const dds_channel_config_t *channel_b,
                       int phase_reload,
                       int run);
int dds_control_adjust_b_phase(dds_control_t *control,
                               const dds_channel_config_t *channel_a,
                               const dds_channel_config_t *channel_b,
                               float32_t phase_delta_degrees);

#endif /* USER_INCLUDE_DDS_CONTROL_H_ */
