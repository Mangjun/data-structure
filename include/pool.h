/*
 * pool.h - 고정 크기 블록 메모리 풀 (free list, 동적 할당 없음)
 *
 * 호출자가 넘긴 바이트 저장소(storage)를 같은 크기의 블록으로 나눠 빌려준다.
 * 빈 블록은 블록 안에 next 포인터를 넣어 단일 연결 free list 로 관리한다
 * (빈 블록의 메모리를 링크 저장에 재사용 -> 별도 메타데이터 없음).
 *
 * ---------------------------------------------------------------------------
 * 정렬
 * ---------------------------------------------------------------------------
 *   모든 블록 시작 주소는 POOL_ALIGN 의 배수다. 어떤 타입의 객체든 담을 수 있다.
 *   실제 블록 크기 = 요청 크기와 sizeof(struct pool_free_node) 중 큰 값을
 *                   POOL_ALIGN 의 배수로 올린 값. pool_block_size() 로 조회.
 *   storage 시작 주소는 정렬되어 있지 않아도 된다. init 이 앞부분을 건너뛴다.
 *   [TODO(구현자)] 정렬/올림 계산에서 오버플로가 나지 않게 할 것.
 *
 * ---------------------------------------------------------------------------
 * 이중 해제 검출
 * ---------------------------------------------------------------------------
 *   pool_free 는 이중 해제를 O(1) 로 검출해야 한다 (free list 를 훑는 O(n) 금지:
 *   실행 시간이 결정적이지 않다). 블록별 상태(state)를 저장소 안에서 떼어 내 쓴다.
 *   그 결과 사용 가능 블록 수는 storage_size / block_size 보다 작을 수 있다.
 *   사용 가능 블록 수는 반드시 pool_capacity() 로 조회한다.
 *   선택: 블록당 1비트 비트맵, 블록 영역 바로 뒤에 배치.
 *     비트 i == 1 : 블록 i 를 빌려줌(사용 중), 0 : 빈 블록.
 *     비트 i 의 위치 : state[i / 8] 의 (i % 8) 번째 비트.
 *     메모리 배치   : [정렬로 건너뜀][블록 0 .. n-1][비트맵 ceil(n/8) 바이트][남는 바이트]
 *   근거:
 *     - 모든 블록 크기가 같아 블록마다 필요한 정보는 "빈/사용 중" 1비트뿐이다.
 *       (malloc 은 블록마다 크기가 달라 크기 헤더가 필요하지만 풀은 필요 없다)
 *     - 상태 오버헤드가 블록당 1/8 바이트로 바이트 방식(1바이트)보다 작다.
 *     - 블록 뒤에 두면 블록 영역 시작이 곧 정렬된 주소라 정렬을 한 번만 하면 된다.
 *       (앞에 두면 비트맵 끝에서 블록을 다시 정렬해야 해 패딩이 한 번 더 생긴다)
 *   치르는 것:
 *     - 블록 수 계산: n * block_size + ceil(n/8) <= 남은 바이트 를 만족하는 최대 n.
 *       경계에서 하나 틀리기 쉽다.
 *     - 비트는 0/1 뿐이라 상태 손상(쓰레기 값)을 검출할 수 없다
 *       (바이트 방식이면 magic 값으로 검출 가능).
 *     - 비트 설정/해제는 바이트 읽기-수정-쓰기다. uint8_t 로 되돌릴 때 캐스트 필요.
 *   참고: 실제로 더 큰 낭비는 블록 크기를 POOL_ALIGN 으로 올리는 데서 생긴다
 *        (예: x86-64 에서 24 -> 32, 블록당 8바이트).
 *
 * ---------------------------------------------------------------------------
 * 포인터 범위 검사 주의
 * ---------------------------------------------------------------------------
 *   같은 배열(객체) 안을 가리키지 않는 두 포인터의 관계 비교(<, <=, >, >=)는
 *   미정의 동작이다. pool_free 는 "풀 밖" 포인터도 받아야 하므로 이 점을 고려한다.
 *
 * ---------------------------------------------------------------------------
 * 동시성
 * ---------------------------------------------------------------------------
 *   재진입/ISR 안전하지 않다. 여러 문맥에서 쓰려면 호출자가 임계 구역으로 보호한다.
 */
#ifndef POOL_H
#define POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 블록 정렬 단위. max_align_t 정렬이면 어떤 객체든 담을 수 있다. */
#define POOL_ALIGN (_Alignof(max_align_t))

typedef enum {
    POOL_OK = 0,
    POOL_ERR_NULL,        /* 필수 포인터 인자가 NULL */
    POOL_ERR_INVAL,       /* 잘못된 인자 (block_size 0, 블록 1개도 못 담는 저장소, 크기 오버플로) */
    POOL_ERR_EXHAUSTED,   /* 빈 블록 없음 */
    POOL_ERR_FOREIGN,     /* 이 풀의 블록 영역 밖 포인터 */
    POOL_ERR_MISALIGNED,  /* 블록 영역 안이지만 블록 시작 주소가 아님 */
    POOL_ERR_DOUBLE_FREE  /* 이미 빈 블록 */
} pool_status_t;

/* 빈 블록 안에 저장되는 링크 */
struct pool_free_node {
    struct pool_free_node *next;
};

/*
 * 필드는 공개되어 있지만 사용자는 직접 접근하지 않는다.
 * 불변식 (init 성공 후):
 *   blocks 는 POOL_ALIGN 정렬, block_size 는 POOL_ALIGN 의 배수이며
 *   sizeof(struct pool_free_node) 이상, block_count >= 1, 0 <= free_count <= block_count
 *   free_list 의 노드 수 == free_count
 *   블록 영역 [blocks, blocks + block_count * block_size) 와 state 는 storage 안에 있다.
 *   state == blocks + block_count * block_size, 길이 ceil(block_count / 8) 바이트.
 *   state 의 1 비트 수 == block_count - free_count.
 */
typedef struct {
    unsigned char         *blocks;      /* 첫 블록 주소 */
    size_t                 block_size;  /* 실제 블록 크기 (올림 반영) */
    size_t                 block_count; /* 사용 가능 블록 수 */
    size_t                 free_count;  /* 현재 빈 블록 수 */
    struct pool_free_node *free_list;   /* 빈 블록 연결 리스트 */
    unsigned char         *state;       /* 사용 중 비트맵 (블록 영역 바로 뒤) */
} pool_t;

/*
 * pool_init
 *   p            : 초기화할 풀 객체. 호출자가 준비한다.
 *   storage      : 블록과 상태 정보를 담을 바이트 저장소. 호출자가 소유하며
 *                  풀을 사용하는 동안 수명이 유지되어야 한다. 정렬은 요구하지 않는다.
 *   storage_size : storage 의 바이트 수.
 *   block_size   : 블록 하나에 담을 객체의 바이트 수 (예: sizeof(my_node_t)).
 *   사전: p, storage 는 NULL 이 아님.
 *   사후(OK): 모든 블록이 빈 상태. pool_free_count == pool_capacity >= 1.
 *             pool_block_size >= block_size, POOL_ALIGN 의 배수.
 *   반환: POOL_OK
 *         POOL_ERR_NULL  - p 또는 storage 가 NULL (우선)
 *         POOL_ERR_INVAL - block_size == 0, 블록 1개도 담을 수 없음,
 *                          또는 크기 계산이 size_t 범위를 넘음
 *   실패 시: *p 는 수정하지 않는다.
 *   실행 시간: O(블록 수) (free list 구성).
 */
pool_status_t pool_init(pool_t *p, void *storage, size_t storage_size, size_t block_size);

/*
 * pool_alloc
 *   p   : init 성공한 풀.
 *   out : [출력] 빌린 블록 주소를 받을 위치. 성공했을 때만 기록된다.
 *   사후(OK): *out 은 POOL_ALIGN 정렬, 블록 영역 안, 다른 사용 중 블록과 겹치지 않음.
 *             블록 내용은 정해지지 않는다 (0 으로 초기화하지 않음). free_count 1 감소.
 *   반환: POOL_OK, POOL_ERR_NULL (p 또는 out), POOL_ERR_EXHAUSTED
 *   실패 시: 풀 상태와 *out 모두 불변.
 *   실행 시간: O(1).
 */
pool_status_t pool_alloc(pool_t *p, void **out);

/*
 * pool_free
 *   p   : init 성공한 풀.
 *   ptr : pool_alloc 이 돌려준 블록 주소.
 *         표준 free() 와 달리 NULL 은 오류로 보고한다 (해제 누락/이중 해제 추적용).
 *   사후(OK): 블록이 빈 상태가 됨. free_count 1 증가.
 *   반환: POOL_OK
 *         POOL_ERR_NULL        - p 또는 ptr 가 NULL
 *         POOL_ERR_FOREIGN     - 블록 영역 밖
 *         POOL_ERR_MISALIGNED  - 블록 영역 안이지만 블록 시작 주소가 아님
 *         POOL_ERR_DOUBLE_FREE - 이미 빈 블록
 *   실패 시: 풀 상태 불변.
 *   실행 시간: O(1).
 */
pool_status_t pool_free(pool_t *p, void *ptr);

/*
 * 조회 함수. NULL 이면 0.
 *   pool_capacity   : 사용 가능 블록 수 (block_count)
 *   pool_free_count : 현재 빈 블록 수
 *   pool_block_size : 실제 블록 크기 (올림 반영)
 */
size_t pool_capacity(const pool_t *p);
size_t pool_free_count(const pool_t *p);
size_t pool_block_size(const pool_t *p);

#endif /* POOL_H */
