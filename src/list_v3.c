/*
 * list_v3.c - 구현은 직접 채운다.
 * 현재는 모든 함수가 실패를 반환하는 스텁이다.
 * list3_container_of, list3_for_each, list3_for_each_safe 는 include/list_v3.h 에서 채운다.
 */
#include "list_v3.h"

list3_status_t list3_init(list3_head_t *h)
{
    (void)h;
    /* TODO */
    return LIST3_ERR_INVAL;
}

list3_status_t list3_add(list3_head_t *entry, list3_head_t *head)
{
    (void)entry;
    (void)head;
    /* TODO */
    return LIST3_ERR_INVAL;
}

list3_status_t list3_add_tail(list3_head_t *entry, list3_head_t *head)
{
    (void)entry;
    (void)head;
    /* TODO */
    return LIST3_ERR_INVAL;
}

list3_status_t list3_del(list3_head_t *entry)
{
    (void)entry;
    /* TODO */
    return LIST3_ERR_INVAL;
}

bool list3_is_empty(const list3_head_t *head)
{
    (void)head;
    /* TODO */
    return false;
}

bool list3_is_linked(const list3_head_t *entry)
{
    (void)entry;
    /* TODO */
    return false;
}

size_t list3_count(const list3_head_t *head)
{
    (void)head;
    /* TODO */
    return 0;
}
