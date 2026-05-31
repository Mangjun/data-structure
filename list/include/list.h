#pragma once

#include <stddef.h>

/**
 * @brief   노드로 구조체 시작 주소 반환 함수
 */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * @brief   구조체 임베딩 노드
 */
struct node {
    struct node *prev;    /**< 노드의 이전 노드 */
    struct node *next;    /**< 노드의 다음 노드 */
};

struct list;

/**
 * @brief   함수 포인터 테이블
 */
struct ops {
    int (*is_empty)(const struct list* const self);
    int (*size)(const struct list* const self);
    int (*clear)(struct list* const self, void (*free_func)(struct node*));

    struct node* (*find)(const struct list* const self, int (*compare)(struct node*, void*), void* arg);
    struct node* (*at)(const struct list* const self, const int index);

    void (*for_each)(const struct list* const self, void (*print_func)(struct node*));

    int (*insert_at)(struct list* const self, const int index,struct node* const node);
    int (*insert_first)(struct list* const self,struct node* const node);
    int (*insert_last)(struct list* const self,struct node* const node);

    struct node* (*remove_at)(struct list* const self, const int index);
    struct node* (*remove_first)(struct list* const self);
    struct node* (*remove_last)(struct list* const self);

    int (*move)(struct list* const self, const int index, struct node* const node);
    int (*move_first)(struct list* const self, struct node* const node);
    int (*move_last)(struct list* const self, struct node* const node);
};

/**
 * @brief   리스트 관리 구조체
 * @details pimpl 구조 사용하여 컴파일 의존성 낮춤
 */
struct list {
    struct list_internal *pimpl;   /**< pimpl 포인터 */
    const struct ops *ops;         /**< 함수 포인터 연결 */
};

/**
 * @brief       리스트 초기화
 * @param self  대상
 * @return      성공(0), 실패(1)
 */
int init_list(struct list* const self);

/**
 * @brief           리스트 소멸
 * @param self      대상
 * @param free_func 노드 초기화 함수
 * @return          성공(0), 실패(1)
 */
int destroy_list(struct list* const self, void (*free_func)(struct node*));