/*
 * list_v1.h - 단일 연결 리스트 v1: 정적 노드 배열 + 인덱스 기반 next
 *
 * 노드 배열은 호출자가 준비한다. 포인터 대신 배열 인덱스로 연결한다.
 *   - 사용 중 노드: head 부터 next 를 따라가는 리스트
 *   - 빈 노드    : free_head 부터 next 를 따라가는 free list (같은 배열, 같은 next 필드)
 * 인덱스를 쓰는 이유: 노드 배열 전체를 복사/이동/직렬화해도 링크가 유효하다.
 * 인덱스 폭(16비트)이 포인터보다 작아 RAM 을 아낀다.
 *
 * 인덱스 LIST1_NIL 은 "없음"(NULL 에 해당)을 뜻한다. 그래서 노드 수는 최대 LIST1_MAX_CAP.
 *
 * 순회 예:
 *   for (list1_idx_t i = list1_first(&l); i != LIST1_NIL; i = list1_next(&l, i)) {
 *       list1_elem_t v;
 *       list1_get(&l, i, &v);
 *   }
 *
 * 동시성: 재진입/ISR 안전하지 않다.
 */
#ifndef LIST_V1_H
#define LIST_V1_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t  list1_elem_t;
typedef uint16_t list1_idx_t;

#define LIST1_NIL     ((list1_idx_t)UINT16_MAX)
#define LIST1_MAX_CAP ((size_t)UINT16_MAX)   /* 인덱스 0 .. MAX_CAP-1, MAX_CAP 은 NIL */

typedef enum {
    LIST1_OK = 0,
    LIST1_ERR_NULL,       /* 필수 포인터 인자가 NULL */
    LIST1_ERR_INVAL,      /* 잘못된 인자 (cap 범위, 인덱스 범위) */
    LIST1_ERR_EMPTY,      /* 빈 리스트에서 pop */
    LIST1_ERR_FULL,       /* 빈 노드 없음 */
    LIST1_ERR_NOT_FOUND   /* remove 할 값이 없음 */
} list1_status_t;

typedef struct {
    list1_elem_t value;
    list1_idx_t  next;    /* 다음 노드 인덱스 또는 LIST1_NIL */
} list1_node_t;

/*
 * 필드는 공개되어 있지만 사용자는 직접 접근하지 않는다.
 * 불변식 (init 성공 후):
 *   nodes != NULL, 1 <= cap <= LIST1_MAX_CAP
 *   head 리스트의 노드 수 == count, free list 의 노드 수 == cap - count
 *   두 리스트는 서로소이고 합치면 0 .. cap-1 전부
 *   count == 0 <=> head == LIST1_NIL <=> tail == LIST1_NIL
 *   tail 은 head 리스트의 마지막 노드 (그 next == LIST1_NIL)
 */
typedef struct {
    list1_node_t *nodes;
    list1_idx_t   cap;
    list1_idx_t   count;
    list1_idx_t   head;
    list1_idx_t   tail;       /* push_back O(1) 용 */
    list1_idx_t   free_head;
} list1_t;

/*
 * list1_init
 *   l     : 초기화할 리스트 객체.
 *   nodes : 노드 저장소. 호출자가 소유하며 리스트를 쓰는 동안 수명이 유지되어야 한다.
 *   cap   : nodes 의 원소 개수 (바이트 수 아님). 1 .. LIST1_MAX_CAP.
 *   사후(OK): 빈 리스트. 모든 노드가 free list 에 있음.
 *   반환: LIST1_OK, LIST1_ERR_NULL (우선), LIST1_ERR_INVAL (cap 범위 밖)
 *   실패 시: *l 불변.
 *   실행 시간: O(cap) (free list 구성).
 */
list1_status_t list1_init(list1_t *l, list1_node_t *nodes, size_t cap);

/*
 * list1_clear
 *   사후(OK): 빈 리스트. 모든 노드가 free list 로 돌아감.
 *   반환: LIST1_OK, LIST1_ERR_NULL
 *   실행 시간: O(cap) 또는 O(count) (구현 선택. 선택을 여기 적는다: ____)
 */
list1_status_t list1_clear(list1_t *l);

/*
 * list1_push_front / list1_push_back
 *   value : 넣을 값.
 *   사후(OK): 맨 앞/맨 뒤에 value 추가. count 1 증가.
 *   반환: LIST1_OK, LIST1_ERR_NULL, LIST1_ERR_FULL
 *   실패 시: 상태 불변.  실행 시간: O(1).
 */
list1_status_t list1_push_front(list1_t *l, list1_elem_t value);
list1_status_t list1_push_back(list1_t *l, list1_elem_t value);

/*
 * list1_pop_front
 *   out : [출력] 꺼낸 값. 성공했을 때만 기록된다.
 *   사후(OK): 맨 앞 노드를 free list 로 반환. count 1 감소.
 *   반환: LIST1_OK, LIST1_ERR_NULL (l 또는 out), LIST1_ERR_EMPTY
 *   실패 시: 상태와 *out 불변.  실행 시간: O(1).
 */
list1_status_t list1_pop_front(list1_t *l, list1_elem_t *out);

/*
 * list1_insert_after
 *   idx   : 기준 노드. list1_first/list1_next/list1_find 로 얻은, 현재 리스트에 있는 인덱스.
 *   value : 넣을 값.
 *   사후(OK): idx 노드 바로 뒤에 value 추가 (idx 가 tail 이면 tail 갱신).
 *   반환: LIST1_OK, LIST1_ERR_NULL, LIST1_ERR_INVAL (idx >= cap 또는 LIST1_NIL),
 *         LIST1_ERR_FULL
 *   주의: 범위 안이지만 free list 에 있는 idx 는 검출하지 않는다 (사전 조건 위반).
 *   실패 시: 상태 불변.  실행 시간: O(1).
 */
list1_status_t list1_insert_after(list1_t *l, list1_idx_t idx, list1_elem_t value);

/*
 * list1_remove
 *   value : 지울 값. 앞에서부터 처음 만나는 하나만 지운다.
 *   사후(OK): 해당 노드를 free list 로 반환. head/tail 갱신.
 *   반환: LIST1_OK, LIST1_ERR_NULL, LIST1_ERR_NOT_FOUND
 *   실패 시: 상태 불변.  실행 시간: O(count).
 */
list1_status_t list1_remove(list1_t *l, list1_elem_t value);

/*
 * list1_find
 *   반환: value 를 가진 첫 노드의 인덱스. 없거나 l == NULL 이면 LIST1_NIL.
 *   실행 시간: O(count).
 */
list1_idx_t list1_find(const list1_t *l, list1_elem_t value);

/*
 * 순회
 *   list1_first : 첫 노드 인덱스. 빈 리스트 또는 l == NULL 이면 LIST1_NIL.
 *   list1_next  : idx 다음 노드 인덱스. 마지막이면 LIST1_NIL.
 *                 idx 가 LIST1_NIL 이거나 범위 밖이면 LIST1_NIL.
 *   list1_get   : idx 노드의 값을 *out 에 기록.
 *                 반환: LIST1_OK, LIST1_ERR_NULL, LIST1_ERR_INVAL (idx 범위 밖/NIL)
 */
list1_idx_t    list1_first(const list1_t *l);
list1_idx_t    list1_next(const list1_t *l, list1_idx_t idx);
list1_status_t list1_get(const list1_t *l, list1_idx_t idx, list1_elem_t *out);

/*
 * 조회. NULL 이면 size/capacity == 0, is_empty == true, is_full == true.
 */
size_t list1_size(const list1_t *l);
size_t list1_capacity(const list1_t *l);
bool   list1_is_empty(const list1_t *l);
bool   list1_is_full(const list1_t *l);

#endif /* LIST_V1_H */
