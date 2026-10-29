/*
 * list_v2.c - 구현은 직접 채운다.
 * 현재는 모든 함수가 실패를 반환하는 스텁이다.
 */
#include "list_v2.h"

list2_status_t list2_init(list2_t *l, pool_t *pool)
{
    (void)l;
    (void)pool;
    /* TODO */
    return LIST2_ERR_INVAL;
}

list2_status_t list2_push_front(list2_t *l, list2_elem_t value)
{
    (void)l;
    (void)value;
    /* TODO */
    return LIST2_ERR_INVAL;
}

list2_status_t list2_push_back(list2_t *l, list2_elem_t value)
{
    (void)l;
    (void)value;
    /* TODO */
    return LIST2_ERR_INVAL;
}

list2_status_t list2_pop_front(list2_t *l, list2_elem_t *out)
{
    (void)l;
    (void)out;
    /* TODO */
    return LIST2_ERR_INVAL;
}

list2_status_t list2_remove(list2_t *l, list2_elem_t value)
{
    (void)l;
    (void)value;
    /* TODO: 이중 포인터 */
    return LIST2_ERR_INVAL;
}

list2_status_t list2_remove_if(list2_t *l, list2_pred_t pred, void *ctx, size_t *removed)
{
    (void)l;
    (void)pred;
    (void)ctx;
    (void)removed;
    /* TODO: 이중 포인터 */
    return LIST2_ERR_INVAL;
}

list2_status_t list2_clear(list2_t *l)
{
    (void)l;
    /* TODO */
    return LIST2_ERR_INVAL;
}

const list2_node_t *list2_first(const list2_t *l)
{
    (void)l;
    /* TODO */
    return NULL;
}

const list2_node_t *list2_next(const list2_node_t *node)
{
    (void)node;
    /* TODO */
    return NULL;
}

bool list2_contains(const list2_t *l, list2_elem_t value)
{
    (void)l;
    (void)value;
    /* TODO */
    return false;
}

size_t list2_size(const list2_t *l)
{
    (void)l;
    /* TODO */
    return 0;
}

bool list2_is_empty(const list2_t *l)
{
    (void)l;
    /* TODO */
    return false;
}
