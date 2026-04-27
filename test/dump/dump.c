#include <stdio.h>
#include <stdlib.h>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

int main(int ac, char* av[])
{
    if (ac != 3) {
        fprintf(stderr, "Usage: dump <input audio> <output.pgm>\n");
        return 1;
    }

    const char* input_wav = av[1];
    const char* output_pgm = av[2];
    (void)output_pgm;

    drwav wav;
    if (!drwav_init_file(&wav, input_wav, NULL)) {
        fprintf(stderr, "dump: failed to open %s\n", input_wav);
        exit(1);
    }

    printf("ok\n");
    return 0;
}
