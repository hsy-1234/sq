/**
 * @file    ds_search.h
 * @brief   顺序查找与折半查找（带比较次数统计）。
 *
 * 比较函数约定：cmp(key, element)，返回 0 表示相等。
 * 折半查找要求数组已按 cmp 升序排列。
 */
#ifndef DS_SEARCH_H
#define DS_SEARCH_H

#include <stddef.h>
#include <stdbool.h>
#include "ds/ds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 查找结果。 */
typedef struct DsSearchResult {
    bool found;           /**< 是否找到。 */
    size_t index;         /**< 找到时为 0 基下标，否则为 count。 */
    size_t comparisons;   /**< 比较次数。 */
} DsSearchResult;

/** @brief 顺序查找。 */
DsSearchResult ds_search_sequential(const void *key, const void *base,
                                    size_t count, size_t size, DsCompareFn cmp);

/** @brief 非递归折半查找。 */
DsSearchResult ds_search_binary(const void *key, const void *base,
                                size_t count, size_t size, DsCompareFn cmp);

/** @brief 递归折半查找。 */
DsSearchResult ds_search_binary_recursive(const void *key, const void *base,
                                          size_t count, size_t size, DsCompareFn cmp);

#ifdef __cplusplus
}
#endif

#endif /* DS_SEARCH_H */
