// SPDX-License-Identifier: GPL-3.0-or-later
#include <complex.h>
#include <math.h>
#include <stdio.h>

#include <fftw3.h>
#include <raylib.h>

#define N 64

int main(void)
{
    // better than straight malloc as it aligns for potential SIMD
    // https://www.fftw.org/fftw3_doc/SIMD-alignment-and-fftw_005fmalloc.html
    float* in = fftwf_alloc_real(N);

    InitWindow(100, 100, "title");

    CloseWindow();
    fftwf_free(in);

    return 0;
}
