#ifndef RLC_FILTER_CLASSIFIER_H
#define RLC_FILTER_CLASSIFIER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RLC_FILTER_CLASS_UNKNOWN = 0,
    RLC_FILTER_CLASS_LOW_PASS,
    RLC_FILTER_CLASS_HIGH_PASS,
    RLC_FILTER_CLASS_BAND_PASS,
    RLC_FILTER_CLASS_BAND_STOP
} rlc_filter_class_t;

int rlc_filter_classify(const float *frequency_hz,
                               const float *magnitude,
                               size_t count,
                               rlc_filter_class_t *result);

#ifdef __cplusplus
}
#endif

#endif