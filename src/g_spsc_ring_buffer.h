#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "definitions.h"
// check that atomic u32s are lock-free
#include "platform_check.h"

#define BUFFER_SIZE (8 * FFT_SIZE)

// a global spsc ring buffer meant to transport data from the audio thread to
// the fft thread
//
// lock-less, wait-free
//
// underrun/overrun behaviour tbd

// consumer (fft thread)
// returns false if not enough data is available
bool g_buffer_pop(float* dest, size_t count);

// producer (audio thread)
void g_buffer_push_block(const float* samples, size_t count);

size_t g_buffer_available(void);
