// SPDX-License-Identifier: GPL-3.0-or-later
#include <complex.h>

#include <raylib.h>

#define N 64

int main(void)
{
    InitWindow(100, 100, "title");
    InitAudioDevice();

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
