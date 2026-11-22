# 数据结构与算法课程设计 —— 基础库 Makefile
# 使用 MinGW-w64 的 mingw32-make 构建（cmd.exe 作为 shell）。

CC      := gcc
AR      := ar
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O2 -g -Iinclude \
           -finput-charset=UTF-8 -fexec-charset=UTF-8 \
           -D_WIN32_WINNT=0x0601 -D__USE_MINGW_ANSI_STDIO=1
LDLIBS  := -lm

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
BIN_DIR   := $(BUILD_DIR)/bin
LIB       := $(BUILD_DIR)/libds.a

LIB_SRC := $(wildcard src/*.c)
LIB_OBJ := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(LIB_SRC))
LIB_DEP := $(LIB_OBJ:.o=.d)

APPS := lab01 lab02 lab03 lab04 smoke
APP_BINS := $(patsubst %,$(BIN_DIR)/%.exe,$(APPS))
APP_SRC := $(patsubst %,apps/%/main.c,$(APPS))

# 趣味题与基础题隔离：源码在 fun/metro，单独链接 libds.a。
METRO_DIR      := fun/metro
METRO_INC      := -I$(METRO_DIR)/include
METRO_SRC      := $(METRO_DIR)/main.c $(METRO_DIR)/src/ds_metro.c
METRO_LIB_SRC  := $(METRO_DIR)/src/ds_metro.c
METRO_HEADERS  := $(METRO_DIR)/include/ds/ds_metro.h
METRO_BIN      := $(BIN_DIR)/metro.exe
METRO_TEST_BIN := $(BIN_DIR)/test_metro.exe

MKDIR = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"

.PHONY: all lib clean metro metro-test run-metro run-metro-test \
        run-smoke run-lab01 run-lab02 run-lab03 run-lab04

all: lib $(APP_BINS) $(METRO_BIN)

lib: $(LIB)

$(LIB): $(LIB_OBJ)
	@$(call MKDIR,$(BUILD_DIR))
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: src/%.c
	@$(call MKDIR,$(OBJ_DIR))
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BIN_DIR)/%.exe: apps/%/main.c $(LIB)
	@$(call MKDIR,$(BIN_DIR))
	$(CC) $(CFLAGS) $< $(LIB) $(LDLIBS) -o $@

metro: $(METRO_BIN)

$(METRO_BIN): $(METRO_SRC) $(METRO_HEADERS) $(LIB)
	@$(call MKDIR,$(BIN_DIR))
	$(CC) $(CFLAGS) $(METRO_INC) $(METRO_SRC) $(LIB) $(LDLIBS) -o $@

metro-test: $(METRO_TEST_BIN)
	$(METRO_TEST_BIN)

$(METRO_TEST_BIN): $(METRO_DIR)/tests/test_metro.c $(METRO_LIB_SRC) $(METRO_HEADERS) $(LIB)
	@$(call MKDIR,$(BIN_DIR))
	$(CC) $(CFLAGS) $(METRO_INC) $(METRO_DIR)/tests/test_metro.c $(METRO_LIB_SRC) $(LIB) $(LDLIBS) -o $@

run-metro: $(METRO_BIN)
	$(METRO_BIN)

run-metro-test: $(METRO_TEST_BIN)
	$(METRO_TEST_BIN)

run-smoke: $(BIN_DIR)/smoke.exe
	$(BIN_DIR)/smoke.exe

run-lab01: $(BIN_DIR)/lab01.exe
	$(BIN_DIR)/lab01.exe

run-lab02: $(BIN_DIR)/lab02.exe
	$(BIN_DIR)/lab02.exe

run-lab03: $(BIN_DIR)/lab03.exe
	$(BIN_DIR)/lab03.exe

run-lab04: $(BIN_DIR)/lab04.exe
	$(BIN_DIR)/lab04.exe

clean:
	@if exist "$(subst /,\,$(BUILD_DIR))" rmdir /S /Q "$(subst /,\,$(BUILD_DIR))"

-include $(LIB_DEP)
