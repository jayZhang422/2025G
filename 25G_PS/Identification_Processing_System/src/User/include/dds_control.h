/******************************************************************************
 * dds_control.h
 *
 * PS writer for the ten-word DDS control snapshot consumed by ad9767.sv.
 ******************************************************************************/

#ifndef USER_INCLUDE_DDS_CONTROL_H_
#define USER_INCLUDE_DDS_CONTROL_H_

#include "signal_processing.h"
#include "xil_types.h"

typedef struct {
    UINTPTR base_address;
    u32 next_commit_sequence;
} dds_control_t;

typedef struct {
    signal_waveform_t waveform;
    u32 phase_step;
    u32 phase_word;
    u16 amplitude_code;
} dds_channel_config_t;

void dds_control_init(dds_control_t *control);
void dds_control_from_component(const signal_component_t *component,
                                float32_t initial_phase_degrees,
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
