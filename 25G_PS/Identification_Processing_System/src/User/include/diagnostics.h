/******************************************************************************
 * diagnostics.h
 *
 * UART-only observations that separate button, DMA/ADC, algorithm, and DDS
 * failures without changing the normal data path.
 ******************************************************************************/

#ifndef USER_INCLUDE_DIAGNOSTICS_H_
#define USER_INCLUDE_DIAGNOSTICS_H_

#include "dds_control.h"
#include "signal_processing.h"
#include "xil_types.h"

int diagnostics_should_report(u32 attempt);
void diagnostics_report_adc_frame(const u16 *raw_samples, u32 attempt);
void diagnostics_report_analysis(u32 attempt, int analysis_status,
                                 int lock_status,
                                 const signal_analysis_result_t *result);
void diagnostics_report_dds_snapshot(const char *tag,
                                     const dds_control_t *control);

#endif /* USER_INCLUDE_DIAGNOSTICS_H_ */
