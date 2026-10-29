/*
 * pool.c - 구현은 직접 채운다.
 * 현재는 모든 함수가 실패를 반환하는 스텁이다.
 */
#include "pool.h"

pool_status_t pool_init(pool_t *p, void *storage, size_t storage_size, size_t block_size)
{
    if (p == NULL || storage == NULL) {
        return POOL_ERR_NULL;
    }

    if (block_size == 0) {
        return POOL_ERR_INVAL;
    }

    if (block_size < sizeof(struct pool_free_node)) {
        block_size = sizeof(struct pool_free_node);
    }

    if (block_size > SIZE_MAX - POOL_ALIGN) {
        return POOL_ERR_INVAL;
    }

    size_t storage_offset = (POOL_ALIGN - (uintptr_t)storage % POOL_ALIGN) % POOL_ALIGN;
    
    if (storage_size < storage_offset) {
        return POOL_ERR_INVAL;
    }

    unsigned char *aligned_storage = (unsigned char *)storage + storage_offset;

    size_t aligned_storage_size = storage_size - storage_offset;

    block_size = (block_size + POOL_ALIGN - 1) & ~(POOL_ALIGN - 1);

    size_t block_count = aligned_storage_size / block_size;

    if (block_count == 0) {
        return POOL_ERR_INVAL;
    }

    p->blocks = aligned_storage;
    p->block_size = block_size;
    p->block_count = block_count;
    p->free_count = block_count;
    p->free_list = NULL;
    p->state = aligned_storage + block_count * block_size;

    return POOL_OK;
}

pool_status_t pool_alloc(pool_t *p, void **out)
{
    (void)p;
    (void)out;
    /* TODO */
    return POOL_ERR_INVAL;
}

pool_status_t pool_free(pool_t *p, void *ptr)
{
    (void)p;
    (void)ptr;
    /* TODO */
    return POOL_ERR_INVAL;
}

size_t pool_capacity(const pool_t *p)
{
    (void)p;
    /* TODO */
    return 0;
}

size_t pool_free_count(const pool_t *p)
{
    (void)p;
    /* TODO */
    return 0;
}

size_t pool_block_size(const pool_t *p)
{
    (void)p;
    /* TODO */
    return 0;
}