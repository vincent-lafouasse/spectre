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

static float sum_array(const float* arr, SizeType N)
{
    float acc = 0;

    for (SizeType i = 0; i < N; ++i) {
        acc += arr[i];
    }

    return acc;
}

// CG is the coherent gain of the window, i.e. its arithmetic average
//
// that means that in the time domain, the window scales down amplitude by CG
// (ie it divides by CG)
//
// DFT of a unit amplitude pure tone yields a frequency-domain amplitude of N
// (it is a sum after all) (a dot product even) so the amplitude of the FFT
// peaks of a windowed unit tone is N * CG (ie sum(window)) and its power is (N
// * CG)^2, (i.e. sum(window)^2)
//
// then to recover unit power we need to divide by (N CG)^2
// ie the reference power is:
float window_power_reference(const float* window, SizeType N)
{
    const float sum = sum_array(window, N);

    return 1.0f / (sum * sum);
}
