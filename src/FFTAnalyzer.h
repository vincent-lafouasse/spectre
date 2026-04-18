#pragma once

#include "kiss_fftr.h"

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

    kiss_fftr_cfg plan;
    float* input;   // where we collect the samples
    float* buffer;  // where we filter, window and FFT the samples
    kiss_fft_cpx* output;
    const SizeType n_bins;

    LockFreeQueueConsumer rx;
    FFTHistory history;
    OnePoleFilter dc_blocker;
} FFTAnalyzer;

FFTAnalyzer fft_analyzer_new(const FFTConfig* cfg, LockFreeQueueConsumer rx);
void fft_analyzer_free(FFTAnalyzer* analyzer);

// returns number of frames pushed onto the history
SizeType fft_analyzer_update(FFTAnalyzer* analyzer);
