#include "FFTAnalyzer.h"

#include <stdlib.h>
#include <string.h>

FFTAnalyzer fft_analyzer_new(const FFTConfig* cfg, LockFreeQueueConsumer rx)
{
    float* input = calloc(cfg->size, sizeof(float));
    kiss_fft_cpx* output = malloc((1 + cfg->size / 2) * sizeof(kiss_fft_cpx));
    kiss_fftr_cfg plan = kiss_fftr_alloc(cfg->size, 0, NULL, NULL);

    const SizeType n_bins = cfg->size / 2;  // ditch DC
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

    kiss_fftr_free(analyzer->plan);
    free(analyzer->input);
    free(analyzer->output);
    fft_history_free(&analyzer->history);
}

// returns number of frames pushed onto the history
SizeType fft_analyzer_update(FFTAnalyzer* analyzer)
{
    const SizeType to_keep = analyzer->cfg.size - analyzer->cfg.stride;
    const SizeType to_read = analyzer->cfg.stride;

    SizeType n = 0;
    while (clfq_pop(&analyzer->rx, analyzer->input + to_keep, to_read)) {
        // TODO: add a window

        // HPF the incoming slice
        filter_hpf_process(&analyzer->dc_blocker, analyzer->input + to_keep,
                           to_read);

        kiss_fftr(analyzer->plan, analyzer->input, analyzer->output);

        // ditch DC bin
        fft_history_push(&analyzer->history, (Complex*)(analyzer->output + 1));

        memmove(analyzer->input, analyzer->input + to_read,
                to_keep * sizeof(float));
        ++n;
    }

    return n;
}
