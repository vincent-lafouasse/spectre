// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdatomic.h>

#include <fftw3.h>
#include <raylib.h>

#include "window/window.h"

#define FFT_SIZE 1024

#define STRIDE_RATIO 2
#define STRIDE_SIZE (FFT_SIZE / STRIDE_RATIO)

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define BUFFER_SIZE (8 * FFT_SIZE)

float g_ring_buffer[BUFFER_SIZE];
_Atomic size_t g_write_index = 0;
_Atomic size_t g_read_index = 0;

// producer (audio thread)
void g_buffer_enqueue(float f)
{
    const size_t write_idx =
        atomic_load_explicit(&g_write_index, memory_order_relaxed);
    g_ring_buffer[write_idx] = f;

    const size_t next_write_idx = (write_idx + 1) & (BUFFER_SIZE - 1);
    atomic_store_explicit(&g_write_index, next_write_idx, memory_order_release);
}

// consumer (fft thread)
float g_buffer_dequeue(void)
{
    const size_t read_idx =
        atomic_load_explicit(&g_read_index, memory_order_relaxed);
    const float f = g_ring_buffer[read_idx];

    const size_t next_read_idx = (read_idx + 1) % BUFFER_SIZE;
    atomic_store_explicit(&g_read_index, next_read_idx, memory_order_release);
    return f;
}

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
