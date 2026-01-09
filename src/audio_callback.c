#include "audio_callback.h"

#include <assert.h>
#include "LockFreeQueue.h"

static LockFreeQueueProducer* sample_tx = NULL;

void pass_sample_tx(LockFreeQueueProducer* sample_tx_passed)
{
    sample_tx = sample_tx_passed;
}

// always interleaved stereo
void pull_samples_from_audio_thread(void* buffer, unsigned int frames)
{
    assert(sample_tx != NULL);

    const float* samples = (const float*)buffer;
    (void)clfq_push_partial(sample_tx, samples, 2 * frames, 2);
}
