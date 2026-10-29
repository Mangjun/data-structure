/*
 * test_list_v1.c - 연결 리스트 v1 (정적 배열 + 인덱스) 테스트
 *
 * 동작은 공개 API(first/next/get)로 확인하고, list1_invariant() 로
 * 내부 구조(head 리스트 + free list 가 배열을 정확히 나눠 갖는지)를 화이트박스 검사한다.
 */
#include <stdint.h>
#include <string.h>

#include "list_v1.h"
#include "test_common.h"

#define CAP 8u

/* ------------------------------------------------------------------ */
/* 보조 함수                                                           */
/* ------------------------------------------------------------------ */

/* 화이트박스: 두 리스트가 서로소이고 0..cap-1 을 전부 덮는지, count/tail 이 맞는지 */
static bool list1_invariant(const list1_t *l)
{
    static bool seen[LIST1_MAX_CAP];
    size_t used = 0;
    size_t free_n = 0;
    list1_idx_t last = LIST1_NIL;

    if (l->nodes == NULL || l->cap == 0u) {
        return false;
    }
    memset(seen, 0, (size_t)l->cap * sizeof seen[0]);

    for (list1_idx_t i = l->head; i != LIST1_NIL; i = l->nodes[i].next) {
        if (i >= l->cap || seen[i] || used > l->cap) {
            return false;                      /* 범위 밖, 순환, 중복 */
        }
        seen[i] = true;
        last = i;
        used++;
    }
    for (list1_idx_t i = l->free_head; i != LIST1_NIL; i = l->nodes[i].next) {
        if (i >= l->cap || seen[i] || free_n > l->cap) {
            return false;
        }
        seen[i] = true;
        free_n++;
    }
    return used == l->count && used + free_n == l->cap && last == l->tail;
}

/* 공개 API 로 값을 모은다. 순환이면 SIZE_MAX. */
static size_t collect(const list1_t *l, list1_elem_t *out, size_t max)
{
    size_t n = 0;

    for (list1_idx_t i = list1_first(l); i != LIST1_NIL; i = list1_next(l, i)) {
        if (n >= max) {
            return SIZE_MAX;
        }
        if (list1_get(l, i, &out[n]) != LIST1_OK) {
            return SIZE_MAX;
        }
        n++;
    }
    return n;
}

/* 리스트 내용이 exp[0..n) 과 같은지 */
static void expect_list(const list1_t *l, const list1_elem_t *exp, size_t n)
{
    list1_elem_t got[CAP + 1u];
    size_t m = collect(l, got, CAP + 1u);

    CHECK_EQ(m, n);
    CHECK_EQ(list1_size(l), n);
    if (m == n) {
        for (size_t i = 0; i < n; i++) {
            CHECK_EQ(got[i], exp[i]);
        }
    }
    CHECK(list1_invariant(l));
}

#define EXPECT(l, ...)                                                     \
    expect_list((l), (const list1_elem_t[]){ __VA_ARGS__ },                \
                sizeof((const list1_elem_t[]){ __VA_ARGS__ }) / sizeof(list1_elem_t))

static void expect_empty(const list1_t *l)
{
    CHECK_EQ(list1_size(l), 0);
    CHECK(list1_is_empty(l));
    CHECK_EQ(list1_first(l), LIST1_NIL);
    CHECK(list1_invariant(l));
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
    list1_t l;
    list1_node_t nodes[CAP];

    CHECK_EQ(list1_init(NULL, nodes, CAP), LIST1_ERR_NULL);
    CHECK_EQ(list1_init(&l, NULL, CAP), LIST1_ERR_NULL);
    CHECK_EQ(list1_init(NULL, NULL, 0u), LIST1_ERR_NULL);
}

static void t_init_bad_cap(void)
{
    list1_t l;
    list1_node_t nodes[CAP];

    CHECK_EQ(list1_init(&l, nodes, 0u), LIST1_ERR_INVAL);
    CHECK_EQ(list1_init(&l, nodes, LIST1_MAX_CAP + 1u), LIST1_ERR_INVAL);
    CHECK_EQ(list1_init(&l, nodes, SIZE_MAX), LIST1_ERR_INVAL);
}

static void t_init_failure_untouched(void)
{
    list1_t l;
    list1_t snapshot;
    list1_node_t nodes[CAP];

    memset(&l, 0x5C, sizeof l);
    memcpy(&snapshot, &l, sizeof l);
    CHECK(list1_init(&l, nodes, 0u) != LIST1_OK);
    CHECK(memcmp(&l, &snapshot, sizeof l) == 0);
    CHECK(list1_init(&l, NULL, CAP) != LIST1_OK);
    CHECK(memcmp(&l, &snapshot, sizeof l) == 0);
}

static void t_init_empty(void)
{
    list1_t l;
    list1_node_t nodes[CAP];

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    expect_empty(&l);
    CHECK_EQ(list1_capacity(&l), CAP);
    CHECK(!list1_is_full(&l));
}

/* ------------------------------------------------------------------ */
/* push / pop                                                          */
/* ------------------------------------------------------------------ */

static void t_pop_empty(void)
{
    list1_t l;
    list1_node_t nodes[CAP];
    list1_elem_t out = 77;

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    CHECK_EQ(list1_pop_front(&l, &out), LIST1_ERR_EMPTY);
    CHECK_EQ(out, 77);
    expect_empty(&l);

    REQUIRE(list1_push_back(&l, 1) == LIST1_OK);
    REQUIRE(list1_pop_front(&l, &out) == LIST1_OK);
    out = 77;
    CHECK_EQ(list1_pop_front(&l, &out), LIST1_ERR_EMPTY);
    CHECK_EQ(out, 77);
    expect_empty(&l);
}

static void t_push_order(void)
{
    list1_t l;
    list1_node_t nodes[CAP];

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 1) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 2) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 3) == LIST1_OK);
    EXPECT(&l, 1, 2, 3);

    REQUIRE(list1_push_front(&l, 0) == LIST1_OK);
    REQUIRE(list1_push_front(&l, -1) == LIST1_OK);
    EXPECT(&l, -1, 0, 1, 2, 3);

    /* 빈 리스트에 push_front 후 push_back: tail 이 제대로 잡혔는지 */
    REQUIRE(list1_clear(&l) == LIST1_OK);
    REQUIRE(list1_push_front(&l, 10) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 11) == LIST1_OK);
    EXPECT(&l, 10, 11);
}

static void t_pop_order(void)
{
    list1_t l;
    list1_node_t nodes[CAP];
    list1_elem_t out;

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    for (int32_t v = 0; v < 4; v++) {
        REQUIRE(list1_push_back(&l, v) == LIST1_OK);
    }
    for (int32_t v = 0; v < 4; v++) {
        REQUIRE(list1_pop_front(&l, &out) == LIST1_OK);
        CHECK_EQ(out, v);
        CHECK(list1_invariant(&l));
    }
    expect_empty(&l);

    /* 마지막 원소를 pop 한 뒤 push_back: tail 이 NIL 로 정리됐는지 */
    REQUIRE(list1_push_back(&l, 9) == LIST1_OK);
    EXPECT(&l, 9);
}

static void t_full(void)
{
    list1_t l;
    list1_node_t nodes[CAP];

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    for (size_t i = 0; i < CAP; i++) {
        CHECK(!list1_is_full(&l));
        REQUIRE(list1_push_back(&l, (list1_elem_t)i) == LIST1_OK);
    }
    CHECK(list1_is_full(&l));
    CHECK_EQ(list1_push_back(&l, 99), LIST1_ERR_FULL);
    CHECK_EQ(list1_push_front(&l, 99), LIST1_ERR_FULL);
    CHECK_EQ(list1_insert_after(&l, list1_first(&l), 99), LIST1_ERR_FULL);
    EXPECT(&l, 0, 1, 2, 3, 4, 5, 6, 7);
}

/* 빈 노드가 재사용되는지: 채우기/비우기를 반복해도 매번 cap 개가 들어가야 한다 */
static void t_fill_drain_cycles(void)
{
    list1_t l;
    list1_node_t nodes[CAP];
    list1_elem_t out;

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    for (int round = 0; round < 5; round++) {
        for (size_t i = 0; i < CAP; i++) {
            REQUIRE(list1_push_back(&l, (list1_elem_t)i) == LIST1_OK);
        }
        CHECK_EQ(list1_push_back(&l, 0), LIST1_ERR_FULL);
        for (size_t i = 0; i < CAP; i++) {
            REQUIRE(list1_pop_front(&l, &out) == LIST1_OK);
            CHECK_EQ(out, (list1_elem_t)i);
        }
        expect_empty(&l);
    }
}

/* ------------------------------------------------------------------ */
/* insert_after / remove / find                                        */
/* ------------------------------------------------------------------ */

static void t_insert_after(void)
{
    list1_t l;
    list1_node_t nodes[CAP];

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 1) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 3) == LIST1_OK);

    CHECK_EQ(list1_insert_after(&l, list1_find(&l, 1), 2), LIST1_OK);   /* 중간 */
    EXPECT(&l, 1, 2, 3);
    CHECK_EQ(list1_insert_after(&l, list1_find(&l, 3), 4), LIST1_OK);   /* tail 뒤 */
    EXPECT(&l, 1, 2, 3, 4);
    CHECK_EQ(list1_push_back(&l, 5), LIST1_OK);                         /* tail 갱신 확인 */
    EXPECT(&l, 1, 2, 3, 4, 5);
}

static void t_insert_after_invalid(void)
{
    list1_t l;
    list1_node_t nodes[CAP];

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 1) == LIST1_OK);

    CHECK_EQ(list1_insert_after(&l, LIST1_NIL, 2), LIST1_ERR_INVAL);
    CHECK_EQ(list1_insert_after(&l, (list1_idx_t)CAP, 2), LIST1_ERR_INVAL);
    CHECK_EQ(list1_insert_after(NULL, 0u, 2), LIST1_ERR_NULL);
    EXPECT(&l, 1);
}

static void t_remove_positions(void)
{
    list1_t l;
    list1_node_t nodes[CAP];

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    for (int32_t v = 1; v <= 5; v++) {
        REQUIRE(list1_push_back(&l, v) == LIST1_OK);
    }

    CHECK_EQ(list1_remove(&l, 1), LIST1_OK);   /* head */
    EXPECT(&l, 2, 3, 4, 5);
    CHECK_EQ(list1_remove(&l, 3), LIST1_OK);   /* 중간 */
    EXPECT(&l, 2, 4, 5);
    CHECK_EQ(list1_remove(&l, 5), LIST1_OK);   /* tail */
    EXPECT(&l, 2, 4);
    CHECK_EQ(list1_push_back(&l, 6), LIST1_OK); /* tail 이 4 로 갱신됐는지 */
    EXPECT(&l, 2, 4, 6);

    CHECK_EQ(list1_remove(&l, 42), LIST1_ERR_NOT_FOUND);
    EXPECT(&l, 2, 4, 6);
}

static void t_remove_only_and_duplicates(void)
{
    list1_t l;
    list1_node_t nodes[CAP];

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 7) == LIST1_OK);
    CHECK_EQ(list1_remove(&l, 7), LIST1_OK);   /* 유일한 원소 */
    expect_empty(&l);
    CHECK_EQ(list1_remove(&l, 7), LIST1_ERR_NOT_FOUND);
    CHECK_EQ(list1_push_back(&l, 8), LIST1_OK);
    EXPECT(&l, 8);

    REQUIRE(list1_push_back(&l, 7) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 8) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 7) == LIST1_OK);
    CHECK_EQ(list1_remove(&l, 8), LIST1_OK);   /* 첫 번째 8 만 */
    EXPECT(&l, 7, 8, 7);
}

static void t_find(void)
{
    list1_t l;
    list1_node_t nodes[CAP];
    list1_elem_t v;

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    CHECK_EQ(list1_find(&l, 1), LIST1_NIL);
    REQUIRE(list1_push_back(&l, 5) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 6) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 5) == LIST1_OK);

    list1_idx_t i = list1_find(&l, 5);
    REQUIRE(i != LIST1_NIL);
    CHECK_EQ(i, list1_first(&l));              /* 첫 번째 5 */
    CHECK_EQ(list1_get(&l, i, &v), LIST1_OK);
    CHECK_EQ(v, 5);
    CHECK_EQ(list1_find(&l, 9), LIST1_NIL);
    CHECK_EQ(list1_find(NULL, 5), LIST1_NIL);
}

static void t_get_next_invalid(void)
{
    list1_t l;
    list1_node_t nodes[CAP];
    list1_elem_t v = 33;

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 1) == LIST1_OK);

    CHECK_EQ(list1_get(&l, LIST1_NIL, &v), LIST1_ERR_INVAL);
    CHECK_EQ(list1_get(&l, (list1_idx_t)CAP, &v), LIST1_ERR_INVAL);
    CHECK_EQ(v, 33);
    CHECK_EQ(list1_get(&l, list1_first(&l), NULL), LIST1_ERR_NULL);
    CHECK_EQ(list1_next(&l, LIST1_NIL), LIST1_NIL);
    CHECK_EQ(list1_next(&l, (list1_idx_t)CAP), LIST1_NIL);
    CHECK_EQ(list1_next(&l, list1_first(&l)), LIST1_NIL);  /* 마지막 다음 */
    CHECK_EQ(list1_next(NULL, 0u), LIST1_NIL);
}

/* ------------------------------------------------------------------ */
/* 용량 1 / 최대 용량 / clear                                          */
/* ------------------------------------------------------------------ */

static void t_capacity_one(void)
{
    list1_t l;
    list1_node_t nodes[1];
    list1_elem_t out;

    REQUIRE(list1_init(&l, nodes, 1u) == LIST1_OK);
    for (int round = 0; round < 3; round++) {
        expect_empty(&l);
        CHECK_EQ(list1_push_front(&l, 1), LIST1_OK);
        CHECK(list1_is_full(&l));
        CHECK_EQ(list1_push_back(&l, 2), LIST1_ERR_FULL);
        CHECK_EQ(list1_insert_after(&l, list1_first(&l), 2), LIST1_ERR_FULL);
        EXPECT(&l, 1);
        CHECK_EQ(list1_pop_front(&l, &out), LIST1_OK);
        CHECK_EQ(out, 1);
    }
}

static void t_max_capacity(void)
{
    static list1_node_t nodes[LIST1_MAX_CAP];
    list1_t l;
    list1_elem_t out;

    REQUIRE(list1_init(&l, nodes, LIST1_MAX_CAP) == LIST1_OK);
    for (size_t i = 0; i < LIST1_MAX_CAP; i++) {
        REQUIRE(list1_push_back(&l, (list1_elem_t)i) == LIST1_OK);
    }
    CHECK(list1_is_full(&l));
    CHECK_EQ(list1_size(&l), LIST1_MAX_CAP);
    CHECK_EQ(list1_push_back(&l, 0), LIST1_ERR_FULL);
    CHECK(list1_invariant(&l));
    REQUIRE(list1_pop_front(&l, &out) == LIST1_OK);
    CHECK_EQ(out, 0);
    CHECK(list1_invariant(&l));
}

static void t_clear_and_reuse(void)
{
    list1_t l;
    list1_node_t nodes[CAP];

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 1) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 2) == LIST1_OK);
    REQUIRE(list1_remove(&l, 1) == LIST1_OK);   /* free list 순서를 섞어 둔다 */

    CHECK_EQ(list1_clear(&l), LIST1_OK);
    expect_empty(&l);
    for (size_t i = 0; i < CAP; i++) {
        CHECK_EQ(list1_push_front(&l, (list1_elem_t)i), LIST1_OK);
    }
    CHECK(list1_is_full(&l));
    EXPECT(&l, 7, 6, 5, 4, 3, 2, 1, 0);
}

/* ------------------------------------------------------------------ */
/* 참조 모델 대비 무작위 시퀀스                                         */
/* ------------------------------------------------------------------ */

static void t_random_model(void)
{
    list1_t l;
    list1_node_t nodes[CAP];
    list1_elem_t model[CAP];
    size_t n = 0;
    uint32_t seed = 0x51u;
    const int failures_before = g_failures;

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);

    for (unsigned step = 0; step < 20000u; step++) {
        uint32_t x = lcg_next(&seed);
        list1_elem_t v = (list1_elem_t)((x >> 8) % 6u);   /* 값 범위를 좁혀 중복을 만든다 */
        list1_elem_t out;

        switch ((x >> 4) % 5u) {
        case 0: /* push_front */
            if (n < CAP) {
                CHECK_EQ(list1_push_front(&l, v), LIST1_OK);
                memmove(&model[1], &model[0], n * sizeof model[0]);
                model[0] = v;
                n++;
            } else {
                CHECK_EQ(list1_push_front(&l, v), LIST1_ERR_FULL);
            }
            break;
        case 1: /* push_back */
            if (n < CAP) {
                CHECK_EQ(list1_push_back(&l, v), LIST1_OK);
                model[n++] = v;
            } else {
                CHECK_EQ(list1_push_back(&l, v), LIST1_ERR_FULL);
            }
            break;
        case 2: /* pop_front */
            if (n > 0u) {
                CHECK_EQ(list1_pop_front(&l, &out), LIST1_OK);
                CHECK_EQ(out, model[0]);
                memmove(&model[0], &model[1], (n - 1u) * sizeof model[0]);
                n--;
            } else {
                CHECK_EQ(list1_pop_front(&l, &out), LIST1_ERR_EMPTY);
            }
            break;
        case 3: { /* remove */
            size_t k = 0;
            while (k < n && model[k] != v) {
                k++;
            }
            if (k < n) {
                CHECK_EQ(list1_remove(&l, v), LIST1_OK);
                memmove(&model[k], &model[k + 1u], (n - k - 1u) * sizeof model[0]);
                n--;
            } else {
                CHECK_EQ(list1_remove(&l, v), LIST1_ERR_NOT_FOUND);
            }
            break;
        }
        default: { /* insert_after: v 를 가진 첫 노드 뒤에 100+v */
            size_t k = 0;
            while (k < n && model[k] != v) {
                k++;
            }
            if (k < n && n < CAP) {
                CHECK_EQ(list1_insert_after(&l, list1_find(&l, v), 100 + v), LIST1_OK);
                memmove(&model[k + 2u], &model[k + 1u], (n - k - 1u) * sizeof model[0]);
                model[k + 1u] = 100 + v;
                n++;
            }
            break;
        }
        }

        expect_list(&l, model, n);
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
    list1_t l;
    list1_node_t nodes[CAP];
    list1_elem_t out = 5;

    CHECK_EQ(list1_clear(NULL), LIST1_ERR_NULL);
    CHECK_EQ(list1_push_front(NULL, 1), LIST1_ERR_NULL);
    CHECK_EQ(list1_push_back(NULL, 1), LIST1_ERR_NULL);
    CHECK_EQ(list1_pop_front(NULL, &out), LIST1_ERR_NULL);
    CHECK_EQ(list1_remove(NULL, 1), LIST1_ERR_NULL);
    CHECK_EQ(list1_get(NULL, 0u, &out), LIST1_ERR_NULL);
    CHECK_EQ(out, 5);

    CHECK_EQ(list1_first(NULL), LIST1_NIL);
    CHECK_EQ(list1_size(NULL), 0);
    CHECK_EQ(list1_capacity(NULL), 0);
    CHECK(list1_is_empty(NULL));
    CHECK(list1_is_full(NULL));

    REQUIRE(list1_init(&l, nodes, CAP) == LIST1_OK);
    REQUIRE(list1_push_back(&l, 3) == LIST1_OK);
    CHECK_EQ(list1_pop_front(&l, NULL), LIST1_ERR_NULL);
    EXPECT(&l, 3);
}

int main(void)
{
    RUN_TEST(t_init_null_args);
    RUN_TEST(t_init_bad_cap);
    RUN_TEST(t_init_failure_untouched);
    RUN_TEST(t_init_empty);
    RUN_TEST(t_pop_empty);
    RUN_TEST(t_push_order);
    RUN_TEST(t_pop_order);
    RUN_TEST(t_full);
    RUN_TEST(t_fill_drain_cycles);
    RUN_TEST(t_insert_after);
    RUN_TEST(t_insert_after_invalid);
    RUN_TEST(t_remove_positions);
    RUN_TEST(t_remove_only_and_duplicates);
    RUN_TEST(t_find);
    RUN_TEST(t_get_next_invalid);
    RUN_TEST(t_capacity_one);
    RUN_TEST(t_max_capacity);
    RUN_TEST(t_clear_and_reuse);
    RUN_TEST(t_random_model);
    RUN_TEST(t_null_args);
    return TEST_REPORT();
}
