#ifndef DAC_VPP_CALIBRATION_H
#define DAC_VPP_CALIBRATION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DAC_VPP_CALIBRATION_MAX_POINTS 16U

typedef struct {
    uint16_t amplitude_code;
    float vpp;
} dac_vpp_calibration_point_t;

typedef struct {
    dac_vpp_calibration_point_t points[DAC_VPP_CALIBRATION_MAX_POINTS];
    size_t count;
} dac_vpp_calibration_curve_t;

int dac_vpp_calibration_curve_validate(const dac_vpp_calibration_curve_t *curve);
int dac_vpp_calibration_vpp_from_code(const dac_vpp_calibration_curve_t *curve,
                              uint16_t amplitude_code,
                              float *vpp);
int dac_vpp_calibration_code_for_vpp(const dac_vpp_calibration_curve_t *curve,
                             float target_vpp,
                             uint16_t *amplitude_code);

#ifdef __cplusplus
}
#endif

#endif