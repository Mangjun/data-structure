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

struct queue;

/**
 * @brief   함수 포인터 테이블
 */
struct ops {
    int (*is_empty)(const struct queue* const self);
    int (*size)(const struct queue* const self);
    int (*clear)(struct queue* const self, void (*free_func)(struct node*));

    int (*enqueue)(struct queue* const self, struct node* const item);
    struct node* (*dequeue)(struct queue* const self);
    struct node* (*peek)(const struct queue* const self);  
};

/**
 * @brief   큐 관리 구조체
 * @details pimpl 구조 사용하여 컴파일 의존성 낮춤
 */
struct queue {
    struct queue_internal *pimpl;  /**< pimpl 포인터 */
    const struct ops *ops;         /**< 함수 포인터 연결 */
};

/**
 * @brief       큐 초기화
 * @param self  대상
 * @return      성공(0), 실패(1)
 */
int init_queue(struct queue* const self);

/**
 * @brief           큐 소멸
 * @param self      대상
 * @param free_func 노드 초기화 함수
 * @return          성공(0), 실패(1)
 */
int destroy_queue(struct queue* const self, void (*free_func)(struct node*));