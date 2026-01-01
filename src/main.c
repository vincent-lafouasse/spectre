// SPDX-License-Identifier: GPL-3.0-or-later
#include <fftw3.h>
#include <raylib.h>

#include "definitions.h"
#include "window/window.h"

#define STRIDE_RATIO 2
#define STRIDE_SIZE (FFT_SIZE / STRIDE_RATIO)

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

int main(void)
{
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "title");
    InitAudioDevice();

    float* in = fftwf_alloc_real(FFT_SIZE);
    fftwf_complex* out = fftwf_alloc_complex(FFT_SIZE);
    fftwf_plan p = fftwf_plan_dft_r2c_1d(FFT_SIZE, in, out, FFTW_MEASURE);

    float window[FFT_SIZE];
    make_hann_window(window, FFT_SIZE);

    Sound sound = LoadSound("audio/Bbmaj9.wav");
    PlaySound(sound);

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        EndDrawing();
    }

    fftwf_destroy_plan(p);
    fftwf_free(in);
    fftwf_free(out);
    UnloadSound(sound);
    CloseAudioDevice();
    CloseWindow();
}
