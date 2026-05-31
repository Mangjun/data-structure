#include "_internal.h"

#include <stdlib.h>

static int is_empty_impl(const struct queue* const self);
static int size_impl(const struct queue* const self);
static int clear_impl(struct queue* const self, void (*free_func)(struct node*));
static int enqueue_impl(struct queue* const self, struct node* const item);
static struct node* dequeue_impl(struct queue* const self);
static struct node* peek_impl(const struct queue* const self);  

static const struct ops _ops = {
    .is_empty   = is_empty_impl,
    .size       = size_impl,
    .clear      = clear_impl,
    .enqueue    = enqueue_impl,
    .dequeue    = dequeue_impl,
    .peek       = peek_impl
};

// helper function to check if the queue is valid (not NULL)
static inline int _is_valid_queue(const struct queue* const self)
{
    return self != NULL && self->pimpl != NULL;
}

int init_queue(struct queue* const self)
{
    if (!self) {
        return 1;
    }

    self->pimpl = (struct queue_internal*)malloc(sizeof(struct queue_internal));

    if (!self->pimpl) {
        return 1;
    }

    self->ops = &_ops;

    self->pimpl->front = NULL;
    self->pimpl->rear = NULL;
    self->pimpl->size = 0;

    return 0;
}

int destroy_queue(struct queue* const self, void (*free_func)(struct node*))
{
    if (!_is_valid_queue(self)) {
        return 1;
    }

    if (clear_impl(self, free_func)) {
        return 1;
    }

    free(self->pimpl);
    self->pimpl = NULL;
    self->ops = NULL;

    return 0;
}
    
static int is_empty_impl(const struct queue* const self)
{
    if (!_is_valid_queue(self)) {
        return 1;
    }

    return self->pimpl->size == 0;
}

static int size_impl(const struct queue* const self)
{
    if (!_is_valid_queue(self)) {
        return 0;
    }

    return self->pimpl->size;
}

static int clear_impl(struct queue* const self, void (*free_func)(struct node*))
{
    if (!_is_valid_queue(self)) {
        return 1;
    }

    struct node* front_node = self->pimpl->front;
    struct node* next;

    while (front_node != NULL) {
        next = front_node->next;

        if (free_func) {
            free_func(front_node);
        }

        front_node = next;
    }

    self->pimpl->front = NULL;
    self->pimpl->rear = NULL;
    self->pimpl->size = 0;

    return 0;
}

static int enqueue_impl(struct queue* const self, struct node* const item)
{
    if (!_is_valid_queue(self) || !item) {
        return 1;
    }

    item->next = NULL;

    if (!self->pimpl->rear) {
        self->pimpl->front = item;
        self->pimpl->rear = item;
    }
    else {
        self->pimpl->rear->next = item;
        self->pimpl->rear = item;
    }

    self->pimpl->size++;

    return 0;
}

static struct node* dequeue_impl(struct queue* const self)
{
    if (!_is_valid_queue(self) || is_empty_impl(self)) {
        return NULL;
    }

    struct node* front_node = self->pimpl->front;
    self->pimpl->front = front_node->next;
    self->pimpl->size--;

    if (self->pimpl->front == NULL) {
        self->pimpl->rear = NULL;
    }

    front_node->next = NULL;

    return front_node;
}

static struct node* peek_impl(const struct queue* const self)
{
    if (!_is_valid_queue(self) || is_empty_impl(self)) {
        return NULL;
    }

    return self->pimpl->front;
}