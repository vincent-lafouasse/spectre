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

static float clamp_unit(float f)
{
    return fminf(fmaxf(f, 0.0f), 1.0f);
}

static Color float_to_color(float intensity,
                            const uint8_t (*const cmap)[4],
                            SizeType cmap_size)
{
    const float clamped = clamp_unit(intensity);
    const int index = (int)(clamped * (cmap_size - 0.0001f));
    return *(Color*)cmap[index];
}

typedef struct {
    // the texture is contains the same info as the history
    // except it's on the GPU
    Texture2D texture;
    float height, width;
    Vector2 origin;
    SizeType size;
} RMSVisualizer;

RMSVisualizer rms_vis_new(SizeType size, float w, float h, Vector2 origin)
{
    Image img = GenImageColor(size, 1, BLACK);
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    return (RMSVisualizer){
        .texture = texture,
        .height = h,
        .width = w,
        .origin = origin,
        .size = size,
    };
}

void rms_vis_destroy(RMSVisualizer* rv)
{
    if (!rv) {
        return;
    }

    UnloadTexture(rv->texture);
}

void rms_vis_update_value(RMSVisualizer* rv, float value, SizeType index)
{
    const uint8_t(*const cmap)[4] = plasma_rgba;
    value = clamp_unit(value);
    const Color color = float_to_color(value, cmap, COLORMAP_SIZE);
    const Color* pixels = &color;  // 1 pixel

    UpdateTextureRec(rv->texture, (Rectangle){(float)index, 0, 1, 1}, pixels);
}

// called after processing and before drawing block
void rms_vis_update(RMSVisualizer* rv,
                    const FloatHistory* rms_history,
                    SizeType n)
{
    n = n > rms_history->cap ? rms_history->cap : n;
    const SizeType start =
        (rms_history->cap + rms_history->tail - n) % rms_history->cap;

    for (SizeType i = 0; i < n; i++) {
        const SizeType index = (start + i) % rms_history->cap;
        const float value = rms_history->data[index];
        rms_vis_update_value(rv, value, index);
    }
}

void rms_vis_render_static(const RMSVisualizer* rv,
                           const FloatHistory* rms_history)
{
    const float band_width = rv->width / rms_history->cap;

    for (SizeType i = 0; i < rms_history->cap; i++) {
        if (rms_history->len < rms_history->cap && i >= rms_history->tail) {
            break;
        }

        // stretch this
        const Rectangle src = {(float)i, 0, 1, 1};

        // to this
        const float height = clamp_unit(rms_history->data[i]) * rv->height;
        const Vector2 corner = {
            i * band_width,
            0.5f * (rv->height - height),
        };
        const Rectangle dest = {
            corner.x,
            corner.y,
            band_width,
            height,
        };

        const float angle = 0.0f;
        const Color tint = WHITE;  // no tint
        DrawTexturePro(rv->texture, src, dest, rv->origin, angle, tint);
    }
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
