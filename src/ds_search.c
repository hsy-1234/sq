/**
 * @file    ds_search.c
 * @brief   顺序查找与折半查找（含比较次数统计）。
 *
 * 所有函数都通过 DsCompareFn 比较“关键字”和“数组元素”，因此同一套
 * 代码既能查找整数，也能查找 Student 这样的结构体。比较函数的约定：
 *   cmp(key, element) < 0  表示 key 在 element 之前；
 *   cmp(key, element) == 0 表示命中；
 *   cmp(key, element) > 0  表示 key 在 element 之后。
 *
 * 折半查找要求数组已经按同一个比较函数升序排列；顺序查找没有这个
 * 要求。返回值的 index 在命中时是 0 基下标，未命中时等于 count。
 */
#include "ds/ds_search.h"

/**
 * @brief 构造查找结果，避免每个分支重复写三个字段。
 *
 * @param found       是否命中。
 * @param index       命中下标，或未命中时的 count。
 * @param comparisons 比较次数。
 * @return 填充好的结果结构体。
 */
static DsSearchResult search_result(bool found, size_t index, size_t comparisons)
{
    DsSearchResult result;

    result.found = found;
    result.index = index;
    result.comparisons = comparisons;
    return result;
}

/**
 * @brief 顺序查找：从下标 0 开始逐个比较。
 *
 * @param key   指向关键字的指针。
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @return 查找结果；参数非法时返回 found=false、comparisons=0。
 *
 * 最坏情况扫描全部 n 个元素，时间复杂度 O(n)；空间复杂度 O(1)。
 */
DsSearchResult ds_search_sequential(const void *key, const void *base,
                                    size_t count, size_t size, DsCompareFn cmp)
{
    const unsigned char *bytes = (const unsigned char *)base;
    size_t i;

    if (key == NULL || base == NULL || cmp == NULL || size == 0) {
        return search_result(false, count, 0);
    }

    for (i = 0; i < count; ++i) {
        /* 用字节指针 + i * size 计算第 i 个元素的地址，避免依赖具体类型。 */
        if (cmp(key, bytes + i * size) == 0) {
            return search_result(true, i, i + 1);
        }
    }
    return search_result(false, count, count);
}

/**
 * @brief 非递归折半查找。
 *
 * @param key   指向关键字的指针。
 * @param base  已按 cmp 升序排列的数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @return 查找结果；参数非法时返回 found=false、comparisons=0。
 *
 * 区间采用左闭右开 [low, high)：
 *   - high 初始为 count，循环条件 low < high；
 *   - 命中直接返回 middle；
 *   - key 更小则 high = middle，否则 low = middle + 1。
 * 这样不会出现 low/high 相邻时死循环，也不需要处理“最后一个元素”的
 * 特殊分支。
 *
 * 时间复杂度 O(log n)，空间复杂度 O(1)。
 */
DsSearchResult ds_search_binary(const void *key, const void *base,
                                size_t count, size_t size, DsCompareFn cmp)
{
    const unsigned char *bytes = (const unsigned char *)base;
    size_t low = 0;
    size_t high = count;
    size_t comparisons = 0;

    if (key == NULL || base == NULL || cmp == NULL || size == 0) {
        return search_result(false, count, 0);
    }

    while (low < high) {
        /* 写成 low + (high - low) / 2，避免 low + high 溢出 size_t。 */
        size_t middle = low + (high - low) / 2;
        int order;

        ++comparisons;
        order = cmp(key, bytes + middle * size);
        if (order == 0) {
            return search_result(true, middle, comparisons);
        }
        if (order < 0) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    return search_result(false, count, comparisons);
}

/**
 * @brief 递归折半查找的辅助函数。
 *
 * @param key         关键字指针。
 * @param bytes       数组首地址（字节指针）。
 * @param low         当前区间左端点（包含）。
 * @param high        当前区间右端点（不包含）。
 * @param size        单个元素字节数。
 * @param cmp         比较函数。
 * @param comparisons 比较次数累加器。
 * @param index       命中时写入下标。
 * @return 命中返回 true；区间为空返回 false。
 *
 * 递归深度为 O(log n)，每层只做一次比较。
 */
static bool binary_recursive(const void *key, const unsigned char *bytes,
                             size_t low, size_t high, size_t size,
                             DsCompareFn cmp, size_t *comparisons, size_t *index)
{
    size_t middle;
    int order;

    if (low >= high) {
        return false;
    }

    middle = low + (high - low) / 2;
    ++(*comparisons);
    order = cmp(key, bytes + middle * size);
    if (order == 0) {
        *index = middle;
        return true;
    }
    if (order < 0) {
        return binary_recursive(key, bytes, low, middle, size, cmp,
                                comparisons, index);
    }
    return binary_recursive(key, bytes, middle + 1, high, size, cmp,
                            comparisons, index);
}

/**
 * @brief 递归折半查找的对外接口。
 *
 * @param key   关键字指针。
 * @param base  已排序数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @return 查找结果；参数非法时返回 found=false、comparisons=0。
 *
 * 与 ds_search_binary 的差别只在实现方式：本函数把区间缩小的过程写成
 * 递归，便于在报告里对比“递归/非递归”的调用栈深度。
 */
DsSearchResult ds_search_binary_recursive(const void *key, const void *base,
                                          size_t count, size_t size,
                                          DsCompareFn cmp)
{
    size_t comparisons = 0;
    size_t index = count;

    if (key == NULL || base == NULL || cmp == NULL || size == 0) {
        return search_result(false, count, 0);
    }
    if (!binary_recursive(key, (const unsigned char *)base, 0, count, size,
                          cmp, &comparisons, &index)) {
        index = count;
        return search_result(false, index, comparisons);
    }
    return search_result(true, index, comparisons);
}
