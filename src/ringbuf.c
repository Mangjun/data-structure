#include "ringbuf.h"
#include <limits.h>

/* ========================= ringbuf_t ========================= */

rb_status_t ringbuf_init(ringbuf_t *rb, rb_elem_t *buf, size_t size)
{
    if (rb == NULL || buf == NULL) {
        return RB_ERR_NULL;
    }

    if (size < 2 || size > UINT_MAX) {
        return RB_ERR_INVAL;
    }

    rb->buf = buf;
    rb->size = (rb_index_t)size;
    atomic_init(&rb->head, 0u);
    atomic_init(&rb->tail, 0u);

    return RB_OK;
}

rb_status_t ringbuf_push(ringbuf_t *rb, rb_elem_t value)
{
    if (rb == NULL) {
        return RB_ERR_NULL;
    }

    if (ringbuf_is_full(rb)) {
        return RB_ERR_FULL;
    }

    rb_index_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    rb->buf[head] = value;

    rb_index_t temp = head + 1u;

    if (temp >= rb->size) {
        temp = 0;
    }

    atomic_store_explicit(&rb->head, temp, memory_order_release);

    return RB_OK;
}

rb_status_t ringbuf_pop(ringbuf_t *rb, rb_elem_t *out)
{
    if (rb == NULL || out == NULL) {
        return RB_ERR_NULL;
    }

    if (ringbuf_is_empty(rb)) {
        return RB_ERR_EMPTY;
    }

    rb_index_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);

    *out = rb->buf[tail];

    rb_index_t temp = tail + 1u;

    if (temp >= rb->size) {
        temp = 0;
    }

    atomic_store_explicit(&rb->tail, temp, memory_order_release);

    return RB_OK;
}

rb_status_t ringbuf_peek(const ringbuf_t *rb, rb_elem_t *out)
{
    if (rb == NULL || out == NULL) {
        return RB_ERR_NULL;
    }

    if (ringbuf_is_empty(rb)) {
        return RB_ERR_EMPTY;
    }

    rb_index_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);

    *out = rb->buf[tail];

    return RB_OK;
}

rb_status_t ringbuf_reset(ringbuf_t *rb)
{
    if (rb == NULL) {
        return RB_ERR_NULL;
    }

    atomic_store_explicit(&rb->head, 0u, memory_order_relaxed);
    atomic_store_explicit(&rb->tail, 0u, memory_order_relaxed);

    return RB_OK;
}

size_t ringbuf_count(const ringbuf_t *rb)
{
    if (rb == NULL) {
        return 0;
    }

    rb_index_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    rb_index_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);

    if (head >= tail) {
        return head - tail;
    } else {
        return head + (rb->size - tail);
    }
}

size_t ringbuf_capacity(const ringbuf_t *rb)
{
    if (rb == NULL) {
        return 0;
    }

    return rb->size - 1;
}

bool ringbuf_is_empty(const ringbuf_t *rb)
{
    if (rb == NULL) {
        return true;
    }

    rb_index_t head = atomic_load_explicit(&rb->head, memory_order_acquire);
    rb_index_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);

    return head == tail;
}

bool ringbuf_is_full(const ringbuf_t *rb)
{
    if (rb == NULL) {
        return true;
    }

    rb_index_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    rb_index_t temp = head + 1u;

    if (temp >= rb->size) {
        temp = 0;
    }

    rb_index_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

    return temp == tail;
}

/* ======================== ringbuf_p2_t ======================== */

rb_status_t ringbuf_p2_init(ringbuf_p2_t *rb, rb_elem_t *buf, size_t size)
{
    if (rb == NULL || buf == NULL) {
        return RB_ERR_NULL;
    }

    if (size == 0 || size > UINT_MAX / 2 + 1 || (size & (size - 1)) != 0) {
        return RB_ERR_INVAL;
    }

    rb->buf = buf;
    rb->mask = (rb_index_t)(size - 1);
    atomic_init(&rb->head, 0u);
    atomic_init(&rb->tail, 0u);

    return RB_OK;
}

rb_status_t ringbuf_p2_push(ringbuf_p2_t *rb, rb_elem_t value)
{
    if (rb == NULL) {
        return RB_ERR_NULL;
    }

    if (ringbuf_p2_is_full(rb)) {
        return RB_ERR_FULL;
    }

    rb_index_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);

    rb->buf[head & rb->mask] = value;
    atomic_store_explicit(&rb->head, head + 1u, memory_order_release);

    return RB_OK;
}

rb_status_t ringbuf_p2_pop(ringbuf_p2_t *rb, rb_elem_t *out)
{
    if (rb == NULL || out == NULL) {
        return RB_ERR_NULL;
    }

    if (ringbuf_p2_is_empty(rb)) {
        return RB_ERR_EMPTY;
    }

    rb_index_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);

    *out = rb->buf[tail & rb->mask];
    atomic_store_explicit(&rb->tail, tail + 1u, memory_order_release);

    return RB_OK;
}

rb_status_t ringbuf_p2_peek(const ringbuf_p2_t *rb, rb_elem_t *out)
{
    if (rb == NULL || out == NULL) {
        return RB_ERR_NULL;
    }

    if (ringbuf_p2_is_empty(rb)) {
        return RB_ERR_EMPTY;
    }

    rb_index_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);

    *out = rb->buf[tail & rb->mask];

    return RB_OK;
}

rb_status_t ringbuf_p2_reset(ringbuf_p2_t *rb)
{
    if (rb == NULL) {
        return RB_ERR_NULL;
    }

    atomic_store_explicit(&rb->head, 0u, memory_order_relaxed);
    atomic_store_explicit(&rb->tail, 0u, memory_order_relaxed);

    return RB_OK;
}

size_t ringbuf_p2_count(const ringbuf_p2_t *rb)
{
    if (rb == NULL) {
        return 0;
    }

    rb_index_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    rb_index_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);

    return head - tail;
}

size_t ringbuf_p2_capacity(const ringbuf_p2_t *rb)
{
    if (rb == NULL) {
        return 0;
    }

    return rb->mask + 1u;
}

bool ringbuf_p2_is_empty(const ringbuf_p2_t *rb)
{
    if (rb == NULL) {
        return true;
    }

    rb_index_t head = atomic_load_explicit(&rb->head, memory_order_acquire);
    rb_index_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);

    return head == tail;
}

bool ringbuf_p2_is_full(const ringbuf_p2_t *rb)
{
    if (rb == NULL) {
        return true;
    }

    rb_index_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    rb_index_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

    return (head - tail) > rb->mask;
}