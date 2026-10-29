/*
 * test_list_v2.c - 연결 리스트 v2 (포인터 + 풀 노드) 테스트
 *
 * 3단계 pool 구현이 끝나야 의미 있게 돈다 (setup 이 pool_init 에 의존).
 * 노드 누수는 "리스트 크기 + 풀의 빈 블록 수 == 풀 용량" 으로 확인한다.
 */
#include <stdint.h>
#include <string.h>

#include "list_v2.h"
#include "pool.h"
#include "test_common.h"

#define ARENA   1024u
#define MAX_N   (ARENA / sizeof(struct pool_free_node))   /* 풀 용량의 상한 */

typedef struct {
    _Alignas(max_align_t) unsigned char mem[ARENA];
} arena_t;

/* ------------------------------------------------------------------ */
/* 보조 함수                                                           */
/* ------------------------------------------------------------------ */

static bool make_pool(pool_t *p, arena_t *a)
{
    return pool_init(p, a->mem, sizeof a->mem, sizeof(list2_node_t)) == POOL_OK;
}

/* 노드 누수/이중 반환 검사: 사용 중 노드 수 == 리스트 크기 합 */
static bool no_leak(const pool_t *p, size_t total_in_lists)
{
    return pool_free_count(p) + total_in_lists == pool_capacity(p);
}

/* 순환이면 SIZE_MAX */
static size_t collect(const list2_t *l, list2_elem_t *out, size_t max)
{
    size_t n = 0;

    for (const list2_node_t *it = list2_first(l); it != NULL; it = list2_next(it)) {
        if (n >= max) {
            return SIZE_MAX;
        }
        out[n++] = it->value;
    }
    return n;
}

static void expect_list(const list2_t *l, const list2_elem_t *exp, size_t n)
{
    list2_elem_t got[MAX_N + 1u];
    size_t m = collect(l, got, MAX_N + 1u);

    CHECK_EQ(m, n);
    CHECK_EQ(list2_size(l), n);
    CHECK(list2_is_empty(l) == (n == 0u));
    if (m == n) {
        for (size_t i = 0; i < n; i++) {
            CHECK_EQ(got[i], exp[i]);
        }
    }
}

#define EXPECT(l, ...)                                                     \
    expect_list((l), (const list2_elem_t[]){ __VA_ARGS__ },                \
                sizeof((const list2_elem_t[]){ __VA_ARGS__ }) / sizeof(list2_elem_t))

static void expect_empty(const list2_t *l)
{
    CHECK_EQ(list2_size(l), 0);
    CHECK(list2_is_empty(l));
    CHECK(list2_first(l) == NULL);
}

/* 1..n 을 push_back 한 리스트 */
static bool fill_seq(list2_t *l, int32_t n)
{
    for (int32_t v = 1; v <= n; v++) {
        if (list2_push_back(l, v) != LIST2_OK) {
            return false;
        }
    }
    return true;
}

static bool pred_even(list2_elem_t v, void *ctx)
{
    (void)ctx;
    return (v % 2) == 0;
}

static bool pred_all(list2_elem_t v, void *ctx)
{
    (void)v;
    (void)ctx;
    return true;
}

static bool pred_none(list2_elem_t v, void *ctx)
{
    (void)v;
    (void)ctx;
    return false;
}

/* ctx 로 받은 값과 같으면 지우고, 호출 횟수를 센다 */
typedef struct {
    list2_elem_t target;
    size_t       calls;
} eq_ctx_t;

static bool pred_eq_count(list2_elem_t v, void *ctx)
{
    eq_ctx_t *c = ctx;
    c->calls++;
    return v == c->target;
}

static uint32_t lcg_next(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

/* ------------------------------------------------------------------ */
/* init                                                                */
/* ------------------------------------------------------------------ */

static void t_init_null_args(void)
{
    list2_t l;
    pool_t p;
    arena_t a;

    REQUIRE(make_pool(&p, &a));
    CHECK_EQ(list2_init(NULL, &p), LIST2_ERR_NULL);
    CHECK_EQ(list2_init(&l, NULL), LIST2_ERR_NULL);
}

static void t_init_pool_too_small(void)
{
    list2_t l;
    list2_t snapshot;
    pool_t p;
    arena_t a;

    /* 블록 크기가 노드보다 작은 풀: POOL_ALIGN 올림 때문에 실제로 작아지는 경우에만 의미가 있다 */
    REQUIRE(pool_init(&p, a.mem, sizeof a.mem, 1u) == POOL_OK);
    memset(&l, 0x5C, sizeof l);
    memcpy(&snapshot, &l, sizeof l);
    if (pool_block_size(&p) < sizeof(list2_node_t)) {
        CHECK_EQ(list2_init(&l, &p), LIST2_ERR_INVAL);
        CHECK(memcmp(&l, &snapshot, sizeof l) == 0);
    } else {
        CHECK_EQ(list2_init(&l, &p), LIST2_OK);
    }
}

static void t_init_empty(void)
{
    list2_t l;
    pool_t p;
    arena_t a;

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);
    expect_empty(&l);
    CHECK(!list2_contains(&l, 0));
    CHECK(no_leak(&p, 0u));
}

/* ------------------------------------------------------------------ */
/* push / pop                                                          */
/* ------------------------------------------------------------------ */

static void t_push_order(void)
{
    list2_t l;
    pool_t p;
    arena_t a;

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);

    REQUIRE(list2_push_back(&l, 1) == LIST2_OK);   /* 빈 리스트에 push_back */
    REQUIRE(list2_push_back(&l, 2) == LIST2_OK);
    REQUIRE(list2_push_front(&l, 0) == LIST2_OK);
    REQUIRE(list2_push_back(&l, 3) == LIST2_OK);
    EXPECT(&l, 0, 1, 2, 3);
    CHECK(no_leak(&p, 4u));
}

static void t_pop(void)
{
    list2_t l;
    pool_t p;
    arena_t a;
    list2_elem_t out = 55;

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);

    CHECK_EQ(list2_pop_front(&l, &out), LIST2_ERR_EMPTY);
    CHECK_EQ(out, 55);

    REQUIRE(fill_seq(&l, 3));
    for (int32_t v = 1; v <= 3; v++) {
        CHECK_EQ(list2_pop_front(&l, &out), LIST2_OK);
        CHECK_EQ(out, v);
        CHECK(no_leak(&p, (size_t)(3 - v)));
    }
    expect_empty(&l);
    CHECK_EQ(list2_pop_front(&l, &out), LIST2_ERR_EMPTY);
}

/* 풀 고갈: 실패한 push 는 리스트를 바꾸지 않고, 노드를 잃지 않는다 */
static void t_pool_exhausted(void)
{
    list2_t l;
    pool_t p;
    arena_t a;
    list2_elem_t out;

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);
    size_t cap = pool_capacity(&p);
    REQUIRE(cap <= MAX_N);

    for (size_t i = 0; i < cap; i++) {
        REQUIRE(list2_push_back(&l, (list2_elem_t)i) == LIST2_OK);
    }
    CHECK_EQ(list2_push_back(&l, 99), LIST2_ERR_NOMEM);
    CHECK_EQ(list2_push_front(&l, 99), LIST2_ERR_NOMEM);
    CHECK_EQ(list2_size(&l), cap);
    CHECK(!list2_contains(&l, 99));
    CHECK(no_leak(&p, cap));

    REQUIRE(list2_pop_front(&l, &out) == LIST2_OK);
    CHECK_EQ(list2_push_back(&l, 99), LIST2_OK);   /* 반환된 노드 재사용 */
    CHECK(no_leak(&p, cap));
}

/* ------------------------------------------------------------------ */
/* remove (이중 포인터)                                                */
/* ------------------------------------------------------------------ */

static void t_remove_positions(void)
{
    list2_t l;
    pool_t p;
    arena_t a;

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);
    REQUIRE(fill_seq(&l, 5));

    CHECK_EQ(list2_remove(&l, 1), LIST2_OK);   /* head */
    EXPECT(&l, 2, 3, 4, 5);
    CHECK_EQ(list2_remove(&l, 3), LIST2_OK);   /* 중간 */
    EXPECT(&l, 2, 4, 5);
    CHECK_EQ(list2_remove(&l, 5), LIST2_OK);   /* 마지막 */
    EXPECT(&l, 2, 4);
    CHECK_EQ(list2_push_back(&l, 6), LIST2_OK); /* 마지막 삭제 후 push_back */
    EXPECT(&l, 2, 4, 6);
    CHECK_EQ(list2_remove(&l, 42), LIST2_ERR_NOT_FOUND);
    EXPECT(&l, 2, 4, 6);
    CHECK(no_leak(&p, 3u));
}

static void t_remove_only_and_duplicates(void)
{
    list2_t l;
    pool_t p;
    arena_t a;

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);

    CHECK_EQ(list2_remove(&l, 1), LIST2_ERR_NOT_FOUND);  /* 빈 리스트 */
    REQUIRE(list2_push_back(&l, 1) == LIST2_OK);
    CHECK_EQ(list2_remove(&l, 1), LIST2_OK);              /* 유일한 원소 */
    expect_empty(&l);
    CHECK(no_leak(&p, 0u));

    REQUIRE(list2_push_back(&l, 7) == LIST2_OK);
    REQUIRE(list2_push_back(&l, 8) == LIST2_OK);
    REQUIRE(list2_push_back(&l, 7) == LIST2_OK);
    CHECK_EQ(list2_remove(&l, 7), LIST2_OK);              /* 첫 번째 7 만 */
    EXPECT(&l, 8, 7);
    CHECK(no_leak(&p, 2u));
}

/* ------------------------------------------------------------------ */
/* remove_if (이중 포인터)                                             */
/* ------------------------------------------------------------------ */

static void t_remove_if_patterns(void)
{
    list2_t l;
    pool_t p;
    arena_t a;
    size_t removed = 999;

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);

    /* 짝수 제거: 연속하지 않은 삭제, 마지막 원소 삭제 포함 */
    REQUIRE(fill_seq(&l, 6));
    CHECK_EQ(list2_remove_if(&l, pred_even, NULL, &removed), LIST2_OK);
    CHECK_EQ(removed, 3);
    EXPECT(&l, 1, 3, 5);

    /* 아무것도 안 지움 */
    CHECK_EQ(list2_remove_if(&l, pred_none, NULL, &removed), LIST2_OK);
    CHECK_EQ(removed, 0);
    EXPECT(&l, 1, 3, 5);

    /* 전부 지움: 연속한 head 삭제 */
    CHECK_EQ(list2_remove_if(&l, pred_all, NULL, &removed), LIST2_OK);
    CHECK_EQ(removed, 3);
    expect_empty(&l);
    CHECK(no_leak(&p, 0u));

    /* 빈 리스트 */
    CHECK_EQ(list2_remove_if(&l, pred_all, NULL, &removed), LIST2_OK);
    CHECK_EQ(removed, 0);

    /* removed == NULL 허용 */
    REQUIRE(fill_seq(&l, 4));
    CHECK_EQ(list2_remove_if(&l, pred_even, NULL, NULL), LIST2_OK);
    EXPECT(&l, 1, 3);
    CHECK(no_leak(&p, 2u));
}

static void t_remove_if_consecutive_head(void)
{
    list2_t l;
    pool_t p;
    arena_t a;
    size_t removed;
    eq_ctx_t ctx = { 5, 0u };

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);
    const list2_elem_t vals[] = { 5, 5, 5, 1, 5, 2, 5, 5 };
    for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        REQUIRE(list2_push_back(&l, vals[i]) == LIST2_OK);
    }

    CHECK_EQ(list2_remove_if(&l, pred_eq_count, &ctx, &removed), LIST2_OK);
    CHECK_EQ(removed, 6);
    CHECK_EQ(ctx.calls, 8);                    /* 원소당 정확히 한 번 */
    EXPECT(&l, 1, 2);
    CHECK(no_leak(&p, 2u));
}

/* ------------------------------------------------------------------ */
/* clear / 풀 공유                                                     */
/* ------------------------------------------------------------------ */

static void t_clear(void)
{
    list2_t l;
    pool_t p;
    arena_t a;

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);
    CHECK_EQ(list2_clear(&l), LIST2_OK);       /* 빈 리스트 clear */
    REQUIRE(fill_seq(&l, 10));
    CHECK_EQ(list2_clear(&l), LIST2_OK);
    expect_empty(&l);
    CHECK(no_leak(&p, 0u));
    REQUIRE(fill_seq(&l, 3));                  /* clear 후 재사용 */
    EXPECT(&l, 1, 2, 3);
}

static void t_shared_pool(void)
{
    list2_t a_list, b_list;
    pool_t p;
    arena_t a;

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&a_list, &p) == LIST2_OK);
    REQUIRE(list2_init(&b_list, &p) == LIST2_OK);
    size_t cap = pool_capacity(&p);
    REQUIRE(cap >= 2u);

    /* 두 리스트가 번갈아 풀을 다 쓴다 */
    size_t na = 0, nb = 0;
    for (size_t i = 0; i < cap; i++) {
        if (i % 2u == 0u) {
            REQUIRE(list2_push_back(&a_list, (list2_elem_t)i) == LIST2_OK);
            na++;
        } else {
            REQUIRE(list2_push_back(&b_list, (list2_elem_t)i) == LIST2_OK);
            nb++;
        }
    }
    CHECK_EQ(list2_push_back(&a_list, 0), LIST2_ERR_NOMEM);
    CHECK_EQ(list2_push_back(&b_list, 0), LIST2_ERR_NOMEM);
    CHECK(no_leak(&p, na + nb));

    REQUIRE(list2_clear(&a_list) == LIST2_OK);
    CHECK_EQ(list2_size(&b_list), nb);         /* 다른 리스트에 영향 없음 */
    CHECK(no_leak(&p, nb));
    CHECK_EQ(list2_push_back(&b_list, 1000), LIST2_OK);
    CHECK(no_leak(&p, nb + 1u));
}

/* ------------------------------------------------------------------ */
/* 참조 모델 대비 무작위 시퀀스                                         */
/* ------------------------------------------------------------------ */

static void t_random_model(void)
{
    list2_t l;
    pool_t p;
    arena_t a;
    list2_elem_t model[MAX_N];
    size_t n = 0;
    uint32_t seed = 0x2024u;
    const int failures_before = g_failures;

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);
    size_t cap = pool_capacity(&p);
    REQUIRE(cap <= MAX_N);

    for (unsigned step = 0; step < 20000u; step++) {
        uint32_t x = lcg_next(&seed);
        list2_elem_t v = (list2_elem_t)((x >> 8) % 8u);
        list2_elem_t out;

        switch ((x >> 4) % 5u) {
        case 0:
            if (n < cap) {
                CHECK_EQ(list2_push_front(&l, v), LIST2_OK);
                memmove(&model[1], &model[0], n * sizeof model[0]);
                model[0] = v;
                n++;
            } else {
                CHECK_EQ(list2_push_front(&l, v), LIST2_ERR_NOMEM);
            }
            break;
        case 1:
            if (n < cap) {
                CHECK_EQ(list2_push_back(&l, v), LIST2_OK);
                model[n++] = v;
            } else {
                CHECK_EQ(list2_push_back(&l, v), LIST2_ERR_NOMEM);
            }
            break;
        case 2:
            if (n > 0u) {
                CHECK_EQ(list2_pop_front(&l, &out), LIST2_OK);
                CHECK_EQ(out, model[0]);
                memmove(&model[0], &model[1], (n - 1u) * sizeof model[0]);
                n--;
            } else {
                CHECK_EQ(list2_pop_front(&l, &out), LIST2_ERR_EMPTY);
            }
            break;
        case 3: {
            size_t k = 0;
            while (k < n && model[k] != v) {
                k++;
            }
            if (k < n) {
                CHECK_EQ(list2_remove(&l, v), LIST2_OK);
                memmove(&model[k], &model[k + 1u], (n - k - 1u) * sizeof model[0]);
                n--;
            } else {
                CHECK_EQ(list2_remove(&l, v), LIST2_ERR_NOT_FOUND);
            }
            break;
        }
        default: { /* remove_if: v 와 같은 원소 전부 */
            eq_ctx_t ctx = { v, 0u };
            size_t removed = 0;
            size_t w = 0;
            for (size_t r = 0; r < n; r++) {
                if (model[r] != v) {
                    model[w++] = model[r];
                }
            }
            CHECK_EQ(list2_remove_if(&l, pred_eq_count, &ctx, &removed), LIST2_OK);
            CHECK_EQ(removed, n - w);
            CHECK_EQ(ctx.calls, n);
            n = w;
            break;
        }
        }

        expect_list(&l, model, n);
        CHECK(no_leak(&p, n));
        if (g_failures != failures_before) {
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* NULL 인자                                                           */
/* ------------------------------------------------------------------ */

static void t_null_args(void)
{
    list2_t l;
    pool_t p;
    arena_t a;
    list2_elem_t out = 4;
    size_t removed = 77;

    CHECK_EQ(list2_push_front(NULL, 1), LIST2_ERR_NULL);
    CHECK_EQ(list2_push_back(NULL, 1), LIST2_ERR_NULL);
    CHECK_EQ(list2_pop_front(NULL, &out), LIST2_ERR_NULL);
    CHECK_EQ(list2_remove(NULL, 1), LIST2_ERR_NULL);
    CHECK_EQ(list2_remove_if(NULL, pred_all, NULL, &removed), LIST2_ERR_NULL);
    CHECK_EQ(list2_clear(NULL), LIST2_ERR_NULL);
    CHECK_EQ(out, 4);

    CHECK(list2_first(NULL) == NULL);
    CHECK(list2_next(NULL) == NULL);
    CHECK(!list2_contains(NULL, 1));
    CHECK_EQ(list2_size(NULL), 0);
    CHECK(list2_is_empty(NULL));

    REQUIRE(make_pool(&p, &a));
    REQUIRE(list2_init(&l, &p) == LIST2_OK);
    REQUIRE(fill_seq(&l, 2));
    CHECK_EQ(list2_pop_front(&l, NULL), LIST2_ERR_NULL);
    CHECK_EQ(list2_remove_if(&l, NULL, NULL, &removed), LIST2_ERR_NULL);
    EXPECT(&l, 1, 2);
    CHECK(no_leak(&p, 2u));
}

int main(void)
{
    RUN_TEST(t_init_null_args);
    RUN_TEST(t_init_pool_too_small);
    RUN_TEST(t_init_empty);
    RUN_TEST(t_push_order);
    RUN_TEST(t_pop);
    RUN_TEST(t_pool_exhausted);
    RUN_TEST(t_remove_positions);
    RUN_TEST(t_remove_only_and_duplicates);
    RUN_TEST(t_remove_if_patterns);
    RUN_TEST(t_remove_if_consecutive_head);
    RUN_TEST(t_clear);
    RUN_TEST(t_shared_pool);
    RUN_TEST(t_random_model);
    RUN_TEST(t_null_args);
    return TEST_REPORT();
}
