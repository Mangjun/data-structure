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
    struct node* next;  /**< 노드의 다음 노드 */
};

struct stack;

/**
 * @brief   함수 포인터 테이블
 */
struct ops {
    int (*is_empty)(const struct stack* const self);
    int (*size)(const struct stack* const self);
    int (*clear)(struct stack* const self, void (*free_func)(struct node*));

    int (*push)(struct stack* const self, struct node* item);
    struct node* (*pop)(struct stack* const self);
    struct node* (*peek)(const struct stack* const self);
};

/**
 * @brief   리스트 관리 구조체
 * @details pimpl 구조 사용하여 컴파일 의존성 낮춤
 */
struct stack {
    struct stack_internal *pimpl;  /**< pimpl 포인터 */
    const struct ops *ops;         /**< 함수 포인터 연결 */
};

/**
 * @brief       스택 초기화
 * @param self  대상
 * @return      성공(0), 실패(1)
 */
int init_stack(struct stack* const self);

/**
 * @brief           스택 소멸
 * @param self      대상
 * @param free_func 노드 초기화 함수
 * @return          성공(0), 실패(1)
 */
int destroy_stack(struct stack* const self, void (*free_func)(struct node*));