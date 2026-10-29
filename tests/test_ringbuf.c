/*
 * test_ringbuf.c - ringbuf 단계 테스트
 *
 * 동작 테스트는 rb_ops_t 를 통해 ringbuf_t / ringbuf_p2_t 양쪽에 똑같이 돌린다.
 * 헤더에서 선택한 방식을 전제로 한다:
 *   ringbuf_t    = (A) 한 칸 비우기 -> 용량 size - 1, size 1 거부
 *   ringbuf_p2_t = (C) free-running -> 용량 size,     size 1 허용
 * 방식을 바꾸면 OPS_G / OPS_P2 의 cap_delta 와 invariant 를 함께 바꾼다.
 *
 * NULL 인자 테스트는 래퍼를 거치지 않고 실제 함수를 직접 호출한다.
 * (래퍼에서 &r->g 처럼 NULL 의 멤버 주소를 구하는 것 자체가 미정의 동작이다.)
 *
 * 이 파일은 단일 스레드 테스트다. ISR 동시성은 여기서 검증되지 않는다.
 * -> include/ringbuf.h 의 "ISR 안전성 설계" 절과 코드 리뷰로 다룬다.
 */
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ringbuf.h"
#include "test_common.h"

#define MAX_SIZE 16u
#define GUARD    ((rb_elem_t)0xA5)

/* ------------------------------------------------------------------ */
/* 구현 추상화                                                         */
/* ------------------------------------------------------------------ */

typedef union {
    ringbuf_t    g;
    ringbuf_p2_t p;
} rb_any_t;

typedef struct {
    const char *name;
    const size_t *sizes;  /* 테스트할 저장소 크기들 (오름차순) */
    size_t n_sizes;
    size_t cap_delta;     /* 기대 용량 == size - cap_delta  ((A): 1, (C): 0) */
    bool (*invariant)(const rb_any_t *);  /* 화이트박스: 내부 인덱스 불변식 */
    rb_status_t (*init)(rb_any_t *, rb_elem_t *, size_t);
    rb_status_t (*push)(rb_any_t *, rb_elem_t);
    rb_status_t (*pop)(rb_any_t *, rb_elem_t *);
    rb_status_t (*peek)(const rb_any_t *, rb_elem_t *);
    rb_status_t (*reset)(rb_any_t *);
    size_t (*count)(const rb_any_t *);
    size_t (*capacity)(const rb_any_t *);
    bool (*is_empty)(const rb_any_t *);
    bool (*is_full)(const rb_any_t *);
} rb_ops_t;

static rb_status_t g_init(rb_any_t *r, rb_elem_t *b, size_t n) { return ringbuf_init(&r->g, b, n); }
static rb_status_t g_push(rb_any_t *r, rb_elem_t v) { return ringbuf_push(&r->g, v); }
static rb_status_t g_pop(rb_any_t *r, rb_elem_t *o) { return ringbuf_pop(&r->g, o); }
static rb_status_t g_peek(const rb_any_t *r, rb_elem_t *o) { return ringbuf_peek(&r->g, o); }
static rb_status_t g_reset(rb_any_t *r) { return ringbuf_reset(&r->g); }
static size_t g_count(const rb_any_t *r) { return ringbuf_count(&r->g); }
static size_t g_cap(const rb_any_t *r) { return ringbuf_capacity(&r->g); }
static bool g_empty(const rb_any_t *r) { return ringbuf_is_empty(&r->g); }
static bool g_full(const rb_any_t *r) { return ringbuf_is_full(&r->g); }

static rb_status_t p_init(rb_any_t *r, rb_elem_t *b, size_t n) { return ringbuf_p2_init(&r->p, b, n); }
static rb_status_t p_push(rb_any_t *r, rb_elem_t v) { return ringbuf_p2_push(&r->p, v); }
static rb_status_t p_pop(rb_any_t *r, rb_elem_t *o) { return ringbuf_p2_pop(&r->p, o); }
static rb_status_t p_peek(const rb_any_t *r, rb_elem_t *o) { return ringbuf_p2_peek(&r->p, o); }
static rb_status_t p_reset(rb_any_t *r) { return ringbuf_p2_reset(&r->p); }
static size_t p_count(const rb_any_t *r) { return ringbuf_p2_count(&r->p); }
static size_t p_cap(const rb_any_t *r) { return ringbuf_p2_capacity(&r->p); }
static bool p_empty(const rb_any_t *r) { return ringbuf_p2_is_empty(&r->p); }
static bool p_full(const rb_any_t *r) { return ringbuf_p2_is_full(&r->p); }

/* (A): 저장된 인덱스는 항상 [0, size) 안에 있어야 한다 (되감기 누락 검출). */
static bool g_invariant(const rb_any_t *r)
{
    return r->g.size >= 2u && r->g.head < r->g.size && r->g.tail < r->g.size;
}

/* (C): 인덱스는 되감지 않지만 차이는 항상 0..size 범위여야 한다. */
static bool p_invariant(const rb_any_t *r)
{
    /* atomic 객체를 일반 lvalue 로 읽으면 seq_cst load 가 된다. 테스트에서는 충분하다.
     * (const 포인터에 atomic_load 를 쓰는 것은 C11 원문 시그니처상 논란이 있어 피한다) */
    rb_index_t head = r->p.head;
    rb_index_t tail = r->p.tail;

    return (rb_index_t)(head - tail) <= r->p.mask + 1u;
}

static const size_t G_SIZES[]  = { 1u, 2u, 3u, 5u, 8u, 16u };
static const size_t P2_SIZES[] = { 1u, 2u, 4u, 8u, 16u };

static const rb_ops_t OPS_G = {
    "ringbuf", G_SIZES, sizeof G_SIZES / sizeof G_SIZES[0], 1u, g_invariant,
    g_init, g_push, g_pop, g_peek, g_reset, g_count, g_cap, g_empty, g_full
};

static const rb_ops_t OPS_P2 = {
    "ringbuf_p2", P2_SIZES, sizeof P2_SIZES / sizeof P2_SIZES[0], 0u, p_invariant,
    p_init, p_push, p_pop, p_peek, p_reset, p_count, p_cap, p_empty, p_full
};

/* ------------------------------------------------------------------ */
/* 보조 함수                                                           */
/* ------------------------------------------------------------------ */

/* 저장소 앞뒤를 GUARD 로 채워 범위 밖 쓰기를 검출한다.
 * 저장소 = mem[1 .. size], 나머지는 전부 GUARD 여야 한다. */
typedef struct {
    rb_elem_t mem[MAX_SIZE + 2u];
} guarded_t;

static rb_elem_t *gb_init(guarded_t *g, size_t size)
{
    assert(size <= MAX_SIZE);
    memset(g->mem, GUARD, sizeof g->mem);
    return &g->mem[1];
}

static bool gb_intact(const guarded_t *g, size_t size)
{
    if (g->mem[0] != GUARD) {
        return false;
    }
    for (size_t i = size + 1u; i < MAX_SIZE + 2u; i++) {
        if (g->mem[i] != GUARD) {
            return false;
        }
    }
    return true;
}

/* 이 구현이 해당 크기를 거부해야 하는가 (용량이 0 이 되는 크기) */
static bool size_rejected(const rb_ops_t *ops, size_t size)
{
    return size <= ops->cap_delta;
}

/*
 * 초기화 + 기대 용량 확인.
 * 반환 false: 이 크기를 사용할 수 없음.
 *   거부되어야 하는 크기(size_rejected)는 INVAL 이 정상이므로 실패로 세지 않는다.
 *   그 외의 거부, 또는 기대와 다른 용량은 실패로 기록한다.
 */
static bool setup(const rb_ops_t *ops, rb_any_t *r, guarded_t *g, size_t size)
{
    rb_status_t st = ops->init(r, gb_init(g, size), size);

    if (size_rejected(ops, size)) {
        CHECK_EQ(st, RB_ERR_INVAL);
        return false;
    }
    CHECK_EQ(st, RB_OK);
    if (st != RB_OK) {
        return false;
    }

    size_t cap = ops->capacity(r);
    CHECK_EQ(cap, size - ops->cap_delta);
    CHECK_EQ(ops->count(r), 0);
    CHECK(ops->invariant(r));
    return cap == size - ops->cap_delta;
}

/* push/pop 쌍을 n 번 수행해 내부 인덱스를 n 칸 전진시킨다 (버퍼는 빈 상태 유지). */
static bool advance(const rb_ops_t *ops, rb_any_t *r, size_t n)
{
    rb_elem_t out;

    for (size_t i = 0; i < n; i++) {
        if (ops->push(r, (rb_elem_t)i) != RB_OK) {
            CHECK(!"advance: push failed");
            return false;
        }
        if (ops->pop(r, &out) != RB_OK || out != (rb_elem_t)i) {
            CHECK(!"advance: pop failed or wrong value");
            return false;
        }
    }
    return true;
}

/* 결정적 의사난수 (LCG, Numerical Recipes 상수) */
static uint32_t lcg_next(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

/* ------------------------------------------------------------------ */
/* init                                                                */
/* ------------------------------------------------------------------ */

static void t_init_zero_size(const rb_ops_t *ops)
{
    rb_any_t r;
    rb_elem_t buf[1];

    CHECK_EQ(ops->init(&r, buf, 0u), RB_ERR_INVAL);
}

static void t_init_failure_untouched(const rb_ops_t *ops)
{
    rb_any_t r;
    rb_any_t snapshot;
    rb_elem_t buf[MAX_SIZE];

    /* 바이트 단위 비교. atomic_uint 가 lock-free(헤더의 _Static_assert)이므로
     * 객체 표현에 숨은 잠금 데이터가 없다는 전제. 단일 스레드라 경합도 없다. */
    memset(&r, 0x5C, sizeof r);
    memcpy(&snapshot, &r, sizeof r);

    CHECK(ops->init(&r, buf, 0u) != RB_OK);
    CHECK(memcmp(&r, &snapshot, sizeof r) == 0);
    CHECK(ops->init(&r, NULL, 8u) != RB_OK);
    CHECK(memcmp(&r, &snapshot, sizeof r) == 0);
}

static void t_init_empty(const rb_ops_t *ops)
{
    rb_any_t r;
    guarded_t g;

    REQUIRE(setup(ops, &r, &g, 8u));
    CHECK(ops->is_empty(&r));
    CHECK(!ops->is_full(&r));
    CHECK_EQ(ops->count(&r), 0);
}

/* 모든 크기에서 용량이 정확히 size - cap_delta 인지, 거부할 크기는 INVAL 인지.
 * (A) 에서 size 1 거부, (C) 에서 size 1 허용(용량 1)도 여기서 확인된다. */
static void t_capacity_exact(const rb_ops_t *ops)
{
    rb_any_t r;
    rb_elem_t buf[MAX_SIZE];

    for (size_t i = 0; i < ops->n_sizes; i++) {
        size_t size = ops->sizes[i];
        rb_status_t st = ops->init(&r, buf, size);

        if (size_rejected(ops, size)) {
            CHECK_EQ(st, RB_ERR_INVAL);
            continue;
        }
        CHECK_EQ(st, RB_OK);
        if (st == RB_OK) {
            CHECK_EQ(ops->capacity(&r), size - ops->cap_delta);
        }
    }
}

/* ------------------------------------------------------------------ */
/* 빈 상태                                                             */
/* ------------------------------------------------------------------ */

static void t_pop_peek_empty(const rb_ops_t *ops)
{
    rb_any_t r;
    guarded_t g;
    rb_elem_t out = 0x3C;

    REQUIRE(setup(ops, &r, &g, 8u));

    CHECK_EQ(ops->pop(&r, &out), RB_ERR_EMPTY);
    CHECK_EQ(out, 0x3C);
    CHECK_EQ(ops->peek(&r, &out), RB_ERR_EMPTY);
    CHECK_EQ(out, 0x3C);
    CHECK_EQ(ops->count(&r), 0);
    CHECK(ops->is_empty(&r));

    /* 빈 상태 pop 이 인덱스를 움직이지 않았는지: 이후 정상 동작 확인 */
    CHECK_EQ(ops->push(&r, 1), RB_OK);
    CHECK_EQ(ops->count(&r), 1);
    CHECK_EQ(ops->pop(&r, &out), RB_OK);
    CHECK_EQ(out, 1);
}

static void t_pop_empty_after_drain(const rb_ops_t *ops)
{
    rb_any_t r;
    guarded_t g;
    rb_elem_t out;

    REQUIRE(setup(ops, &r, &g, 8u));
    REQUIRE(ops->push(&r, 10) == RB_OK);
    REQUIRE(ops->push(&r, 11) == RB_OK);
    REQUIRE(ops->pop(&r, &out) == RB_OK);
    REQUIRE(ops->pop(&r, &out) == RB_OK);

    out = 0x77;
    CHECK_EQ(ops->pop(&r, &out), RB_ERR_EMPTY);
    CHECK_EQ(out, 0x77);
    CHECK(ops->is_empty(&r));
    CHECK_EQ(ops->count(&r), 0);
}

/* ------------------------------------------------------------------ */
/* 가득 찬 상태                                                        */
/* ------------------------------------------------------------------ */

static void t_push_full(const rb_ops_t *ops)
{
    rb_any_t r;
    guarded_t g;
    rb_elem_t out;

    REQUIRE(setup(ops, &r, &g, 8u));
    size_t cap = ops->capacity(&r);

    for (size_t i = 0; i < cap; i++) {
        CHECK(!ops->is_full(&r));
        CHECK_EQ(ops->push(&r, (rb_elem_t)(100u + i)), RB_OK);
        CHECK_EQ(ops->count(&r), i + 1u);
    }
    CHECK(ops->is_full(&r));
    CHECK(!ops->is_empty(&r));

    CHECK_EQ(ops->push(&r, 0xEE), RB_ERR_FULL);
    CHECK_EQ(ops->push(&r, 0xEE), RB_ERR_FULL);
    CHECK_EQ(ops->count(&r), cap);
    CHECK(gb_intact(&g, 8u));

    /* FULL 거부가 가장 오래된/가장 최근 원소를 덮어쓰지 않았는지 */
    for (size_t i = 0; i < cap; i++) {
        REQUIRE(ops->pop(&r, &out) == RB_OK);
        CHECK_EQ(out, (rb_elem_t)(100u + i));
    }
    CHECK(ops->is_empty(&r));
}

static void t_full_pop_one_push_one(const rb_ops_t *ops)
{
    rb_any_t r;
    guarded_t g;
    rb_elem_t out;

    REQUIRE(setup(ops, &r, &g, 8u));
    size_t cap = ops->capacity(&r);

    for (size_t i = 0; i < cap; i++) {
        REQUIRE(ops->push(&r, (rb_elem_t)i) == RB_OK);
    }
    /* full 상태에서 한 칸씩 비우고 채우기를 size 의 2배만큼 반복 -> 여러 번 랩어라운드 */
    for (size_t i = 0; i < 2u * 8u; i++) {
        REQUIRE(ops->pop(&r, &out) == RB_OK);
        CHECK_EQ(out, (rb_elem_t)i);
        CHECK(!ops->is_full(&r));
        CHECK_EQ(ops->push(&r, (rb_elem_t)(i + cap)), RB_OK);
        CHECK(ops->is_full(&r));
        CHECK_EQ(ops->push(&r, 0xEE), RB_ERR_FULL);
    }
    CHECK(gb_intact(&g, 8u));
}

/* ------------------------------------------------------------------ */
/* 순서 / 값                                                           */
/* ------------------------------------------------------------------ */

static void t_fifo_order(const rb_ops_t *ops)
{
    rb_any_t r;
    guarded_t g;
    rb_elem_t out;

    REQUIRE(setup(ops, &r, &g, 8u));
    size_t cap = ops->capacity(&r);

    for (size_t i = 0; i < cap; i++) {
        REQUIRE(ops->push(&r, (rb_elem_t)(i * 3u)) == RB_OK);
    }
    for (size_t i = 0; i < cap; i++) {
        REQUIRE(ops->pop(&r, &out) == RB_OK);
        CHECK_EQ(out, (rb_elem_t)(i * 3u));
        CHECK_EQ(ops->count(&r), cap - i - 1u);
    }
}

static void t_peek_no_modify(const rb_ops_t *ops)
{
    rb_any_t r;
    guarded_t g;
    rb_elem_t out = 0;

    REQUIRE(setup(ops, &r, &g, 8u));
    REQUIRE(ops->push(&r, 7) == RB_OK);
    REQUIRE(ops->push(&r, 8) == RB_OK);

    CHECK_EQ(ops->peek(&r, &out), RB_OK);
    CHECK_EQ(out, 7);              /* 가장 오래된 원소 */
    CHECK_EQ(ops->peek(&r, &out), RB_OK);
    CHECK_EQ(out, 7);
    CHECK_EQ(ops->count(&r), 2);
    REQUIRE(ops->pop(&r, &out) == RB_OK);
    CHECK_EQ(out, 7);
}

/* 0..255 모든 바이트 값이 손실/부호 확장 없이 통과하는지 */
static void t_all_byte_values(const rb_ops_t *ops)
{
    rb_any_t r;
    guarded_t g;
    rb_elem_t out;

    REQUIRE(setup(ops, &r, &g, 16u));
    size_t cap = ops->capacity(&r);
    unsigned next_in = 0;
    unsigned next_out = 0;

    while (next_out < 256u) {
        while (next_in < 256u && ops->count(&r) < cap) {
            REQUIRE(ops->push(&r, (rb_elem_t)next_in) == RB_OK);
            next_in++;
        }
        REQUIRE(ops->pop(&r, &out) == RB_OK);
        CHECK_EQ(out, next_out);
        next_out++;
    }
    CHECK(ops->is_empty(&r));
    CHECK(gb_intact(&g, 16u));
}

/* ------------------------------------------------------------------ */
/* 랩어라운드                                                          */
/* ------------------------------------------------------------------ */

/*
 * 쓰기 위치가 저장소 마지막 칸 직전/마지막/첫 칸(랩 직후)/그 다음일 때
 * 2개를 넣고 빼서 경계를 가로지르는 경우를 명시적으로 확인한다.
 * (인덱스가 0 에서 시작한다고 가정: push/pop 쌍 k 번 후 쓰기 위치 == k mod size)
 */
static void t_wrap_boundary(const rb_ops_t *ops)
{
    const size_t size = 8u;
    const size_t offsets[] = { size - 2u, size - 1u, size, size + 1u };

    for (size_t oi = 0; oi < sizeof offsets / sizeof offsets[0]; oi++) {
        rb_any_t r;
        guarded_t g;
        rb_elem_t out;

        REQUIRE(setup(ops, &r, &g, size));
        REQUIRE(advance(ops, &r, offsets[oi]));
        CHECK(ops->is_empty(&r));

        CHECK_EQ(ops->push(&r, 0xA1), RB_OK);
        CHECK_EQ(ops->push(&r, 0xA2), RB_OK);
        CHECK_EQ(ops->count(&r), 2);
        CHECK_EQ(ops->peek(&r, &out), RB_OK);
        CHECK_EQ(out, 0xA1);
        CHECK_EQ(ops->pop(&r, &out), RB_OK);
        CHECK_EQ(out, 0xA1);
        CHECK_EQ(ops->pop(&r, &out), RB_OK);
        CHECK_EQ(out, 0xA2);
        CHECK(ops->is_empty(&r));
        CHECK(gb_intact(&g, size));
    }
}

/* 모든 크기 x 모든 시작 오프셋(0 .. 2*size+1)에서 full 까지 채우고 비우기 */
static void t_wrap_sweep(const rb_ops_t *ops)
{
    for (size_t si = 0; si < ops->n_sizes; si++) {
        size_t size = ops->sizes[si];

        for (size_t off = 0; off <= 2u * size + 1u; off++) {
            rb_any_t r;
            guarded_t g;
            rb_elem_t out;

            if (!setup(ops, &r, &g, size)) {
                break;
            }
            size_t cap = ops->capacity(&r);
            REQUIRE(advance(ops, &r, off));

            CHECK(ops->invariant(&r));

            for (size_t i = 0; i < cap; i++) {
                REQUIRE(ops->push(&r, (rb_elem_t)(off + i)) == RB_OK);
                CHECK(ops->invariant(&r));
            }
            CHECK(ops->is_full(&r));
            CHECK_EQ(ops->count(&r), cap);
            CHECK_EQ(ops->push(&r, 0xEE), RB_ERR_FULL);
            CHECK(ops->invariant(&r));

            for (size_t i = 0; i < cap; i++) {
                REQUIRE(ops->pop(&r, &out) == RB_OK);
                CHECK_EQ(out, (rb_elem_t)(off + i));
                CHECK(ops->invariant(&r));
            }
            CHECK(ops->is_empty(&r));
            CHECK_EQ(ops->pop(&r, &out), RB_ERR_EMPTY);
            CHECK(gb_intact(&g, size));
        }
    }
}

/* ------------------------------------------------------------------ */
/* 용량 1                                                              */
/* ------------------------------------------------------------------ */

static void t_capacity_one(const rb_ops_t *ops)
{
    size_t size = 1u + ops->cap_delta;  /* 용량 1 이 되는 저장소 크기 */
    rb_any_t r;
    guarded_t g;
    rb_elem_t out = 0;

    REQUIRE(setup(ops, &r, &g, size));
    REQUIRE(ops->capacity(&r) == 1u);

    /* 여러 사이클 반복 -> 매 사이클 인덱스가 랩어라운드 */
    for (unsigned cycle = 0; cycle < 5u; cycle++) {
        CHECK(ops->is_empty(&r));
        CHECK(!ops->is_full(&r));
        CHECK_EQ(ops->pop(&r, &out), RB_ERR_EMPTY);

        CHECK_EQ(ops->push(&r, (rb_elem_t)(cycle + 1u)), RB_OK);
        CHECK(ops->is_full(&r));
        CHECK(!ops->is_empty(&r));
        CHECK_EQ(ops->count(&r), 1);
        CHECK_EQ(ops->push(&r, 0xEE), RB_ERR_FULL);

        CHECK_EQ(ops->pop(&r, &out), RB_OK);
        CHECK_EQ(out, cycle + 1u);
    }
    CHECK(gb_intact(&g, size));
}

/* ------------------------------------------------------------------ */
/* reset                                                               */
/* ------------------------------------------------------------------ */

static void t_reset(const rb_ops_t *ops)
{
    rb_any_t r;
    guarded_t g;
    rb_elem_t out;

    REQUIRE(setup(ops, &r, &g, 8u));
    size_t cap = ops->capacity(&r);
    REQUIRE(advance(ops, &r, 5u));
    REQUIRE(ops->push(&r, 1) == RB_OK);
    REQUIRE(ops->push(&r, 2) == RB_OK);

    CHECK_EQ(ops->reset(&r), RB_OK);
    CHECK(ops->is_empty(&r));
    CHECK_EQ(ops->count(&r), 0);
    CHECK_EQ(ops->capacity(&r), cap);
    CHECK_EQ(ops->pop(&r, &out), RB_ERR_EMPTY);

    for (size_t i = 0; i < cap; i++) {
        CHECK_EQ(ops->push(&r, (rb_elem_t)(50u + i)), RB_OK);
    }
    CHECK(ops->is_full(&r));
    for (size_t i = 0; i < cap; i++) {
        REQUIRE(ops->pop(&r, &out) == RB_OK);
        CHECK_EQ(out, (rb_elem_t)(50u + i));
    }
}

/* ------------------------------------------------------------------ */
/* 참조 모델 대비 무작위 시퀀스                                         */
/* ------------------------------------------------------------------ */

static void t_model_random(const rb_ops_t *ops)
{
    const int failures_before = g_failures;

    for (size_t si = 0; si < ops->n_sizes; si++) {
        size_t size = ops->sizes[si];
        rb_any_t r;
        guarded_t g;

        if (!setup(ops, &r, &g, size)) {
            continue;
        }
        size_t cap = ops->capacity(&r);

        /* 모델: 단순 선형 배열 큐 (앞에서 빼면 shift) */
        rb_elem_t model[MAX_SIZE];
        size_t model_n = 0;
        uint32_t seed = 0x1234u + (uint32_t)size;

        for (unsigned step = 0; step < 20000u; step++) {
            uint32_t x = lcg_next(&seed);
            rb_elem_t v = (rb_elem_t)(x >> 24);
            rb_elem_t out = 0;

            if ((x & 1u) != 0u) {
                rb_status_t st = ops->push(&r, v);
                if (model_n < cap) {
                    CHECK_EQ(st, RB_OK);
                    model[model_n++] = v;
                } else {
                    CHECK_EQ(st, RB_ERR_FULL);
                }
            } else {
                rb_status_t st = ops->pop(&r, &out);
                if (model_n > 0u) {
                    CHECK_EQ(st, RB_OK);
                    CHECK_EQ(out, model[0]);
                    memmove(&model[0], &model[1], (model_n - 1u) * sizeof model[0]);
                    model_n--;
                } else {
                    CHECK_EQ(st, RB_ERR_EMPTY);
                }
            }
            CHECK_EQ(ops->count(&r), model_n);
            CHECK(ops->is_empty(&r) == (model_n == 0u));
            CHECK(ops->is_full(&r) == (model_n == cap));
            CHECK(ops->invariant(&r));
            if (g_failures != failures_before) {
                return;  /* 첫 불일치에서 멈춰 출력 폭주 방지 */
            }
        }
        CHECK(gb_intact(&g, size));
    }
}

/* ------------------------------------------------------------------ */
/* 구현별 테스트                                                       */
/* ------------------------------------------------------------------ */

static void t_g_accepts_non_pow2(void)
{
    ringbuf_t r;
    rb_elem_t buf[MAX_SIZE];
    const size_t sizes[] = { 3u, 5u, 6u, 7u, 12u, 15u };

    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        CHECK_EQ(ringbuf_init(&r, buf, sizes[i]), RB_OK);
    }
}

/* rb_index_t(unsigned int) 로 표현할 수 없는 크기는 거부.
 * buf 가 그만큼 크지 않지만 INVAL 경로에서 buf 를 건드리면 안 된다. */
static void t_g_rejects_too_large(void)
{
#if SIZE_MAX > UINT_MAX
    ringbuf_t r;
    rb_elem_t buf[MAX_SIZE];
    const size_t sizes[] = { (size_t)UINT_MAX + 1u, SIZE_MAX };

    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        CHECK_EQ(ringbuf_init(&r, buf, sizes[i]), RB_ERR_INVAL);
    }
#endif
}

static void t_p2_rejects_non_pow2(void)
{
    ringbuf_p2_t r;
    rb_elem_t buf[MAX_SIZE];
    /* SIZE_MAX 등 큰 값: buf 가 그만큼 크지 않지만 INVAL 경로에서 buf 를 건드리면 안 된다 */
    const size_t sizes[] = { 3u, 5u, 6u, 7u, 9u, 12u, 15u, SIZE_MAX, SIZE_MAX - 1u };

    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        CHECK_EQ(ringbuf_p2_init(&r, buf, sizes[i]), RB_ERR_INVAL);
    }
}

/* (C) 에서 count 는 0..size 를 표현해야 하므로 size <= UINT_MAX/2 + 1.
 * 그보다 큰 2의 거듭제곱은 거부해야 한다. */
static void t_p2_rejects_too_large(void)
{
#if SIZE_MAX > UINT_MAX
    ringbuf_p2_t r;
    rb_elem_t buf[MAX_SIZE];
    const size_t too_large = ((size_t)UINT_MAX / 2u + 1u) * 2u;  /* 2의 거듭제곱 */

    CHECK_EQ(ringbuf_p2_init(&r, buf, too_large), RB_ERR_INVAL);
#endif
}

static void t_p2_accepts_pow2(void)
{
    ringbuf_p2_t r;
    rb_elem_t buf[MAX_SIZE];

    for (size_t size = 2u; size <= MAX_SIZE; size <<= 1) {
        CHECK_EQ(ringbuf_p2_init(&r, buf, size), RB_OK);
    }
}

#ifdef RINGBUF_P2_FREE_RUNNING
/* (C) free-running 인덱스: UINT_MAX -> 0 오버플로를 가로질러도 count/순서가 맞는지.
 * 화이트박스 테스트: init 후 head/tail 을 직접 오버플로 직전으로 옮긴다. */
static void t_p2_index_overflow(void)
{
    ringbuf_p2_t r;
    guarded_t g;
    rb_elem_t out;
    const size_t size = 8u;

    REQUIRE(ringbuf_p2_init(&r, gb_init(&g, size), size) == RB_OK);
    atomic_store_explicit(&r.head, UINT_MAX - 2u, memory_order_relaxed);
    atomic_store_explicit(&r.tail, UINT_MAX - 2u, memory_order_relaxed);
    REQUIRE(ringbuf_p2_is_empty(&r));

    for (size_t i = 0; i < size; i++) {
        CHECK_EQ(ringbuf_p2_push(&r, (rb_elem_t)i), RB_OK);
        CHECK_EQ(ringbuf_p2_count(&r), i + 1u);
    }
    CHECK(ringbuf_p2_is_full(&r));
    CHECK_EQ(ringbuf_p2_push(&r, 0xEE), RB_ERR_FULL);
    for (size_t i = 0; i < size; i++) {
        REQUIRE(ringbuf_p2_pop(&r, &out) == RB_OK);
        CHECK_EQ(out, (rb_elem_t)i);
    }
    CHECK(ringbuf_p2_is_empty(&r));
    CHECK(gb_intact(&g, size));
}
#endif

/* ------------------------------------------------------------------ */
/* NULL 인자 (직접 호출)                                               */
/* ------------------------------------------------------------------ */

static void t_g_null_args(void)
{
    ringbuf_t r;
    rb_elem_t buf[8];
    rb_elem_t out = 0x42;

    CHECK_EQ(ringbuf_init(NULL, buf, 8u), RB_ERR_NULL);
    CHECK_EQ(ringbuf_init(&r, NULL, 8u), RB_ERR_NULL);
    CHECK_EQ(ringbuf_init(NULL, NULL, 0u), RB_ERR_NULL);   /* NULL 이 INVAL 보다 우선 */
    CHECK_EQ(ringbuf_init(&r, NULL, 0u), RB_ERR_NULL);
    CHECK_EQ(ringbuf_push(NULL, 1), RB_ERR_NULL);
    CHECK_EQ(ringbuf_pop(NULL, &out), RB_ERR_NULL);
    CHECK_EQ(ringbuf_peek(NULL, &out), RB_ERR_NULL);
    CHECK_EQ(ringbuf_reset(NULL), RB_ERR_NULL);
    CHECK_EQ(out, 0x42);

    CHECK_EQ(ringbuf_count(NULL), 0);
    CHECK_EQ(ringbuf_capacity(NULL), 0);
    CHECK(ringbuf_is_empty(NULL));
    CHECK(ringbuf_is_full(NULL));

    REQUIRE(ringbuf_init(&r, buf, 8u) == RB_OK);
    REQUIRE(ringbuf_push(&r, 9) == RB_OK);
    CHECK_EQ(ringbuf_pop(&r, NULL), RB_ERR_NULL);   /* 비어 있지 않아도 NULL 우선 */
    CHECK_EQ(ringbuf_count(&r), 1);                 /* 원소를 잃지 않음 */
    CHECK_EQ(ringbuf_peek(&r, NULL), RB_ERR_NULL);
}

static void t_p2_null_args(void)
{
    ringbuf_p2_t r;
    rb_elem_t buf[8];
    rb_elem_t out = 0x42;

    CHECK_EQ(ringbuf_p2_init(NULL, buf, 8u), RB_ERR_NULL);
    CHECK_EQ(ringbuf_p2_init(&r, NULL, 8u), RB_ERR_NULL);
    CHECK_EQ(ringbuf_p2_init(NULL, NULL, 3u), RB_ERR_NULL);  /* NULL 이 INVAL 보다 우선 */
    CHECK_EQ(ringbuf_p2_push(NULL, 1), RB_ERR_NULL);
    CHECK_EQ(ringbuf_p2_pop(NULL, &out), RB_ERR_NULL);
    CHECK_EQ(ringbuf_p2_peek(NULL, &out), RB_ERR_NULL);
    CHECK_EQ(ringbuf_p2_reset(NULL), RB_ERR_NULL);
    CHECK_EQ(out, 0x42);

    CHECK_EQ(ringbuf_p2_count(NULL), 0);
    CHECK_EQ(ringbuf_p2_capacity(NULL), 0);
    CHECK(ringbuf_p2_is_empty(NULL));
    CHECK(ringbuf_p2_is_full(NULL));

    REQUIRE(ringbuf_p2_init(&r, buf, 8u) == RB_OK);
    REQUIRE(ringbuf_p2_push(&r, 9) == RB_OK);
    CHECK_EQ(ringbuf_p2_pop(&r, NULL), RB_ERR_NULL);
    CHECK_EQ(ringbuf_p2_count(&r), 1);
    CHECK_EQ(ringbuf_p2_peek(&r, NULL), RB_ERR_NULL);
}

/* ------------------------------------------------------------------ */

#define RUN_RB(fn)                                \
    do {                                          \
        RUN_TEST_ARG(fn, &OPS_G, OPS_G.name);     \
        RUN_TEST_ARG(fn, &OPS_P2, OPS_P2.name);   \
    } while (0)

int main(void)
{
    RUN_RB(t_init_zero_size);
    RUN_RB(t_init_failure_untouched);
    RUN_RB(t_init_empty);
    RUN_RB(t_capacity_exact);
    RUN_RB(t_pop_peek_empty);
    RUN_RB(t_pop_empty_after_drain);
    RUN_RB(t_push_full);
    RUN_RB(t_full_pop_one_push_one);
    RUN_RB(t_fifo_order);
    RUN_RB(t_peek_no_modify);
    RUN_RB(t_all_byte_values);
    RUN_RB(t_wrap_boundary);
    RUN_RB(t_wrap_sweep);
    RUN_RB(t_capacity_one);
    RUN_RB(t_reset);
    RUN_RB(t_model_random);

    RUN_TEST(t_g_accepts_non_pow2);
    RUN_TEST(t_g_rejects_too_large);
    RUN_TEST(t_p2_rejects_too_large);
    RUN_TEST(t_p2_rejects_non_pow2);
    RUN_TEST(t_p2_accepts_pow2);
#ifdef RINGBUF_P2_FREE_RUNNING
    RUN_TEST(t_p2_index_overflow);
#endif
    RUN_TEST(t_g_null_args);
    RUN_TEST(t_p2_null_args);

    return TEST_REPORT();
}
