/**
 * @file    ds_util.h
 * @brief   字符串、计时、随机数等通用工具。
 */
#ifndef DS_UTIL_H
#define DS_UTIL_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* 字符串工具                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief 安全复制字符串。
 * @param dst       目标缓冲区。
 * @param dst_size  目标缓冲区字节数，函数保证写入 '\0'。
 * @param src       源字符串，可为 NULL（此时写入空串）。
 */
void ds_str_copy(char *dst, size_t dst_size, const char *src);

/**
 * @brief 原地去掉字符串首尾的空白字符（空格、\t、\r、\n 等）。
 * @param text 以 '\0' 结尾的字符串，可为 NULL。
 * @return 返回去掉前导空白后的指针（可能指向 text 内部）。
 */
char *ds_str_trim(char *text);

/** @brief 忽略大小写比较两个字符串，任一为 NULL 时按空串处理。 */
bool ds_str_equal_ci(const char *a, const char *b);

/** @brief 将字符串解析为 int，要求整串都是合法整数。 */
bool ds_str_to_int(const char *text, int *value);

/* ------------------------------------------------------------------------- */
/* 计时                                                                       */
/* ------------------------------------------------------------------------- */

/**
 * @brief 返回进程启动以来的 CPU 时间（毫秒）。
 *
 * 使用标准库 clock()，精度足以比较排序/查找算法在同一台机器上的相对耗时。
 */
double ds_clock_ms(void);

/* ------------------------------------------------------------------------- */
/* 随机数与洗牌                                                               */
/* ------------------------------------------------------------------------- */

/**
 * @brief 初始化随机数种子。
 * @param seed 0 表示用当前时间作为种子，非 0 表示使用指定种子（便于复现实验）。
 */
void ds_random_seed(unsigned int seed);

/** @brief 返回 [min, max] 闭区间内的随机整数。 */
int ds_random_int(int min, int max);

/**
 * @brief 原地随机打乱数组（Fisher-Yates 算法）。
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 */
void ds_shuffle(void *base, size_t count, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* DS_UTIL_H */
