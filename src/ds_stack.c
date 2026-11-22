/**
 * @file    ds_stack.c
 * @brief   基于动态数组的整数栈。
 *
 * 数据布局：
 *
 *     data[0] data[1] ... data[top-1] [空位] ...
 *                                       ^
 *                                       top 指向下一个可写位置
 *
 * 因此栈顶元素是 data[top - 1]，元素个数等于 top。容量不足时按 2 倍
 * 扩容，push 的均摊复杂度为 O(1)。
 */
#include "ds/ds_stack.h"
#include "ds/ds_memory.h"

#include <stdint.h>

#define STACK_DEFAULT_CAPACITY 16

/**
 * @brief 初始化栈。
 *
 * @param stack    待初始化的栈。
 * @param capacity 初始容量；为 0 时使用默认值 16。
 * @return DS_OK；stack 为 NULL 时返回 DS_ERR_NULL。
 *
 * 复杂度：O(1)。
 */
DsStatus stack_init(IntStack *stack, size_t capacity)
{
    if (stack == NULL) {
        return DS_ERR_NULL;
    }
    if (capacity == 0) {
        capacity = STACK_DEFAULT_CAPACITY;
    }

    stack->data = (int *)ds_malloc(capacity * sizeof(int));
    stack->capacity = capacity;
    stack->top = 0;
    return DS_OK;
}

/**
 * @brief 释放栈内部数组，并把所有字段恢复为初始状态。
 *
 * @param stack 栈；为 NULL 时直接返回。
 *
 * 复杂度：O(1)。
 */
void stack_destroy(IntStack *stack)
{
    if (stack == NULL) {
        return;
    }
    ds_free((void **)&stack->data);
    stack->capacity = 0;
    stack->top = 0;
}

/**
 * @brief 清空栈中元素，但保留已分配内存。
 *
 * @param stack 栈；为 NULL 时直接返回。
 *
 * 复杂度：O(1)。
 */
void stack_clear(IntStack *stack)
{
    if (stack != NULL) {
        stack->top = 0;
    }
}

/**
 * @brief 压栈。
 *
 * @param stack 栈。
 * @param value 待压入整数。
 * @return DS_OK；stack 为 NULL 返回 DS_ERR_NULL；容量无法再扩大时
 *         返回 DS_ERR_FULL（实际只在 size_t 溢出时发生）。
 *
 * 扩容策略：容量翻倍；若翻倍后反而变小，说明 size_t 已溢出，此时
 * 放弃本次操作而不是继续写入越界内存。
 *
 * 复杂度：单次最坏 O(n)（扩容并搬迁），均摊 O(1)。
 */
DsStatus stack_push(IntStack *stack, int value)
{
    if (stack == NULL) {
        return DS_ERR_NULL;
    }

    if (stack->top == stack->capacity) {
        size_t new_capacity = stack->capacity > 0 ? stack->capacity * 2 : STACK_DEFAULT_CAPACITY;
        if (new_capacity <= stack->capacity) {
            /* 2 倍增长发生回绕：容量已经到达 size_t 上限。 */
            return DS_ERR_FULL;
        }
        stack->data = (int *)ds_realloc(stack->data, new_capacity * sizeof(int));
        stack->capacity = new_capacity;
    }

    stack->data[stack->top++] = value;
    return DS_OK;
}

/**
 * @brief 弹栈。
 *
 * @param stack 栈。
 * @param out   接收栈顶元素的指针；可以为 NULL，表示只弹出不取值。
 * @return DS_OK；stack 为 NULL 返回 DS_ERR_NULL；栈空返回 DS_ERR_EMPTY。
 *
 * 先减小 top，再读取 data[top]，因此不会读到已弹出的旧值。
 * 复杂度：O(1)。
 */
DsStatus stack_pop(IntStack *stack, int *out)
{
    if (stack == NULL) {
        return DS_ERR_NULL;
    }
    if (stack->top == 0) {
        return DS_ERR_EMPTY;
    }

    --stack->top;
    if (out != NULL) {
        *out = stack->data[stack->top];
    }
    return DS_OK;
}

/**
 * @brief 读取栈顶元素，但不弹出。
 *
 * @param stack 栈。
 * @param out   接收栈顶元素的指针；不能为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；栈空返回 DS_ERR_EMPTY。
 *
 * 复杂度：O(1)。
 */
DsStatus stack_peek(const IntStack *stack, int *out)
{
    if (stack == NULL || out == NULL) {
        return DS_ERR_NULL;
    }
    if (stack->top == 0) {
        return DS_ERR_EMPTY;
    }
    *out = stack->data[stack->top - 1];
    return DS_OK;
}

/**
 * @brief 判断栈是否为空；NULL 也视为空。
 *
 * 复杂度：O(1)。
 */
bool stack_is_empty(const IntStack *stack)
{
    return stack == NULL || stack->top == 0;
}

/**
 * @brief 返回当前元素个数；NULL 返回 0。
 *
 * 复杂度：O(1)。
 */
size_t stack_size(const IntStack *stack)
{
    return stack != NULL ? stack->top : 0;
}
