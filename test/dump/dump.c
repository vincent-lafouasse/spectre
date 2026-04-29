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
    float sample_rate;
    size_t size;
};

static struct MonoAudioBuffer decode_wav_or_exit(const char* path)
{
    if (!str_ends_with(path, ".wav")) {
        fprintf(stderr, "Not a wav file\n");
        exit(1);
    }

    struct MonoAudioBuffer out = {0};

    drwav decoder;
    if (!drwav_init_file(&decoder, path, NULL)) {
        fprintf(stderr, "drwav: failed to init wav decoder from file %s\n",
                path);
        exit(1);
    }

    drwav_uninit(&decoder);

    return out;
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
