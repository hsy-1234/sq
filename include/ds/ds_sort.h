/**
 * @file    ds_sort.h
 * @brief   通用排序算法集合（void* + 元素大小 + 比较函数）。
 *
 * 所有算法都支持统计比较次数、元素移动次数和耗时，
 * 便于在报告中比较不同排序算法的性能。
 *
 * 比较函数约定与 qsort 相同：
 *   cmp(a, b) < 0 表示 a 应排在 b 前面。
 */
#ifndef DS_SORT_H
#define DS_SORT_H

#include <stddef.h>
#include <stdbool.h>
#include "ds/ds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 排序统计信息。 */
typedef struct DsSortStats {
    unsigned long long comparisons; /**< 关键字比较次数。 */
    unsigned long long moves;       /**< 元素赋值/交换次数（一次交换计 3 次）。 */
    double milliseconds;            /**< 耗时（毫秒）。 */
} DsSortStats;

//-------------------- 排序算法接口 --------------------

void ds_sort_bubble(void *base, size_t count, size_t size,
                    DsCompareFn cmp, DsSortStats *stats);  //冒泡排序

void ds_sort_insertion(void *base, size_t count, size_t size,
                       DsCompareFn cmp, DsSortStats *stats);  //直接插入排序

void ds_sort_selection(void *base, size_t count, size_t size,
                       DsCompareFn cmp, DsSortStats *stats);  //简单选择排序

void ds_sort_shell(void *base, size_t count, size_t size,
                   DsCompareFn cmp, DsSortStats *stats);  //希尔排序

void ds_sort_quick(void *base, size_t count, size_t size,
                   DsCompareFn cmp, DsSortStats *stats);  //快速排序

void ds_sort_merge(void *base, size_t count, size_t size,
                   DsCompareFn cmp, DsSortStats *stats);  //归并排序

void ds_sort_heap(void *base, size_t count, size_t size,
                  DsCompareFn cmp, DsSortStats *stats);  //堆排序

/** @brief 输出一行统计信息。 */
void ds_sort_stats_print(const char *name, const DsSortStats *stats);

/** @brief 检查数组是否已按 cmp 升序排列。 */
bool ds_sort_is_sorted(const void *base, size_t count, size_t size, DsCompareFn cmp);

#ifdef __cplusplus
}
#endif

#endif /* DS_SORT_H */
