#include "filters.h"

#include <math.h>

// a = exp(-2 * PI * f_c / f_s)
OnePoleFilter filter_init(float cutoff_frequency, float sample_rate)
{
    const float alpha = expf(-2.0f * M_PI * cutoff_frequency / sample_rate);

    return (OnePoleFilter){
        .alpha = alpha,
        .x_prev = 0,
        .y_prev = 0,
    };
}

// y[n] = (1 - a) * x[n] + a * y[n-1]
void filter_lpf_process(OnePoleFilter* restrict f,
                        float* restrict data,
                        SizeType size)
{
    const float a = f->alpha;

    for (SizeType i = 0; i < size; ++i) {
        const float x_n_cache = data[i];
        data[i] = (1.0f - a) * data[i] + a * f->y_prev;
        f->x_prev = x_n_cache;
        f->y_prev = data[i];
    }
}

// y[n] = a * y[n-1] + a * (x[n] - x[n-1])
void filter_hpf_process(OnePoleFilter* restrict f,
                        float* restrict data,
                        SizeType size)
{
    const float a = f->alpha;

    for (SizeType i = 0; i < size; ++i) {
        const float x_n_cache = data[i];
        data[i] = a * f->y_prev + a * (data[i] - f->x_prev);
        f->x_prev = x_n_cache;
        f->y_prev = data[i];
    }
}
