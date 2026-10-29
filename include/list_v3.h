/*
 * list_v3.h - 연결 리스트 v3: intrusive 원형 이중 연결 리스트 (sentinel head)
 *
 * 리스트가 노드를 소유하지 않는다. 사용자의 구조체 안에 list3_head_t 멤버를 넣고,
 * 그 멤버끼리 연결한다. 멤버 주소에서 바깥 구조체 주소는 list3_container_of 로 되찾는다.
 *
 *   typedef struct {
 *       int          id;
 *       list3_head_t node;       -- 첫 멤버일 필요 없음
 *   } item_t;
 *
 *   list3_head_t items;          -- 리스트 머리 (sentinel). 원소가 아니다.
 *   list3_init(&items);
 *   list3_add_tail(&it.node, &items);
 *   list3_head_t *pos;
 *   list3_for_each(pos, &items) {
 *       item_t *p = list3_entry(pos, item_t, node);
 *   }
 *
 * 장점: 할당이 전혀 없다. 한 객체가 멤버를 여러 개 두면 동시에 여러 리스트에 들어간다.
 *
 * ---------------------------------------------------------------------------
 * 표현
 * ---------------------------------------------------------------------------
 *   원형: 마지막 원소의 next == head, 첫 원소의 prev == head.
 *   빈 리스트: head->next == head->prev == head.
 *   연결되지 않은 엔트리도 같은 모양(next == prev == 자기 자신)으로 둔다.
 *   -> list3_del 후 엔트리를 다시 이 모양으로 되돌려 이중 삭제를 검출한다.
 *   사용 전 모든 head 와 엔트리는 list3_init 또는 LIST3_HEAD_INIT 으로 초기화해야 한다.
 *   초기화되지 않은 엔트리는 검출할 수 없다 (미정의 동작).
 *
 * 동시성: 재진입/ISR 안전하지 않다.
 */
#ifndef LIST_V3_H
#define LIST_V3_H

#include <stdbool.h>
#include <stddef.h>

typedef struct list3_head {
    struct list3_head *next;
    struct list3_head *prev;
} list3_head_t;

typedef enum {
    LIST3_OK = 0,
    LIST3_ERR_NULL,        /* 필수 포인터 인자가 NULL */
    LIST3_ERR_INVAL,       /* entry == head (자기 자신에 추가) */
    LIST3_ERR_LINKED,      /* 이미 어떤 리스트에 연결된 엔트리를 추가하려 함 */
    LIST3_ERR_NOT_LINKED   /* 연결되지 않은 엔트리를 삭제하려 함 */
} list3_status_t;

/* 정적 초기화: static list3_head_t q = LIST3_HEAD_INIT(q); */
#define LIST3_HEAD_INIT(name) { &(name), &(name) }

/*
 * list3_container_of(ptr, type, member)
 *   ptr    : type 구조체 안의 member 를 가리키는 포인터 (list3_head_t *).
 *   반환   : 그 member 를 품은 type 구조체의 포인터 (type *).
 *   요구   : 표준 C11 만 사용 (GNU typeof / statement expression 금지). offsetof 사용.
 *            포인터 산술은 char * 단위로 해야 한다 (왜인지 한 줄 적는다: ____).
 *   [TODO(구현자)] 아래 자리표시 정의를 실제 정의로 바꾼다.
 *   자리표시는 항상 NULL 을 돌려주므로 테스트가 실패한다.
 */
#define list3_container_of(ptr, type, member) \
    ((type *)((void)(ptr), NULL))                 /* TODO(구현자) */

/* list3_entry: list3_container_of 의 별칭 (리스트 원소 접근용 이름) */
#define list3_entry(ptr, type, member) list3_container_of(ptr, type, member)

/*
 * list3_for_each(pos, head)
 *   pos  : list3_head_t * 변수. 첫 원소부터 마지막 원소까지 차례로 가리킨다 (head 제외).
 *   head : 리스트 머리.
 *   루프 안에서 pos 를 삭제하면 안 된다 -> list3_for_each_safe 를 쓴다.
 *
 * list3_for_each_safe(pos, tmp, head)
 *   tmp  : list3_head_t * 임시 변수. pos 의 다음 원소를 미리 저장한다.
 *   루프 안에서 pos 를 list3_del 해도 안전하다.
 *
 *   [TODO(구현자)] 아래 자리표시 정의를 실제 for 문으로 바꾼다.
 *   자리표시는 한 번도 반복하지 않는다.
 */
#define list3_for_each(pos, head) \
    for ((pos) = (head); (void)(pos), 0;)                       /* TODO(구현자) */

#define list3_for_each_safe(pos, tmp, head) \
    for ((pos) = (head), (tmp) = (pos); (void)(tmp), 0;)        /* TODO(구현자) */

/*
 * list3_init
 *   h : 초기화할 head 또는 엔트리.
 *   사후(OK): h->next == h->prev == h (빈 리스트 / 연결 안 된 엔트리).
 *   반환: LIST3_OK, LIST3_ERR_NULL
 */
list3_status_t list3_init(list3_head_t *h);

/*
 * list3_add       : entry 를 head 바로 뒤(맨 앞)에 넣는다.
 * list3_add_tail  : entry 를 head 바로 앞(맨 뒤)에 넣는다.
 *   entry : 연결되지 않은 (list3_init 된) 엔트리.
 *   head  : 리스트 머리.
 *   반환: LIST3_OK
 *         LIST3_ERR_NULL   - entry 또는 head 가 NULL
 *         LIST3_ERR_INVAL  - entry == head
 *         LIST3_ERR_LINKED - entry 가 이미 연결됨 (list3_is_linked(entry))
 *   실패 시: 모든 리스트 불변.  실행 시간: O(1).
 */
list3_status_t list3_add(list3_head_t *entry, list3_head_t *head);
list3_status_t list3_add_tail(list3_head_t *entry, list3_head_t *head);

/*
 * list3_del
 *   entry : 연결된 엔트리. 어느 리스트에 있는지 몰라도 된다 (prev/next 만으로 해제).
 *   사후(OK): 리스트에서 빠지고 entry 는 다시 연결 안 된 모양 (next == prev == entry).
 *   반환: LIST3_OK, LIST3_ERR_NULL, LIST3_ERR_NOT_LINKED
 *   주의: head 를 넘기면 그 head 가 다른 원소처럼 빠진다 (검출하지 않음, 사전 조건 위반).
 *   실행 시간: O(1).
 */
list3_status_t list3_del(list3_head_t *entry);

/*
 * 조회
 *   list3_is_empty  : head 에 원소가 없으면 true. NULL 이면 true.
 *   list3_is_linked : entry 가 어떤 리스트에 연결되어 있으면 true. NULL 이면 false.
 *   list3_count     : 원소 수. O(n). NULL 이면 0.
 */
bool   list3_is_empty(const list3_head_t *head);
bool   list3_is_linked(const list3_head_t *entry);
size_t list3_count(const list3_head_t *head);

#endif /* LIST_V3_H */
