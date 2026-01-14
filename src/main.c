// SPDX-License-Identifier: GPL-3.0-or-later
#include <assert.h>
#include <math.h>
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
    const float Q;  // f / bandwith => bandwidth = f * Q. depends only on BPO
    const float sharpness;   // scales the gaussian sigma from Q. lower means
                             // more overlap
    const float freq_ratio;  // f[n+1]/f[n]
    const SizeType fft_size;
    const SizeType fft_n_bins;
    const float log_f_min;
    const float log_f_max;
    const float power_reference;  // defines 0dB
    const float min_dB;           // cut stuff below -60dB or something
} LogSpectrogramConfig;

#define TODO(T) ((T){0})

LogSpectrogramConfig log_spectrogram_config(SizeType bins_per_octave,
                                            Rectangle panel,
                                            const FFTAnalyzer* analyzer)
{
    const float f_min = 20.0f;
    const float f_max = 20000.0f;

    const SizeType fft_size = analyzer->cfg.size;

    // BPO => 2 = freq_ratio^(BPO), e.g. 12-TET => r = 2^(1/12)
    // => freq_ratio = 2^(1/BPO)
    const float freq_ratio = powf(2.0f, 1.0f / (float)(bins_per_octave));

    const float logical_height_f =
        (float)bins_per_octave * log2f(f_max / f_min);

    // Q of band `n` = f[n] / bandwidth[n]
    // constant Q => adaptative bandwidths (musical)
    // dictates the width of the gaussian that sets the weight of FFT bins as a
    // function of the (log?) frequency offset
    //
    // BW[n] = f[n + 1] - f[n] = r * f[n] - f[n] = f[n] (r - 1)
    // => Q = f[n]/BW[n] = 1 / (r - 1)
    const float Q = 1.0f / (freq_ratio - 1.0f);

    // we define the log2 distance dist2(a, b) = max(log2(a / b), log2(b / a))
    // ie we measure in octaves
    // since log2(up_the_octave / reference) = log2(2) = 1 (octave)
    //
    // let i be a frequency bin, f_c its center frequency, Q its factor, bw its
    // bandwidth
    //
    // we will select FFT bins based on the value of a gaussian centered on f_c
    const float cutoff_value = 0.5f; // -3 dB

    // we will find where the band end (f_end) and compute sigma by inverting
    // the gaussian
    //
    // first, what's the distance to the next band ?
    // dist2(f[n+1], f[n]) = dist2(r * f[n], f[n]) = log2(r) = 1/BPO
    // makes sense, e.g. if 12-TET, the next bin is 1/12 octave away
    // ie a semitone
    //
    // so the band boundary is equidistant from f[n] and f[n+1]
    // ie log2(f[n+1] / f_end) = log2(f_end, f[n])
    // ie r * f[n] / f_end = f_end / f[n]
    // ie f_end^2 = r * f[n]^2
    // ie f_end = sqrt(r) * f[n]
    //
    // with d(end, center) = log2(sqrt(r)) = 1/2 log2(r) = 1/2 * 1/BPO
    //
    // resp, dist(f_start, f[n-1]) = dist(f[n], f_start)
    // => f_start^2 = f[n] * f[n-1] = f[n]^2 / r
    // => f_start = sqrt(1/r) f[n]
    //
    // this is just the geometric mean/midpoint lol, well at least i'm sure of
    // myself
    //
    // anw, we want G(f_end) = cutoff
    // where G(f) = exp(-0.5 * d^2 / sigma^2)
    // where d = 1 / 2BPO
    //
    // so d^2 / sigma^2 = -2 ln(cutoff) := C for cutoff
    // cutoff < 1 so C > 0
    // ie sigma^2 = d^2 / C
    // sigma = d * C^-1/2
    // sigma = (1 / 2BPO) / sqrt(C)
    assert(cutoff_value < 1.0f && cutoff_value > 0.0f);
    const float sigma = (0.5f / (float)bins_per_octave) / sqrtf(-2.0f * logf(cutoff_value));
    assert(sigma > 0.0f);

    // A.N. : 12-TET with -3 dB cutoff
    // => BPO = 12 => d = 1/24
    // cutoff = 0.5 => C = 1.4
    // => sigma = 0.035
    // sigma is a constant in this setup because the frequency log-scaling is
    // taken care of by the log distance

    return (LogSpectrogramConfig){
        .screen = panel,
        .logical_height = (SizeType)logical_height_f,
        .logical_width = analyzer->history.cap,
        .bins_per_octave = bins_per_octave,
        .f_min = f_min,
        .f_max = f_max,
        .cmap = plasma_rgba,

        .Q = Q,
        .sharpness = 2.5f,  // idk
        .freq_ratio = freq_ratio,
        .fft_size = fft_size,
        .fft_n_bins = fft_size / 2,
        .log_f_min = log10f(f_min),
        .log_f_max = log10f(f_max),
        .power_reference = 0.25f * (float)(fft_size * fft_size),  // parseval
        .min_dB = -60.0f,
    };
}

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
