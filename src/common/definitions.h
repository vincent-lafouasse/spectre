#pragma once

#include <complex.h>

// check that a suitable size type is always lock free and defines it
#include "platform_check.h"

// common definitions that need to be sync'd
// e.g. global ring buffer size depends on FFT size
// but the ring buffer should NOT include the FFT module

#define FFT_SIZE 2048

#define CLF_QUEUE_SIZE (8 * FFT_SIZE)

#define HISTORY_SIZE 1024

// one or two slices
typedef struct {
    const float* slice1;
    SizeType size1;
    const float* slice2;
    SizeType size2;
} SplitSlice;

typedef _Complex float Complex;
