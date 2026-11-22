/**
 * @file    ds_seqlist.h
 * @brief   顺序表（动态数组），元素类型为 Student。
 *
 * 位置约定：所有带 pos 参数的接口都使用 1 基下标，
 * 即第 1 个元素 pos = 1，合法插入范围为 [1, length + 1]。
 */
#ifndef DS_SEQLIST_H
#define DS_SEQLIST_H

#include <stddef.h>
#include "ds/ds_types.h"
#include "ds/student.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SeqList {
    Student *data;   /**< 连续存储区。 */
    size_t length;   /**< 当前元素个数。 */
    size_t capacity; /**< 当前容量。 */
} SeqList;

/** @brief 初始化顺序表；capacity 为 0 时使用默认容量 16。 */
DsStatus seqlist_init(SeqList *list, size_t capacity);

/** @brief 释放顺序表内部存储，可重复调用。 */
void seqlist_destroy(SeqList *list);

/** @brief 清空元素，保留已分配容量。 */
void seqlist_clear(SeqList *list);

/** @brief 用数组内容覆盖顺序表。 */
DsStatus seqlist_assign(SeqList *list, const Student *items, size_t count);

/** @brief 在表尾追加元素。 */
DsStatus seqlist_push_back(SeqList *list, Student value);

/** @brief 在第 pos 个位置插入元素，pos ∈ [1, length + 1]。 */
DsStatus seqlist_insert(SeqList *list, size_t pos, Student value);

/** @brief 删除第 pos 个元素，pos ∈ [1, length]；out 可为 NULL。 */
DsStatus seqlist_delete_at(SeqList *list, size_t pos, Student *out);

/** @brief 删除学号等于 id 的第一个元素；out 可为 NULL。 */
DsStatus seqlist_delete_by_id(SeqList *list, const char *id, Student *out);

/** @brief 取第 pos 个元素。 */
DsStatus seqlist_get(const SeqList *list, size_t pos, Student *out);

/** @brief 按学号查找，返回 1 基位置。 */
DsStatus seqlist_find_by_id(const SeqList *list, const char *id, size_t *pos, Student *out);

/** @brief 求学号为 id 的结点的前驱；首元素无前驱时返回 DS_ERR_RANGE。 */
DsStatus seqlist_predecessor(const SeqList *list, const char *id, Student *out);

/** @brief 按加权成绩从大到小排序。 */
void seqlist_sort_by_weight_desc(SeqList *list);

/** @brief 按学号升序排序（供折半查找使用）。 */
void seqlist_sort_by_id(SeqList *list);

/** @brief 分页输出；page_size 为 0 表示不分页。 */
void seqlist_print(const SeqList *list, size_t page_size);

#ifdef __cplusplus
}
#endif

#endif /* DS_SEQLIST_H */
