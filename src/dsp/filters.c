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
    const float b = 1.0f - a;

    float x_prev = f->x_prev;
    float y_prev = f->y_prev;

    for (SizeType i = 0; i < size; ++i) {
        const float x_n_cache = data[i];
        data[i] = b * data[i] + a * y_prev;
        x_prev = x_n_cache;
        y_prev = data[i];
    }

    f->x_prev = x_prev;
    f->y_prev = y_prev;
}

// y[n] = a * y[n-1] + a * (x[n] - x[n-1])
void filter_hpf_process(OnePoleFilter* restrict f,
                        float* restrict data,
                        SizeType size)
{
    const float a = f->alpha;

    float x_prev = f->x_prev;
    float y_prev = f->y_prev;

    for (SizeType i = 0; i < size; ++i) {
        const float x_n_cache = data[i];
        data[i] = a * (y_prev + data[i] - x_prev);
        x_prev = x_n_cache;
        y_prev = data[i];
    }

    f->x_prev = x_prev;
    f->y_prev = y_prev;
}
