#pragma once

#include <complex.h>

#include <fftw3.h>

#include "core/History.h"
#include "core/LockFreeQueue.h"
#include "definitions.h"
#include "dsp/filters.h"

typedef struct {
    const SizeType size;
    const SizeType stride;
    const float sample_rate;
    const float dc_blocker_frequency;
    const SizeType history_size;
} FFTConfig;

typedef struct {
    FFTConfig cfg;

    fftwf_plan plan;
    float* input;
    Complex* output;
    const SizeType n_bins;

    LockFreeQueueConsumer rx;
    FFTHistory history;
    OnePoleFilter dc_blocker;
} FFTAnalyzer;

FFTAnalyzer fft_analyzer_new(const FFTConfig* cfg, LockFreeQueueConsumer rx);
void fft_analyzer_free(FFTAnalyzer* analyzer);

// returns number of elements pushed onto its history
SizeType fft_analyzer_update(FFTAnalyzer* analyzer);
