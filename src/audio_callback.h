#pragma once

#include "LockFreeQueue.h"

void pass_sample_tx(LockFreeQueueProducer* sample_tx);
void pull_samples_from_audio_thread(void* buffer, unsigned int frames);
