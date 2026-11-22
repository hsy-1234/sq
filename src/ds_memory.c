/**
 * @file    ds_memory.c
 * @brief   带失败检查的动态内存分配封装。
 *
 * 课设代码里如果每次 malloc 都写一遍 NULL 判断，业务代码会被错误处理
 * 淹没。本模块把“分配失败”统一提升为致命错误：打印请求字节数后立即
 * 退出，保证所有返回的指针都可以直接使用。这样顺序表、链表、图等模块
 * 只需要关心 DS_ERR_NULL / DS_ERR_RANGE 这类逻辑错误。
 *
 * 注意：ds_realloc 失败时原指针仍然有效，但本实现选择直接终止程序，
 * 因此不会出现“原指针泄漏 + 返回值丢失”的中间状态。
 */
#include "ds/ds_memory.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief 打印内存分配失败信息并终止进程。
 *
 * @param size 本次请求的字节数，仅用于错误提示。
 *
 * 该函数不返回；使用 exit(EXIT_FAILURE) 让操作系统回收全部资源。
 */
static void ds_memory_fail(size_t size)
{
    fprintf(stderr, "[致命错误] 内存分配失败，请求 %llu 字节。\n",
            (unsigned long long)size);
    exit(EXIT_FAILURE);
}

/**
 * @brief 分配 size 字节内存。
 *
 * @param size 请求字节数；传入 0 时按 1 字节处理，避免 malloc(0) 的
 *             实现相关行为（可能返回 NULL，也可能返回可 free 的指针）。
 * @return 指向新内存的指针，保证非 NULL。
 *
 * 复杂度：O(1)（不计分配器内部查找空闲块的时间）。
 */
void *ds_malloc(size_t size)
{
    void *ptr;

    if (size == 0) {
        /* 统一成 1 字节，保证返回值语义稳定。 */
        size = 1;
    }
    ptr = malloc(size);
    if (ptr == NULL) {
        ds_memory_fail(size);
    }
    return ptr;
}

/**
 * @brief 分配 count 个元素、每个 size 字节，并全部清零。
 *
 * @param count 元素个数；为 0 时按 1 处理。
 * @param size  单个元素字节数；为 0 时按 1 处理。
 * @return 指向清零内存的指针，保证非 NULL。
 *
 * 复杂度：O(count * size)。
 */
void *ds_calloc(size_t count, size_t size)
{
    void *ptr;

    if (count == 0 || size == 0) {
        /* 避免 calloc(0, n) 的实现相关行为。 */
        count = 1;
        size = 1;
    }
    ptr = calloc(count, size);
    if (ptr == NULL) {
        ds_memory_fail(count * size);
    }
    return ptr;
}

/**
 * @brief 把已分配内存调整为 size 字节。
 *
 * @param ptr  原内存指针，可以为 NULL（等价于 malloc）。
 * @param size 新字节数；为 0 时按 1 处理。
 * @return 调整后的指针，保证非 NULL。
 *
 * 复杂度：O(1) 或 O(size)，取决于分配器是否需要搬迁数据。
 */
void *ds_realloc(void *ptr, size_t size)
{
    void *result;

    if (size == 0) {
        /* 与 ds_malloc 保持一致：不产生 0 字节分配。 */
        size = 1;
    }
    result = realloc(ptr, size);
    if (result == NULL) {
        /* 此时 ptr 仍然有效，但本程序选择终止，不做半失败恢复。 */
        ds_memory_fail(size);
    }
    return result;
}

/**
 * @brief 释放 *ptr 指向的内存，并把 *ptr 置为 NULL。
 *
 * @param ptr 指向指针变量的地址；可以为 NULL。
 *
 * 传入二级指针是为了从根上消除悬空指针：调用者之后无法再通过该变量
 * 访问已释放内存。对 NULL 或 *ptr == NULL 直接返回，因此可以安全地
 * 重复调用。
 *
 * 复杂度：O(1)。
 */
void ds_free(void **ptr)
{
    if (ptr == NULL || *ptr == NULL) {
        return;
    }
    free(*ptr);
    *ptr = NULL;
}
