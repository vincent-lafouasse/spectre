#include "LockFreeQueue.h"

#include <stdatomic.h>

// -------------------- Shared storage --------------------
void clfq_new(LockFreeQueue* clfq)
{
    atomic_init(&clfq->front, 0);
    atomic_init(&clfq->back, 0);
}

// -------------------- Producer API --------------------
LockFreeQueueProducer clfq_producer(LockFreeQueue* restrict clfq)
{
    return (LockFreeQueueProducer){.back = &clfq->back,
                                   .front = &clfq->front,
                                   .cached_front = 0,
                                   .data = clfq->data};
}

static size_t distance(size_t front, size_t back)
{
    // scary underflow is actualy fine and expected behaviour
    // expects the queue size to be a power of 2
    return (back - front) & (CLF_QUEUE_SIZE - 1);
}

size_t clfq_producer_size_lazy(const LockFreeQueueProducer* restrict producer)
{
    const size_t back =
        atomic_load_explicit(producer->back, memory_order_relaxed);
    return distance(back, producer->cached_front + CLF_QUEUE_SIZE);
}

size_t clfq_producer_size_eager(LockFreeQueueProducer* restrict producer)
{
    producer->cached_front =
        atomic_load_explicit(producer->front, memory_order_acquire);
    return clfq_producer_size_lazy(producer);
}

bool clfq_push(LockFreeQueueProducer* restrict producer,
               const float* restrict elems,
               size_t n)
{
    // check pessimistically, if it's fine do not reload `front`
    // we consider relaxed loading of private `back` free
    if (clfq_producer_size_lazy(producer) < n &&
        clfq_producer_size_eager(producer) < n) {
        return false;
    }

    // Producer is sole writer so there is no contention on `back`
    const size_t back =
        atomic_load_explicit(producer->back, memory_order_relaxed);

    for (size_t i = 0; i < n; ++i) {
        producer->data[(back + i) & (CLF_QUEUE_SIZE - 1)] = elems[i];
    }

    // publish/commit
    atomic_store_explicit(producer->back, back + n, memory_order_release);
    return true;
}

static size_t min(size_t a, size_t b)
{
    return a < b ? a : b;
}

size_t clfq_push_partial(LockFreeQueueProducer* restrict producer,
                         const float* restrict elems,
                         size_t n,
                         size_t frame_size)
{
    const size_t available = clfq_producer_size_eager(producer);
    const size_t maximum_n = min(n, available);
    const size_t actual_n = maximum_n - (maximum_n % frame_size);

    if (actual_n == 0) {
        return 0;
    }

    // will not trigger unnecessary loads as cached_front is fresh and big
    // enough
    const bool success = clfq_push(producer, elems, actual_n);
    return success ? actual_n : 0;
}

// -------------------- Consumer API --------------------
LockFreeQueueConsumer clfq_consumer(LockFreeQueue* restrict clfq)
{
    return (LockFreeQueueConsumer){
        .front = &clfq->front,
        .back = &clfq->back,
        .cached_back = 0,
        .data = clfq->data,
    };
}

size_t clfq_consumer_size_lazy(const LockFreeQueueConsumer* restrict consumer)
{
    const size_t front =
        atomic_load_explicit(consumer->front, memory_order_relaxed);

    return distance(front, consumer->cached_back);
}

size_t clfq_consumer_size_eager(LockFreeQueueConsumer* restrict consumer)
{
    consumer->cached_back =
        atomic_load_explicit(consumer->back, memory_order_acquire);
    return clfq_consumer_size_lazy(consumer);
}

bool clfq_pop(LockFreeQueueConsumer* restrict consumer,
              float* restrict elems,
              size_t n)
{
    if (clfq_consumer_size_lazy(consumer) < n &&
        clfq_consumer_size_eager(consumer) < n) {
        return false;
    }

    const size_t front =
        atomic_load_explicit(consumer->front, memory_order_relaxed);

    for (size_t i = 0; i < n; i++) {
        elems[i] = consumer->data[(front + i) & (CLF_QUEUE_SIZE - 1)];
    }

    atomic_store_explicit(consumer->front, front + n, memory_order_release);
    return true;
}

size_t clfq_pop_partial(LockFreeQueueConsumer* restrict consumer,
                        float* restrict elems,
                        size_t n,
                        size_t frame_size)
{
    const size_t available = clfq_consumer_size_eager(consumer);
    const size_t maximum_n = min(n, available);
    const size_t actual_n = maximum_n - (maximum_n % frame_size);

    if (actual_n == 0) {
        return 0;
    }

    const bool success = clfq_pop(consumer, elems, actual_n);
    return success ? actual_n : 0;
}

size_t clfq_consumer_peek_lazy(const LockFreeQueueConsumer* restrict consumer,
                               const float** restrict ptr)
{
    const size_t available = clfq_consumer_size_lazy(consumer);
    const size_t front =
        atomic_load_explicit(consumer->front, memory_order_relaxed) &
        (CLF_QUEUE_SIZE - 1);
    const size_t until_buffer_end = CLF_QUEUE_SIZE - front;
    const size_t actual_n = min(available, until_buffer_end);

    if (actual_n == 0) {
        *ptr = NULL;
        return 0;
    }

    *ptr = consumer->data + front;
    return actual_n;
}

// same but with a fresh cache
size_t clfq_consumer_peek_eager(LockFreeQueueConsumer* restrict consumer,
                                const float** restrict ptr)
{
    consumer->cached_back =
        atomic_load_explicit(consumer->back, memory_order_acquire);
    return clfq_consumer_peek_lazy(consumer, ptr);
}

// no checks, good luck
void clfq_consumer_skip(LockFreeQueueConsumer* restrict consumer, size_t n)
{
    const size_t front =
        atomic_load_explicit(consumer->front, memory_order_relaxed);
    atomic_store_explicit(consumer->front, front + n, memory_order_release);
}
