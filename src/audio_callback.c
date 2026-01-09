#include "audio_callback.h"

static LockFreeQueueProducer* sample_tx = NULL;

void pull_samples_from_audio_thread(void *buffer, unsigned int frames)
{
    if (sample_tx == NULL) {
        return;
    }

    (void)buffer;
    (void)frames;
}
