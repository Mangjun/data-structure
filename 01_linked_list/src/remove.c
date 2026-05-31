#include "_internal.h"

// helper function to check if the index is valid
static inline int _is_valid_index(const struct list* const self, const int index)
{
    return index >= 0 && index < self->pimpl->size;
}

struct node* remove_at_impl(struct list* const self, const int index)
{
    if (!_is_valid_list(self) || is_empty_impl(self) || !_is_valid_index(self, index)) {
        return NULL;
    }

    int mid = self->pimpl->size / 2;
    int cnt = 0;
    struct node* curr = &self->pimpl->host;

    if (index < mid) {
        cnt = index;
        for (int i = 0; i <= cnt; i++, curr = curr->next);
    }
    else {
        cnt = self->pimpl->size - index;
        for (int i = 0; i < cnt; i++, curr = curr->prev);
    }

    _remove(curr->prev, curr->next);

    curr->prev = NULL;
    curr->next = NULL;

    self->pimpl->size--;

    return curr;
}

struct node* remove_first_impl(struct list* const self)
{
    if (!_is_valid_list(self) || is_empty_impl(self)) {
        return NULL;
    }

    struct node* first_node = self->pimpl->host.next;

    _remove(first_node->prev, first_node->next);

    first_node->prev = NULL;
    first_node->next = NULL;

    self->pimpl->size--;

    return first_node;
}

struct node* remove_last_impl(struct list* const self)
{
    if (!_is_valid_list(self) || is_empty_impl(self)) {
        return NULL;
    }

    struct node* last_node = self->pimpl->host.prev;

    _remove(last_node->prev, last_node->next);

    last_node->prev = NULL;
    last_node->next = NULL;

    self->pimpl->size--;

    return last_node;
}