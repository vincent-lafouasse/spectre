// SPDX-License-Identifier: GPL-3.0-or-later
#include <complex.h>

#include <raylib.h>

#include "window/window.h"

#define FFT_SIZE 1024

#define STRIDE_RATIO 2
#define STRIDE_SIZE (FFT_SIZE / STRIDE_RATIO)

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

int main(void)
{
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "title");
    InitAudioDevice();

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

    UnloadSound(sound);
    CloseAudioDevice();
    CloseWindow();
}
