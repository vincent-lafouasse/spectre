// SPDX-License-Identifier: GPL-3.0-or-later
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>

#include "FFTAnalyzer.h"
#include "audio_callback.h"
#include "colormap/palette.h"
#include "core/History.h"
#include "core/LockFreeQueue.h"
#include "definitions.h"
#include "dsp/filters.h"

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

#define ALERT_FRACTION 16
// if ALERT_FRACTION is 10, alert at 90% fullness
#define ALMOSTFULL_ALERT \
    ((ALERT_FRACTION - 1) * CLF_QUEUE_SIZE / ALERT_FRACTION)

#include "colormap/colormap.h"

typedef struct {
    // the actual config
    const Rectangle screen;
    const SizeType logical_height;  // the number of frequency bands to
                                    // interpolate from the FFT bins
    const SizeType logical_width;   // number of datapoints
    const float f_min;
    const float f_max;
    Colormap cmap;

    // some cached values
    const SizeType fft_size;
    const SizeType fft_n_bins;
    const float log_f_min;
    const float log_f_max;
    const float power_reference;  // defines 0dB
    const float min_dB;           // cut stuff below -60dB or something
} FFTVisualizerConfig;

FFTVisualizerConfig fft_vis_config(SizeType n_freq_bands,
                                   Rectangle screen,
                                   Colormap cmap,
                                   const FFTConfig* analyzer_cfg)
{
    const SizeType logical_height = n_freq_bands;
    const SizeType logical_width = analyzer_cfg->history_size;
    const float f_min = 20.0f;
    const float f_max = 20000.0f;

    const SizeType fft_size = analyzer_cfg->size;
    const SizeType fft_n_bins = fft_size / 2;

    const float log_f_min = log10f(f_min);
    const float log_f_max = log10f(f_max);
    // estimated using parseval
    const float power_reference = 0.25f * (float)(fft_size * fft_size);
    const float min_dB = -60.0f;  // -60 dB should be quiet enough

    return (FFTVisualizerConfig){
        .screen = screen,
        .logical_height = logical_height,
        .logical_width = logical_width,
        .f_min = f_min,
        .f_max = f_max,
        .fft_size = fft_size,
        .fft_n_bins = fft_n_bins,
        .log_f_min = log_f_min,
        .log_f_max = log_f_max,
        .power_reference = power_reference,
        .min_dB = min_dB,
        .cmap = cmap,
    };
}

typedef struct {
    Texture2D texture;
    Color* column_buffer;  // precomputed buffer to move data from CPU to GPU
    FFTVisualizerConfig cfg;
} FFTVisualizer;

static float clamp_unit(float f)
{
    return fminf(fmaxf(f, 0.0f), 1.0f);
}

static Color float_to_color(float intensity, Colormap cmap, SizeType cmap_size)
{
    const float clamped = clamp_unit(intensity);
    const int index = (int)(clamped * (cmap_size - 0.0001f));
    return *(Color*)cmap[index];
}

FFTVisualizer fft_vis_new(const FFTVisualizerConfig* cfg)
{
    Image img = GenImageColor(cfg->logical_width, cfg->logical_height, BLACK);
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    Color* column_buffer = malloc(sizeof(Color) * cfg->logical_height);

    return (FFTVisualizer){
        .texture = texture,
        .column_buffer = column_buffer,
        .cfg = *cfg,
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

static Color fft_vis_assign_color(const FFTVisualizer* fv, Complex bin)
{
    const FFTVisualizerConfig* cfg = &fv->cfg;

    const float re = crealf(bin);
    const float im = cimagf(bin);

    const float power = re * re + im * im;
    const float db = 10.0f * log10f((power / cfg->power_reference) + 1e-9f);

    // scale [min_db, reference_power] to [0, 1]
    // reference_power would appear here but it's 0dB by definition
    const float intensity = (db - cfg->min_dB) / (-cfg->min_dB);

    return float_to_color(intensity, cfg->cmap, COLORMAP_SIZE);
}

static void fft_vis_update_column(FFTVisualizer* fv,
                                  const Complex* bins,
                                  SizeType index)
{
    for (SizeType b = 0; b < fv->cfg.logical_height; b++) {
        fv->column_buffer[b] = fft_vis_assign_color(fv, bins[b]);
    }

    UpdateTextureRec(
        fv->texture,
        (Rectangle){(float)index, 0, 1, (float)fv->cfg.logical_height},
        fv->column_buffer);
}

void fft_vis_update(FFTVisualizer* fv, const FFTHistory* h, SizeType n)
{
    // h->cap aliases fv->cfg.logical_width as the texture is the fft history
    // but on the GPU. hence how `i` indexes both the history and the texture
    n = (n >= h->cap) ? h->cap : n;
    const SizeType start = (h->tail - n + h->cap) % h->cap;

    for (SizeType i = 0; i < n; i++) {
        const SizeType index = (start + i) % h->cap;
        const Complex* bins = fft_history_get_row(h, index);
        fft_vis_update_column(fv, bins, index);
    }
}

void fft_vis_render_wrap(const FFTVisualizer* fv, const FFTHistory* h)
{
    //                    REVERSED HORIZONTALLY v
    const Rectangle src = {0, 0, (float)h->len, -(float)fv->cfg.logical_height};

    const Rectangle* screen = &fv->cfg.screen;
    const float screen_draw_width =
        ((float)h->len / (float)h->cap) * screen->width;
    const Rectangle dest = {screen->x, screen->y, screen_draw_width,
                            screen->height};

    DrawTexturePro(fv->texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);

    if (h->len >= h->cap) {
        const float cursor_x =
            screen->x + ((float)h->tail / h->cap) * screen->width;
        DrawLineV((Vector2){cursor_x, screen->y},
                  (Vector2){cursor_x, screen->y + screen->height}, RED);
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

    Music music = LoadMusicStream(music_path);
    if (!IsMusicValid(music)) {
        printf("Failed to open %s\n", music_path);
        exit(1);
    }

    // analyzer
    const FFTConfig fft_config = {
        .size = FFT_SIZE,
        .stride = FFT_SIZE / 2,
        .dc_blocker_frequency = 10.0f,  // 10 Hz
        .history_size = HISTORY_SIZE,
        .sample_rate = music.stream.sampleRate,
    };
    LockFreeQueueConsumer sample_rx = clfq_consumer(sample_queue);
    FFTAnalyzer analyzer = fft_analyzer_new(&fft_config, sample_rx);

    // visualizer
    const SizeType n_freq_bands = fft_config.size / 2;
    const Rectangle spectrogram_panel = {
        .x = 0,
        .y = 0,
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
    };
    FFTVisualizerConfig vis_cfg = fft_vis_config(
        n_freq_bands, spectrogram_panel, plasma_rgba, &fft_config);
    FFTVisualizer visualizer = fft_vis_new(&vis_cfg);

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

        // pull samples from queue and push onto its history
        const SizeType processed = fft_analyzer_update(&analyzer);
        fft_vis_update(&visualizer, &analyzer.history, processed);

        {
            BeginDrawing();
            ClearBackground(BACKGROUND_COLOR);
            fft_vis_render_wrap(&visualizer, &analyzer.history);
            EndDrawing();
        }

        frame_counter++;
    }

    fft_analyzer_free(&analyzer);
    fft_vis_destroy(&visualizer);
    deinit_audio_processor();
    UnloadMusicStream(music);
    CloseAudioDevice();
    free(sample_queue);
    CloseWindow();
}
