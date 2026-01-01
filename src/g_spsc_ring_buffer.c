#include "g_spsc_ring_buffer.h"

#include <stdatomic.h>
#include <string.h>

static float g_ring_buffer[BUFFER_SIZE];
static _Atomic size_t g_write_index = 0;
static _Atomic size_t g_read_index = 0;

size_t g_buffer_available(void)
{
    size_t w = atomic_load_explicit(&g_write_index, memory_order_acquire);
    size_t r = atomic_load_explicit(&g_read_index, memory_order_acquire);

    if (w >= r) {
        return w - r;
    } else {
        return (BUFFER_SIZE - r) + w;
    }
}

// consumer (fft thread)
// returns false if not enough data is available
bool g_buffer_pop(float* dest, size_t count)
{
    if (g_buffer_available() < count)
        return false;

    size_t r = atomic_load_explicit(&g_read_index, memory_order_relaxed);

    for (size_t i = 0; i < count; i++) {
        dest[i] = g_ring_buffer[r];
        r = (r + 1) & (BUFFER_SIZE - 1);
    }

    atomic_store_explicit(&g_read_index, r, memory_order_release);
    return true;
}

// producer (audio thread)
void g_buffer_push_block(const float* samples, size_t count)
{
    const size_t w = atomic_load_explicit(&g_write_index, memory_order_relaxed);
    const size_t available = g_buffer_available();

    if (count > available) {
        count = available;
    }

    // check if 1 or 2 memcpy are needed
    size_t first_part = BUFFER_SIZE - w;
    if (count <= first_part) {
        memcpy(&g_ring_buffer[w], samples, count * sizeof(float));
    } else {
        memcpy(&g_ring_buffer[w], samples, first_part * sizeof(float));
        memcpy(&g_ring_buffer[0], samples + first_part,
               (count - first_part) * sizeof(float));
    }

    atomic_store_explicit(&g_write_index, (w + count) & (BUFFER_SIZE - 1),
                          memory_order_release);
}
