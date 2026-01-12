#pragma once

#include <stdint.h>

#define COLORMAP_SIZE 256

typedef const uint8_t (*const Colormap)[4];

extern const unsigned char viridis_rgba[256][4];
extern const unsigned char plasma_rgba[256][4];
extern const unsigned char inferno_rgba[256][4];
extern const unsigned char magma_rgba[256][4];
