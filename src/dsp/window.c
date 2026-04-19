#include "window.h"

#include <assert.h>
#include <math.h>

void window_make_hann(float* window, SizeType N)
{
    assert(N > 0);

    for (SizeType i = 0; i < N; i++) {
        const float fraction = (float)i / (float)(N - 1);
        window[i] = 0.5f * (1.0f - cosf(2.0f * PI * fraction));
    }
}

void window_apply(float* data, const float* window, SizeType N)
{
    for (SizeType i = 0; i < N; i++) {
        data[i] *= window[i];
    }
}
