#pragma once

#include <raylib.h>

#include "FFTAnalyzer.h"
#include "colormap/colormap.h"
#include "definitions.h"

typedef struct {
    // the actual config
    const Rectangle screen;
    const SizeType logical_height;  // aliases the number of FFT bins
    const SizeType
        logical_width;  // number of datapoints, aliases the history size
    Colormap cmap;

    // some cached values
    const float power_reference;  // defines 0dB
    const float min_dB;
} LinearSpectrogramConfig;

LinearSpectrogramConfig linear_spectrogram_config(
    Rectangle screen,
    Colormap cmap,
    const FFTConfig* analyzer_cfg);

typedef struct {
    Texture2D texture;
    Color* column_buffer;  // precomputed buffer to move data from CPU to GPU
    const LinearSpectrogramConfig cfg;
} LinearSpectrogram;

LinearSpectrogram linear_spectrogram_new(const LinearSpectrogramConfig* cfg);
void linear_spectrogram_destroy(LinearSpectrogram* spec);
void linear_spectrogram_update(LinearSpectrogram* spec,
                               const FFTHistory* h,
                               SizeType n);
void linear_spectrogram_render_wrap(const LinearSpectrogram* spec,
                                    const FFTHistory* h);
