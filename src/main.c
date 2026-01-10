// SPDX-License-Identifier: GPL-3.0-or-later
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <raylib.h>

#include "LockFreeQueue.h"
#include "audio_callback.h"

#define BUFFER_SIZE 256
#define STRIDE_SIZE (BUFFER_SIZE / 2)

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define ALERT_FRACTION 10
// if ALERT_FRACTION is 10, alert at 10% and 90% fullness
#define UNDERFULL_ALERT (CLF_QUEUE_SIZE / ALERT_FRACTION)
#define ALMOSTFULL_ALERT \
    ((ALERT_FRACTION - 1) * CLF_QUEUE_SIZE / ALERT_FRACTION)

float rms(const float* data, SizeType size)
{
    float sum = 0;

    for (SizeType i = 0; i < size; i++) {
        sum += data[i] * data[i];
    }

    return sqrtf(sum);
}

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
    init_audio_processor(&sample_tx);

    LockFreeQueueConsumer sample_rx = clfq_consumer(sample_queue);
    (void)sample_rx;

    float buffer[BUFFER_SIZE];
    SizeType buffer_idx = 0;

    Sound sound = LoadSound("audio/Bbmaj9.wav");
    PlaySound(sound);

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        EndDrawing();
    }

    free(sample_queue);
    deinit_audio_processor();
    UnloadSound(sound);
    CloseAudioDevice();
    CloseWindow();
}
