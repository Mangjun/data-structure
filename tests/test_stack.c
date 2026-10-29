/*
 * test_stack.c - stack 단계 테스트
 */
#include <stdint.h>
#include <string.h>

#include "stack.h"
#include "test_common.h"

#define CAP 4u
#define GUARD ((stack_elem_t)0x5A5A5A5A)

/* buf 앞뒤에 가드 원소를 두어 범위 밖 쓰기를 검출한다.
 * (ASan 은 같은 배열 내부의 오버런을 잡지 못하므로 별도로 확인) */
typedef struct {
    stack_elem_t mem[CAP + 2u];
} guarded_buf_t;

static stack_elem_t *gb_init(guarded_buf_t *g)
{
    for (size_t i = 0; i < CAP + 2u; i++) {
        g->mem[i] = GUARD;
    }
    return &g->mem[1];
}

static int gb_intact(const guarded_buf_t *g)
{
    return g->mem[0] == GUARD && g->mem[CAP + 1u] == GUARD;
}

/* ---- init ---- */

static void test_init_null_args(void)
{
    ds_stack_t s;
    stack_elem_t buf[CAP];

    CHECK_EQ(stack_init(NULL, buf, CAP), STACK_ERR_NULL);
    CHECK_EQ(stack_init(&s, NULL, CAP), STACK_ERR_NULL);
    CHECK_EQ(stack_init(NULL, NULL, 0), STACK_ERR_NULL);
}

static void test_init_zero_cap(void)
{
    ds_stack_t s;
    stack_elem_t buf[1];

    CHECK_EQ(stack_init(&s, buf, 0), STACK_ERR_INVAL);
}

static void test_init_failure_leaves_object_untouched(void)
{
    ds_stack_t s;
    ds_stack_t snapshot;
    stack_elem_t buf[CAP];

    memset(&s, 0xA5, sizeof s);
    memcpy(&snapshot, &s, sizeof s);

    CHECK(stack_init(&s, buf, 0) != STACK_OK);
    CHECK(memcmp(&s, &snapshot, sizeof s) == 0);
    CHECK(stack_init(&s, NULL, CAP) != STACK_OK);
    CHECK(memcmp(&s, &snapshot, sizeof s) == 0);
}

static void test_init_ok_is_empty(void)
{
    ds_stack_t s;
    stack_elem_t buf[CAP];

    REQUIRE(stack_init(&s, buf, CAP) == STACK_OK);
    CHECK_EQ(stack_size(&s), 0);
    CHECK_EQ(stack_capacity(&s), CAP);
    CHECK(stack_is_empty(&s));
    CHECK(!stack_is_full(&s));
}

/* ---- 빈 상태 ---- */

static void test_pop_peek_empty(void)
{
    ds_stack_t s;
    stack_elem_t buf[CAP];
    stack_elem_t out = 1234;

    REQUIRE(stack_init(&s, buf, CAP) == STACK_OK);

    CHECK_EQ(stack_pop(&s, &out), STACK_ERR_EMPTY);
    CHECK_EQ(out, 1234);              /* 실패 시 *out 불변 */
    CHECK_EQ(stack_peek(&s, &out), STACK_ERR_EMPTY);
    CHECK_EQ(out, 1234);
    CHECK_EQ(stack_size(&s), 0);      /* 언더플로로 size 가 SIZE_MAX 가 되면 안 됨 */
}

static void test_pop_empty_after_drain(void)
{
    ds_stack_t s;
    stack_elem_t buf[CAP];
    stack_elem_t out;

    REQUIRE(stack_init(&s, buf, CAP) == STACK_OK);
    REQUIRE(stack_push(&s, 7) == STACK_OK);
    REQUIRE(stack_pop(&s, &out) == STACK_OK);

    out = -1;
    CHECK_EQ(stack_pop(&s, &out), STACK_ERR_EMPTY);
    CHECK_EQ(out, -1);
    CHECK(stack_is_empty(&s));
}

/* ---- 가득 찬 상태 ---- */

static void test_push_full(void)
{
    ds_stack_t s;
    guarded_buf_t g;
    stack_elem_t out;

    REQUIRE(stack_init(&s, gb_init(&g), CAP) == STACK_OK);

    for (size_t i = 0; i < CAP; i++) {
        CHECK_EQ(stack_push(&s, (stack_elem_t)i), STACK_OK);
    }
    CHECK(stack_is_full(&s));
    CHECK_EQ(stack_size(&s), CAP);

    CHECK_EQ(stack_push(&s, 99), STACK_ERR_FULL);
    CHECK_EQ(stack_push(&s, 99), STACK_ERR_FULL);  /* 반복 실패에도 상태 불변 */
    CHECK_EQ(stack_size(&s), CAP);
    CHECK(gb_intact(&g));

    REQUIRE(stack_peek(&s, &out) == STACK_OK);
    CHECK_EQ(out, (stack_elem_t)(CAP - 1u));       /* 최상단이 덮어써지지 않음 */
}

static void test_full_pop_push_again(void)
{
    ds_stack_t s;
    guarded_buf_t g;
    stack_elem_t out;

    REQUIRE(stack_init(&s, gb_init(&g), CAP) == STACK_OK);
    for (size_t i = 0; i < CAP; i++) {
        REQUIRE(stack_push(&s, (stack_elem_t)i) == STACK_OK);
    }
    REQUIRE(stack_pop(&s, &out) == STACK_OK);
    CHECK(!stack_is_full(&s));
    CHECK_EQ(stack_push(&s, 42), STACK_OK);
    CHECK(stack_is_full(&s));
    CHECK_EQ(stack_push(&s, 43), STACK_ERR_FULL);
    CHECK(gb_intact(&g));
}

/* ---- 순서 / 값 ---- */

static void test_lifo_order(void)
{
    ds_stack_t s;
    guarded_buf_t g;
    stack_elem_t out;

    REQUIRE(stack_init(&s, gb_init(&g), CAP) == STACK_OK);
    for (size_t i = 0; i < CAP; i++) {
        REQUIRE(stack_push(&s, (stack_elem_t)(i * 10u)) == STACK_OK);
    }
    for (size_t i = CAP; i > 0; i--) {
        REQUIRE(stack_pop(&s, &out) == STACK_OK);
        CHECK_EQ(out, (stack_elem_t)((i - 1u) * 10u));
        CHECK_EQ(stack_size(&s), i - 1u);
    }
    CHECK(stack_is_empty(&s));
    CHECK(gb_intact(&g));
}

static void test_peek_does_not_modify(void)
{
    ds_stack_t s;
    stack_elem_t buf[CAP];
    stack_elem_t out = 0;

    REQUIRE(stack_init(&s, buf, CAP) == STACK_OK);
    REQUIRE(stack_push(&s, 5) == STACK_OK);
    REQUIRE(stack_push(&s, 6) == STACK_OK);

    CHECK_EQ(stack_peek(&s, &out), STACK_OK);
    CHECK_EQ(out, 6);
    CHECK_EQ(stack_peek(&s, &out), STACK_OK);
    CHECK_EQ(out, 6);
    CHECK_EQ(stack_size(&s), 2);
}

static void test_extreme_values(void)
{
    ds_stack_t s;
    stack_elem_t buf[CAP];
    stack_elem_t out;

    REQUIRE(stack_init(&s, buf, CAP) == STACK_OK);
    REQUIRE(stack_push(&s, INT32_MIN) == STACK_OK);
    REQUIRE(stack_push(&s, INT32_MAX) == STACK_OK);
    REQUIRE(stack_push(&s, 0) == STACK_OK);
    REQUIRE(stack_push(&s, -1) == STACK_OK);

    REQUIRE(stack_pop(&s, &out) == STACK_OK);
    CHECK_EQ(out, -1);
    REQUIRE(stack_pop(&s, &out) == STACK_OK);
    CHECK_EQ(out, 0);
    REQUIRE(stack_pop(&s, &out) == STACK_OK);
    CHECK_EQ(out, INT32_MAX);
    REQUIRE(stack_pop(&s, &out) == STACK_OK);
    CHECK_EQ(out, INT32_MIN);
}

/* ---- 용량 1 ---- */

static void test_capacity_one(void)
{
    ds_stack_t s;
    stack_elem_t mem[3] = { GUARD, GUARD, GUARD };
    stack_elem_t out = 0;

    REQUIRE(stack_init(&s, &mem[1], 1) == STACK_OK);
    CHECK(stack_is_empty(&s));
    CHECK(!stack_is_full(&s));

    CHECK_EQ(stack_push(&s, 11), STACK_OK);
    CHECK(stack_is_full(&s));
    CHECK(!stack_is_empty(&s));
    CHECK_EQ(stack_push(&s, 12), STACK_ERR_FULL);

    CHECK_EQ(stack_pop(&s, &out), STACK_OK);
    CHECK_EQ(out, 11);
    CHECK(stack_is_empty(&s));
    CHECK_EQ(stack_pop(&s, &out), STACK_ERR_EMPTY);

    CHECK_EQ(mem[0], GUARD);
    CHECK_EQ(mem[2], GUARD);
}

/* ---- clear ---- */

static void test_clear_and_reuse(void)
{
    ds_stack_t s;
    stack_elem_t buf[CAP];
    stack_elem_t out;

    REQUIRE(stack_init(&s, buf, CAP) == STACK_OK);
    REQUIRE(stack_push(&s, 1) == STACK_OK);
    REQUIRE(stack_push(&s, 2) == STACK_OK);

    CHECK_EQ(stack_clear(&s), STACK_OK);
    CHECK(stack_is_empty(&s));
    CHECK_EQ(stack_capacity(&s), CAP);
    CHECK_EQ(stack_pop(&s, &out), STACK_ERR_EMPTY);

    for (size_t i = 0; i < CAP; i++) {
        CHECK_EQ(stack_push(&s, (stack_elem_t)i), STACK_OK);
    }
    CHECK(stack_is_full(&s));
}

/* ---- NULL 인자 ---- */

static void test_null_args(void)
{
    ds_stack_t s;
    stack_elem_t buf[CAP];
    stack_elem_t out = 77;

    CHECK_EQ(stack_push(NULL, 1), STACK_ERR_NULL);
    CHECK_EQ(stack_pop(NULL, &out), STACK_ERR_NULL);
    CHECK_EQ(stack_peek(NULL, &out), STACK_ERR_NULL);
    CHECK_EQ(stack_clear(NULL), STACK_ERR_NULL);
    CHECK_EQ(out, 77);

    REQUIRE(stack_init(&s, buf, CAP) == STACK_OK);
    REQUIRE(stack_push(&s, 3) == STACK_OK);

    /* out == NULL 은 비어 있지 않아도 NULL 오류가 우선, 상태 불변 */
    CHECK_EQ(stack_pop(&s, NULL), STACK_ERR_NULL);
    CHECK_EQ(stack_size(&s), 1);
    CHECK_EQ(stack_peek(&s, NULL), STACK_ERR_NULL);
}

static void test_null_queries(void)
{
    CHECK_EQ(stack_size(NULL), 0);
    CHECK_EQ(stack_capacity(NULL), 0);
    CHECK(stack_is_empty(NULL));
    CHECK(stack_is_full(NULL));
}

/* ---- 객체 독립성 ---- */

static void test_two_instances_independent(void)
{
    ds_stack_t a, b;
    stack_elem_t ba[CAP], bb[CAP];
    stack_elem_t out;

    REQUIRE(stack_init(&a, ba, CAP) == STACK_OK);
    REQUIRE(stack_init(&b, bb, CAP) == STACK_OK);
    REQUIRE(stack_push(&a, 1) == STACK_OK);

    CHECK_EQ(stack_size(&b), 0);
    CHECK_EQ(stack_pop(&b, &out), STACK_ERR_EMPTY);
}

int main(void)
{
    RUN_TEST(test_init_null_args);
    RUN_TEST(test_init_zero_cap);
    RUN_TEST(test_init_failure_leaves_object_untouched);
    RUN_TEST(test_init_ok_is_empty);
    RUN_TEST(test_pop_peek_empty);
    RUN_TEST(test_pop_empty_after_drain);
    RUN_TEST(test_push_full);
    RUN_TEST(test_full_pop_push_again);
    RUN_TEST(test_lifo_order);
    RUN_TEST(test_peek_does_not_modify);
    RUN_TEST(test_extreme_values);
    RUN_TEST(test_capacity_one);
    RUN_TEST(test_clear_and_reuse);
    RUN_TEST(test_null_args);
    RUN_TEST(test_null_queries);
    RUN_TEST(test_two_instances_independent);
    return TEST_REPORT();
}
