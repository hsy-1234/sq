/**
 * @file    ds_types.h
 * @brief   基础库公共类型与状态码定义。
 *
 * 本文件只定义跨模块共用的类型，不包含任何实现。
 * 所有数据结构模块统一使用 DsStatus 作为返回值，便于在应用层
 * 直接根据状态码输出提示信息。
 */
#ifndef DS_TYPES_H
#define DS_TYPES_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 通用比较函数：a < b 返回负数，相等返回 0，a > b 返回正数。 */
typedef int (*DsCompareFn)(const void *a, const void *b);

/** 基础库统一状态码。 */
typedef enum DsStatus {
    DS_OK = 0,           /**< 操作成功。 */
    DS_ERR = -1,         /**< 一般性错误。 */
    DS_ERR_NULL = -2,    /**< 空指针参数。 */
    DS_ERR_MEMORY = -3,  /**< 内存分配失败。 */
    DS_ERR_RANGE = -4,   /**< 下标/位置越界。 */
    DS_ERR_EMPTY = -5,   /**< 表、栈、队列为空。 */
    DS_ERR_FULL = -6,    /**< 容量已满。 */
    DS_ERR_NOT_FOUND = -7, /**< 未找到目标。 */
    DS_ERR_DUPLICATE = -8, /**< 关键字重复。 */
    DS_ERR_IO = -9,      /**< 文件打开/读写失败。 */
    DS_ERR_FORMAT = -10, /**< 数据格式错误。 */
    DS_ERR_STATE = -11   /**< 对象状态不允许该操作。 */
} DsStatus;

/** 返回状态码对应的中文说明（静态字符串，不需要释放）。 */
const char *ds_status_text(DsStatus status);

/** 数组元素个数。 */
#define DS_ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

/** 显式标记未使用参数，避免编译警告。 */
#define DS_UNUSED(value) ((void)(value))

#ifdef __cplusplus
}
#endif

#endif /* DS_TYPES_H */
