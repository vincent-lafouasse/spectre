#pragma once

#include <stdatomic.h>
#include <stdint.h>

// checks for portability

// i will always use u32 for size_t
// the pointer check is only for atomic pointers

_Static_assert(sizeof(int) == 4,
               "This codebase strictly requires 4-byte integers.");
_Static_assert(
    ATOMIC_INT_LOCK_FREE == 2,
    "32-bit operations are not natively lock-free on this platform.");
// now that we are assured that u32 are always lock-free
typedef uint32_t SizeType;

_Static_assert(ATOMIC_POINTER_LOCK_FREE == 2,
               "Pointer-sized operations are not natively lock-free.");
