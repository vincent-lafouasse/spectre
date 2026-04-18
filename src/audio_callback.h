#pragma once

#include "LockFreeQueue.h"

void init_audio_processor(LockFreeQueueProducer* sample_tx_passed);
void deinit_audio_processor(void);

void pull_samples_from_audio_thread(void* buffer, unsigned int frames);
