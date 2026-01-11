// SPDX-License-Identifier: GPL-3.0-or-later
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>

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
    fftwf_plan plan;
    float* input;
    Complex* output;

    FFTHistory history;
    LockFreeQueueConsumer rx;

    OnePoleFilter dc_blocker;
    float sample_rate;
} FFTAnalyzer;

FFTAnalyzer fft_ana_new(float sample_rate, LockFreeQueueConsumer rx)
{
    float* input = fftwf_alloc_real(FFT_SIZE);
    Complex* output = fftwf_alloc_complex(1 + (FFT_SIZE / 2));
    fftwf_plan plan =
        fftwf_plan_dft_r2c_1d(FFT_SIZE, input, output, FFTW_MEASURE);

    const SizeType n_bins = FFT_SIZE / 2;  // ditch the DC information
    FFTHistory history = fft_history_new(HISTORY_SIZE, n_bins);

    const float dc_frequency_cutoff = 10.0f;  // 10 Hz
    OnePoleFilter dc_blocker = filter_init(dc_frequency_cutoff, sample_rate);

    return (FFTAnalyzer){
        .plan = plan,
        .input = input,
        .output = output,
        .history = history,
        .rx = rx,
        .dc_blocker = dc_blocker,
        .sample_rate = sample_rate,
    };
}

void fft_ana_free(FFTAnalyzer* analyzer)
{
    if (!analyzer) {
        return;
    }

    fftwf_destroy_plan(analyzer->plan);
    fftwf_free(analyzer->input);
    fftwf_free(analyzer->output);
    fft_history_free(&analyzer->history);
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
