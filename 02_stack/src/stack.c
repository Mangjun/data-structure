#include "_internal.h"

int push_impl(struct stack* const self, struct node* item)
{
    if (!_is_valid_stack(self) || !item) {
        return 1;
    }

    item->next = self->pimpl->top;
    self->pimpl->top = item;
    self->pimpl->size++;

    return 0;
}

struct node* pop_impl(struct stack* const self)
{
    if (!_is_valid_stack(self) || is_empty_impl(self)) {
        return NULL;
    }

    struct node* top = self->pimpl->top;
    self->pimpl->top = top->next;
    self->pimpl->size--;

    top->next = NULL;

    return top;
}

struct node* peek_impl(const struct stack* const self)
{
    if (!_is_valid_stack(self) || is_empty_impl(self)) {
        return NULL;
    }

    return self->pimpl->top;
}