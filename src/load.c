#include "load.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// This define tells stb_vorbis to create the actual function logic here.
// Only do this in ONE .c file in your entire project.
#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

MonoBuffer decodeOgg(const char* path)
{
    MonoBuffer buffer = {.data = NULL, .sampleRate = 0, .nFrames = 0};

    int channels, sample_rate;
    int16_t* output;
    int num_samples =
        stb_vorbis_decode_filename(path, &channels, &sample_rate, &output);

    if (num_samples <= 0) {
        return buffer;
    }

    buffer.data = (float*)malloc(sizeof(float) * num_samples);
    if (!buffer.data) {
        free(output);
        return buffer;
    }

    // select left channel
    for (int i = 0; i < num_samples; i++) {
        buffer.data[i] = output[i * channels] / (float)(1 << 15);
    }

    buffer.sampleRate = (size_t)sample_rate;
    buffer.nFrames = (size_t)num_samples;

    free(output);
    return buffer;
}
