#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

static bool str_ends_with(const char* s, const char* suffix)
{
    if (strlen(suffix) > strlen(s)) {
        return false;
    }

    const char* s_suffix = s + strlen(s) - strlen(suffix);

    return (strcmp(s_suffix, suffix) == 0);
}

struct MonoAudioBuffer {
    float* samples;
    uint32_t sample_rate;
    uint64_t size;
};

// sloppy ressource management; shouldn't matter here
static struct MonoAudioBuffer decode_wav_or_exit(const char* path)
{
    if (!str_ends_with(path, ".wav")) {
        fprintf(stderr, "Not a wav file\n");
        exit(1);
    }

    drwav decoder;
    if (!drwav_init_file(&decoder, path, NULL)) {
        fprintf(stderr, "drwav: failed to init wav decoder from file %s\n",
                path);
        exit(1);
    }

    uint32_t channels = decoder.channels;
    uint64_t frames = decoder.totalPCMFrameCount;
    if (frames == 0) {
        fprintf(stderr, "empty wav file\n");
        exit(1);
    }
    if (channels == 0) {
        fprintf(stderr, "invalid channel layout: somehow no channels\n");
        exit(1);
    }

    float* interleaved = calloc(channels * frames, sizeof(float));
    if (interleaved == NULL) {
        fprintf(stderr, "oom\n");
        exit(1);
    }
    drwav_read_pcm_frames_f32(&decoder, frames, interleaved);
    drwav_uninit(&decoder);

    float* mono = calloc(frames, sizeof(float));
    if (mono == NULL) {
        fprintf(stderr, "oom\n");
        exit(1);
    }

    const float gain = 1.0f / (float)channels;
    for (uint64_t i = 0; i < frames; ++i) {
        float sample = 0.0f;
        for (uint32_t c = 0; c < channels; ++c) {
            sample += interleaved[c * channels + i];
        }

        mono[i] = gain * sample;
    }
    free(interleaved);

    return (struct MonoAudioBuffer){
        .samples = mono,
        .sample_rate = decoder.sampleRate,
        .size = decoder.totalPCMFrameCount,
    };
}

int main(int ac, char* av[])
{
    if (ac != 2) {
        fprintf(stderr, "Usage: dump <input audio>\n");
        return 1;
    }

    const char* input = av[1];

    struct MonoAudioBuffer audio = decode_wav_or_exit(input);
    (void)audio;

    printf("ok\n");
    return 0;
}
