/*
 * list_v2.h - 단일 연결 리스트 v2: 포인터 링크 + 메모리 풀 노드
 *
 * 노드는 pool_t (include/pool.h) 에서 빌리고 돌려준다. malloc 을 쓰지 않는다.
 * 하나의 풀을 여러 리스트가 공유할 수 있다 (풀 블록 크기가 노드보다 크거나 같으면).
 *
 * ---------------------------------------------------------------------------
 * 이중 포인터 삭제 (구현 요구 사항)
 * ---------------------------------------------------------------------------
 *   list2_remove / list2_remove_if 는 "이전 노드" 변수 없이 이중 포인터로 구현한다.
 *     list2_node_t **link = &l->head;   -- "현재 노드를 가리키는 포인터"의 주소
 *   head 삭제와 중간 삭제를 같은 코드로 처리할 수 있다 (head 특수 처리 분기 없음).
 *   [TODO(구현자)] 왜 분기가 사라지는지 한두 줄로 여기 적는다: ____
 *
 * ---------------------------------------------------------------------------
 * 실행 시간
 * ---------------------------------------------------------------------------
 *   push_front, pop_front: O(1)
 *   push_back, remove, remove_if, contains, clear: O(n)
 *   (tail 포인터를 두지 않는다. 이중 포인터 삭제에서 tail 갱신 분기가 다시 생기기 때문.
 *    push_back 을 O(1) 로 만들고 싶다면 그 비용을 근거와 함께 여기 적는다.)
 *
 * 동시성: 재진입/ISR 안전하지 않다. 풀도 마찬가지다.
 */
#ifndef LIST_V2_H
#define LIST_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pool.h"

typedef int32_t list2_elem_t;

typedef enum {
    LIST2_OK = 0,
    LIST2_ERR_NULL,       /* 필수 포인터 인자가 NULL */
    LIST2_ERR_INVAL,      /* 잘못된 인자 (풀 블록이 노드보다 작음) */
    LIST2_ERR_EMPTY,      /* 빈 리스트에서 pop */
    LIST2_ERR_NOMEM,      /* 풀에 빈 블록 없음 */
    LIST2_ERR_NOT_FOUND,  /* remove 할 값이 없음 */
    LIST2_ERR_POOL        /* 풀이 노드 반환을 거부함 (리스트/풀 불일치 - 내부 오류) */
} list2_status_t;

typedef struct list2_node {
    struct list2_node *next;
    list2_elem_t       value;
} list2_node_t;

/*
 * 필드는 공개되어 있지만 사용자는 직접 접근하지 않는다.
 * 불변식 (init 성공 후):
 *   pool != NULL, pool_block_size(pool) >= sizeof(list2_node_t)
 *   head 부터 NULL 까지의 노드 수 == count, 모든 노드는 pool 에서 빌린 블록
 */
typedef struct {
    list2_node_t *head;
    size_t        count;
    pool_t       *pool;
} list2_t;

/* list2_remove_if 의 판정 함수. true 를 반환한 원소를 지운다. ctx 는 그대로 전달된다. */
typedef bool (*list2_pred_t)(list2_elem_t value, void *ctx);

/*
 * list2_init
 *   l    : 초기화할 리스트 객체.
 *   pool : 노드를 빌릴 풀. init 성공한 풀이어야 하며, 리스트를 쓰는 동안 유지되어야 한다.
 *   사후(OK): 빈 리스트.
 *   반환: LIST2_OK, LIST2_ERR_NULL (우선),
 *         LIST2_ERR_INVAL (pool_block_size(pool) < sizeof(list2_node_t))
 *   실패 시: *l 불변.
 */
list2_status_t list2_init(list2_t *l, pool_t *pool);

/*
 * list2_push_front / list2_push_back
 *   사후(OK): 맨 앞/맨 뒤에 value 추가. count 1 증가.
 *   반환: LIST2_OK, LIST2_ERR_NULL, LIST2_ERR_NOMEM
 *   실패 시: 리스트와 풀 모두 불변.
 */
list2_status_t list2_push_front(list2_t *l, list2_elem_t value);
list2_status_t list2_push_back(list2_t *l, list2_elem_t value);

/*
 * list2_pop_front
 *   out : [출력] 꺼낸 값. 성공했을 때만 기록된다.
 *   사후(OK): 맨 앞 노드를 풀에 반환. count 1 감소.
 *   반환: LIST2_OK, LIST2_ERR_NULL (l 또는 out), LIST2_ERR_EMPTY, LIST2_ERR_POOL
 *   실패 시: 리스트와 *out 불변.
 */
list2_status_t list2_pop_front(list2_t *l, list2_elem_t *out);

/*
 * list2_remove   [이중 포인터로 구현]
 *   value : 지울 값. 앞에서부터 처음 만나는 하나만 지운다.
 *   반환: LIST2_OK, LIST2_ERR_NULL, LIST2_ERR_NOT_FOUND, LIST2_ERR_POOL
 */
list2_status_t list2_remove(list2_t *l, list2_elem_t value);

/*
 * list2_remove_if   [이중 포인터로 구현]
 *   pred    : 판정 함수. true 인 원소를 모두 지운다. 한 원소당 정확히 한 번 호출된다.
 *   ctx     : pred 에 그대로 넘긴다 (NULL 가능).
 *   removed : [출력, NULL 가능] 지운 개수.
 *   반환: LIST2_OK (0개 지워도 OK), LIST2_ERR_NULL (l 또는 pred), LIST2_ERR_POOL
 *   LIST2_ERR_POOL 이면 그때까지 지운 개수가 *removed 에 기록되고 리스트는 일관된 상태다.
 */
list2_status_t list2_remove_if(list2_t *l, list2_pred_t pred, void *ctx, size_t *removed);

/*
 * list2_clear
 *   사후(OK): 모든 노드를 풀에 반환. 빈 리스트.
 *   반환: LIST2_OK, LIST2_ERR_NULL, LIST2_ERR_POOL
 */
list2_status_t list2_clear(list2_t *l);

/*
 * 순회 (읽기 전용)
 *   list2_first : 첫 노드. 빈 리스트 또는 l == NULL 이면 NULL.
 *   list2_next  : 다음 노드. 마지막이거나 node == NULL 이면 NULL.
 *   순회 중에 리스트를 변경하면 안 된다.
 */
const list2_node_t *list2_first(const list2_t *l);
const list2_node_t *list2_next(const list2_node_t *node);

/*
 * 조회. NULL 이면 size == 0, is_empty == true, contains == false.
 */
bool   list2_contains(const list2_t *l, list2_elem_t value);
size_t list2_size(const list2_t *l);
bool   list2_is_empty(const list2_t *l);

#endif /* LIST_V2_H */
