#pragma once

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
