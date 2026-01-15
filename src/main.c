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
    const float Q;  // Q = f / bandwidth
    // constant Q => adaptative bandwidth
    const float sigma;       // the gaussian that computes weights
    const float freq_ratio;  // f[n+1]/f[n]
    const float sample_rate;
    const SizeType fft_size;
    const SizeType fft_n_bins;
    const float power_reference;  // defines 0dB
    const float min_dB;           // cut stuff below -60dB or something
} LogSpectrogramConfig;

// sharpness multiplies the gaussian sigma
// so higher sharpness mean narrower bands
LogSpectrogramConfig log_spectrogram_config(float sharpness,
                                            SizeType bins_per_octave,
                                            Rectangle panel,
                                            const FFTAnalyzer* analyzer)
{
    const float f_min = 35.0f;
    const float f_max = 18000.0f;

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
    // we will find where the band end (f_end) and compute sigma by inverting
    // the gaussian
    const float band_cutoff = 0.5f;  // -3 dB

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
    // and f_start = f[n] / sqrt(r)
    //
    // this is just the geometric mean/midpoint lol, well at least i'm sure of
    // myself
    //
    // anw, we want G(f_end) = cutoff = 0.5f for the base sigma
    // where G(f) = exp(-0.5 * d^2 / sigma^2)
    // where d = 1 / 2BPO
    //
    // so d^2 / sigma^2 = -2 ln(cutoff) := C for cutoff
    // cutoff < 1 so C > 0
    // ie sigma^2 = d^2 / C
    // sigma = d * C^-1/2
    // sigma = (1 / 2BPO) / sqrt(C)
    const float base_sigma =
        (0.5f / (float)bins_per_octave) / sqrtf(-2.0f * logf(band_cutoff));
    assert(base_sigma > 0.0f);

    assert(sharpness > 0.0f);
    const float sigma = base_sigma * sharpness;

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
        .sigma = sigma,
        .freq_ratio = freq_ratio,
        .fft_size = fft_size,
        .sample_rate = analyzer->cfg.sample_rate,
        .fft_n_bins = fft_size / 2,
        .power_reference = 0.25f * (float)(fft_size * fft_size),  // parseval
        .min_dB = -60.0f,
    };
}

float frequency_weight(float f, float f_c, float sigma)
{
    const float distance = fabs(log2f(f / f_c));

    return expf(-0.5f * distance * distance / (sigma * sigma));
}

// no padding and accessed entirely at once
// fine to make an array of
typedef struct {
    uint32_t fft_bin;
    float weight;
} WeightEntry;

// same
// indexes the weights
typedef struct {
    uint32_t offset;
    uint32_t len;
} BandMetadata;

typedef struct {
    const SizeType n_bands;
    BandMetadata* bands;   // [n_bands], indexes the weights
    WeightEntry* weights;  // [sum bands->len]
    // optional metadata
    float* center_frequencies;  // [n_bands]
} FrequencyBands;

float adaptive_sigma(float base_sigma, SizeType bin, SizeType n_bins)
{
    const float lf_multiplier = 10.0f;  // bigger search range in the bass
    const float hf_multiplier = 0.5f;   // less resolution in the treble is fine

    const float progress = (float)bin / (float)(n_bins - 1);
    const float multiplier =
        hf_multiplier * progress + lf_multiplier * (1.0f - progress);
    return base_sigma * multiplier;
}

FrequencyBands compute_frequency_bands(const LogSpectrogramConfig* cfg)
{
    const SizeType n_bands = cfg->logical_height;

    float* center_frequencies = malloc(sizeof(float) * n_bands);
    center_frequencies[0] = cfg->f_min;
    for (SizeType i = 1; i < n_bands; i++) {
        center_frequencies[i] = cfg->freq_ratio * center_frequencies[i - 1];
    }

    SizeType* band_start = malloc(sizeof(SizeType) * n_bands);
    SizeType* band_len = malloc(sizeof(SizeType) * n_bands);
    SizeType* weight_offsets = malloc(sizeof(SizeType) * n_bands);

    const float fft_bw = cfg->sample_rate / (float)cfg->fft_size;

    const float base_sigma = cfg->sigma;

    SizeType weight_count = 0;
    for (SizeType i = 0; i < n_bands; i++) {
        // adaptative sigma
        const float sigma = adaptive_sigma(base_sigma, i, n_bands);
        const float f_c = center_frequencies[i];
        const float search_range = 3.0f * sigma;
        const float f_low = f_c * powf(2.0f, -search_range);
        const float f_high = f_c * powf(2.0f, search_range);

        const int start_bin = (int)floorf(f_low / fft_bw);
        const SizeType end_bin = (SizeType)ceilf(f_high / fft_bw);
        const SizeType end =
            (end_bin >= cfg->fft_n_bins) ? cfg->fft_n_bins - 1 : end_bin;

        band_start[i] = (start_bin < 1) ? 1 : (SizeType)start_bin;
        band_len[i] = end + 1 - band_start[i];  // include end bin
        weight_offsets[i] = weight_count;

        weight_count += band_len[i];
    }

    float* weights = malloc(sizeof(float) * weight_count);
    for (SizeType i = 0; i < n_bands; i++) {
        const float f_c = center_frequencies[i];
        const SizeType fft_start = band_start[i];
        const SizeType len = band_len[i];
        const SizeType weight_offset = weight_offsets[i];
        const float sigma = adaptive_sigma(base_sigma, i, n_bands);

        for (SizeType j = 0; j < len; j++) {
            const SizeType fft_bin = fft_start + j;
            const float f = (float)fft_bin * fft_bw;
            const float w = frequency_weight(f, f_c, sigma);
            weights[weight_offset + j] = w;
        }
    }

    // normalize
    for (SizeType i = 0; i < n_bands; i++) {
        const SizeType offset = weight_offsets[i];
        const SizeType len = band_len[i];

        float sum = 0.0f;
        for (SizeType j = 0; j < len; j++) {
            sum += weights[offset + j];
        }
        for (SizeType j = 0; j < len; j++) {
            weights[offset + j] /= sum;
        }
    }

    // gather
    BandMetadata* bands = malloc(n_bands * sizeof(*bands));
    WeightEntry* weight_entries =
        malloc(weight_count * sizeof(*weight_entries));

    SizeType index = 0;
    for (SizeType i = 0; i < n_bands; i++) {
        const BandMetadata band = (BandMetadata){
            .offset = weight_offsets[i],
            .len = band_len[i],
        };
        bands[i] = band;

        for (SizeType j = 0; j < band.len; j++) {
            weight_entries[index] = (WeightEntry){
                .fft_bin = band_start[i] + j,
                .weight = weights[index],
            };
            index += 1;
        }
    }

    free(band_start);
    free(band_len);
    free(weight_offsets);
    free(weights);

    return (FrequencyBands){
        .n_bands = n_bands,
        .bands = bands,
        .weights = weight_entries,
        .center_frequencies = center_frequencies,
    };
}

typedef struct {
    Texture2D texture;
    Color* column_buffer;  // [cfg.n_bands] precomputed buffer to move data from
                           // CPU to GPU
    float*
        mag_buffer;  // [cfg.fft_n_bins] store magnitude before recombining them
    FrequencyBands bands;
    const LogSpectrogramConfig cfg;
} LogSpectrogram;

LogSpectrogram log_spectrogram_new(const LogSpectrogramConfig* cfg)
{
    FrequencyBands bands = compute_frequency_bands(cfg);
    float* mag_buffer = malloc(cfg->fft_n_bins * sizeof(float));
    Color* column_buffer = malloc(bands.n_bands);

    Image img = GenImageColor(cfg->logical_width, cfg->logical_height, BLACK);
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    return (LogSpectrogram){
        .texture = texture,
        .column_buffer = column_buffer,
        .mag_buffer = mag_buffer,
        .bands = bands,
        .cfg = *cfg,
    };
}

void log_spectrogram_destroy(LogSpectrogram* spec)
{
    free(spec->column_buffer);
    free(spec->mag_buffer);
    UnloadTexture(spec->texture);
    free(spec->bands.bands);
    free(spec->bands.center_frequencies);
    free(spec->bands.weights);
}

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

static void log_spectrogram_update_column(LogSpectrogram* spec,
                                          const Complex* bins,
                                          SizeType index)
{
}

void log_spectrogram_update(LogSpectrogram* spec,
                            const FFTHistory* h,
                            SizeType n)
{
    n = (n >= h->cap) ? h->cap : n;
    const SizeType start = (h->tail - n + h->cap) % h->cap;

    for (SizeType i = 0; i < n; i++) {
        const SizeType index = (start + i) % h->cap;
        const Complex* bins = fft_history_get_row(h, index);
        log_spectrogram_update_column(spec, bins, index);
    }
}

void log_spectrogram_render_wrap(const LogSpectrogram* spec,
                                 const FFTHistory* h);

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

    const LogSpectrogramConfig log_spectrogram_cfg =
        log_spectrogram_config(1.0f, 18, spectrogram_panel, &analyzer);
    FrequencyBands bands = compute_frequency_bands(&log_spectrogram_cfg);
    {
        const LogSpectrogramConfig* cfg = &log_spectrogram_cfg;
        const float fft_bw = cfg->sample_rate / cfg->fft_size;

        for (SizeType i = 0; i < bands.n_bands; i++) {
            const float fc = bands.center_frequencies[i];
            printf("Band %u %f {\n", i, fc);

            const BandMetadata band = bands.bands[i];
            for (SizeType j = 0; j < band.len; j++) {
                const SizeType offset = band.offset + j;
                const WeightEntry weight = bands.weights[offset];

                const float f = fft_bw * (float)weight.fft_bin;
                const float w = weight.weight;
                printf("\t%f:\tweight %f\n", f, w);
            }

            printf("}\n\n");
        }
        exit(0);
    }

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
