/**
 * @file    ds_stack.h
 * @brief   顺序栈，元素类型为 int。
 *
 * 用于迷宫求解、括号匹配、图的非递归遍历等场景。
 */
#ifndef DS_STACK_H
#define DS_STACK_H

#include <stddef.h>
#include <stdbool.h>
#include "ds/ds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IntStack {
    int *data;       /**< 存储区。 */
    size_t capacity; /**< 容量。 */
    size_t top;      /**< 栈顶下标 + 1，也等于元素个数。 */
} IntStack;

/** @brief 初始化栈；capacity 为 0 时使用默认容量 16。 */
DsStatus stack_init(IntStack *stack, size_t capacity);

/** @brief 释放存储区，可重复调用。 */
void stack_destroy(IntStack *stack);

/** @brief 清空元素，保留容量。 */
void stack_clear(IntStack *stack);

/** @brief 入栈。 */
DsStatus stack_push(IntStack *stack, int value);

/** @brief 出栈；栈空返回 DS_ERR_EMPTY。 */
DsStatus stack_pop(IntStack *stack, int *out);

/** @brief 取栈顶元素但不出栈。 */
DsStatus stack_peek(const IntStack *stack, int *out);

/** @brief 栈是否为空。 */
bool stack_is_empty(const IntStack *stack);

/** @brief 元素个数。 */
size_t stack_size(const IntStack *stack);

#ifdef __cplusplus
}
#endif

#endif /* DS_STACK_H */
