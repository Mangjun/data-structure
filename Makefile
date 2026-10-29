# C 선형 자료구조 복습 - Makefile
#
# 사용법
#   make test            모든 단계 테스트 빌드 + 실행
#   make test-stack      stack 테스트만
#   make test-ringbuf    ringbuf 테스트만
#   make test-pool / test-list_v1 / test-list_v2 / test-list_v3
#   make -k test         한 단계가 실패해도 나머지 단계를 계속 실행
#   make clean
#   make test EXTRA_CFLAGS=-DFOO   추가 플래그 (플래그 변경 시 make clean 먼저)
#
# 새 모듈 추가: MODULES에 이름을 추가하고 include/<m>.h, src/<m>.c,
# tests/test_<m>.c 를 만들면 test-<m> 타깃이 자동으로 생긴다.
#
# 주의: MinGW gcc는 -fsanitize=address,undefined 를 지원하지 않는다.
#       그 환경에서는 `make test SAN=` 으로 새니타이저를 끈다.

CC      ?= gcc
MODULES := stack ringbuf pool list_v1 list_v2 list_v3

INC_DIR := include
SRC_DIR := src
TST_DIR := tests
BLD_DIR := build

SAN     ?= -fsanitize=address,undefined
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -Wconversion \
           -g -O1 -fno-omit-frame-pointer $(SAN) -I$(INC_DIR) $(EXTRA_CFLAGS)
LDFLAGS := $(SAN)
DEPFLAGS = -MMD -MP

ifeq ($(OS),Windows_NT)
EXE := .exe
else
EXE :=
endif

.PHONY: all test clean $(addprefix test-,$(MODULES))

all: $(foreach m,$(MODULES),$(BLD_DIR)/test_$(m)$(EXE))

test: $(addprefix test-,$(MODULES))

# test-<m>: 빌드 후 실행. 실패 시 make가 비0 종료.
$(addprefix test-,$(MODULES)): test-%: $(BLD_DIR)/test_%$(EXE)
	./$<

# 테스트 바이너리 = 테스트 소스 + 해당 모듈 구현
$(BLD_DIR)/test_%$(EXE): $(BLD_DIR)/test_%.o $(BLD_DIR)/%.o
	$(CC) $^ $(LDFLAGS) -o $@

# 모듈 간 의존: list_v2 는 pool 노드를 쓴다
$(BLD_DIR)/test_list_v2$(EXE): $(BLD_DIR)/pool.o

# 중간 산출물(.o)을 make 가 자동 삭제하지 않게 한다 (증분 빌드 유지)
.SECONDARY:

$(BLD_DIR)/%.o: $(SRC_DIR)/%.c | $(BLD_DIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BLD_DIR)/test_%.o: $(TST_DIR)/test_%.c | $(BLD_DIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BLD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BLD_DIR)

-include $(wildcard $(BLD_DIR)/*.d)