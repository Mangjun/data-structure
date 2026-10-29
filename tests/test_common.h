/*
 * test_common.h - 최소 테스트 매크로 (외부 프레임워크 없음)
 *
 * CHECK      : 실패해도 계속 진행. 실패 위치를 출력하고 카운트만 올린다.
 * CHECK_EQ   : 정수 비교. 실패 시 두 값을 출력한다 (long long 으로 변환).
 * REQUIRE    : 이후 진행이 무의미한 전제 조건. 실패 시 현재 테스트 함수를 중단.
 * RUN_TEST   : 테스트 함수 실행.
 * RUN_TEST_ARG: 인자 하나를 받는 테스트 함수 실행 (같은 테스트를 여러 구현에 돌릴 때).
 * TEST_REPORT: main 끝에서 호출. 실패가 있으면 1 반환.
 *
 * assert()는 "테스트 코드 자체의 버그"를 잡는 용도로만 쓴다.
 * NDEBUG 를 정의하고 빌드하면 assert 가 사라지므로 Makefile 에서 정의하지 않는다.
 */
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <assert.h>
#include <stdio.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                       \
    do {                                                                  \
        g_checks++;                                                       \
        if (!(cond)) {                                                    \
            g_failures++;                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b)                                                    \
    do {                                                                  \
        long long va_ = (long long)(a);                                   \
        long long vb_ = (long long)(b);                                   \
        g_checks++;                                                       \
        if (va_ != vb_) {                                                 \
            g_failures++;                                                 \
            printf("  FAIL %s:%d: %s == %s (%lld != %lld)\n",             \
                   __FILE__, __LINE__, #a, #b, va_, vb_);                 \
        }                                                                 \
    } while (0)

#define REQUIRE(cond)                                                     \
    do {                                                                  \
        g_checks++;                                                       \
        if (!(cond)) {                                                    \
            g_failures++;                                                 \
            printf("  FAIL(REQUIRE) %s:%d: %s\n", __FILE__, __LINE__,     \
                   #cond);                                                \
            return;                                                       \
        }                                                                 \
    } while (0)

#define RUN_TEST(fn)                                                      \
    do {                                                                  \
        int before_ = g_failures;                                         \
        fn();                                                             \
        printf("%s %s\n", (g_failures == before_) ? "[ OK ]" : "[FAIL]", \
               #fn);                                                      \
    } while (0)

/* 인자를 받는 테스트 함수 실행. label 은 출력용 문자열. */
#define RUN_TEST_ARG(fn, arg, label)                                      \
    do {                                                                  \
        int before_ = g_failures;                                         \
        fn(arg);                                                          \
        printf("%s %s [%s]\n",                                            \
               (g_failures == before_) ? "[ OK ]" : "[FAIL]", #fn,        \
               (label));                                                  \
    } while (0)

#define TEST_REPORT()                                                   \
    (printf("\n%d checks, %d failures\n", g_checks, g_failures),          \
     (g_failures == 0) ? 0 : 1)

#endif /* TEST_COMMON_H */
