/*
 * test_list_v3.c - 연결 리스트 v3 (intrusive, list3_head_t + container_of) 테스트
 *
 * 헤더의 list3_container_of / list3_for_each 자리표시가 남아 있으면
 * container_of 가 NULL 을 돌려주고 for_each 가 돌지 않아 대부분 실패한다.
 *
 * check_links() 는 next/prev 양방향 링크가 서로 맞는지 화이트박스로 검사한다.
 */
#include <stdint.h>
#include <string.h>

#include "list_v3.h"
#include "test_common.h"

#define N_ITEMS 100u
#define CANARY  ((int32_t)0x5A5A5A5A)

/* node 를 일부러 첫 멤버가 아닌 곳에 둔다: container_of 가 offset 을 빼야 한다 */
typedef struct {
    int32_t      id;
    list3_head_t node;
    int32_t      canary;   /* node 뒤 메모리를 덮어쓰지 않는지 확인용 */
} item_t;

/* 한 객체가 두 리스트에 동시에 들어간다 */
typedef struct {
    int32_t      id;
    list3_head_t by_a;
    list3_head_t by_b;
} multi_t;

/* node 가 첫 멤버인 경우 (offset 0) */
typedef struct {
    list3_head_t node;
    int32_t      id;
} first_t;

/* ------------------------------------------------------------------ */
/* 보조 함수                                                           */
/* ------------------------------------------------------------------ */

static void items_init(item_t *items, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        items[i].id = (int32_t)i;
        items[i].canary = CANARY;
        list3_init(&items[i].node);
    }
}

static bool canaries_ok(const item_t *items, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (items[i].canary != CANARY || items[i].id != (int32_t)i) {
            return false;
        }
    }
    return true;
}

/* 화이트박스: 모든 x 에 대해 x->next->prev == x, 앞/뒤로 센 원소 수가 같고 count 와 같은지 */
static bool check_links(const list3_head_t *head)
{
    size_t fwd = 0;
    size_t bwd = 0;

    for (const list3_head_t *x = head->next; x != head; x = x->next) {
        if (x == NULL || x->next == NULL || x->next->prev != x || fwd > N_ITEMS) {
            return false;
        }
        fwd++;
    }
    if (head->next->prev != head) {
        return false;
    }
    for (const list3_head_t *x = head->prev; x != head; x = x->prev) {
        if (x == NULL || bwd > N_ITEMS) {
            return false;
        }
        bwd++;
    }
    return fwd == bwd && fwd == list3_count(head);
}

/* for_each + entry 로 id 를 모은다. 실패하면 SIZE_MAX */
static size_t collect_ids(list3_head_t *head, int32_t *out, size_t max)
{
    size_t n = 0;
    list3_head_t *pos;

    list3_for_each(pos, head) {
        item_t *it = list3_entry(pos, item_t, node);
        if (it == NULL || n >= max) {
            return SIZE_MAX;
        }
        out[n++] = it->id;
    }
    return n;
}

static void expect_ids(list3_head_t *head, const int32_t *exp, size_t n)
{
    int32_t got[N_ITEMS + 1u];
    size_t m = collect_ids(head, got, N_ITEMS + 1u);

    CHECK_EQ(m, n);
    CHECK_EQ(list3_count(head), n);
    CHECK(list3_is_empty(head) == (n == 0u));
    if (m == n) {
        for (size_t i = 0; i < n; i++) {
            CHECK_EQ(got[i], exp[i]);
        }
    }
    CHECK(check_links(head));
}

#define EXPECT(head, ...)                                                  \
    expect_ids((head), (const int32_t[]){ __VA_ARGS__ },                   \
               sizeof((const int32_t[]){ __VA_ARGS__ }) / sizeof(int32_t))

static void expect_empty(list3_head_t *head)
{
    CHECK(list3_is_empty(head));
    CHECK_EQ(list3_count(head), 0);
    CHECK(head->next == head && head->prev == head);
}

/* ------------------------------------------------------------------ */
/* container_of                                                        */
/* ------------------------------------------------------------------ */

static void t_container_of(void)
{
    item_t it;
    multi_t m;
    first_t f;

    CHECK(list3_container_of(&it.node, item_t, node) == &it);
    CHECK(list3_entry(&it.node, item_t, node) == &it);
    CHECK(list3_container_of(&m.by_a, multi_t, by_a) == &m);
    CHECK(list3_container_of(&m.by_b, multi_t, by_b) == &m);
    CHECK(list3_container_of(&f.node, first_t, node) == &f);  /* offset 0 */

    /* 계산된 포인터로 실제 멤버에 접근해 본다 */
    it.id = 42;
    item_t *p = list3_entry(&it.node, item_t, node);
    REQUIRE(p != NULL);
    CHECK_EQ(p->id, 42);
}

/* ------------------------------------------------------------------ */
/* init                                                                */
/* ------------------------------------------------------------------ */

static void t_init(void)
{
    list3_head_t head;
    list3_head_t entry;

    memset(&head, 0, sizeof head);
    CHECK_EQ(list3_init(&head), LIST3_OK);
    expect_empty(&head);
    CHECK(!list3_is_linked(&head));

    CHECK_EQ(list3_init(&entry), LIST3_OK);
    CHECK(!list3_is_linked(&entry));
    CHECK(entry.next == &entry && entry.prev == &entry);
}

static void t_static_init(void)
{
    static list3_head_t head = LIST3_HEAD_INIT(head);

    expect_empty(&head);
}

/* ------------------------------------------------------------------ */
/* add                                                                 */
/* ------------------------------------------------------------------ */

static void t_add_front(void)
{
    list3_head_t head;
    item_t items[3];

    items_init(items, 3u);
    REQUIRE(list3_init(&head) == LIST3_OK);
    for (size_t i = 0; i < 3u; i++) {
        CHECK_EQ(list3_add(&items[i].node, &head), LIST3_OK);
        CHECK(list3_is_linked(&items[i].node));
    }
    EXPECT(&head, 2, 1, 0);
    CHECK(canaries_ok(items, 3u));
}

static void t_add_tail(void)
{
    list3_head_t head;
    item_t items[3];

    items_init(items, 3u);
    REQUIRE(list3_init(&head) == LIST3_OK);
    for (size_t i = 0; i < 3u; i++) {
        CHECK_EQ(list3_add_tail(&items[i].node, &head), LIST3_OK);
    }
    EXPECT(&head, 0, 1, 2);
    CHECK(canaries_ok(items, 3u));
}

static void t_add_mixed(void)
{
    list3_head_t head;
    item_t items[4];

    items_init(items, 4u);
    REQUIRE(list3_init(&head) == LIST3_OK);
    REQUIRE(list3_add_tail(&items[1].node, &head) == LIST3_OK);
    REQUIRE(list3_add(&items[0].node, &head) == LIST3_OK);
    REQUIRE(list3_add_tail(&items[2].node, &head) == LIST3_OK);
    REQUIRE(list3_add(&items[3].node, &head) == LIST3_OK);
    EXPECT(&head, 3, 0, 1, 2);
}

static void t_add_errors(void)
{
    list3_head_t a, b;
    item_t items[2];

    items_init(items, 2u);
    REQUIRE(list3_init(&a) == LIST3_OK);
    REQUIRE(list3_init(&b) == LIST3_OK);
    REQUIRE(list3_add_tail(&items[0].node, &a) == LIST3_OK);
    REQUIRE(list3_add_tail(&items[1].node, &a) == LIST3_OK);

    /* 이미 연결된 엔트리: 같은 리스트, 다른 리스트, 앞/뒤 모두 거부 */
    CHECK_EQ(list3_add(&items[0].node, &a), LIST3_ERR_LINKED);
    CHECK_EQ(list3_add_tail(&items[0].node, &a), LIST3_ERR_LINKED);
    CHECK_EQ(list3_add(&items[1].node, &b), LIST3_ERR_LINKED);
    CHECK_EQ(list3_add_tail(&items[1].node, &b), LIST3_ERR_LINKED);
    EXPECT(&a, 0, 1);
    expect_empty(&b);

    /* 자기 자신에 추가 */
    CHECK_EQ(list3_add(&b, &b), LIST3_ERR_INVAL);
    CHECK_EQ(list3_add_tail(&b, &b), LIST3_ERR_INVAL);
    expect_empty(&b);
}

/* ------------------------------------------------------------------ */
/* del                                                                 */
/* ------------------------------------------------------------------ */

static void t_del_positions(void)
{
    list3_head_t head;
    item_t items[5];

    items_init(items, 5u);
    REQUIRE(list3_init(&head) == LIST3_OK);
    for (size_t i = 0; i < 5u; i++) {
        REQUIRE(list3_add_tail(&items[i].node, &head) == LIST3_OK);
    }

    CHECK_EQ(list3_del(&items[0].node), LIST3_OK);   /* 첫 원소 */
    EXPECT(&head, 1, 2, 3, 4);
    CHECK_EQ(list3_del(&items[2].node), LIST3_OK);   /* 중간 */
    EXPECT(&head, 1, 3, 4);
    CHECK_EQ(list3_del(&items[4].node), LIST3_OK);   /* 마지막 */
    EXPECT(&head, 1, 3);

    /* 삭제된 엔트리는 연결 안 된 모양으로 돌아온다 */
    CHECK(!list3_is_linked(&items[0].node));
    CHECK(items[0].node.next == &items[0].node && items[0].node.prev == &items[0].node);

    /* 이중 삭제 */
    CHECK_EQ(list3_del(&items[0].node), LIST3_ERR_NOT_LINKED);
    EXPECT(&head, 1, 3);

    /* 다시 추가 가능 */
    CHECK_EQ(list3_add(&items[0].node, &head), LIST3_OK);
    EXPECT(&head, 0, 1, 3);
    CHECK(canaries_ok(items, 5u));
}

static void t_del_only(void)
{
    list3_head_t head;
    item_t items[1];

    items_init(items, 1u);
    REQUIRE(list3_init(&head) == LIST3_OK);
    REQUIRE(list3_add(&items[0].node, &head) == LIST3_OK);
    CHECK_EQ(list3_del(&items[0].node), LIST3_OK);
    expect_empty(&head);
    CHECK_EQ(list3_del(&items[0].node), LIST3_ERR_NOT_LINKED);
}

static void t_for_each_safe_delete(void)
{
    list3_head_t head;
    item_t items[10];
    list3_head_t *pos;
    list3_head_t *tmp;
    size_t visited = 0;

    items_init(items, 10u);
    REQUIRE(list3_init(&head) == LIST3_OK);
    for (size_t i = 0; i < 10u; i++) {
        REQUIRE(list3_add_tail(&items[i].node, &head) == LIST3_OK);
    }

    /* 짝수 id 를 순회 중에 삭제 */
    list3_for_each_safe(pos, tmp, &head) {
        item_t *it = list3_entry(pos, item_t, node);
        REQUIRE(it != NULL);
        visited++;
        if (it->id % 2 == 0) {
            CHECK_EQ(list3_del(pos), LIST3_OK);
        }
    }
    CHECK_EQ(visited, 10);
    EXPECT(&head, 1, 3, 5, 7, 9);

    /* 전부 삭제 */
    visited = 0;
    list3_for_each_safe(pos, tmp, &head) {
        CHECK_EQ(list3_del(pos), LIST3_OK);
        visited++;
    }
    CHECK_EQ(visited, 5);
    expect_empty(&head);
    CHECK(canaries_ok(items, 10u));
}

/* ------------------------------------------------------------------ */
/* 한 객체, 두 리스트                                                  */
/* ------------------------------------------------------------------ */

static void t_two_lists_same_objects(void)
{
    list3_head_t a, b;
    multi_t m[4];
    list3_head_t *pos;
    int32_t ids[4];
    size_t n;

    REQUIRE(list3_init(&a) == LIST3_OK);
    REQUIRE(list3_init(&b) == LIST3_OK);
    for (size_t i = 0; i < 4u; i++) {
        m[i].id = (int32_t)i;
        REQUIRE(list3_init(&m[i].by_a) == LIST3_OK);
        REQUIRE(list3_init(&m[i].by_b) == LIST3_OK);
        REQUIRE(list3_add_tail(&m[i].by_a, &a) == LIST3_OK);   /* a: 0 1 2 3 */
        REQUIRE(list3_add(&m[i].by_b, &b) == LIST3_OK);        /* b: 3 2 1 0 */
    }

    REQUIRE(list3_del(&m[1].by_a) == LIST3_OK);                /* a 에서만 뺀다 */
    CHECK(!list3_is_linked(&m[1].by_a));
    CHECK(list3_is_linked(&m[1].by_b));

    n = 0;
    list3_for_each(pos, &a) {
        multi_t *p = list3_entry(pos, multi_t, by_a);
        REQUIRE(p != NULL && n < 4u);
        ids[n++] = p->id;
    }
    CHECK_EQ(n, 3);
    CHECK(n == 3u && ids[0] == 0 && ids[1] == 2 && ids[2] == 3);

    n = 0;
    list3_for_each(pos, &b) {
        multi_t *p = list3_entry(pos, multi_t, by_b);
        REQUIRE(p != NULL && n < 4u);
        ids[n++] = p->id;
    }
    CHECK_EQ(n, 4);
    CHECK(n == 4u && ids[0] == 3 && ids[1] == 2 && ids[2] == 1 && ids[3] == 0);
    CHECK(check_links(&a));
    CHECK(check_links(&b));
}

/* ------------------------------------------------------------------ */
/* 많은 원소                                                           */
/* ------------------------------------------------------------------ */

static void t_many(void)
{
    list3_head_t head;
    item_t items[N_ITEMS];
    int32_t exp[N_ITEMS];
    size_t n = 0;

    items_init(items, N_ITEMS);
    REQUIRE(list3_init(&head) == LIST3_OK);
    for (size_t i = 0; i < N_ITEMS; i++) {
        REQUIRE(list3_add_tail(&items[i].node, &head) == LIST3_OK);
    }
    CHECK_EQ(list3_count(&head), N_ITEMS);

    for (size_t i = 0; i < N_ITEMS; i++) {
        if (i % 3u == 0u) {
            CHECK_EQ(list3_del(&items[i].node), LIST3_OK);
        } else {
            exp[n++] = (int32_t)i;
        }
    }
    expect_ids(&head, exp, n);
    CHECK(canaries_ok(items, N_ITEMS));
}

/* ------------------------------------------------------------------ */
/* NULL 인자                                                           */
/* ------------------------------------------------------------------ */

static void t_null_args(void)
{
    list3_head_t head;

    REQUIRE(list3_init(&head) == LIST3_OK);
    CHECK_EQ(list3_init(NULL), LIST3_ERR_NULL);
    CHECK_EQ(list3_add(NULL, &head), LIST3_ERR_NULL);
    CHECK_EQ(list3_add(&head, NULL), LIST3_ERR_NULL);
    CHECK_EQ(list3_add_tail(NULL, &head), LIST3_ERR_NULL);
    CHECK_EQ(list3_add_tail(&head, NULL), LIST3_ERR_NULL);
    CHECK_EQ(list3_del(NULL), LIST3_ERR_NULL);
    CHECK(list3_is_empty(NULL));
    CHECK(!list3_is_linked(NULL));
    CHECK_EQ(list3_count(NULL), 0);
    expect_empty(&head);
}

int main(void)
{
    RUN_TEST(t_container_of);
    RUN_TEST(t_init);
    RUN_TEST(t_static_init);
    RUN_TEST(t_add_front);
    RUN_TEST(t_add_tail);
    RUN_TEST(t_add_mixed);
    RUN_TEST(t_add_errors);
    RUN_TEST(t_del_positions);
    RUN_TEST(t_del_only);
    RUN_TEST(t_for_each_safe_delete);
    RUN_TEST(t_two_lists_same_objects);
    RUN_TEST(t_many);
    RUN_TEST(t_null_args);
    return TEST_REPORT();
}
