// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdatomic.h>
#include <string.h>

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

size_t g_buffer_available(void)
{
    size_t w = atomic_load_explicit(&g_write_index, memory_order_acquire);
    size_t r = atomic_load_explicit(&g_read_index, memory_order_acquire);

    if (w >= r) {
        return w - r;
    } else {
        return (BUFFER_SIZE - r) + w;
    }
}

// consumer (fft thread)
// returns false if not enough data is available
bool g_buffer_pop(float* dest, size_t count)
{
    if (g_buffer_available() < count)
        return false;

    size_t r = atomic_load_explicit(&g_read_index, memory_order_relaxed);

    for (size_t i = 0; i < count; i++) {
        dest[i] = g_ring_buffer[r];
        r = (r + 1) & (BUFFER_SIZE - 1);
    }

    atomic_store_explicit(&g_read_index, r, memory_order_release);
    return true;
}

// producer (audio thread)
void g_buffer_push_block(const float* samples, size_t count)
{
    const size_t w = atomic_load_explicit(&g_write_index, memory_order_relaxed);
    const size_t available = g_buffer_available();

    if (count > available) {
        count = available;
    }

    // check if 1 or 2 memcpy are needed
    size_t first_part = BUFFER_SIZE - w;
    if (count <= first_part) {
        memcpy(&g_ring_buffer[w], samples, count * sizeof(float));
    } else {
        memcpy(&g_ring_buffer[w], samples, first_part * sizeof(float));
        memcpy(&g_ring_buffer[0], samples + first_part,
               (count - first_part) * sizeof(float));
    }

    atomic_store_explicit(&g_write_index, (w + count) & (BUFFER_SIZE - 1),
                          memory_order_release);
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
