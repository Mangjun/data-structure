/*
 * list_v1.c - 구현은 직접 채운다.
 * 현재는 모든 함수가 실패를 반환하는 스텁이다.
 */
#include "list_v1.h"

list1_status_t list1_init(list1_t *l, list1_node_t *nodes, size_t cap)
{
    (void)l;
    (void)nodes;
    (void)cap;
    /* TODO */
    return LIST1_ERR_INVAL;
}

list1_status_t list1_clear(list1_t *l)
{
    (void)l;
    /* TODO */
    return LIST1_ERR_INVAL;
}

list1_status_t list1_push_front(list1_t *l, list1_elem_t value)
{
    (void)l;
    (void)value;
    /* TODO */
    return LIST1_ERR_INVAL;
}

list1_status_t list1_push_back(list1_t *l, list1_elem_t value)
{
    (void)l;
    (void)value;
    /* TODO */
    return LIST1_ERR_INVAL;
}

list1_status_t list1_pop_front(list1_t *l, list1_elem_t *out)
{
    (void)l;
    (void)out;
    /* TODO */
    return LIST1_ERR_INVAL;
}

list1_status_t list1_insert_after(list1_t *l, list1_idx_t idx, list1_elem_t value)
{
    (void)l;
    (void)idx;
    (void)value;
    /* TODO */
    return LIST1_ERR_INVAL;
}

list1_status_t list1_remove(list1_t *l, list1_elem_t value)
{
    (void)l;
    (void)value;
    /* TODO */
    return LIST1_ERR_INVAL;
}

list1_idx_t list1_find(const list1_t *l, list1_elem_t value)
{
    (void)l;
    (void)value;
    /* TODO */
    return LIST1_NIL;
}

list1_idx_t list1_first(const list1_t *l)
{
    (void)l;
    /* TODO */
    return LIST1_NIL;
}

list1_idx_t list1_next(const list1_t *l, list1_idx_t idx)
{
    (void)l;
    (void)idx;
    /* TODO */
    return LIST1_NIL;
}

list1_status_t list1_get(const list1_t *l, list1_idx_t idx, list1_elem_t *out)
{
    (void)l;
    (void)idx;
    (void)out;
    /* TODO */
    return LIST1_ERR_INVAL;
}

size_t list1_size(const list1_t *l)
{
    (void)l;
    /* TODO */
    return 0;
}

size_t list1_capacity(const list1_t *l)
{
    (void)l;
    /* TODO */
    return 0;
}

bool list1_is_empty(const list1_t *l)
{
    (void)l;
    /* TODO */
    return false;
}

bool list1_is_full(const list1_t *l)
{
    (void)l;
    /* TODO */
    return false;
}
