#pragma once

#include <stdbool.h>

#include "definitions.h"

typedef struct {
    SizeType head;  // always point to the oldest sample
    SizeType tail;  // the next position to write to. can be equal to head
    SizeType len;
    SizeType cap;
    float* data;
} FloatHistory;

FloatHistory fhistory_new(SizeType cap);
void fhistory_destroy(FloatHistory fh);

void fhistory_push(FloatHistory* fh, float f);
SplitSlice fhistory_get(const FloatHistory* fh);

typedef struct {
    SizeType head;  // always point to the oldest sample
    SizeType tail;  // the next position to write to. can be equal to head
    SizeType len;
    SizeType cap;
    SizeType n_bins;
    Complex* data;  // n_bins * cap flattened
} FFTHistory;

FFTHistory fft_history_new(SizeType cap, SizeType n_bins);
bool fft_history_ok(const FFTHistory* fft_history);
void fft_history_free(FFTHistory* fft_history);
void fft_history_push(FFTHistory* fh, const Complex* row);
const Complex* fft_history_get_row(const FFTHistory* fh, SizeType i);
