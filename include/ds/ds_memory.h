/**
 * @file    ds_memory.h
 * @brief   内存分配包装函数。
 *
 * 课设代码中大量使用 malloc/free。统一包装后可以：
 * 1. 集中检查分配失败；
 * 2. 避免忘记判断 NULL；
 * 3. 在释放后将指针置空，减少悬空指针。
 */
#ifndef DS_MEMORY_H
#define DS_MEMORY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 分配 size 字节内存，失败时打印错误并终止程序。
 * @param size 字节数，为 0 时按 1 字节处理。
 */
void *ds_malloc(size_t size);

/** @brief 分配并清零 count * size 字节内存。 */
void *ds_calloc(size_t count, size_t size);

/** @brief 调整已分配内存大小，失败时打印错误并终止程序。 */
void *ds_realloc(void *ptr, size_t size);

/**
 * @brief 释放 *ptr 指向的内存，并把 *ptr 置为 NULL。
 * @param ptr 指向指针变量的地址；ptr 或 *ptr 为 NULL 时安全返回。
 */
void ds_free(void **ptr);

#ifdef __cplusplus
}
#endif

#endif /* DS_MEMORY_H */
