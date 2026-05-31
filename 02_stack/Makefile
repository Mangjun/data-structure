# ------------------------------------------------
# VEDA C Data Structure ADT - Makefile
# ------------------------------------------------

CFLAGS = -Wall -Wextra -I./include
DEBUG_FLAGS = -g -O0

# 디렉토리 구조
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

# 최종 실행 파일 경로 및 이름
TARGET = $(BIN_DIR)/stack_test

# src/ 폴더 내의 모든 .c 파일을 찾아 대응하는 .o 파일 이름으로 변환
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all debug clean dirs

# 일반 빌드
all: dirs $(TARGET)

# 디버그 빌드
debug: CFLAGS += $(DEBUG_FLAGS)
debug: all

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	rm -f tags cscope.out cscope.in.out cscope.po.out
