// SPDX-License-Identifier: GPL-3.0-or-later
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

#define ALERT_FRACTION 16
// if ALERT_FRACTION is 10, alert at 90% fullness
#define ALMOSTFULL_ALERT \
    ((ALERT_FRACTION - 1) * CLF_QUEUE_SIZE / ALERT_FRACTION)

typedef struct {
    // the actual config
    const Rectangle screen;
    const SizeType logical_height;  // the number of frequency bands to
                                    // interpolate from the FFT bins
    const SizeType logical_width;   // number of datapoints
    const SizeType bins_per_octave;
    const float f_min;
    const float f_max;
    Colormap cmap;

    // some cached values
    const float Q; // f / bandwith => bandwidth = f * Q. depends only on BPO
    const SizeType fft_size;
    const SizeType fft_n_bins;
    const float log_f_min;
    const float log_f_max;
    const float power_reference;  // defines 0dB
    const float min_dB;           // cut stuff below -60dB or something
} LogSpectrogramConfig;

LogSpectrogramConfig log_spectrogram_config(SizeType bins_per_octave,
                                            Rectangle panel,
                                            const FFTAnalyzer* analyzer);

// partition the FFT bins into (geometric) frequency bands
// store as a start bin + number of bins in the band
// + the center frequency for monitoring
//
// ok i said partition but they're actually overlap for smoothness
typedef struct {
    // struct of arrays
    const SizeType n_bands;
    SizeType* band_start;       // [n_bands]
    SizeType* band_len;         // [n_bands]
    float* weights;             // [sum n_bands]
    float* center_frequencies;  // opt. metadata
    float* bandwidth;           // constant Q => adaptative bandwidth
} FrequencyBands;

FrequencyBands compute_frequency_bands(const LogSpectrogramConfig* cfg);

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
    LinearSpectrogramConfig spectrogram_cfg =
        linear_spectrogram_config(spectrogram_panel, plasma_rgba, &fft_config);
    LinearSpectrogram spectrogram = linear_spectrogram_new(&spectrogram_cfg);

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
