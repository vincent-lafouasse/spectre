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

Color color_from_floats(const float color[4])
{
    return (Color){.r = (unsigned char)(color[0] * 255.0f),
                   .g = (unsigned char)(color[1] * 255.0f),
                   .b = (unsigned char)(color[2] * 255.0f),
                   .a = (unsigned char)(color[3] * 255.0f)};
}

float clamp_float_exlusive(float f, float min_value, float max_value)
{
    if (f < min_value) {
        return min_value;
    } else if (f >= max_value) {
        return nextafter(max_value, min_value);
    } else {
        return f;
    }
}

// expected in [0.0, 1.0[ but will be clamped
Color float_to_color(float f, const float (*const cmap)[4])
{
    const float clamped = clamp_float_exlusive(f, 0.0f, 1.0f);
    const int color_index = (int)(clamped * COLORMAP_SIZE);
    return color_from_floats(cmap[color_index]);
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
    const float(*const cmap)[4] = plasma_rgba;

    for (SizeType i = 0; i < rms_values.size1; i++) {
        const float value =
            clamp_float_exlusive(rms_values.slice1[i], 0.0f, 1.0f);
        const Color color = float_to_color(value, cmap);
        render_band(i, value, color);
    }

    for (SizeType i = 0; i < rms_values.size2; i++) {
        const float value =
            clamp_float_exlusive(rms_values.slice2[i], 0.0f, 1.0f);
        const Color color = float_to_color(value, cmap);
        render_band(rms_values.size1 + i, value, color);
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
        ClearBackground(RAYWHITE);
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
