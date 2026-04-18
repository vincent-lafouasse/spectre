#include "window.h"

#include <assert.h>
#include <math.h>

#include "definitions.h"

void make_hann_window(float* window, SizeType N)
{
    assert(N > 0);

    for (SizeType i = 0; i < N; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * PI * i / (N - 1)));
    }
}

void apply_window(float* data, const float* window, SizeType N)
{
    for (SizeType i = 0; i < N; i++) {
        data[i] *= window[i];
    }
}
