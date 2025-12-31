#pragma once

void make_hann_window(float* window, int N);
void apply_window(float* data, const float* window, int N);
