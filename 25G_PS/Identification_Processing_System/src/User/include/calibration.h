#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CALIBRATION_MAX_POINTS 16U

typedef struct {
    uint16_t amplitude_code;
    float vpp;
} calibration_point_t;

typedef struct {
    calibration_point_t points[CALIBRATION_MAX_POINTS];
    size_t count;
} calibration_curve_t;

int calibration_curve_validate(const calibration_curve_t *curve);
int calibration_vpp_from_code(const calibration_curve_t *curve,
                              uint16_t amplitude_code,
                              float *vpp);
int calibration_code_for_vpp(const calibration_curve_t *curve,
                             float target_vpp,
                             uint16_t *amplitude_code);

#ifdef __cplusplus
}
#endif

#endif