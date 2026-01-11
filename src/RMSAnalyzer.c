#include "RMSAnalyzer.h"

#include <math.h>
#include <string.h>

static float compute_rms(const float* restrict data, SizeType size)
{
    float sum = 0;

    for (SizeType i = 0; i < size; i++) {
        sum += data[i] * data[i];
    }

    return sqrtf(sum / (float)size);
}

RMSAnalyzer rms_analyzer_new(LockFreeQueueConsumer sample_rx)
{
    return (RMSAnalyzer){
        .buffer = {0},
        .size = RMS_SIZE,
        .stride = RMS_STRIDE,
        .rx = sample_rx,
        .history = fhistory_new(HISTORY_SIZE),
    };
}

void rms_analyzer_destroy(RMSAnalyzer* analyzer)
{
    if (!analyzer) {
        return;
    }

    fhistory_destroy(analyzer->history);
}

// returns number of elements pushed onto its history
SizeType rms_analyzer_update(RMSAnalyzer* analyzer)
{
    // stride is the number of sample we discard and also the number of new
    // samples we need to read
    const SizeType to_keep = analyzer->size - analyzer->stride;
    const SizeType to_read = analyzer->stride;

    SizeType n = 0;
    while (clfq_pop(&analyzer->rx, analyzer->buffer + to_keep, to_read)) {
        const float rms_value = compute_rms(analyzer->buffer, analyzer->size);
        fhistory_push(&analyzer->history, rms_value);
        // ditch the first to_read samples
        memmove(analyzer->buffer, analyzer->buffer + to_read, to_keep);
        ++n;
    }

    return n;
}
