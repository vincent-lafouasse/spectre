#include "audio_callback.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "LockFreeQueue.h"

#define MONO_BUFFER_SIZE 1024

float mono_buffer[MONO_BUFFER_SIZE];

static _Atomic(LockFreeQueueProducer*) s_sample_tx = NULL;

void init_audio_processor(LockFreeQueueProducer* sample_tx_passed)
{
    atomic_store_explicit(&s_sample_tx, sample_tx_passed, memory_order_release);
}

void deinit_audio_processor(void) {}

// always interleaved stereo
void pull_samples_from_audio_thread(void* buffer, unsigned int frames)
{
    LockFreeQueueProducer* sample_tx =
        atomic_load_explicit(&s_sample_tx, memory_order_acquire);
    assert(sample_tx != NULL);

    const float* samples = (const float*)buffer;

    SizeType start = 0;
    while (frames != 0) {
        const SizeType to_pull =
            frames <= MONO_BUFFER_SIZE ? frames : MONO_BUFFER_SIZE;

        for (SizeType i = 0; i < to_pull; i++) {
            mono_buffer[i] =
                0.5f * (samples[start + 2 * i] + samples[start + 2 * i + 1]);
        }

        const SizeType transmitted =
            clfq_push_partial(sample_tx, mono_buffer, to_pull, 1);
        if (transmitted < to_pull) {
            break;
        }
        start += 2 * transmitted;
        frames -= transmitted;
    }
}
