/******************************************************************************
 * dds_control.c
 *
 * Writes one complete shadow configuration, then changes COMMIT_SEQ last.
 ******************************************************************************/

#include "../config/hardware_config.h"
#include "../include/dds_control.h"

#include "xil_io.h"
/** 按 125 MHz DDS 时钟将目标频率换算为 32 位相位步进，并执行饱和。 */
static u32 dds_phase_step_from_frequency(float32_t frequency_hz)
{
    double step = ((double)frequency_hz * 4294967296.0) /
                  (double)APP_DDS_CLOCK_HZ;

    if (step < 0.0) {
        return 0U;
    }
    if (step > 4294967295.0) {
        return 0xFFFFFFFFU;
    }
    return (u32)(step + 0.5);
}

/** 将任意角度归一化到一个周期，并换算为无符号 32 位绝对相位字。 */
static u32 dds_phase_word_from_degrees(float32_t phase_degrees)
{
    double phase_word;

    while (phase_degrees < 0.0f) {
        phase_degrees += 360.0f;
    }
    while (phase_degrees >= 360.0f) {
        phase_degrees -= 360.0f;
    }

    phase_word = ((double)phase_degrees * 4294967296.0) / 360.0;
    return (u32)(phase_word + 0.5);
}

/** 将带符号角度增量编码为 DDS B 通道相位微调使用的补码相位字。 */
static u32 dds_phase_delta_from_degrees(float32_t phase_delta_degrees)
{
    double phase_word = ((double)phase_delta_degrees * 4294967296.0) / 360.0;

    return (u32)(s32)((phase_word >= 0.0) ?
                      (phase_word + 0.5) : (phase_word - 0.5));
}

/** 初始化 DDS BRAM 控制对象，并设置首个有效原子提交序号。 */
void dds_control_init(dds_control_t *control)
{
    control->base_address = (UINTPTR)APP_DDS_BRAM_BASEADDR;
    control->next_commit_sequence = 1U;
}

/** 将已识别的信号分量转换为 PL DDS 所需的波形、步进、相位和幅度配置。 */
void dds_control_from_component(const signal_component_t *component,
                                float32_t initial_phase_degrees,
                                dds_channel_config_t *config)
{
    config->waveform = component->waveform;
    config->phase_step = dds_phase_step_from_frequency(component->frequency_hz);
    config->phase_word = dds_phase_word_from_degrees(initial_phase_degrees);
    config->amplitude_code = APP_DDS_UNITY_AMPLITUDE;
}

/**
 * 写入完整 A/B DDS shadow 配置，最后写 COMMIT_SEQ 触发 PL 原子应用。
 * phase_reload 控制绝对初相装载，run 为零时请求 DAC 回到中点。
 */
int dds_control_commit(dds_control_t *control,
                       const dds_channel_config_t *channel_a,
                       const dds_channel_config_t *channel_b,
                       int phase_reload,
                       int run)
{
    UINTPTR base;
    u32 control_word = 0U;

    if (control == 0 || channel_a == 0 || channel_b == 0 ||
        channel_a->waveform > SIGNAL_WAVE_TRIANGLE ||
        channel_b->waveform > SIGNAL_WAVE_TRIANGLE) {
        return XST_FAILURE;
    }

    base = control->base_address;
    Xil_Out32(base + APP_DDS_A_WAVE_OFFSET, (u32)channel_a->waveform);
    Xil_Out32(base + APP_DDS_A_STEP_OFFSET, channel_a->phase_step);
    Xil_Out32(base + APP_DDS_A_PHASE_OFFSET, channel_a->phase_word);
    Xil_Out32(base + APP_DDS_A_AMPLITUDE_OFFSET,
              (u32)channel_a->amplitude_code);
    Xil_Out32(base + APP_DDS_B_WAVE_OFFSET, (u32)channel_b->waveform);
    Xil_Out32(base + APP_DDS_B_STEP_OFFSET, channel_b->phase_step);
    Xil_Out32(base + APP_DDS_B_PHASE_OFFSET, channel_b->phase_word);
    Xil_Out32(base + APP_DDS_B_AMPLITUDE_OFFSET,
              (u32)channel_b->amplitude_code);

    if (run) {
        control_word |= APP_DDS_CONTROL_RUN;
    }
    if (phase_reload) {
        control_word |= APP_DDS_CONTROL_PHASE_LOAD;
    }
    Xil_Out32(base + APP_DDS_CONTROL_OFFSET, control_word);

    /* The PL regards only this final write as an atomic apply request. */
    Xil_Out32(base + APP_DDS_COMMIT_OFFSET, control->next_commit_sequence);
    control->next_commit_sequence++;
    if (control->next_commit_sequence == 0U) {
        control->next_commit_sequence = 1U;
    }

    return XST_SUCCESS;
}

/** 在保持 A/B 当前频率配置的前提下，以原子提交方式调整 B 相位增量。 */
int dds_control_adjust_b_phase(dds_control_t *control,
                               const dds_channel_config_t *channel_a,
                               const dds_channel_config_t *channel_b,
                               float32_t phase_delta_degrees)
{
    UINTPTR base;

    if (control == 0 || channel_a == 0 || channel_b == 0 ||
        channel_a->waveform > SIGNAL_WAVE_TRIANGLE ||
        channel_b->waveform > SIGNAL_WAVE_TRIANGLE) {
        return XST_FAILURE;
    }

    base = control->base_address;
    Xil_Out32(base + APP_DDS_A_WAVE_OFFSET, (u32)channel_a->waveform);
    Xil_Out32(base + APP_DDS_A_STEP_OFFSET, channel_a->phase_step);
    Xil_Out32(base + APP_DDS_A_PHASE_OFFSET, channel_a->phase_word);
    Xil_Out32(base + APP_DDS_A_AMPLITUDE_OFFSET,
              (u32)channel_a->amplitude_code);
    Xil_Out32(base + APP_DDS_B_WAVE_OFFSET, (u32)channel_b->waveform);
    Xil_Out32(base + APP_DDS_B_STEP_OFFSET, channel_b->phase_step);
    Xil_Out32(base + APP_DDS_B_PHASE_OFFSET,
              dds_phase_delta_from_degrees(phase_delta_degrees));
    Xil_Out32(base + APP_DDS_B_AMPLITUDE_OFFSET,
              (u32)channel_b->amplitude_code);
    Xil_Out32(base + APP_DDS_CONTROL_OFFSET,
              APP_DDS_CONTROL_RUN | APP_DDS_CONTROL_B_PHASE_ADJUST);

    /* The phase delta becomes visible only on this final atomic commit. */
    Xil_Out32(base + APP_DDS_COMMIT_OFFSET, control->next_commit_sequence);
    control->next_commit_sequence++;
    if (control->next_commit_sequence == 0U) {
        control->next_commit_sequence = 1U;
    }

    return XST_SUCCESS;
}
