#include "History.h"

#include <stdlib.h>

FloatHistory fhistory_new(SizeType cap)
{
    float* data = malloc(cap * sizeof(*data));

    return (FloatHistory){
        .head = 0,
        .tail = 0,
        .len = 0,
        .cap = cap,
        .data = data,
    };
}

void fhistory_destroy(FloatHistory fh)
{
    free(fh.data);
}

void fhistory_push(FloatHistory* fh, float f)
{
    fh->data[fh->tail] = f;
    fh->tail = (fh->tail + 1) % fh->cap;

    if (fh->len == fh->cap) {
        fh->head = fh->tail;
    } else {
        fh->len += 1;
    }
}

SplitSlice fhistory_get(const FloatHistory* fh)
{
    // might be off the buffer
    const SizeType end = fh->head + fh->len;

    if (end <= fh->cap) {
        return (SplitSlice){
            .slice1 = fh->data + fh->head,
            .size1 = fh->len,
            .slice2 = NULL,
            .size2 = 0,
        };
    }

    const SizeType size1 = fh->cap - fh->head;
    const SizeType size2 = fh->len - size1;

    return (SplitSlice){
        .slice1 = fh->data + fh->head,
        .size1 = size1,
        .slice2 = fh->data,
        .size2 = size2,
    };
}
