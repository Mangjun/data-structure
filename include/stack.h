/*
 * stack.h - 고정 용량 배열 스택 (동적 할당 없음)
 *
 * 저장소는 호출자가 소유한다. stack 은 buf 를 빌려 쓸 뿐이며
 * buf 의 수명은 stack 을 사용하는 동안 유지되어야 한다.
 *
 * 타입 이름이 stack_t 가 아닌 이유: POSIX <signal.h> 가 sigaltstack 용으로
 * stack_t 를 이미 정의한다. 또한 POSIX 는 *_t 접미사 전체를 예약한다
 * (이 프로젝트는 호스트 테스트 편의상 ds_ 접두어로 충돌만 피한다).
 *
 * 동시성: 재진입/ISR 안전하지 않다. 여러 문맥에서 같은 객체를 쓰려면
 * 호출자가 임계 구역으로 보호해야 한다.
 */
#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 원소 타입. 바꾸려면 여기만 수정한다. */
typedef int32_t stack_elem_t;

typedef enum {
    STACK_OK = 0,
    STACK_ERR_NULL,   /* 필수 포인터 인자가 NULL */
    STACK_ERR_INVAL,  /* 잘못된 인자 (예: cap == 0) */
    STACK_ERR_EMPTY,  /* 빈 스택에서 pop/peek */
    STACK_ERR_FULL    /* 가득 찬 스택에 push */
} stack_status_t;

/*
 * 필드는 구현 편의를 위해 공개되어 있지만, 사용자는 직접 접근하지 않는다.
 * 불변식 (init 성공 후):
 *   buf != NULL, cap >= 1, 0 <= top <= cap
 *   유효 원소는 buf[0 .. top-1], buf[top-1] 이 최상단
 */
typedef struct {
    stack_elem_t *buf;
    size_t        cap;
    size_t        top;
} ds_stack_t;

/*
 * stack_init
 *   s   : 초기화할 스택 객체. 메모리는 호출자가 준비한다 (전역/static/지역 변수).
 *   buf : 원소 저장소. 호출자가 소유하며, stack 은 주소만 보관한다.
 *         stack 을 사용하는 동안 수명이 유지되어야 한다
 *         (예: 함수 지역 배열을 넘기고 그 함수가 반환되면 이후 사용은 미정의 동작).
 *   cap : buf 에 담을 수 있는 원소 개수. 바이트 수가 아니다.
 *         예: stack_elem_t storage[8]; -> cap = 8 (sizeof storage 가 아님)
 *   사전: s, buf 는 NULL 이 아님. buf 는 cap 개 원소를 담을 수 있음.
 *   사후(OK): 빈 스택. size == 0, capacity == cap.
 *   반환: STACK_OK
 *         STACK_ERR_NULL  - s 또는 buf 가 NULL
 *         STACK_ERR_INVAL - cap == 0
 *   여러 오류가 동시에 해당하면 STACK_ERR_NULL 이 우선한다.
 *   실패 시: s 가 NULL 이 아니라도 *s 는 수정하지 않는다.
 */
stack_status_t stack_init(ds_stack_t *s, stack_elem_t *buf, size_t cap);

/*
 * stack_push
 *   s     : init 성공한 스택.
 *   value : 넣을 값. 값으로 복사되어 buf 에 저장된다.
 *   사전: s 는 init 성공한 객체.
 *   사후(OK): size 가 1 증가, 최상단 == value.
 *   반환: STACK_OK, STACK_ERR_NULL, STACK_ERR_FULL
 *   실패 시: 스택 상태 불변.
 */
stack_status_t stack_push(ds_stack_t *s, stack_elem_t value);

/*
 * stack_pop
 *   s   : init 성공한 스택.
 *   out : [출력] 꺼낸 값을 받을 위치. 호출자가 준비한다. 성공했을 때만 기록된다.
 *   사전: s 는 init 성공한 객체.
 *   사후(OK): *out == 이전 최상단, size 가 1 감소.
 *   반환: STACK_OK, STACK_ERR_NULL (s 또는 out), STACK_ERR_EMPTY
 *   실패 시: 스택 상태와 *out 모두 불변.
 */
stack_status_t stack_pop(ds_stack_t *s, stack_elem_t *out);

/*
 * stack_peek
 *   s   : init 성공한 스택. 변경되지 않는다 (const).
 *   out : [출력] 최상단 값을 받을 위치. 성공했을 때만 기록된다.
 *   pop 과 같지만 스택을 변경하지 않는다.
 *   반환: STACK_OK, STACK_ERR_NULL, STACK_ERR_EMPTY
 *   실패 시: *out 불변.
 */
stack_status_t stack_peek(const ds_stack_t *s, stack_elem_t *out);

/*
 * stack_clear
 *   s : init 성공한 스택.
 *   사후(OK): size == 0. buf 내용은 지우지 않는다 (O(1)).
 *   반환: STACK_OK, STACK_ERR_NULL
 */
stack_status_t stack_clear(ds_stack_t *s);

/*
 * 조회 함수. s 는 init 성공한 스택이며 변경되지 않는다 (const).
 * 반환 코드 대신 값을 돌려주므로 NULL 처리를 명시적으로 정의한다.
 *   stack_size(NULL)     == 0
 *   stack_capacity(NULL) == 0
 *   stack_is_empty(NULL) == true
 *   stack_is_full(NULL)  == true   (NULL 에 push 를 시도하지 않도록 보수적으로)
 */
size_t stack_size(const ds_stack_t *s);
size_t stack_capacity(const ds_stack_t *s);
bool   stack_is_empty(const ds_stack_t *s);
bool   stack_is_full(const ds_stack_t *s);

#endif /* STACK_H */