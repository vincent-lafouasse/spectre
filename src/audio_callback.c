#include "audio_callback.h"

#include <assert.h>

static LockFreeQueueProducer* sample_tx = NULL;

void pass_sample_tx(LockFreeQueueProducer* sample_tx_passed)
{
    sample_tx = sample_tx_passed;
}

void pull_samples_from_audio_thread(void* buffer, unsigned int frames)
{
    assert(sample_tx != NULL);

    (void)buffer;
    (void)frames;
}
