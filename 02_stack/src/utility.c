#include "_internal.h"

#include <stdlib.h>

static const struct ops _ops = {
    .is_empty       = is_empty_impl,
    .size           = size_impl,
    .clear          = clear_impl,
    .push           = push_impl,
    .pop            = pop_impl,
    .peek           = peek_impl
};

int init_stack(struct stack* const self)
{
    if (!_is_valid_stack(self)) {
        return 1;
    }

    self->pimpl = (struct stack_internal*)malloc(sizeof(struct stack_internal));

    if (!self->pimpl) {
        return 1;
    }

    self->ops = &_ops;

    self->pimpl->top = NULL;
    self->pimpl->size = 0;

    return 0;
}

int destroy_stack(struct stack* const self, void (*free_func)(struct node*))
{
    if (!_is_valid_stack(self)) {
        return 1;
    }

    if (clear_impl(self, free_func)) {
        return 1;
    }

    free(self->pimpl);
    self->ops = NULL;

    return 0;
}

int is_empty_impl(const struct stack* const self)
{
    if (!_is_valid_stack(self)) {
        return 1;
    }

    return self->pimpl->size == 0; 
}

int size_impl(const struct stack* const self)
{
    if (!_is_valid_stack(self)) {
        return 0;
    }

    return self->pimpl->size;
}

int clear_impl(struct stack* const self, void (*free_func)(struct node*))
{
    if (!_is_valid_stack(self)) {
        return 1;
    }

    struct node* top = self->pimpl->top;
    struct node* next = NULL;
    while (top != NULL) {
        next = top->next;

        free_func(top);

        top = next;
    }

    self->pimpl->top = NULL;
    self->pimpl->size = 0;

    return 0;
}