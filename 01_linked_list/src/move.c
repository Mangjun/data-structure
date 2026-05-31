#include "_internal.h"

// helper function to check if the index is valid
static inline int _is_valid_index(const struct list* const self, const int index)
{
    return index >= 0 && index < self->pimpl->size;
}

int move_impl(struct list* const self, const int index, struct node* const node)
{
    if (!_is_valid_list(self) || !_is_valid_index(self, index)) {
        return 1;
    }

    _remove(node->prev, node->next);
    self->pimpl->size--;

    int target_idx = index;
    if (target_idx > self->pimpl->size) {
        target_idx = self->pimpl->size;
    }

    int mid = self->pimpl->size / 2;
    int cnt = 0;
    struct node* curr = &self->pimpl->host;

    if (target_idx < mid) {
        cnt = target_idx;
        for (int i = 0; i < cnt; i++, curr = curr->next);
    }
    else {
        cnt = self->pimpl->size - target_idx;
        for (int i = 0; i <= cnt; i++, curr = curr->prev);
    }

    _insert(curr, curr->next, node);
    self->pimpl->size++;

    return 0;
}

int move_first_impl(struct list* const self, struct node* const node)
{
    if (!_is_valid_list(self)) {
        return 1;
    }

    struct node* first_node = self->pimpl->host.next;

    if (node == first_node) {
        return 0;
    }

    _remove(node->prev, node->next);
    _insert(first_node->prev, first_node, node);

    return 0;
}

int move_last_impl(struct list* const self, struct node* const node)
{
    if (!_is_valid_list(self)) {
        return 1;
    }

    struct node* last_node = self->pimpl->host.prev;

    if (node == last_node) {
        return 0;
    }

    _remove(node->prev, node->next);
    _insert(last_node, last_node->next, node);

    return 0;
}