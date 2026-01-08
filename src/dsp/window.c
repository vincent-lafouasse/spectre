#include "window.h"

#include <math.h>

void make_hann_window(float* window, int N)
{
    for (int i = 0; i < N; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (N - 1)));
    }
}

void apply_window(float* data, const float* window, int N)
{
    for (int i = 0; i < N; i++) {
        data[i] *= window[i];
    }
}
