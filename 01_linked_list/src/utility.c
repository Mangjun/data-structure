#include "_internal.h"

#include <stdio.h>
#include <stdlib.h>

static const struct ops _ops = {
    .is_empty           = is_empty_impl,
    .size               = size_impl,
    .clear              = clear_impl,
    .find               = find_impl,
    .at                 = at_impl,
    .for_each           = for_each_impl,
    .insert_at          = insert_at_impl,
    .insert_first       = insert_first_impl,
    .insert_last        = insert_last_impl,
    .remove_at          = remove_at_impl,
    .remove_first       = remove_first_impl,
    .remove_last        = remove_last_impl,
    .move               = move_impl,
    .move_first         = move_first_impl,
    .move_last          = move_last_impl
};

int init_list(struct list* const self)
{
    if (!_is_valid_list(self)) {
        return 1;
    }

    self->pimpl = (struct list_internal*)malloc(sizeof(struct list_internal));

    if (self->pimpl == NULL) {
        return 1;
    }

    self->ops = &_ops;

    self->pimpl->host.prev = &self->pimpl->host;
    self->pimpl->host.next = &self->pimpl->host;
    self->pimpl->size = 0;

    return 0;
}

int destroy_list(struct list* const self, void (*free_func)(struct node*))
{
    if (!_is_valid_list(self)) {
        return 1;
    }

    if (clear_impl(self, free_func)) {
        perror("clear_impl");
        return 1;
    }

    free(self->pimpl);
    self->pimpl = NULL;
    self->ops = NULL;

    return 0;
}

int is_empty_impl(const struct list* const self)
{
    if (!_is_valid_list(self)) {
        return 1;
    }
    
    return self->pimpl->size == 0;
}

int size_impl(const struct list* const self)
{
    if (!_is_valid_list(self)) {
        return 0;
    }
    
    return self->pimpl->size;
}

int clear_impl(struct list* const self, void (*free_func)(struct node*))
{
    if (!_is_valid_list(self)) {
        return 1;
    }

    if (is_empty_impl(self)) {
        return 0;
    }    
    
    struct node* curr = self->pimpl->host.next;
    struct node* next = curr;

    while (curr != &self->pimpl->host) {
        next = curr->next;

        free_func(curr);

        curr = next;
    }

    self->pimpl->host.prev = &self->pimpl->host;
    self->pimpl->host.next = &self->pimpl->host;
    self->pimpl->size = 0;

    return 0;
}

struct node* find_impl(const struct list* const self, int (*compare)(struct node*, void*), void* arg)
{
    if (!_is_valid_list(self)) {
        return NULL;
    }

    struct node* curr = self->pimpl->host.next;
    while (curr != &self->pimpl->host) {
        if (compare(curr, arg)) {
            return curr;
        }
        curr = curr->next;
    }

    return NULL;
}

// helper function to check if the index is valid
static inline int _is_vaild_index(const struct list* const self, const int index)
{
    return index >= 0 && index < self->pimpl->size;
}

struct node* at_impl(const struct list* const self, const int index)
{
    if (!_is_valid_list(self) || !_is_vaild_index(self, index)) {
        return NULL;
    }

    struct node* curr = self->pimpl->host.next;
    for (int i = 0; i < index; i++, curr = curr->next);

    return curr;
}

void for_each_impl(const struct list* const self, void (*print_func)(struct node*))
{
    if (!_is_valid_list(self) || is_empty_impl(self)) {
        return;
    }

    struct node* curr = self->pimpl->host.next;
    while (curr != &self->pimpl->host) {
        print_func(curr);
        curr = curr->next;
    }
}