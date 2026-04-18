#pragma once

#include "definitions.h"

void make_hann_window(float* window, SizeType N);
void apply_window(float* data, const float* window, SizeType N);
