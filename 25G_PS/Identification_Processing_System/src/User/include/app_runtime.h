#ifndef USER_INCLUDE_APP_RUNTIME_H_
#define USER_INCLUDE_APP_RUNTIME_H_

#include "arm_math.h"
#include "button_input.h"
#include "dds_control.h"
#include "dma_utils.h"
#include "../algorithms/two_channel_signal_analyzer.h"
#include "xaxidma.h"
#include "xil_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    XAxiDma dma;
    dds_control_t dds;
    button_input_t buttons;
    arm_rfft_fast_instance_f32 fft;
    int initialized;
} app_runtime_t;

int app_runtime_init(app_runtime_t *runtime);
int app_runtime_run_algorithm_self_tests(void);
int app_runtime_capture_and_analyze(app_runtime_t *runtime,
                                    signal_analysis_result_t *result);

#ifdef __cplusplus
}
#endif

#endif