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

int main(int ac, char* av[])
{
    if (ac != 2) {
        fprintf(stderr, "Usage: dump <input audio>\n");
        return 1;
    }

    const char* input_wav_path = av[1];

    if (!str_ends_with(input_wav_path, ".wav")) {
        fprintf(stderr, "Only wav files are supported for now\n");
        return 1;
    }

    drwav wav;
    if (!drwav_init_file(&wav, input_wav_path, NULL)) {
        fprintf(stderr, "drwav: failed to init wav decoder from file %s\n",
                input_wav_path);
        exit(1);
    }

    printf("ok\n");
    return 0;
}
