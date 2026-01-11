#pragma once

#include "definitions.h"

typedef struct {
    float alpha;
    float x_prev;  // x[n - 1]
    float y_prev;  // y[n - 1]
} OnePoleFilter;

// a = exp(-2 * PI * f_c / f_s)
OnePoleFilter filter_init(float cutoff_frequency, float sample_rate);

// y[n] = (1 - a) * x[n] + a * y[n-1]
void filter_lpf_process(OnePoleFilter* f, float* data, SizeType size);

// y[n] = a * y[n-1] + a * (x[n] - x[n-1])
void filter_hpf_process(OnePoleFilter* f, float* data, SizeType size);
