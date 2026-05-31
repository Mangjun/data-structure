#pragma once

#include "../include/stack.h"

/**
 * @brief   실제 스택 구조체
 */
struct stack_internal {
    struct node* top;   /**< 스택의 top */

    int size;           /**< 스택의 요소 개수 */
};

// helper function to check if the stack is valid (not NULL)
static inline int _is_valid_stack(const struct stack* const self)
{
    return self != NULL;
}

/**
 * @brief       스택이 비어있는지 확인
 * @param self  대상
 * @return      비어있다면(1)
 */
int is_empty_impl(const struct stack* const self);

/**
 * @brief       스택의 요소 개수 확인
 * @param self  대상
 * @return      요소 개수
 */
int size_impl(const struct stack* const self);

/**
 * @brief           스택 비움
 * @param self      대상
 * @param free_func free 사용자 정의 함수
 * @return          성공(0), 실패(1)
 */
int clear_impl(struct stack* const self, void (*free_func)(struct node*));

/**
 * @brief       스택에 요소 추가
 * @param self  대상
 * @param item  요소
 * @return      성공(0), 실패(1)
 */
int push_impl(struct stack* const self, struct node* item);

/**
 * @brief       스택에 최상단 요소 삭제
 * @param self  대상
 * @return      성공(요소), 실패(NULL)
 */
struct node* pop_impl(struct stack* const self);

/**
 * @brief       스택의 최상단 요소 보기
 * @param self  대상
 * @return      성공(요소), 실패(NULL)
 */
struct node* peek_impl(const struct stack* const self);