#include "audio_callback.h"

static LockFreeQueueProducer* sample_tx = NULL;

void pass_sample_tx(LockFreeQueueProducer* sample_tx_passed)
{
    sample_tx = sample_tx_passed;
}

void pull_samples_from_audio_thread(void* buffer, unsigned int frames)
{
    if (sample_tx == NULL) {
        return;
    }

    (void)buffer;
    (void)frames;
}
