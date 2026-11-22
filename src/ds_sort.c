/**
 * @file    ds_sort.c
 * @brief   通用数组排序算法集合（void* + 元素大小 + 比较回调）。
 *
 * 设计目标：
 *   - 同一套接口可以排序 int、double、Student 等任意定长元素；
 *   - 所有算法共享 DsSortStats，统计“比较次数、移动次数、耗时”，
 *     便于在课程设计的性能对比实验中使用同一组数据公平比较；
 *   - 排序函数不关心元素含义，只通过 cmp 判断先后。
 *
 * 元素寻址：
 *   base 被视为 unsigned char*，第 i 个元素地址 = base + i * size。
 *   这样既符合 C 的字节寻址规则，也不会触发 void* 算术运算。
 *
 * 统计口径：
 *   comparisons：每次调用用户 cmp 前 +1；
 *   moves：复制一个元素 +1，交换两个元素 +3（一次临时保存 + 两次写回）；
 *   milliseconds：从参数校验之后开始计时，包含临时缓冲区的分配/释放。
 *
 * 算法与性质：
 *   算法      平均时间      最坏时间      额外空间   稳定性
 *   冒泡      O(n^2)        O(n^2)        O(1)       稳定
 *   直接插入  O(n^2)        O(n^2)        O(size)    稳定
 *   简单选择  O(n^2)        O(n^2)        O(1)       不稳定
 *   希尔      O(n^1.3) 左右  O(n^2)        O(size)    不稳定
 *   快速      O(n log n)    O(n^2)        O(log n)   不稳定
 *   归并      O(n log n)    O(n log n)    O(n*size)  稳定
 *   堆        O(n log n)    O(n log n)    O(1)       不稳定
 */
#include "ds/ds_sort.h"
#include "ds/ds_memory.h"
#include "ds/ds_util.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief 计算第 index 个元素的字节地址。
 *
 * @param base  数组首地址。
 * @param index 元素下标（0 基）。
 * @param size  单个元素字节数。
 * @return 指向该元素的 unsigned char*。
 *
 * 先转成 unsigned char* 再乘加，保证指针算术以字节为单位。
 */
static unsigned char *element_at(void *base, size_t index, size_t size)
{
    return (unsigned char *)base + index * size;
}

/**
 * @brief 带统计地比较两个元素。
 *
 * @param a     左元素地址。
 * @param b     右元素地址。
 * @param cmp   用户比较函数。
 * @param stats 统计对象。
 * @return cmp(a, b) 的返回值：负数表示 a 在前，正数表示 b 在前。
 *
 * 所有比较都必须经过这里，否则统计次数会漏算。
 */
static int compare_elements(const void *a, const void *b, DsCompareFn cmp,
                            DsSortStats *stats)
{
    ++stats->comparisons;  //统计比较次数
    return cmp(a, b);
}

/**
 * @brief 交换两个元素（按字节逐位交换）。
 *
 * @param a     第一个元素地址。
 * @param b     第二个元素地址。
 * @param size  元素字节数。
 * @param stats 统计对象；一次交换记为 3 次移动。
 *
 * 不要求元素按某个整数类型对齐，因此用 unsigned char 逐字节交换；
 * 编译器通常会把小尺寸循环展开成几条 mov 指令。
 */
static void swap_elements(unsigned char *a, unsigned char *b, size_t size,
                          DsSortStats *stats)
{
    size_t i;

    for (i = 0; i < size; ++i) {
        unsigned char temp = a[i];
        a[i] = b[i];
        b[i] = temp;
    }
    stats->moves += 3;
}

/**
 * @brief 复制一个元素。
 *
 * @param dst   目标地址。
 * @param src   源地址。
 * @param size  元素字节数。
 * @param stats 统计对象；一次复制记为 1 次移动。
 *
 * 源和目标不能重叠；本模块中的调用都满足这一条件。
 */
static void copy_element(unsigned char *dst, const unsigned char *src,
                         size_t size, DsSortStats *stats)
{
    memcpy(dst, src, size);
    ++stats->moves;
}

/**
 * @brief 把统计对象清零。
 *
 * @param stats 统计对象。
 */
static void stats_begin(DsSortStats *stats)
{
    stats->comparisons = 0;
    stats->moves = 0;
    stats->milliseconds = 0.0;
}

/**
 * @brief 写入耗时。
 *
 * @param stats 统计对象。
 * @param start 排序开始时的毫秒时间戳。
 */
static void stats_end(DsSortStats *stats, double start)
{
    stats->milliseconds = ds_clock_ms() - start;
}

/**
 * @brief 准备统计对象，允许调用方传 NULL。
 *
 * @param stats 调用方传入的统计对象，可以为 NULL。
 * @return 可写的统计对象；传 NULL 时返回内部静态对象。
 *
 * 静态 fallback 让“只排序、不关心统计”的调用也能复用同一套实现。
 * 它不是线程安全的：多线程同时传 NULL 会互相覆盖统计值；本课程设计
 * 是单线程程序，因此可以接受。
 */
static DsSortStats *prepare_stats(DsSortStats *stats)
{
    static DsSortStats fallback;

    if (stats == NULL) {
        stats = &fallback;
    }
    stats_begin(stats);
    return stats;
}

/**
 * @brief 冒泡排序（带“本轮无交换则提前结束”优化）。
 *
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @param stats 可选统计对象。
 *
 * 外层 i 表示已经排好的尾部元素个数；内层 j 从左到右比较相邻元素，
 * 逆序就交换。若某一轮一次交换都没发生，说明数组已经有序，可以提前
 * 退出，因此最好情况（已有序）是 O(n)。
 *
 * 稳定性：只在严格大于时交换，相等元素不会跨过彼此，稳定。
 * 最坏时间 O(n^2)，额外空间 O(1)。
 */
void ds_sort_bubble(void *base, size_t count, size_t size, DsCompareFn cmp,
                    DsSortStats *stats)
{
    size_t i;
    size_t j;
    double start;

    stats = prepare_stats(stats);
    if (base == NULL || count < 2 || size == 0 || cmp == NULL) {
        return;
    }
    start = ds_clock_ms();  //开始计时
    //核心算法：外层 i 表示已经排好的尾部元素个数；内层 j 从左到右比较相邻元素，逆序就交换
    for (i = 0; i + 1 < count; ++i) {
        bool swapped = false;
        for (j = 0; j + 1 < count - i; ++j) {
            unsigned char *left = element_at(base, j, size);
            unsigned char *right = element_at(base, j + 1, size);
            if (compare_elements(left, right, cmp, stats) > 0) {
                swap_elements(left, right, size, stats);
                swapped = true;  //标记本轮发生了交换
            }
        }
        if (!swapped) {
            break;
        }
    }
    stats_end(stats, start);
}

/**
 * @brief 直接插入排序。
 *
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @param stats 可选统计对象。
 *
 * 把 a[i] 暂存到 temp，然后从 i 向前找插入点：只要前一个元素比 temp
 * 大，就把前一个元素后移一位；找到位置后把 temp 写回。
 *
 * 与“边比较边交换”的写法相比，移动次数更少；temp 只分配一次，避免
 * 每个元素都 malloc。
 *
 * 稳定性：只有严格小于才继续前移，相等元素保持原相对顺序，稳定。
 * 最好 O(n)（已有序），最坏/平均 O(n^2)，额外空间 O(size)。
 */
void ds_sort_insertion(void *base, size_t count, size_t size, DsCompareFn cmp,
                       DsSortStats *stats)
{
    unsigned char *temp;
    size_t i;
    double start;

    stats = prepare_stats(stats);
    if (base == NULL || count < 2 || size == 0 || cmp == NULL) {
        return;
    }
    start = ds_clock_ms();
    temp = (unsigned char *)ds_malloc(size);

    for (i = 1; i < count; ++i) {
        size_t j = i;
        copy_element(temp, element_at(base, i, size), size, stats);
        while (j > 0 &&
               compare_elements(temp, element_at(base, j - 1, size), cmp,
                                stats) < 0) {
            copy_element(element_at(base, j, size),
                         element_at(base, j - 1, size), size, stats);
            --j;
        }
        copy_element(element_at(base, j, size), temp, size, stats);
    }

    ds_free((void **)&temp);
    stats_end(stats, start);
}

/**
 * @brief 简单选择排序。
 *
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @param stats 可选统计对象。
 *
 * 第 i 轮在 [i, count) 中找最小元素，若不在 i 就与 a[i] 交换。
 * 比较次数固定为 n(n-1)/2，与输入初始顺序无关；交换次数最多 n-1。
 *
 * 稳定性：交换可能把相等元素跨到后面，不稳定。
 * 时间 O(n^2)，额外空间 O(1)。
 */
void ds_sort_selection(void *base, size_t count, size_t size, DsCompareFn cmp,
                       DsSortStats *stats)
{
    size_t i;
    size_t j;
    double start;

    stats = prepare_stats(stats);
    if (base == NULL || count < 2 || size == 0 || cmp == NULL) {
        return;
    }
    start = ds_clock_ms();

    for (i = 0; i + 1 < count; ++i) {
        size_t smallest = i;
        for (j = i + 1; j < count; ++j) {
            if (compare_elements(element_at(base, j, size),
                                 element_at(base, smallest, size), cmp,
                                 stats) < 0) {
                smallest = j;
            }
        }
        if (smallest != i) {
            swap_elements(element_at(base, i, size),
                          element_at(base, smallest, size), size, stats);
        }
    }
    stats_end(stats, start);
}

/**
 * @brief 希尔排序（缩小增量插入排序）。
 *
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @param stats 可选统计对象。
 *
 * gap 从 count/2 开始逐次折半，直到 1。每个 gap 做一次“跨 gap 的插入
 * 排序”：a[i] 与 a[i-gap]、a[i-2*gap] ... 比较并后移。
 *
 * 增量序列会影响复杂度；这里用最简单的折半序列，最坏仍是 O(n^2)，
 * 但平均表现明显优于直接插入，且实现只比插入排序多一层 gap 循环。
 *
 * 稳定性：跨 gap 移动会改变相等元素的相对次序，不稳定。
 * 额外空间 O(size)。
 */
void ds_sort_shell(void *base, size_t count, size_t size, DsCompareFn cmp,
                   DsSortStats *stats)
{
    unsigned char *temp;
    size_t gap;
    size_t i;
    double start;

    stats = prepare_stats(stats);
    if (base == NULL || count < 2 || size == 0 || cmp == NULL) {
        return;
    }
    start = ds_clock_ms();
    temp = (unsigned char *)ds_malloc(size);

    for (gap = count / 2; gap > 0; gap /= 2) {
        for (i = gap; i < count; ++i) {
            size_t j = i;
            copy_element(temp, element_at(base, i, size), size, stats);
            while (j >= gap &&
                   compare_elements(temp, element_at(base, j - gap, size),
                                    cmp, stats) < 0) {
                copy_element(element_at(base, j, size),
                             element_at(base, j - gap, size), size, stats);
                j -= gap;
            }
            copy_element(element_at(base, j, size), temp, size, stats);
        }
    }

    ds_free((void **)&temp);
    stats_end(stats, start);
}

/**
 * @brief 快速排序的递归分区函数（Hoare 分区 + 尾递归优化）。
 *
 * @param base  数组首地址。
 * @param low   当前区间左端点（包含）。
 * @param high  当前区间右端点（包含）。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @param stats 统计对象。
 * @param pivot 可复用的 pivot 缓冲区，大小至少 size。
 *
 * 分区步骤：
 *   1. 取区间中点元素复制到 pivot（三数取中的简化版，避免有序输入退化）；
 *   2. i 从左往右找 >= pivot 的元素，j 从右往左找 <= pivot 的元素；
 *   3. i <= j 时交换并同时收缩，直到 i > j；
 *   4. 此时 [low, j] 和 [i, high] 是两块待排序区间。
 *
 * 尾递归优化：先递归较短的一侧，较长的一侧用 while 循环继续处理。
 * 这样递归深度不超过 O(log n)，避免恶意输入把栈压爆。
 *
 * 注意：pivot 缓冲区在递归调用中会被覆盖，但当前层在调用返回后不再
 * 使用旧 pivot，下一次 while 迭代会重新复制，因此共享一个缓冲区是安全的。
 */
static void quick_recursive(unsigned char *base, long long low, long long high,
                            size_t size, DsCompareFn cmp, DsSortStats *stats,
                            unsigned char *pivot)
{
    while (low < high) {
        long long i = low;
        long long j = high;
        long long middle = low + (high - low) / 2;

        copy_element(pivot, element_at(base, (size_t)middle, size), size, stats);
        while (i <= j) {
            while (compare_elements(element_at(base, (size_t)i, size), pivot,
                                    cmp, stats) < 0) {
                ++i;
            }
            while (compare_elements(element_at(base, (size_t)j, size), pivot,
                                    cmp, stats) > 0) {
                --j;
            }
            if (i <= j) {
                if (i != j) {
                    swap_elements(element_at(base, (size_t)i, size),
                                  element_at(base, (size_t)j, size), size,
                                  stats);
                }
                ++i;
                --j;
            }
        }

        /*
         * 两边都非空时，先递归较短的一侧，再循环处理较长的一侧。
         * j - low 是左区间长度，high - i 是右区间长度。
         */
        if (j - low < high - i) {
            quick_recursive(base, low, j, size, cmp, stats, pivot);
            low = i;
        } else {
            quick_recursive(base, i, high, size, cmp, stats, pivot);
            high = j;
        }
    }
}

/**
 * @brief 快速排序入口。
 *
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @param stats 可选统计对象。
 *
 * 只分配一个 pivot 缓冲区，递归过程中复用。平均 O(n log n)，最坏
 * O(n^2)（极端的 pivot 选择），额外空间 O(log n) 递归栈。
 *
 * 稳定性：分区交换会改变相等元素的相对次序，不稳定。
 */
void ds_sort_quick(void *base, size_t count, size_t size, DsCompareFn cmp,
                   DsSortStats *stats)
{
    unsigned char *pivot;
    double start;

    stats = prepare_stats(stats);
    if (base == NULL || count < 2 || size == 0 || cmp == NULL) {
        return;
    }
    start = ds_clock_ms();
    pivot = (unsigned char *)ds_malloc(size);
    quick_recursive((unsigned char *)base, 0, (long long)count - 1, size, cmp,
                    stats, pivot);
    ds_free((void **)&pivot);
    stats_end(stats, start);
}

/**
 * @brief 归并排序的递归函数。
 *
 * @param base  数组首地址。
 * @param temp  与 base 等大的临时缓冲区。
 * @param low   当前区间左端点（包含）。
 * @param high  当前区间右端点（包含）。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @param stats 统计对象。
 *
 * 先递归排序左右两半，再把两个有序段合并到 temp，最后整体复制回 base。
 * 合并时用 <= 判断，左段相等元素优先，保证稳定性。
 *
 * 时间复杂度 O(n log n)，空间 O(n*size)（temp 由入口一次性分配）。
 */
static void merge_recursive(unsigned char *base, unsigned char *temp,
                            size_t low, size_t high, size_t size,
                            DsCompareFn cmp, DsSortStats *stats)
{
    size_t middle;
    size_t left;
    size_t right;
    size_t out;

    if (low >= high) {
        return;
    }

    middle = low + (high - low) / 2;
    merge_recursive(base, temp, low, middle, size, cmp, stats);
    merge_recursive(base, temp, middle + 1, high, size, cmp, stats);

    left = low;
    right = middle + 1;
    out = low;
    while (left <= middle && right <= high) {
        if (compare_elements(element_at(base, left, size),
                             element_at(base, right, size), cmp, stats) <= 0) {
            copy_element(element_at(temp, out, size),
                         element_at(base, left, size), size, stats);
            ++left;
        } else {
            copy_element(element_at(temp, out, size),
                         element_at(base, right, size), size, stats);
            ++right;
        }
        ++out;
    }
    while (left <= middle) {
        copy_element(element_at(temp, out, size),
                     element_at(base, left, size), size, stats);
        ++left;
        ++out;
    }
    while (right <= high) {
        copy_element(element_at(temp, out, size),
                     element_at(base, right, size), size, stats);
        ++right;
        ++out;
    }

    /* 把合并结果写回原数组，供上一层继续合并。 */
    for (out = low; out <= high; ++out) {
        copy_element(element_at(base, out, size),
                     element_at(temp, out, size), size, stats);
    }
}

/**
 * @brief 归并排序入口。
 *
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @param stats 可选统计对象。
 *
 * 一次性分配 count*size 的临时数组，避免递归中反复 malloc。
 * 时间 O(n log n)，额外空间 O(n*size)。
 *
 * 稳定性：合并时左段优先，稳定。
 */
void ds_sort_merge(void *base, size_t count, size_t size, DsCompareFn cmp,
                   DsSortStats *stats)
{
    unsigned char *temp;
    double start;

    stats = prepare_stats(stats);
    if (base == NULL || count < 2 || size == 0 || cmp == NULL) {
        return;
    }
    start = ds_clock_ms();
    temp = (unsigned char *)ds_malloc(count * size);
    merge_recursive((unsigned char *)base, temp, 0, count - 1, size, cmp,
                    stats);
    ds_free((void **)&temp);
    stats_end(stats, start);
}

/**
 * @brief 对以 root 为根的子树做下沉调整（大顶堆）。
 *
 * @param base  数组首地址。
 * @param root  子树根下标。
 * @param end   堆的最后一个元素下标（包含）。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @param stats 统计对象。
 *
 * 在完全二叉树中，下标 root 的左孩子是 2*root+1，右孩子是 2*root+2。
 * 先选出较大的孩子，若根比孩子小就交换并继续向下，直到叶子或满足
 * 堆性质。
 *
 * 循环条件是 `root * 2 + 1 <= end`，当 end = 0 时不会进入循环，也不会
 * 发生 size_t 下溢（早期版本用 `root * 2 < end` 在 end=0 时有风险）。
 */
static void sift_down(unsigned char *base, size_t root, size_t end, size_t size,
                      DsCompareFn cmp, DsSortStats *stats)
{
    while (root * 2 + 1 <= end) {
        size_t child = root * 2 + 1;
        if (child + 1 <= end &&
            compare_elements(element_at(base, child, size),
                             element_at(base, child + 1, size), cmp,
                             stats) < 0) {
            ++child;
        }
        if (compare_elements(element_at(base, root, size),
                             element_at(base, child, size), cmp, stats) < 0) {
            swap_elements(element_at(base, root, size),
                          element_at(base, child, size), size, stats);
            root = child;
        } else {
            break;
        }
    }
}

/**
 * @brief 堆排序。
 *
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @param stats 可选统计对象。
 *
 * 两个阶段：
 *   1. 建堆：从最后一个非叶子结点 count/2-1 开始向前依次下沉，
 *      时间复杂度 O(n)；
 *   2. 排序：每次把堆顶（最大值）与当前末尾交换，堆大小减一，
 *      再对新堆顶下沉；共 n-1 次，每次 O(log n)。
 *
 * 总时间 O(n log n)，额外空间 O(1)（递归下沉改为循环）。
 * 稳定性：堆顶与末尾的远距离交换会破坏相等元素顺序，不稳定。
 */
void ds_sort_heap(void *base, size_t count, size_t size, DsCompareFn cmp,
                  DsSortStats *stats)
{
    size_t i;
    size_t end;
    double start;

    stats = prepare_stats(stats);
    if (base == NULL || count < 2 || size == 0 || cmp == NULL) {
        return;
    }
    start = ds_clock_ms();

    /* 建大顶堆：叶子本身满足堆性质，从最后一个非叶子结点开始。 */
    for (i = count / 2; i > 0; --i) {
        sift_down((unsigned char *)base, i - 1, count - 1, size, cmp, stats);
    }
    /* 依次把最大值放到末尾，再恢复剩余部分的堆性质。 */
    for (end = count - 1; end > 0; --end) {
        swap_elements(element_at(base, 0, size), element_at(base, end, size),
                      size, stats);
        sift_down((unsigned char *)base, 0, end - 1, size, cmp, stats);
    }
    stats_end(stats, start);
}

/**
 * @brief 打印一组排序统计。
 *
 * @param name  算法名称，可以为 NULL（显示“排序”）。
 * @param stats 统计对象；为 NULL 时直接返回。
 *
 * 输出格式对齐，便于把多种算法的结果放在一起比较。
 */
void ds_sort_stats_print(const char *name, const DsSortStats *stats)
{
    if (stats == NULL) {
        return;
    }
    printf("%-12s 比较 %12llu 次，移动 %12llu 次，耗时 %8.3f ms\n",
           name != NULL ? name : "排序", stats->comparisons, stats->moves,
           stats->milliseconds);
}

/**
 * @brief 检查数组是否按 cmp 升序排列。
 *
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 * @param cmp   比较函数。
 * @return 有序（相邻元素均满足 cmp <= 0）返回 true，否则 false。
 *
 * 少于 2 个元素时恒为有序，即使 base 为 NULL 也返回 true；这是空数组
 * 的自然语义。count >= 2 时 base/cmp 非法会返回 false。
 *
 * 时间复杂度 O(n)。
 */
bool ds_sort_is_sorted(const void *base, size_t count, size_t size,
                       DsCompareFn cmp)
{
    const unsigned char *bytes = (const unsigned char *)base;
    size_t i;

    if (count < 2) {
        return true;
    }
    if (base == NULL || size == 0 || cmp == NULL) {
        return false;
    }

    for (i = 1; i < count; ++i) {
        if (cmp(bytes + (i - 1) * size, bytes + i * size) > 0) {
            return false;
        }
    }
    return true;
}
