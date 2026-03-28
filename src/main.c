#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>

#include "FFTAnalyzer.h"
#include "LinearSpectrogram.h"
#include "audio_callback.h"
#include "colormap/palette.h"
#include "core/LockFreeQueue.h"
#include "definitions.h"

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900

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

    // spectrogram
    const Rectangle spectrogram_panel = {
        .x = 0,
        .y = 0,
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
    };

    const LinearSpectrogramConfig spectrogram_cfg =
        linear_spectrogram_config(spectrogram_panel, plasma_rgba, &fft_config);
    LinearSpectrogram spectrogram = linear_spectrogram_new(&spectrogram_cfg);

    PlayMusicStream(music);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        UpdateMusicStream(music);

        // pull samples from queue and push onto its history
        const SizeType processed = fft_analyzer_update(&analyzer);
        linear_spectrogram_update(&spectrogram, &analyzer.history, processed);

        {
            BeginDrawing();
            ClearBackground(BACKGROUND_COLOR);
            linear_spectrogram_render_wrap(&spectrogram, &analyzer.history);
            EndDrawing();
        }

        frame_counter++;
    }

    fft_analyzer_free(&analyzer);
    linear_spectrogram_destroy(&spectrogram);
    deinit_audio_processor();
    UnloadMusicStream(music);
    CloseAudioDevice();
    free(sample_queue);
    CloseWindow();
}
