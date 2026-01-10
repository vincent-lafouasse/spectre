// SPDX-License-Identifier: GPL-3.0-or-later
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <raylib.h>
#include <string.h>

#include "LockFreeQueue.h"
#include "audio_callback.h"
#include "colormap/colormap.h"

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define PIXEL_PER_BAND 4
#define HISTORY_SIZE (WINDOW_WIDTH / PIXEL_PER_BAND)

// keep a history of computed values for displaying
// a circular buffer that overwrites its oldest data
//
// payload is `float rms` for now but will be `float fft[]` later
typedef struct {
    float data[HISTORY_SIZE];
    SizeType head;  // always point to the oldest sample
    SizeType tail;  // the next position to write to. can be equal to head
    SizeType len;
} History;

History history_new(void)
{
    return (History){0};
}

void history_push(History* h, float f)
{
    h->data[h->tail] = f;
    const SizeType new_tail = (h->tail + 1) % HISTORY_SIZE;
    h->tail = new_tail;
    if (h->len == HISTORY_SIZE) {
        h->head = h->tail;
    } else {
        h->len += 1;
    }
}

// one or two slices
typedef struct {
    const float* slice1;
    SizeType size1;
    const float* slice2;
    SizeType size2;
} SplitSlice;

SplitSlice history_get(const History* h)
{
    // might be off the buffer
    const SizeType end = h->head + h->len;

    if (end <= HISTORY_SIZE) {
        return (SplitSlice){
            .slice1 = h->data + h->head,
            .size1 = h->len,
            .slice2 = NULL,
            .size2 = 0,
        };
    }

    const SizeType size1 = HISTORY_SIZE - h->head;
    const SizeType size2 = h->len - size1;

    return (SplitSlice){
        .slice1 = h->data + h->head,
        .size1 = size1,
        .slice2 = h->data,
        .size2 = size2,
    };
}
Color color_from_floats(const float color[4])
{
    return (Color){.r = (unsigned char)(color[0] * 255.0f),
                   .g = (unsigned char)(color[1] * 255.0f),
                   .b = (unsigned char)(color[2] * 255.0f),
                   .a = (unsigned char)(color[3] * 255.0f)};
}

void history_render(const History* h)
{
    const SplitSlice data = history_get(h);

    for (SizeType i = 0; i < data.size1; i++) {
        const float value = data.slice1[i];

        const int height = (int)(value * WINDOW_HEIGHT);
        int color_index = (int)(value * COLORMAP_SIZE);
        color_index =
            color_index >= COLORMAP_SIZE ? COLORMAP_SIZE - 1 : color_index;
        color_index = color_index < 0 ? 0 : color_index;

        const Color color = color_from_floats(viridis_rgba[color_index]);

        DrawRectangle(PIXEL_PER_BAND * i, 0, PIXEL_PER_BAND, height, color);
    }

    for (SizeType i = 0; i < data.size2; i++) {
        const float value = data.slice2[i];

        const int height = (int)(value * WINDOW_HEIGHT);
        int color_index = (int)(value * COLORMAP_SIZE);
        color_index =
            color_index >= COLORMAP_SIZE ? COLORMAP_SIZE - 1 : color_index;
        color_index = color_index < 0 ? 0 : color_index;

        const Color color = color_from_floats(viridis_rgba[color_index]);

        DrawRectangle(PIXEL_PER_BAND * (data.size1 + i), 0, PIXEL_PER_BAND,
                      height, color);
    }
}

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

    return sqrtf(sum);
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

    History rms_history = history_new();
    float rms_buffer[RMS_SIZE] = {0};

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

        while (clfq_pop(&sample_rx, rms_buffer + RMS_STRIDE, RMS_STRIDE)) {
            const float rms_value = rms(rms_buffer, RMS_SIZE);
            history_push(&rms_history, rms_value);

            // move old data backward
            memmove(rms_buffer, rms_buffer + RMS_STRIDE, RMS_STRIDE);
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        history_render(&rms_history);
        EndDrawing();

        frame_counter++;
    }

    free(sample_queue);
    deinit_audio_processor();
    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
}
