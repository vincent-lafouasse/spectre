// SPDX-License-Identifier: GPL-3.0-or-later
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fftw3.h>
#include <raylib.h>

#include "RMSAnalyzer.h"
#include "RMSVisualizer.h"
#include "audio_callback.h"
#include "core/LockFreeQueue.h"
#include "definitions.h"
#include "dsp/filters.h"

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define ALERT_FRACTION 16
// if ALERT_FRACTION is 10, alert at 90% fullness
#define ALMOSTFULL_ALERT \
    ((ALERT_FRACTION - 1) * CLF_QUEUE_SIZE / ALERT_FRACTION)

typedef struct {
    SizeType size;
    SizeType stride;

    fftwf_plan plan;
    float* input;
    Complex* output;

    FFTHistory history;
    LockFreeQueueConsumer rx;

    OnePoleFilter dc_blocker;
    float sample_rate;
} FFTAnalyzer;

FFTAnalyzer fft_analyzer_new(float sample_rate,
                             LockFreeQueueConsumer rx,
                             SizeType size,
                             SizeType stride)
{
    float* input = fftwf_alloc_real(size);
    memset(input, 0, sizeof(*input) * size);
    Complex* output = fftwf_alloc_complex(1 + (size / 2));
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(size, input, output,
                                            FFTW_MEASURE | FFTW_PRESERVE_INPUT);

    const SizeType n_bins = size / 2;  // ditch the DC information
    FFTHistory history = fft_history_new(HISTORY_SIZE, n_bins);

    const float dc_frequency_cutoff = 10.0f;  // 10 Hz
    OnePoleFilter dc_blocker = filter_init(dc_frequency_cutoff, sample_rate);

    return (FFTAnalyzer){
        .size = size,
        .stride = stride,
        .plan = plan,
        .input = input,
        .output = output,
        .history = history,
        .rx = rx,
        .dc_blocker = dc_blocker,
        .sample_rate = sample_rate,
    };
}

void fft_analyzer_free(FFTAnalyzer* analyzer)
{
    if (!analyzer) {
        return;
    }

    fftwf_destroy_plan(analyzer->plan);
    fftwf_free(analyzer->input);
    fftwf_free(analyzer->output);
    fft_history_free(&analyzer->history);
}

// returns number of elements pushed onto its history
SizeType fft_analyzer_update(FFTAnalyzer* analyzer)
{
    const SizeType to_keep = analyzer->size - analyzer->stride;
    const SizeType to_read = analyzer->stride;

    SizeType n = 0;
    while (clfq_pop(&analyzer->rx, analyzer->input + to_keep, to_read)) {
        // no window, i want to see what happens when there's no windowing

        // HPF the slice we just pulled
        filter_hpf_process(&analyzer->dc_blocker, analyzer->input + to_keep,
                           to_read);

        // fft
        fftwf_execute(analyzer->plan);

        // ditch DC bin
        fft_history_push(&analyzer->history, analyzer->output + 1);

        // ditch the first to_read samples
        memmove(analyzer->input, analyzer->input + to_read, to_keep);
        ++n;
    }

    return n;
}

#include "colormap/colormap.h"

typedef struct {
    Texture2D texture;
    float height, width;
    Vector2 origin;
    SizeType size, n_bins;
    Color* column_buffer;
} FFTVisualizer;

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

FFTVisualizer fft_vis_new(const FFTAnalyzer* analyzer,
                          float w,
                          float h,
                          Vector2 origin)
{
    const SizeType size = analyzer->size;
    const SizeType n_bins = analyzer->history.n_bins;

    Image img = GenImageColor(size, n_bins, BLACK);
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    Color* column_buffer = malloc(sizeof(Color) * n_bins);

    return (FFTVisualizer){
        .texture = texture,
        .height = h,
        .width = w,
        .origin = origin,
        .size = size,
        .n_bins = n_bins,
        .column_buffer = column_buffer,
    };
}

void fft_vis_destroy(FFTVisualizer* fv)
{
    if (!fv) {
        return;
    }

    UnloadTexture(fv->texture);
    free(fv->column_buffer);
}
static void fft_vis_update_column(FFTVisualizer* fv,
                                  const Complex* bins,
                                  SizeType index)
{
    const uint8_t(*const cmap)[4] = plasma_rgba;

    const float reference_power = (float)(fv->size * fv->size) / 4.0f;
    const float min_db = -60.0f;

    for (SizeType b = 0; b < fv->n_bins; b++) {
        const float re = crealf(bins[b]);
        const float im = cimagf(bins[b]);

        const float power = re * re + im * im;
        const float db = 10.0f * log10f((power / reference_power) + 1e-9f);
        const float intensity = (db - min_db) / (-min_db);

        fv->column_buffer[b] = float_to_color(intensity, cmap, COLORMAP_SIZE);
    }

    UpdateTextureRec(fv->texture,
                     (Rectangle){(float)index, 0, 1, (float)fv->n_bins},
                     fv->column_buffer);
}

void fft_vis_update(FFTVisualizer* fv, const FFTHistory* h, SizeType n);
void fft_vis_render_wrap(const FFTVisualizer* fv, const FFTHistory* h);

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

    // analyzer
    LockFreeQueueConsumer sample_rx = clfq_consumer(sample_queue);
    RMSAnalyzer rms_analyzer = rms_analyzer_new(sample_rx);

    // visualizer
    RMSVisualizer visualizer =
        rms_vis_new(rms_analyzer.history.cap, WINDOW_WIDTH, WINDOW_HEIGHT,
                    (Vector2){0.0f, 0.0f});

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
        if (available > ALMOSTFULL_ALERT) {
            printf("frame %u buffer almost full: %u\n", frame_counter,
                   available);
        }

        SizeType processed = rms_analyzer_update(&rms_analyzer);
        rms_vis_update(&visualizer, &rms_analyzer.history, processed);

        {
            BeginDrawing();
            ClearBackground(BLACK);
            rms_vis_render_wrap(&visualizer, &rms_analyzer.history);
            EndDrawing();
        }

        frame_counter++;
    }

    rms_analyzer_destroy(&rms_analyzer);
    deinit_audio_processor();
    UnloadMusicStream(music);
    CloseAudioDevice();
    free(sample_queue);
    CloseWindow();
}
