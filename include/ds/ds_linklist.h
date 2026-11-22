/**
 * @file    ds_linklist.h
 * @brief   带头结点的单链表，元素类型为 Student。
 *
 * 位置约定：所有带 pos 参数的接口都使用 1 基下标。
 * 头结点不存放数据，不参与计数。
 */
#ifndef DS_LINKLIST_H
#define DS_LINKLIST_H

#include <stddef.h>
#include "ds/ds_types.h"
#include "ds/student.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 单链表结点。 */
typedef struct LNode {
    Student data;
    struct LNode *next;
} LNode;

/** 带头结点的单链表。 */
typedef struct LinkList {
    LNode *head;   /**< 头结点（哨兵），head->next 为首元素。 */
    size_t length; /**< 数据结点个数。 */
} LinkList;

/** @brief 初始化空链表（创建头结点）。 */
DsStatus linklist_init(LinkList *list);

/** @brief 释放所有结点，可重复调用。 */
void linklist_destroy(LinkList *list);

/** @brief 删除所有数据结点，保留头结点。 */
void linklist_clear(LinkList *list);

/** @brief 尾插法追加元素（与课设骨架 CreateListR 对应）。 */
DsStatus linklist_push_back(LinkList *list, Student value);

/** @brief 头插法插入元素（与课设骨架 CreateListF 对应）。 */
DsStatus linklist_push_front(LinkList *list, Student value);

/**
 * @brief 用数组批量重建链表。
 * @param list        已初始化的链表。
 * @param items       学生数组；count 为 0 时可为 NULL。
 * @param count       元素个数。
 * @param head_insert true 头插（结果与 items 顺序相反），
 *                    false 尾插（结果与 items 顺序一致）。
 * @return DS_OK；参数非法返回 DS_ERR_NULL。
 *
 * 先串好新链，成功后才替换旧数据，失败时原链表保持不变。
 * 时间复杂度 O(count)，空间复杂度 O(count)。
 */
DsStatus linklist_assign(LinkList *list, const Student *items, size_t count,
                         bool head_insert);

/** @brief 在第 pos 个位置插入，pos ∈ [1, length + 1]。 */
DsStatus linklist_insert(LinkList *list, size_t pos, Student value);

/** @brief 删除第 pos 个元素；out 可为 NULL。 */
DsStatus linklist_delete_at(LinkList *list, size_t pos, Student *out);

/** @brief 删除学号等于 id 的第一个元素；out 可为 NULL。 */
DsStatus linklist_delete_by_id(LinkList *list, const char *id, Student *out);

/** @brief 取第 pos 个元素。 */
DsStatus linklist_get(const LinkList *list, size_t pos, Student *out);

/** @brief 按学号查找结点，返回结点指针；未找到返回 NULL。 */
LNode *linklist_find_node(const LinkList *list, const char *id);

/** @brief 按学号查找并返回数据。 */
DsStatus linklist_find_by_id(const LinkList *list, const char *id, Student *out);

/** @brief 求前驱；首元素无前驱时返回 DS_ERR_RANGE。 */
DsStatus linklist_predecessor(const LinkList *list, const char *id, Student *out);

/** @brief 原地逆置链表。 */
void linklist_reverse(LinkList *list);

/** @brief 按加权成绩从大到小排序（结点重排，不复制数据）。 */
void linklist_sort_by_weight_desc(LinkList *list);

/** @brief 分页输出；page_size 为 0 表示不分页。 */
void linklist_print(const LinkList *list, size_t page_size);

/** @brief 从 f1.txt 格式文件尾插建表，返回成功读取的条数。 */
DsStatus linklist_load_file(LinkList *list, const char *path, size_t *out_count);

/** @brief 把链表写回 f1.txt 格式。 */
DsStatus linklist_save_file(const LinkList *list, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* DS_LINKLIST_H */
