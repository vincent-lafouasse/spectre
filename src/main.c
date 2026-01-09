// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <stdlib.h>

#include <raylib.h>

#include "LockFreeQueue.h"
#include "audio_callback.h"

/*
#define STRIDE_RATIO 2
#define STRIDE_SIZE (FFT_SIZE / STRIDE_RATIO)
*/

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

int main(void)
{
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "title");
    InitAudioDevice();

    LockFreeQueue* sample_queue = malloc(sizeof(*sample_queue));
    if (sample_queue == NULL) {
        printf("oom\n");
        exit(1);
    }
    clfq_new(sample_queue);

    LockFreeQueueProducer sample_tx = clfq_producer(sample_queue);
    pass_sample_tx(&sample_tx);

    LockFreeQueueConsumer sample_rx = clfq_consumer(sample_queue);
    (void)sample_rx;

    Sound sound = LoadSound("audio/Bbmaj9.wav");
    PlaySound(sound);

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        EndDrawing();
    }

    free(sample_queue);
    UnloadSound(sound);
    CloseAudioDevice();
    CloseWindow();
}
