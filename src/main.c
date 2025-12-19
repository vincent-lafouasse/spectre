// SPDX-License-Identifier: GPL-3.0-or-later
#include <complex.h>
#include <math.h>
#include <stdio.h>

#include <fftw3.h>

#define N 64

int main(void)
{
    // better than straight malloc as it aligns for potential SIMD
    // https://www.fftw.org/fftw3_doc/SIMD-alignment-and-fftw_005fmalloc.html
    float* in = fftwf_alloc_real(N);
    float complex* out = (float complex*)fftwf_alloc_complex(N / 2 + 1);

    fftwf_plan p = fftwf_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);

    // make a sine input
    const float omega0 = 2.0f * M_PI / N;
    for (int i = 0; i < N; i++) {
        in[i] = sinf(4.0f * omega0 * i);
    }

    // fft that shit
    fftwf_execute(p);

    // there should be a peak at bin 4
    printf("Bin | Magnitude\n----|----------\n");
    for (int i = 0; i < 8; i++) {
        const float real = crealf(out[i]);
        const float imag = cimagf(out[i]);
        float mag = sqrtf(real * real + imag * imag);
        printf("%3d | %8.2f %s\n", i, mag, (i == 4) ? "<-- Peak!" : "");
    }

    fftwf_destroy_plan(p);
    fftwf_free(in);
    fftwf_free(out);

    return 0;
}
