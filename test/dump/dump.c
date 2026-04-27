#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

static bool str_ends_with(const char* s, const char* suffix)
{
    if (strlen(s) > strlen(suffix)) {
        return false;
    }

    const char* s_suffix = s + strlen(s) - strlen(suffix);

    return (strcmp(s_suffix, suffix) == 0);
}

int main(int ac, char* av[])
{
    if (ac != 3) {
        fprintf(stderr, "Usage: dump <input audio> <output.pgm>\n");
        return 1;
    }

    const char* input_wav = av[1];
    const char* output_pgm = av[2];
    (void)output_pgm;

    if (!str_ends_with(input_wav, ".wav")) {
        fprintf(stderr, "Only wav files are supported for now\n");
        return 1;
    }

    drwav wav;
    if (!drwav_init_file(&wav, input_wav, NULL)) {
        fprintf(stderr, "dump: failed to open %s\n", input_wav);
        exit(1);
    }

    printf("ok\n");
    return 0;
}
