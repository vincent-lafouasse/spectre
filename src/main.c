// SPDX-License-Identifier: GPL-3.0-or-later
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <raylib.h>
#include <string.h>

#include "RMSVisualizer.h"
#include "audio_callback.h"
#include "core/History.h"
#include "core/LockFreeQueue.h"

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define PIXEL_PER_BAND 4

#define HISTORY_SIZE 1024

#define ALERT_FRACTION 16
// if ALERT_FRACTION is 10, alert at 10% and 90% fullness
#define UNDERFULL_ALERT (CLF_QUEUE_SIZE / ALERT_FRACTION)
#define ALMOSTFULL_ALERT \
    ((ALERT_FRACTION - 1) * CLF_QUEUE_SIZE / ALERT_FRACTION)

#define RMS_SIZE 1024
#define RMS_STRIDE (RMS_SIZE / 2)

float rms(const float* restrict data, SizeType size)
{
    float sum = 0;

    for (SizeType i = 0; i < size; i++) {
        sum += data[i] * data[i];
    }

    return sqrtf(sum / (float)size);
}

typedef struct {
    float buffer[RMS_SIZE];
    SizeType size;
    SizeType stride;
    LockFreeQueueConsumer rx;
    FloatHistory history;
} RMSAnalyzer;

RMSAnalyzer rms_analyzer_new(LockFreeQueueConsumer sample_rx)
{
    return (RMSAnalyzer){
        .buffer = {0},
        .size = RMS_SIZE,
        .stride = RMS_STRIDE,
        .rx = sample_rx,
        .history = fhistory_new(HISTORY_SIZE),
    };
}

void rms_analyzer_destroy(RMSAnalyzer* analyzer)
{
    if (!analyzer) {
        return;
    }

    fhistory_destroy(analyzer->history);
}

// returns number of elements pushed onto its history
SizeType rms_analyzer_update(RMSAnalyzer* analyzer)
{
    SizeType n = 0;

    while (clfq_pop(&analyzer->rx, analyzer->buffer + analyzer->stride,
                    analyzer->stride)) {
        const float rms_value = rms(analyzer->buffer, analyzer->size);
        fhistory_push(&analyzer->history, rms_value);
        memmove(analyzer->buffer, analyzer->buffer + analyzer->stride,
                analyzer->stride);
        ++n;
    }

    return n;
}

int main(int ac, const char** av)
{
    if (ac != 2) {
        printf("Usage: spectre [audio_file]\n");
        exit(1);
    }
    const char* music_path = av[1];

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
    AttachAudioMixedProcessor(pull_samples_from_audio_thread);

    LockFreeQueueConsumer sample_rx = clfq_consumer(sample_queue);
    (void)sample_rx;

    // analyzer
    FloatHistory rms_history = fhistory_new(HISTORY_SIZE);
    float rms_buffer[RMS_SIZE] = {0};

    // visualizer
    RMSVisualizer visualizer = rms_vis_new(
        HISTORY_SIZE, WINDOW_WIDTH, WINDOW_HEIGHT, (Vector2){0.0f, 0.0f});

    Music music = LoadMusicStream(music_path);
    if (!IsMusicValid(music)) {
        printf("Failed to open %s\n", music_path);
        exit(1);
    }
    PlayMusicStream(music);

    SetTargetFPS(60);

    SizeType frame_counter = 0;
    while (!WindowShouldClose()) {
        UpdateMusicStream(music);

        const SizeType available = clfq_consumer_size_eager(&sample_rx);
        if (available < UNDERFULL_ALERT) {
            printf("frame %u buffer underfull: %u\n", frame_counter, available);
        } else if (available > ALMOSTFULL_ALERT) {
            printf("frame %u buffer almost full: %u\n", frame_counter,
                   available);
        }

        SizeType processed = 0;
        while (clfq_pop(&sample_rx, rms_buffer + RMS_STRIDE, RMS_STRIDE)) {
            const float rms_value = rms(rms_buffer, RMS_SIZE);
            fhistory_push(&rms_history, rms_value);

            // move old data backward
            memmove(rms_buffer, rms_buffer + RMS_STRIDE, RMS_STRIDE);
            processed++;
        }

        rms_vis_update(&visualizer, &rms_history, processed);
        {
            BeginDrawing();
            ClearBackground(BLACK);
            rms_vis_render_static(&visualizer, &rms_history);
            EndDrawing();
        }

        frame_counter++;
    }

    free(sample_queue);
    fhistory_destroy(rms_history);
    deinit_audio_processor();
    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
}
