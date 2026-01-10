// SPDX-License-Identifier: GPL-3.0-or-later
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <raylib.h>
#include <string.h>

#include "audio_callback.h"
#include "colormap/colormap.h"
#include "core/History.h"
#include "core/LockFreeQueue.h"

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define PIXEL_PER_BAND 4

float clamp_unit(float f)
{
    return fminf(fmaxf(f, 0.0f), 1.0f);
}

Color float_to_color(float intensity,
                     const uint8_t (*const cmap)[4],
                     SizeType cmap_size)
{
    const float clamped = clamp_unit(intensity);
    const int index = (int)(clamped * (cmap_size - 0.0001f));
    return *(Color*)cmap[index];
}

void render_band(SizeType band, float value, Color color)
{
    const int height = (int)(value * WINDOW_HEIGHT);
    DrawRectangle(PIXEL_PER_BAND * band, WINDOW_HEIGHT - height, PIXEL_PER_BAND,
                  WINDOW_HEIGHT - 1, color);
}

void rms_history_render(const FloatHistory* rms_history)
{
    const SplitSlice rms_values = fhistory_get(rms_history);
    const uint8_t(*const cmap)[4] = plasma_rgba;

    const float analysis_gain = 0.3f;

    for (SizeType i = 0; i < rms_values.size1; i++) {
        const float power = clamp_unit(rms_values.slice1[i] * analysis_gain * analysis_gain);
        const Color color = float_to_color(power, cmap, COLORMAP_SIZE);
        render_band(i, power, color);
    }

    for (SizeType i = 0; i < rms_values.size2; i++) {
        const float power = clamp_unit(rms_values.slice2[i] * analysis_gain * analysis_gain);
        const Color color = float_to_color(power, cmap, COLORMAP_SIZE);
        render_band(rms_values.size1 + i, power, color);
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

    FloatHistory rms_history = fhistory_new(WINDOW_WIDTH / PIXEL_PER_BAND);
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
            fhistory_push(&rms_history, rms_value);

            // move old data backward
            memmove(rms_buffer, rms_buffer + RMS_STRIDE, RMS_STRIDE);
        }

        BeginDrawing();
        ClearBackground(BLACK);
        rms_history_render(&rms_history);
        EndDrawing();

        frame_counter++;
    }

    free(sample_queue);
    fhistory_destroy(rms_history);
    deinit_audio_processor();
    UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
}
