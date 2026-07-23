#ifndef FILTER_CLASSIFIER_H
#define FILTER_CLASSIFIER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FILTER_CLASS_UNKNOWN = 0,
    FILTER_CLASS_LOW_PASS,
    FILTER_CLASS_HIGH_PASS,
    FILTER_CLASS_BAND_PASS,
    FILTER_CLASS_BAND_STOP
} filter_class_t;

int filter_classifier_classify(const float *frequency_hz,
                               const float *magnitude,
                               size_t count,
                               filter_class_t *result);

#ifdef __cplusplus
}
#endif

#endif