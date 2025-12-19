#pragma once

#include <stddef.h>

typedef struct {
	float* data;
	size_t sampleRate;
	size_t nFrames;
} MonoBuffer;

MonoBuffer decodeOgg(const char* path);
