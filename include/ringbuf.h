/*
 * ringbuf.h - 고정 크기 링버퍼 FIFO (동적 할당 없음)
 *
 * 두 가지 변형을 제공한다.
 *   ringbuf_t     : 임의 크기 저장소. 인덱스 랩어라운드를 비교/나머지로 처리.
 *   ringbuf_p2_t  : 저장소 크기가 2의 거듭제곱. 랩어라운드를 마스크(& (size-1))로 처리.
 *
 * 저장소(buf)는 호출자가 소유한다. 링버퍼를 사용하는 동안 buf 의 수명이 유지되어야 한다.
 *
 * ---------------------------------------------------------------------------
 * full / empty 구분 방식
 * ---------------------------------------------------------------------------
 * 후보:
 *   (A) 한 칸 비우기   : head == tail 이면 empty, next(head) == tail 이면 full.
 *                        사용 가능 용량 = size - 1.
 *   (B) count 필드     : 별도 원소 수 카운터.
 *                        생산자/소비자가 같은 변수를 갱신 -> SPSC 에서 read-modify-write 경합.
 *   (C) free-running   : head/tail 을 마스크 없이 계속 증가시키고 접근 시에만 마스킹.
 *                        count = head - tail (부호 없는 모듈러 산술). 용량 = size.
 *                        인덱스 오버플로 시 올바르려면 size 가 2의 거듭제곱이어야 한다.
 *   (D1) full 플래그   : 생산자가 켜고 소비자가 끈다 -> (B) 와 같은 공유 쓰기 문제.
 *   (D2) 2배 범위      : 인덱스를 [0, 2*size) 에서 되감아 바퀴 차이로 full 구분. 용량 = size.
 *
 * 선택
 *   ringbuf_t    : (A) 한 칸 비우기
 *   ringbuf_p2_t : (C) free-running
 *
 * 근거 - 공통
 *   ISR <-> 메인 루프 SPSC 가 전제이므로 두 문맥이 같은 변수를 쓰는 (B), (D1) 은 제외.
 *   남은 (A), (C), (D2) 는 head 는 생산자만, tail 은 소비자만 쓴다.
 *
 * 근거 - ringbuf_t (A)
 *   임의 크기에서는 2의 거듭제곱이 보장되지 않으므로 (C) 는 쓸 수 없다.
 *   (D2) 대신 (A): 한 칸 손실(비율 1/size)을 받아들이고 판정식의 단순함을 택한다.
 *   이 근거가 성립하는 조건: 원소가 작고(rb_elem_t = 1바이트) size 가 충분히 클 때.
 *   size 가 매우 작거나 원소가 큰 구조체가 되면 다시 검토한다.
 *
 * 근거 - ringbuf_p2_t (C)
 *   (A) + 마스크도 가능하지만 학습 목적으로 (C) 를 택한다.
 *   얻는 것: 용량 = size 전체, count = head - tail 뺄셈 하나.
 *   치르는 것: 정확성이 unsigned 모듈러 산술과 2의 거듭제곱 크기에 의존한다.
 *             저장된 head/tail 은 절대 마스킹하지 않는다 (마스킹하면 (A) 가 된다).
 *             인덱스를 int 보다 좁은 타입으로 바꾸면 (T)(head - tail) 캐스트와
 *             size <= 2^(N-1) 조건이 필요하다.
 *
 * 결과로 생기는 인터페이스 차이
 *   같은 size 를 넘겨도 용량이 다르다: ringbuf_t 는 size - 1, ringbuf_p2_t 는 size.
 *   size == 1: ringbuf_t 는 RB_ERR_INVAL (용량 0), ringbuf_p2_t 는 정상 (mask == 0).
 *   사용 가능 용량은 반드시 ringbuf_capacity() / ringbuf_p2_capacity() 로 조회한다.
 *
 * ringbuf_p2_t 가 (C) 이므로 인덱스 오버플로 테스트를 기본으로 켠다.
 * (tests/test_ringbuf.c 의 t_p2_index_overflow)
 *
 * ---------------------------------------------------------------------------
 * 동시성 계약: 단일 생산자 / 단일 소비자 (SPSC), 예: ISR(생산자) <-> 메인 루프(소비자)
 * ---------------------------------------------------------------------------
 *   생산자 문맥에서만 호출: push, is_full
 *   소비자 문맥에서만 호출: pop, peek, is_empty
 *   양쪽 어디서든 호출 가능: count, capacity (단, count 는 호출 순간의 근사값)
 *   동시 접근이 없을 때만 호출: init, reset
 *
 *   head 는 생산자만 쓰고, tail 은 소비자만 쓴다 (소유권 분리).
 *
 * ---------------------------------------------------------------------------
 * ISR 안전성 설계 (C11 <stdatomic.h>)
 * ---------------------------------------------------------------------------
 * 막아야 하는 것
 *   push: 데이터 쓰기 -> head 갱신 순서가 뒤바뀌면 소비자가 아직 안 쓴 칸을 읽는다.
 *   pop : 데이터 읽기 -> tail 갱신 순서가 뒤바뀌면 생산자가 아직 안 읽은 칸을 덮어쓴다.
 *   컴파일러 재배치, CPU 재배치, 인덱스를 여러 번에 나눠 읽기(torn read) 모두 해당한다.
 *
 * volatile 을 쓰지 않는 이유
 *   volatile 은 volatile 접근끼리의 순서만 지킨다. buf[] 접근과 인덱스 갱신 사이의
 *   순서, CPU 재배치, 원자성은 보장하지 않는다.
 *
 * memory_order 값 (<stdatomic.h> 의 열거형 memory_order. 매크로가 아니라 enum 상수)
 *
 *   값                     쓸 수 있는 곳        이 파일에서의 용도
 *   ---------------------  -------------------  ----------------------------------
 *   memory_order_relaxed   load, store, RMW     자기 인덱스 읽기, count
 *   memory_order_consume   load, RMW            사용 안 함
 *   memory_order_acquire   load, RMW            상대 인덱스 읽기
 *   memory_order_release   store, RMW           자기 인덱스 쓰기
 *   memory_order_acq_rel   RMW                  사용 안 함
 *   memory_order_seq_cst   load, store, RMW     사용 안 함 (연산자 =, ++ 의 기본값)
 *
 *   relaxed : 그 변수 하나를 쪼개지지 않게 읽고 쓰기만 한다.
 *             다른 메모리 접근과의 순서는 보장하지 않는다.
 *   consume : acquire 의 약한 형태 (데이터 의존 관계만 보장).
 *             컴파일러 대부분이 acquire 로 취급한다.
 *   acquire : 이 읽기 "뒤"의 접근이 이 읽기보다 앞당겨지지 않는다. release 와 짝.
 *   release : 이 쓰기 "앞"의 접근이 이 쓰기보다 늦춰지지 않는다. acquire 와 짝.
 *   acq_rel : acquire + release. 읽기-수정-쓰기 연산 전용.
 *   seq_cst : 가장 강함. 모든 seq_cst 연산이 하나의 전체 순서를 가진다.
 *             atomic_load, atomic_store, 그리고 atomic 객체에 쓴 =, ++ 의 기본값.
 *
 *   RMW = 읽기-수정-쓰기 (atomic_fetch_add, atomic_exchange, ++ 등).
 *   store 에 acquire/consume/acq_rel, load 에 release/acq_rel 을 주면 안 된다.
 *   짝: 생산자의 release 쓰기를 소비자가 acquire 읽기로 "본" 경우에만, 그 쓰기 앞의
 *   모든 접근(buf[] 쓰기)이 그 읽기 뒤의 접근(buf[] 읽기)보다 먼저임이 보장된다.
 *
 * 메모리 순서 (각 접근에 붙일 순서)
 *   생산자 push / is_full
 *     tail 읽기      : acquire   (소비자가 그 칸을 다 읽은 뒤임을 보장)
 *     buf[] 쓰기     : 일반 접근
 *     head 쓰기      : release   (buf[] 쓰기가 먼저 끝났음을 공개)
 *     head 읽기      : relaxed   (자기 자신만 쓰는 변수)
 *   소비자 pop / peek / is_empty
 *     head 읽기      : acquire   (생산자가 그 칸을 다 쓴 뒤임을 보장)
 *     buf[] 읽기     : 일반 접근
 *     tail 쓰기      : release   (buf[] 읽기가 먼저 끝났음을 공개)  - pop 만
 *     tail 읽기      : relaxed   (자기 자신만 쓰는 변수)
 *   count          : head, tail 모두 relaxed 로 한 번씩만 읽는다 (근사값).
 *   init, reset    : 동시 접근이 없을 때만 호출하므로 순서 지정 불필요.
 *   공유 인덱스에는 되감기까지 끝난 "올바른 값"을 한 번만 저장한다.
 *   atomic 객체에 ++, = 같은 연산자를 쓰지 않는다 (seq_cst 읽기-수정-쓰기가 된다).
 *   atomic_load_explicit / atomic_store_explicit 로 순서를 명시한다.
 *   (같은 효과를 relaxed 접근 + atomic_thread_fence 로 낼 수도 있다.)
 *
 * 인덱스 타입: atomic_uint (rb_index_t = unsigned int)
 *   size_t 용 lock-free 매크로는 표준에 없다. unsigned int 는 ATOMIC_INT_LOCK_FREE 로
 *   컴파일 시점에 lock-free 를 검사할 수 있다 (아래 _Static_assert).
 *   lock-free 가 아니면 구현이 내부 잠금을 쓸 수 있고, 메인 루프가 잠금을 잡은 채
 *   ISR 이 같은 잠금을 기다리면 교착된다.
 *   크기 제한: ringbuf_t 는 size <= UINT_MAX,
 *             ringbuf_p2_t 는 size <= UINT_MAX/2 + 1 ((C) 는 count 가 0..size 를 표현해야 함).
 *
 * 전제와 보장하지 않는 것
 *   전제: 대상 툴체인이 <stdatomic.h> 를 지원하고 atomic_uint 가 항상 lock-free.
 *   다중 생산자 / 다중 소비자는 지원하지 않는다.
 *   init, reset 을 다른 문맥이 사용 중일 때 호출하면 안 된다.
 *   DMA 가 buf 를 직접 쓰는 경우의 캐시 일관성은 다루지 않는다.
 *   호스트 단위 테스트는 단일 스레드라 동시성 자체는 검증하지 않는다.
 */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __STDC_NO_ATOMICS__
#error "ringbuf requires C11 <stdatomic.h>"
#endif
#include <stdatomic.h>

/* 인덱스 타입. 공유 인덱스는 _Atomic rb_index_t (= atomic_uint) 로 선언한다. */
typedef unsigned int rb_index_t;

/* ISR 에서 쓰려면 잠금 없이 동작해야 한다. 2 == 항상 lock-free. */
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "atomic_uint must be always lock-free");

/* ringbuf_p2_t 는 (C) free-running 방식. 인덱스 오버플로 테스트를 켠다. */
#define RINGBUF_P2_FREE_RUNNING 1

/* 원소 타입. UART 수신 바이트 등을 가정. */
typedef uint8_t rb_elem_t;

typedef enum {
    RB_OK = 0,
    RB_ERR_NULL,   /* 필수 포인터 인자가 NULL */
    RB_ERR_INVAL,  /* 잘못된 크기 (범위 밖, p2 에서 2의 거듭제곱 아님) */
    RB_ERR_EMPTY,  /* 빈 버퍼에서 pop/peek */
    RB_ERR_FULL    /* 가득 찬 버퍼에 push (덮어쓰기 정책 아님: 새 데이터를 버린다) */
} rb_status_t;

/*
 * 임의 크기 링버퍼 - (A) 한 칸 비우기.
 * 필드는 공개되어 있지만 사용자는 직접 접근하지 않는다.
 * 불변식 (init 성공 후):
 *   buf != NULL, 2 <= size <= UINT_MAX
 *   0 <= head < size, 0 <= tail < size   (저장 시점에 되감는다)
 *   empty  <=> head == tail
 *   full   <=> head 를 한 칸 전진시킨 위치 == tail
 *   사용 가능 용량 == size - 1
 * buf, size 는 init 이후 읽기만 하므로 atomic 이 아니다.
 */
typedef struct {
    rb_elem_t           *buf;
    rb_index_t           size;  /* 저장소 원소 수 (사용 가능 용량은 size - 1) */
    _Atomic rb_index_t   head;  /* 생산자 소유: 다음에 쓸 칸 번호 */
    _Atomic rb_index_t   tail;  /* 소비자 소유: 다음에 읽을 칸 번호 */
} ringbuf_t;

/*
 * 2의 거듭제곱 크기 링버퍼 - (C) free-running.
 * 불변식 (init 성공 후):
 *   buf != NULL, size == mask + 1 은 2의 거듭제곱 (1 포함), size <= UINT_MAX/2 + 1
 *   head, tail 은 되감지 않고 증가만 한다. 칸 번호는 접근 시 (idx & mask).
 *   count  == head - tail   (unsigned 모듈러 산술, 0 <= count <= size)
 *   empty  <=> head == tail
 *   full   <=> head - tail == size
 *   사용 가능 용량 == size
 * buf, mask 는 init 이후 읽기만 하므로 atomic 이 아니다.
 */
typedef struct {
    rb_elem_t           *buf;
    rb_index_t           mask;  /* size - 1 */
    _Atomic rb_index_t   head;  /* 생산자 소유: 누적 쓰기 횟수 (마스킹하지 않음) */
    _Atomic rb_index_t   tail;  /* 소비자 소유: 누적 읽기 횟수 (마스킹하지 않음) */
} ringbuf_p2_t;

/* ========================= ringbuf_t ========================= */

/*
 * ringbuf_init
 *   rb   : 초기화할 링버퍼 객체. 메모리는 호출자가 준비한다 (전역/static 권장:
 *          ISR 과 공유하므로 지역 변수면 수명 관리가 어렵다).
 *   buf  : 원소 저장소. 호출자가 소유하며, 링버퍼는 주소만 보관한다.
 *          링버퍼를 사용하는 동안 수명이 유지되어야 한다.
 *   size : buf 의 원소 개수 (= 저장소 크기). 바이트 수가 아니다.
 *          (A) 방식이므로 사용 가능 용량은 size - 1 이다. size 는 2 이상이어야 한다.
 *          예: rb_elem_t storage[64]; -> size = 64, 용량 63
 *   사전: rb, buf 는 NULL 이 아님. buf 는 size 개 원소를 담을 수 있음.
 *   사후(OK): 빈 버퍼. count == 0, capacity == size - 1.
 *   반환: RB_OK
 *         RB_ERR_NULL  - rb 또는 buf 가 NULL (다른 오류보다 우선)
 *         RB_ERR_INVAL - size < 2 (size == 1 이면 사용 가능 용량이 0),
 *                        또는 size > UINT_MAX (rb_index_t 로 표현 불가)
 *   실패 시: *rb 는 수정하지 않는다.
 *   동시성: 동시 접근 없을 때만. head/tail 초기화에는 atomic_init 을 쓴다.
 */
rb_status_t ringbuf_init(ringbuf_t *rb, rb_elem_t *buf, size_t size);

/*
 * ringbuf_push  [생산자 전용]
 *   rb    : init 성공한 링버퍼.
 *   value : 넣을 값. 값으로 복사되어 buf 에 저장된다.
 *   사후(OK): value 가 FIFO 끝에 추가, count 가 1 증가.
 *   반환: RB_OK, RB_ERR_NULL, RB_ERR_FULL
 *   실패 시: 상태 불변 (FULL 이면 value 는 버려진다).
 *   실행 시간: O(1), 루프 없음.
 */
rb_status_t ringbuf_push(ringbuf_t *rb, rb_elem_t value);

/*
 * ringbuf_pop  [소비자 전용]
 *   rb  : init 성공한 링버퍼.
 *   out : [출력] 꺼낸 값을 받을 위치. 호출자가 준비한다. 성공했을 때만 기록된다.
 *   사후(OK): *out == 가장 오래된 원소, count 가 1 감소.
 *   반환: RB_OK, RB_ERR_NULL (rb 또는 out), RB_ERR_EMPTY
 *   실패 시: 상태와 *out 모두 불변.
 *   실행 시간: O(1), 루프 없음.
 */
rb_status_t ringbuf_pop(ringbuf_t *rb, rb_elem_t *out);

/*
 * ringbuf_peek  [소비자 전용]
 *   rb  : init 성공한 링버퍼. 변경되지 않는다 (const).
 *   out : [출력] 가장 오래된 값을 받을 위치. 성공했을 때만 기록된다.
 *   pop 과 같지만 버퍼를 변경하지 않는다.
 *   반환: RB_OK, RB_ERR_NULL, RB_ERR_EMPTY.  실패 시 *out 불변.
 */
rb_status_t ringbuf_peek(const ringbuf_t *rb, rb_elem_t *out);

/*
 * ringbuf_reset  [동시 접근 없을 때만]
 *   rb : init 성공한 링버퍼.
 *   사후(OK): count == 0. buf 내용은 지우지 않는다.
 *   반환: RB_OK, RB_ERR_NULL
 */
rb_status_t ringbuf_reset(ringbuf_t *rb);

/*
 * 조회 함수. rb 는 init 성공한 링버퍼이며 변경되지 않는다 (const).
 * NULL 처리:
 *   count(NULL) == 0, capacity(NULL) == 0, is_empty(NULL) == true, is_full(NULL) == true
 * capacity 는 "사용 가능 용량"(동시에 담을 수 있는 최대 원소 수)이다.
 */
size_t ringbuf_count(const ringbuf_t *rb);
size_t ringbuf_capacity(const ringbuf_t *rb);
bool   ringbuf_is_empty(const ringbuf_t *rb);  /* 소비자 전용 */
bool   ringbuf_is_full(const ringbuf_t *rb);   /* 생산자 전용 */

/* ======================== ringbuf_p2_t ======================== */

/*
 * ringbuf_p2_init
 *   rb   : 초기화할 링버퍼 객체. ringbuf_init 과 동일.
 *   buf  : 원소 저장소. ringbuf_init 과 동일 (호출자 소유, 수명 유지).
 *   size : buf 의 원소 개수. 2의 거듭제곱이어야 한다 (1, 2, 4, 8, ...).
 *          바이트 수가 아니다. 내부에서 mask = size - 1 로 쓰인다.
 *   사전: rb, buf 는 NULL 이 아님. buf 는 size 개 원소를 담을 수 있음.
 *   반환: RB_OK
 *         RB_ERR_NULL  - rb 또는 buf 가 NULL (우선)
 *         RB_ERR_INVAL - size == 0, size 가 2의 거듭제곱이 아님,
 *                        또는 size > UINT_MAX/2 + 1 ((C) 에서 count 가 size 를 표현 못함)
 *   사후(OK): 빈 버퍼. count == 0, capacity == size. size == 1 도 허용 (용량 1).
 *   실패 시: *rb 는 수정하지 않는다.
 *
 * 나머지 함수의 매개변수, 사전/사후 조건, 반환 코드, 동시성 계약은 ringbuf_t 와 동일하다.
 */
rb_status_t ringbuf_p2_init(ringbuf_p2_t *rb, rb_elem_t *buf, size_t size);
rb_status_t ringbuf_p2_push(ringbuf_p2_t *rb, rb_elem_t value);
rb_status_t ringbuf_p2_pop(ringbuf_p2_t *rb, rb_elem_t *out);
rb_status_t ringbuf_p2_peek(const ringbuf_p2_t *rb, rb_elem_t *out);
rb_status_t ringbuf_p2_reset(ringbuf_p2_t *rb);

size_t ringbuf_p2_count(const ringbuf_p2_t *rb);
size_t ringbuf_p2_capacity(const ringbuf_p2_t *rb);
bool   ringbuf_p2_is_empty(const ringbuf_p2_t *rb);
bool   ringbuf_p2_is_full(const ringbuf_p2_t *rb);

#endif /* RINGBUF_H */
