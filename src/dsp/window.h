#pragma once

#include "core/definitions.h"

void window_make_hann(float* window, SizeType N);

void window_apply(float* data, const float* window, SizeType N);

float window_power_reference(const float* window, SizeType N);
