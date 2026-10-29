#include "stack.h"

stack_status_t stack_init(ds_stack_t *s, stack_elem_t *buf, size_t cap)
{
    if (s == NULL || buf == NULL) {
        return STACK_ERR_NULL;
    }

    if (cap == 0) {
        return STACK_ERR_INVAL;
    }

    s->buf = buf;
    s->cap = cap;
    s->top = 0;

    return STACK_OK;
}

stack_status_t stack_push(ds_stack_t *s, stack_elem_t value)
{
    if (s == NULL) {
        return STACK_ERR_NULL;
    }

    if (stack_is_full(s)) {
        return STACK_ERR_FULL;
    }

    s->buf[(s->top)++] = value;

    return STACK_OK;
}

stack_status_t stack_pop(ds_stack_t *s, stack_elem_t *out)
{
    if (s == NULL || out == NULL) {
        return STACK_ERR_NULL;
    }

    if (stack_is_empty(s)) {
        return STACK_ERR_EMPTY;
    }

    *out = s->buf[--(s->top)];

    return STACK_OK;
}

stack_status_t stack_peek(const ds_stack_t *s, stack_elem_t *out)
{
    if (s == NULL || out == NULL) {
        return STACK_ERR_NULL;
    }

    if (stack_is_empty(s)) {
        return STACK_ERR_EMPTY;
    }

    *out = s->buf[(s->top) - 1];

    return STACK_OK;
}

stack_status_t stack_clear(ds_stack_t *s)
{
    if (s == NULL) {
        return STACK_ERR_NULL;
    }

    s->top = 0;

    return STACK_OK;
}

size_t stack_size(const ds_stack_t *s)
{
    if (s == NULL) {
        return 0;
    }

    return s->top;
}

size_t stack_capacity(const ds_stack_t *s)
{
    if (s == NULL) {
        return 0;
    }

    return s->cap;
}

bool stack_is_empty(const ds_stack_t *s)
{
    if (s == NULL) {
        return true;
    }

    return s->top == 0;
}

bool stack_is_full(const ds_stack_t *s)
{
    if (s == NULL) {
        return true;
    }

    return s->cap <= s->top;
}