#pragma once

// common definitions that need to be sync'd
// e.g. global ring buffer size depends on FFT size
// but the ring buffer should NOT include the FFT module

#define FFT_SIZE 1024

// 16k floats == 64KB, brutal but kinda needed for audio
// this will be on the heap obviously
#define CLF_QUEUE_SIZE (16 * FFT_SIZE)
