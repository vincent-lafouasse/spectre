#pragma once

#include "core/History.h"
#include "core/LockFreeQueue.h"

#define RMS_SIZE 1024
#define RMS_STRIDE (RMS_SIZE / 2)

typedef struct {
    float buffer[RMS_SIZE];
    SizeType size;
    SizeType stride;
    LockFreeQueueConsumer rx;
    FloatHistory history;
} RMSAnalyzer;

RMSAnalyzer rms_analyzer_new(LockFreeQueueConsumer sample_rx);
void rms_analyzer_destroy(RMSAnalyzer* analyzer);

// returns number of elements pushed onto its history
SizeType rms_analyzer_update(RMSAnalyzer* analyzer);
