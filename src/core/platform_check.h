#pragma once

#include <stdatomic.h>
#include <stdint.h>

// checks for portability

// (int is i32) && (atomic<int> is always lock-free)
//   => atomic<i32> is always lock-free
//   => atomic<u32> is always lock-free
//   => u32 is a suitable size type

_Static_assert(sizeof(int) == 4,
               "This codebase strictly requires 4-byte integers.");
_Static_assert(
    ATOMIC_INT_LOCK_FREE == 2,
    "32-bit operations are not natively lock-free on this platform.");

// if the above assertions passed, then u32 is a suitable size type
typedef uint32_t SizeType;

// atomic pointers being lock-free is nice for CAS operations but i don't
// actually need that so i won't run this assertion
/*
_Static_assert(ATOMIC_POINTER_LOCK_FREE == 2,
               "Pointer-sized operations are not natively lock-free.");
*/
