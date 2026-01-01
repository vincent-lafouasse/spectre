#pragma once

#include <stdatomic.h>

// checks for portability

// i will always use u32 for size_t
// the pointer check is only for atomic pointers

_Static_assert(sizeof(int) == 4,
               "This codebase strictly requires 4-byte integers.");
_Static_assert(
    ATOMIC_INT_LOCK_FREE == 2,
    "32-bit operations are not natively lock-free on this platform.");
_Static_assert(ATOMIC_POINTER_LOCK_FREE == 2,
               "Pointer-sized operations are not natively lock-free.");
