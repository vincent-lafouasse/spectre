#pragma once

#include <stdint.h>

#define COLORMAP_SIZE 256

typedef const uint8_t (*const Colormap)[4];

extern const uint8_t viridis_rgba[256][4];
extern const uint8_t plasma_rgba[256][4];
extern const uint8_t inferno_rgba[256][4];
extern const uint8_t magma_rgba[256][4];
