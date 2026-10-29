/*
 * test_pool.c - pool 단계 테스트
 *
 * 상태(state) 저장 방식에 독립적으로 작성되어 있다:
 * 사용 가능 블록 수는 pool_capacity() 로만 판단한다.
 *
 * 저장소 앞뒤에 가드 바이트를 두어 풀이 storage 밖에 쓰지 않는지 확인한다.
 * 블록마다 고유한 바이트 패턴을 채워 두고 나중에 확인해 블록 겹침을 검출한다.
 */
#include <stdint.h>
#include <string.h>

#include "pool.h"
#include "test_common.h"

#define ARENA      1024u
#define GUARD_LEN  32u
#define GUARD_BYTE 0xA5u
#define MAX_BLOCKS (ARENA / sizeof(struct pool_free_node))

/* 저장소 = mem[GUARD_LEN .. GUARD_LEN + ARENA). 앞뒤는 GUARD_BYTE. */
typedef struct {
    _Alignas(max_align_t) unsigned char mem[GUARD_LEN + ARENA + GUARD_LEN];
} arena_t;

static unsigned char *arena_init(arena_t *a)
{
    memset(a->mem, (int)GUARD_BYTE, sizeof a->mem);
    return &a->mem[GUARD_LEN];
}

/* storage 로 넘긴 구간 [GUARD_LEN + off, GUARD_LEN + off + len) 밖이 전부 가드인지 */
static bool arena_guard_ok(const arena_t *a, size_t off, size_t len)
{
    size_t lo = GUARD_LEN + off;
    size_t hi = lo + len;

    for (size_t i = 0; i < sizeof a->mem; i++) {
        if ((i < lo || i >= hi) && a->mem[i] != GUARD_BYTE) {
            return false;
        }
    }
    return true;
}

static bool is_aligned(const void *ptr)
{
    return ((uintptr_t)ptr % POOL_ALIGN) == 0u;
}

/* ptr 에서 시작하는 len 바이트가 [base, base + size) 안에 있는지 (정수 비교) */
static bool inside(const void *ptr, size_t len, const void *base, size_t size)
{
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t b = (uintptr_t)base;

    return p >= b && len <= size && p - b <= size - len;
}

static void fill(void *ptr, size_t len, unsigned char v)
{
    memset(ptr, (int)v, len);
}

static bool check_fill(const void *ptr, size_t len, unsigned char v)
{
    const unsigned char *b = ptr;

    for (size_t i = 0; i < len; i++) {
        if (b[i] != v) {
            return false;
        }
    }
    return true;
}

/* 결정적 의사난수 (LCG) */
static uint32_t lcg_next(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

/* ---- init ---- */

static void t_init_null_args(void)
{
    pool_t p;
    arena_t a;
    unsigned char *s = arena_init(&a);

    CHECK_EQ(pool_init(NULL, s, ARENA, 16u), POOL_ERR_NULL);
    CHECK_EQ(pool_init(&p, NULL, ARENA, 16u), POOL_ERR_NULL);
    CHECK_EQ(pool_init(NULL, NULL, 0u, 0u), POOL_ERR_NULL);  /* NULL 이 INVAL 보다 우선 */
}

static void t_init_invalid_sizes(void)
{
    pool_t p;
    arena_t a;
    unsigned char *s = arena_init(&a);

    CHECK_EQ(pool_init(&p, s, ARENA, 0u), POOL_ERR_INVAL);       /* block_size 0 */
    CHECK_EQ(pool_init(&p, s, 0u, 16u), POOL_ERR_INVAL);         /* 저장소 0 */
    CHECK_EQ(pool_init(&p, s, 1u, 16u), POOL_ERR_INVAL);         /* 블록 1개도 못 담음 */
    CHECK_EQ(pool_init(&p, s, ARENA, ARENA), POOL_ERR_INVAL);    /* 상태 공간 + 정렬까지 못 담음 */
    CHECK_EQ(pool_init(&p, s, ARENA, SIZE_MAX), POOL_ERR_INVAL); /* 올림 계산 오버플로 */
    CHECK_EQ(pool_init(&p, s, ARENA, SIZE_MAX - 1u), POOL_ERR_INVAL);
    CHECK(arena_guard_ok(&a, 0u, ARENA));
}

static void t_init_failure_untouched(void)
{
    pool_t p;
    pool_t snapshot;
    arena_t a;
    unsigned char *s = arena_init(&a);

    memset(&p, 0x5C, sizeof p);
    memcpy(&snapshot, &p, sizeof p);

    CHECK(pool_init(&p, s, ARENA, 0u) != POOL_OK);
    CHECK(memcmp(&p, &snapshot, sizeof p) == 0);
    CHECK(pool_init(&p, s, 1u, 16u) != POOL_OK);
    CHECK(memcmp(&p, &snapshot, sizeof p) == 0);
    CHECK(pool_init(&p, NULL, ARENA, 16u) != POOL_OK);
    CHECK(memcmp(&p, &snapshot, sizeof p) == 0);
}

static void t_init_ok(void)
{
    pool_t p;
    arena_t a;
    unsigned char *s = arena_init(&a);
    const size_t req = 24u;

    REQUIRE(pool_init(&p, s, ARENA, req) == POOL_OK);

    size_t cap = pool_capacity(&p);
    size_t bs = pool_block_size(&p);

    CHECK(cap >= 1u);
    CHECK_EQ(pool_free_count(&p), cap);
    CHECK(bs >= req);
    CHECK(bs >= sizeof(struct pool_free_node));
    CHECK_EQ(bs % POOL_ALIGN, 0);
    CHECK(cap * bs <= ARENA);
    /* 상태 정보를 블록당 1바이트로 잡고 정렬 손실 2*POOL_ALIGN 을 빼도 이 정도는 담아야 한다 */
    CHECK(cap >= (ARENA - 2u * POOL_ALIGN) / (bs + 1u));
    CHECK(arena_guard_ok(&a, 0u, ARENA));
}

static void t_block_size_rounding(void)
{
    pool_t p;
    arena_t a;
    unsigned char *s = arena_init(&a);
    const size_t reqs[] = { 1u, 7u, POOL_ALIGN - 1u, POOL_ALIGN, POOL_ALIGN + 1u, 100u };

    for (size_t i = 0; i < sizeof reqs / sizeof reqs[0]; i++) {
        REQUIRE(pool_init(&p, s, ARENA, reqs[i]) == POOL_OK);
        size_t bs = pool_block_size(&p);
        CHECK(bs >= reqs[i]);
        CHECK(bs >= sizeof(struct pool_free_node));
        CHECK_EQ(bs % POOL_ALIGN, 0);
        CHECK(bs - reqs[i] < POOL_ALIGN + sizeof(struct pool_free_node)); /* 과도한 올림 금지 */
    }
}

/* ---- alloc ---- */

/* 블록을 전부 빌려 정렬/범위/겹침을 확인한다. storage 오프셋 off 를 바꿔 정렬 처리 확인. */
static void alloc_all_and_check(size_t off, size_t req)
{
    pool_t p;
    arena_t a;
    unsigned char *s = arena_init(&a) + off;
    const size_t size = ARENA - off;
    void *blocks[MAX_BLOCKS];
    int sentinel;
    void *out = &sentinel;

    REQUIRE(pool_init(&p, s, size, req) == POOL_OK);
    size_t cap = pool_capacity(&p);
    size_t bs = pool_block_size(&p);
    REQUIRE(cap <= MAX_BLOCKS);

    for (size_t i = 0; i < cap; i++) {
        REQUIRE(pool_alloc(&p, &blocks[i]) == POOL_OK);
        CHECK(blocks[i] != NULL);
        CHECK(is_aligned(blocks[i]));
        CHECK(inside(blocks[i], bs, s, size));
        CHECK_EQ(pool_free_count(&p), cap - i - 1u);
        fill(blocks[i], bs, (unsigned char)(i + 1u));
    }

    /* 다른 블록에 쓴 내용이 덮어써지지 않았으면 겹침이 없다 */
    for (size_t i = 0; i < cap; i++) {
        CHECK(check_fill(blocks[i], bs, (unsigned char)(i + 1u)));
    }

    CHECK_EQ(pool_alloc(&p, &out), POOL_ERR_EXHAUSTED);
    CHECK(out == &sentinel);                    /* 실패 시 *out 불변 */
    CHECK_EQ(pool_free_count(&p), 0);
    CHECK(arena_guard_ok(&a, off, size));
}

static void t_alloc_all_aligned_storage(void)
{
    alloc_all_and_check(0u, 24u);
}

static void t_alloc_all_misaligned_storage(void)
{
    for (size_t off = 1u; off < POOL_ALIGN; off += 3u) {
        alloc_all_and_check(off, 24u);
    }
}

static void t_alloc_all_tiny_block(void)
{
    alloc_all_and_check(0u, 1u);
}

/*
 * 정렬되지 않은 작은 저장소: 정렬로 건너뛸 바이트가 storage_size 보다 클 수 있다.
 * 이때 "storage_size - 건너뛴 바이트" 가 언더플로하면 블록 수가 거대해진다.
 * init 이 성공했다면 모든 블록이 storage 안에 있어야 하고, 아니면 INVAL 이어야 한다.
 */
static void t_init_misaligned_tiny(void)
{
    for (size_t off = 1u; off < POOL_ALIGN; off++) {
        for (size_t size = 1u; size <= 3u * POOL_ALIGN; size++) {
            pool_t p;
            arena_t a;
            unsigned char *s = arena_init(&a) + off;
            void *blocks[MAX_BLOCKS];

            pool_status_t st = pool_init(&p, s, size, 8u);
            if (st != POOL_OK) {
                CHECK_EQ(st, POOL_ERR_INVAL);
                continue;
            }

            size_t cap = pool_capacity(&p);
            size_t bs = pool_block_size(&p);
            REQUIRE(cap >= 1u);
            REQUIRE(cap <= MAX_BLOCKS);
            REQUIRE(bs != 0u && cap <= size / bs);     /* 블록 영역만으로도 size 를 넘으면 안 됨 */

            for (size_t i = 0; i < cap; i++) {
                REQUIRE(pool_alloc(&p, &blocks[i]) == POOL_OK);
                CHECK(is_aligned(blocks[i]));
                CHECK(inside(blocks[i], bs, s, size));
                fill(blocks[i], bs, 0x3Cu);
            }
            CHECK(arena_guard_ok(&a, off, size));
        }
    }
}

/* ---- free ---- */

static void t_free_all_and_reuse(void)
{
    pool_t p;
    arena_t a;
    unsigned char *s = arena_init(&a);
    void *blocks[MAX_BLOCKS];

    REQUIRE(pool_init(&p, s, ARENA, 32u) == POOL_OK);
    size_t cap = pool_capacity(&p);
    REQUIRE(cap <= MAX_BLOCKS);

    for (int round = 0; round < 3; round++) {
        for (size_t i = 0; i < cap; i++) {
            REQUIRE(pool_alloc(&p, &blocks[i]) == POOL_OK);
        }
        void *extra;
        CHECK_EQ(pool_alloc(&p, &extra), POOL_ERR_EXHAUSTED);

        for (size_t i = 0; i < cap; i++) {
            CHECK_EQ(pool_free(&p, blocks[i]), POOL_OK);
            CHECK_EQ(pool_free_count(&p), i + 1u);
        }
    }
    CHECK(arena_guard_ok(&a, 0u, ARENA));
}

static void t_free_errors(void)
{
    pool_t p;
    arena_t a;
    unsigned char *s = arena_init(&a);
    unsigned char foreign[64];
    void *b0;
    void *b1;

    REQUIRE(pool_init(&p, s, ARENA, 32u) == POOL_OK);
    REQUIRE(pool_capacity(&p) >= 2u);
    size_t bs = pool_block_size(&p);
    REQUIRE(pool_alloc(&p, &b0) == POOL_OK);
    REQUIRE(pool_alloc(&p, &b1) == POOL_OK);
    size_t free_before = pool_free_count(&p);

    CHECK_EQ(pool_free(&p, NULL), POOL_ERR_NULL);
    CHECK_EQ(pool_free(NULL, b0), POOL_ERR_NULL);
    CHECK_EQ(pool_free(&p, foreign), POOL_ERR_FOREIGN);
    CHECK_EQ(pool_free(&p, s + ARENA), POOL_ERR_FOREIGN);         /* storage 끝 바로 다음 */
    CHECK_EQ(pool_free(&p, (unsigned char *)b0 + 1), POOL_ERR_MISALIGNED);
    CHECK_EQ(pool_free(&p, (unsigned char *)b0 + bs / 2u), POOL_ERR_MISALIGNED);
    CHECK_EQ(pool_free(&p, (unsigned char *)b0 + bs - 1u), POOL_ERR_MISALIGNED);
    CHECK_EQ(pool_free_count(&p), free_before);                   /* 오류는 상태를 바꾸지 않음 */

    CHECK_EQ(pool_free(&p, b0), POOL_OK);
    CHECK_EQ(pool_free(&p, b0), POOL_ERR_DOUBLE_FREE);
    CHECK_EQ(pool_free_count(&p), free_before + 1u);

    /* 이중 해제 시도 후에도 free list 가 망가지지 않았는지: 끝까지 빌려 본다 */
    size_t n = 0;
    void *tmp;
    while (pool_alloc(&p, &tmp) == POOL_OK) {
        n++;
        REQUIRE(n <= MAX_BLOCKS);
    }
    CHECK_EQ(n, free_before + 1u);
    CHECK(arena_guard_ok(&a, 0u, ARENA));
}

static void t_two_pools_foreign(void)
{
    pool_t pa, pb;
    arena_t aa, ab;
    void *x;

    REQUIRE(pool_init(&pa, arena_init(&aa), ARENA, 16u) == POOL_OK);
    REQUIRE(pool_init(&pb, arena_init(&ab), ARENA, 16u) == POOL_OK);
    REQUIRE(pool_alloc(&pa, &x) == POOL_OK);

    CHECK_EQ(pool_free(&pb, x), POOL_ERR_FOREIGN);
    CHECK_EQ(pool_free_count(&pb), pool_capacity(&pb));
    CHECK_EQ(pool_free(&pa, x), POOL_OK);
}

/* ---- 용량 1 ---- */

static void t_capacity_one(void)
{
    pool_t p;
    arena_t a;
    unsigned char *s = arena_init(&a);
    size_t size = 0;
    void *b0;
    void *b1;

    /* 블록 1개를 담는 가장 작은 저장소 크기를 찾는다 */
    for (size_t n = 1u; n <= ARENA; n++) {
        if (pool_init(&p, s, n, 16u) == POOL_OK) {
            size = n;
            break;
        }
    }
    REQUIRE(size != 0u);
    CHECK_EQ(pool_capacity(&p), 1);

    CHECK_EQ(pool_alloc(&p, &b0), POOL_OK);
    CHECK_EQ(pool_alloc(&p, &b1), POOL_ERR_EXHAUSTED);
    CHECK_EQ(pool_free(&p, b0), POOL_OK);
    CHECK_EQ(pool_free(&p, b0), POOL_ERR_DOUBLE_FREE);
    CHECK_EQ(pool_alloc(&p, &b1), POOL_OK);
    CHECK(b1 == b0);                            /* 블록이 하나뿐이므로 같은 주소 */
    CHECK(arena_guard_ok(&a, 0u, size));
}

/* ---- 무작위 할당/해제 ---- */

static void t_random_alloc_free(void)
{
    pool_t p;
    arena_t a;
    unsigned char *s = arena_init(&a);
    void *live[MAX_BLOCKS];
    unsigned char tag[MAX_BLOCKS];
    size_t n_live = 0;
    uint32_t seed = 0xBEEFu;
    unsigned char next_tag = 1u;
    const int failures_before = g_failures;

    REQUIRE(pool_init(&p, s, ARENA, 40u) == POOL_OK);
    size_t cap = pool_capacity(&p);
    size_t bs = pool_block_size(&p);
    REQUIRE(cap <= MAX_BLOCKS);

    for (unsigned step = 0; step < 20000u; step++) {
        uint32_t x = lcg_next(&seed);

        if ((x & 1u) != 0u) {
            void *b;
            pool_status_t st = pool_alloc(&p, &b);
            if (n_live < cap) {
                CHECK_EQ(st, POOL_OK);
                if (st == POOL_OK) {
                    fill(b, bs, next_tag);
                    live[n_live] = b;
                    tag[n_live] = next_tag;
                    n_live++;
                    next_tag = (unsigned char)(next_tag == 0xFFu ? 1u : next_tag + 1u);
                }
            } else {
                CHECK_EQ(st, POOL_ERR_EXHAUSTED);
            }
        } else if (n_live > 0u) {
            size_t k = (size_t)(x >> 8) % n_live;
            CHECK(check_fill(live[k], bs, tag[k]));  /* 다른 할당이 덮어쓰지 않음 */
            CHECK_EQ(pool_free(&p, live[k]), POOL_OK);
            live[k] = live[n_live - 1u];
            tag[k] = tag[n_live - 1u];
            n_live--;
        }
        CHECK_EQ(pool_free_count(&p), cap - n_live);
        if (g_failures != failures_before) {
            return;
        }
    }
    CHECK(arena_guard_ok(&a, 0u, ARENA));
}

/* ---- NULL 인자 / 조회 ---- */

static void t_alloc_null_args(void)
{
    pool_t p;
    arena_t a;
    void *out;

    CHECK_EQ(pool_alloc(NULL, &out), POOL_ERR_NULL);
    REQUIRE(pool_init(&p, arena_init(&a), ARENA, 16u) == POOL_OK);
    CHECK_EQ(pool_alloc(&p, NULL), POOL_ERR_NULL);
    CHECK_EQ(pool_free_count(&p), pool_capacity(&p));  /* 블록을 잃지 않음 */
}

static void t_null_queries(void)
{
    CHECK_EQ(pool_capacity(NULL), 0);
    CHECK_EQ(pool_free_count(NULL), 0);
    CHECK_EQ(pool_block_size(NULL), 0);
}

int main(void)
{
    RUN_TEST(t_init_null_args);
    RUN_TEST(t_init_invalid_sizes);
    RUN_TEST(t_init_failure_untouched);
    RUN_TEST(t_init_ok);
    RUN_TEST(t_block_size_rounding);
    RUN_TEST(t_alloc_all_aligned_storage);
    RUN_TEST(t_alloc_all_misaligned_storage);
    RUN_TEST(t_alloc_all_tiny_block);
    RUN_TEST(t_init_misaligned_tiny);
    RUN_TEST(t_free_all_and_reuse);
    RUN_TEST(t_free_errors);
    RUN_TEST(t_two_pools_foreign);
    RUN_TEST(t_capacity_one);
    RUN_TEST(t_random_alloc_free);
    RUN_TEST(t_alloc_null_args);
    RUN_TEST(t_null_queries);
    return TEST_REPORT();
}
