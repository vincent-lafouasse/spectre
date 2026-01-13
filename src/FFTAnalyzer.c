#include "FFTAnalyzer.h"

#include <string.h>

FFTAnalyzer fft_analyzer_new(const FFTConfig* cfg, LockFreeQueueConsumer rx)
{
    float* input = fftwf_alloc_real(cfg->size);
    memset(input, 0, sizeof(*input) * cfg->size);
    Complex* output = fftwf_alloc_complex(1 + (cfg->size / 2));
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(cfg->size, input, output,
                                            FFTW_MEASURE | FFTW_PRESERVE_INPUT);

    const SizeType n_bins = cfg->size / 2;  // ditch the DC information
    FFTHistory history = fft_history_new(cfg->history_size, n_bins);

    OnePoleFilter dc_blocker =
        filter_init(cfg->dc_blocker_frequency, cfg->sample_rate);

    return (FFTAnalyzer){
        .cfg = *cfg,
        .plan = plan,
        .input = input,
        .output = output,
        .n_bins = n_bins,
        .history = history,
        .dc_blocker = dc_blocker,
        .rx = rx,
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
    const SizeType to_keep = analyzer->cfg.size - analyzer->cfg.stride;
    const SizeType to_read = analyzer->cfg.stride;

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
