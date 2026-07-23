#include "calibration.h"

#include <math.h>

static int valid_curve(const calibration_curve_t *curve)
{
    size_t i;

    if (curve == NULL || curve->count < 2U ||
        curve->count > CALIBRATION_MAX_POINTS) {
        return 0;
    }

    for (i = 0U; i < curve->count; ++i) {
        if (!isfinite(curve->points[i].vpp) || curve->points[i].vpp < 0.0f) {
            return 0;
        }
        if (i > 0U &&
            (curve->points[i].amplitude_code <= curve->points[i - 1U].amplitude_code ||
             curve->points[i].vpp < curve->points[i - 1U].vpp)) {
            return 0;
        }
    }
    return 1;
}

int calibration_curve_validate(const calibration_curve_t *curve)
{
    return valid_curve(curve) ? 0 : -1;
}

int calibration_vpp_from_code(const calibration_curve_t *curve,
                              uint16_t amplitude_code,
                              float *vpp)
{
    size_t i;

    if (!valid_curve(curve) || vpp == NULL) {
        return -1;
    }
    if (amplitude_code < curve->points[0].amplitude_code ||
        amplitude_code > curve->points[curve->count - 1U].amplitude_code) {
        return -2;
    }
    for (i = 1U; i < curve->count; ++i) {
        const calibration_point_t *lo = &curve->points[i - 1U];
        const calibration_point_t *hi = &curve->points[i];
        if (amplitude_code <= hi->amplitude_code) {
            const float t = (float)(amplitude_code - lo->amplitude_code) /
                            (float)(hi->amplitude_code - lo->amplitude_code);
            *vpp = lo->vpp + t * (hi->vpp - lo->vpp);
            return 0;
        }
    }
    return -2;
}

int calibration_code_for_vpp(const calibration_curve_t *curve,
                             float target_vpp,
                             uint16_t *amplitude_code)
{
    size_t i;

    if (!valid_curve(curve) || amplitude_code == NULL ||
        !isfinite(target_vpp)) {
        return -1;
    }
    if (target_vpp < curve->points[0].vpp ||
        target_vpp > curve->points[curve->count - 1U].vpp) {
        return -2;
    }
    for (i = 1U; i < curve->count; ++i) {
        const calibration_point_t *lo = &curve->points[i - 1U];
        const calibration_point_t *hi = &curve->points[i];
        if (target_vpp <= hi->vpp) {
            const float span = hi->vpp - lo->vpp;
            const float t = (span > 0.0f) ? (target_vpp - lo->vpp) / span : 0.0f;
            const float code = (float)lo->amplitude_code +
                               t * (float)(hi->amplitude_code - lo->amplitude_code);
            *amplitude_code = (uint16_t)lroundf(code);
            return 0;
        }
    }
    return -2;
}