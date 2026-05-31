#include "_internal.h"

// helper function to check if the index is valid
static inline int _is_valid_index(const struct list* const self, const int index)
{
    return index >= 0 && index <= self->pimpl->size;
}

int insert_at_impl(struct list* const self, const int index, struct node* const node)
{
    if (!_is_valid_list(self) || !_is_valid_index(self, index)) {
        return 1;
    }

    int mid = self->pimpl->size / 2;
    int cnt = 0;
    struct node* curr = &self->pimpl->host;

    if (index < mid) {
        cnt = index;
        for (int i = 0; i < cnt; i++, curr = curr->next);
    }
    else {
        cnt = self->pimpl->size - index;
        for (int i = 0; i <= cnt; i++, curr = curr->prev);
    }

    _insert(curr, curr->next, node);

    self->pimpl->size++;

    return 0;
}

int insert_first_impl(struct list* const self, struct node* const node)
{
    if (!_is_valid_list(self)) {
        return 1;
    }

    struct node* host = &self->pimpl->host;

    _insert(host, host->next, node);

    self->pimpl->size++;

    return 0;
}

int insert_last_impl(struct list* const self, struct node* const node)
{
    if (!_is_valid_list(self)) {
        return 1;
    }

    struct node* host = &self->pimpl->host;

    _insert(host->prev, host, node);

    self->pimpl->size++;

    return 0;
}