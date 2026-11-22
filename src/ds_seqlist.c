/**
 * @file    ds_seqlist.c
 * @brief   学生记录顺序表（动态数组）的实现。
 *
 * 逻辑结构：
 *
 *     data[0] data[1] ... data[length-1] [未使用 ...]
 *       ^                                  ^
 *       第 1 个元素                        第 length 个元素
 *
 * 对外位置参数统一从 1 开始，内部数组下标从 0 开始，因此所有接口在
 * 访问 data 前都要做“位置 - 1”的转换。这种约定与教材中的线性表一致，
 * 报告里描述“第 i 个位置”时不需要再解释下标偏移。
 *
 * 容量不足时按 2 倍扩容，push_back 的均摊复杂度为 O(1)；插入/删除
 * 需要搬移后续元素，时间复杂度为 O(n)。
 */
#include "ds/ds_seqlist.h"
#include "ds/ds_memory.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SEQLIST_DEFAULT_CAPACITY 16

/**
 * @brief 确保顺序表至少有 need 个元素的容量。
 *
 * @param list 顺序表。
 * @param need 需要的最小容量。
 * @return DS_OK；list 为 NULL 返回 DS_ERR_NULL。
 *
 * 扩容策略：
 *   - 初始容量为 16；
 *   - 不够时每次翻倍，直到 >= need；
 *   - 若翻倍会让 size_t 溢出，则直接取 need，避免死循环。
 *
 * 只扩容不缩容，因此 clear 之后内存仍然保留，适合反复增删的菜单程序。
 *
 * 复杂度：单次 O(n) 搬迁，均摊 O(1)。
 */
static DsStatus seqlist_reserve(SeqList *list, size_t need)
{
    size_t new_capacity;
    Student *new_data;

    if (list == NULL) {
        return DS_ERR_NULL;
    }
    if (need <= list->capacity) {
        return DS_OK;
    }

    new_capacity = list->capacity > 0 ? list->capacity : SEQLIST_DEFAULT_CAPACITY;
    while (new_capacity < need) {
        if (new_capacity > SIZE_MAX / 2) {
            /* 再翻倍就会溢出，直接满足本次需求。 */
            new_capacity = need;
            break;
        }
        new_capacity *= 2;
    }

    new_data = (Student *)ds_realloc(list->data, new_capacity * sizeof(Student));
    list->data = new_data;
    list->capacity = new_capacity;
    return DS_OK;
}

/**
 * @brief 初始化空顺序表。
 *
 * @param list     待初始化的顺序表。
 * @param capacity 初始容量；为 0 时使用默认值 16。
 * @return DS_OK；list 为 NULL 返回 DS_ERR_NULL。
 *
 * 复杂度 O(1)。
 */
DsStatus seqlist_init(SeqList *list, size_t capacity)
{
    if (list == NULL) {
        return DS_ERR_NULL;
    }
    if (capacity == 0) {
        capacity = SEQLIST_DEFAULT_CAPACITY;
    }

    list->data = (Student *)ds_malloc(capacity * sizeof(Student));  //分配内存
    list->length = 0;  //设置长度
    list->capacity = capacity;  //设置容量
    return DS_OK;
}

/**
 * @brief 释放顺序表内部数组，并把字段清零。
 *
 * @param list 顺序表；为 NULL 时直接返回。
 *
 * 复杂度 O(1)。
 */
void seqlist_destroy(SeqList *list)
{
    if (list == NULL) {
        return;
    }
    ds_free((void **)&list->data);
    list->length = 0;
    list->capacity = 0;
}

/**
 * @brief 清空元素但保留容量。
 *
 * @param list 顺序表；为 NULL 时直接返回。
 *
 * 只把 length 置 0，不释放内存，适合菜单里反复“重新建表”。
 * 复杂度 O(1)。
 */
void seqlist_clear(SeqList *list)
{
    if (list != NULL) {
        list->length = 0;
    }
}

/**
 * @brief 用一段现成的学生数组覆盖顺序表内容。
 *
 * @param list  顺序表。
 * @param items 源数组；count 为 0 时可以为 NULL。
 * @param count 元素个数。
 * @return DS_OK；参数非法返回 DS_ERR_NULL。
 *
 * 先扩容到 count，再整体 memcpy。源数组和 list->data 不允许重叠；
 * 本程序的调用场景是“从文件加载到新数组，再拷贝进表”，不会重叠。
 *
 * 复杂度 O(count)。
 */
DsStatus seqlist_assign(SeqList *list, const Student *items, size_t count)
{
    DsStatus status;

    if (list == NULL || (items == NULL && count > 0)) {
        return DS_ERR_NULL;
    }

    status = seqlist_reserve(list, count);
    if (status != DS_OK) {
        return status;
    }
    if (count > 0) {
        memcpy(list->data, items, count * sizeof(Student));
    }
    list->length = count;
    return DS_OK;
}

/**
 * @brief 在表尾追加一个学生记录。
 *
 * @param list  顺序表。
 * @param value 待追加记录（按值拷贝）。
 * @return DS_OK；list 为 NULL 返回 DS_ERR_NULL。
 *
 * 先确保容量足够，再写入 data[length] 并递增 length。
 * 均摊复杂度 O(1)，最坏 O(n)（扩容搬迁）。
 */
DsStatus seqlist_push_back(SeqList *list, Student value)
{
    DsStatus status;

    if (list == NULL) {
        return DS_ERR_NULL;
    }
    status = seqlist_reserve(list, list->length + 1);
    if (status != DS_OK) {
        return status;
    }
    list->data[list->length++] = value;
    return DS_OK;
}

/**
 * @brief 在第 pos 个位置插入一个学生记录。
 *
 * @param list  顺序表。
 * @param pos   插入位置，取值 1..length+1。
 * @param value 待插入记录。
 * @return DS_OK；list 为 NULL 返回 DS_ERR_NULL；pos 越界返回 DS_ERR_RANGE。
 *
 * 位置 pos 对应数组下标 pos-1。若 pos <= length，需要把从 pos-1 开始
 * 的 length-pos+1 个元素整体后移一位，给新元素腾出空间；pos ==
 * length+1 时就是追加，不需要搬移。
 *
 * 时间复杂度 O(n)，空间复杂度 O(1)。
 */
DsStatus seqlist_insert(SeqList *list, size_t pos, Student value)
{
    DsStatus status;

    if (list == NULL) {
        return DS_ERR_NULL;
    }
    if (pos < 1 || pos > list->length + 1) {
        return DS_ERR_RANGE;
    }

    status = seqlist_reserve(list, list->length + 1);
    if (status != DS_OK) {
        return status;
    }
    if (pos <= list->length) {
        /*
         * 源区间 [pos-1, length-1]，目标区间 [pos, length]；
         * 两者重叠，必须用 memmove 而不是 memcpy。
         */
        memmove(&list->data[pos], &list->data[pos - 1],
                (list->length - pos + 1) * sizeof(Student));
    }
    list->data[pos - 1] = value;
    ++list->length;
    return DS_OK;
}

/**
 * @brief 删除第 pos 个位置的元素。
 *
 * @param list 顺序表。
 * @param pos  删除位置，取值 1..length。
 * @param out  接收被删除记录；可以为 NULL。
 * @return DS_OK；list 为 NULL 返回 DS_ERR_NULL；pos 越界返回 DS_ERR_RANGE。
 *
 * 先保存被删元素，再把后面的元素整体前移一位。删除最后一个元素时
 * 不需要搬移。
 *
 * 时间复杂度 O(n)，空间复杂度 O(1)。
 */
DsStatus seqlist_delete_at(SeqList *list, size_t pos, Student *out)
{
    if (list == NULL) {
        return DS_ERR_NULL;
    }
    if (pos < 1 || pos > list->length) {
        return DS_ERR_RANGE;
    }

    if (out != NULL) {
        *out = list->data[pos - 1];
    }
    if (pos < list->length) {
        /* 把 [pos, length-1] 前移到 [pos-1, length-2]。 */
        memmove(&list->data[pos - 1], &list->data[pos],
                (list->length - pos) * sizeof(Student));
    }
    --list->length;
    return DS_OK;
}

/**
 * @brief 按学号删除记录。
 *
 * @param list 顺序表。
 * @param id   学号字符串。
 * @param out  接收被删除记录；可以为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；未找到返回 DS_ERR_NOT_FOUND。
 *
 * 先顺序查找学号得到 1 基位置，再复用 seqlist_delete_at 完成删除。
 * 时间复杂度 O(n)。
 */
DsStatus seqlist_delete_by_id(SeqList *list, const char *id, Student *out)
{
    size_t pos;
    Student ignored;

    if (list == NULL || id == NULL) {
        return DS_ERR_NULL;
    }
    if (seqlist_find_by_id(list, id, &pos, &ignored) != DS_OK) {
        return DS_ERR_NOT_FOUND;
    }
    return seqlist_delete_at(list, pos, out);
}

/**
 * @brief 读取第 pos 个位置的元素。
 *
 * @param list 顺序表。
 * @param pos  位置，取值 1..length。
 * @param out  接收结果；不能为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；pos 越界返回 DS_ERR_RANGE。
 *
 * 顺序表支持随机访问，时间复杂度 O(1)。
 */
DsStatus seqlist_get(const SeqList *list, size_t pos, Student *out)
{
    if (list == NULL || out == NULL) {
        return DS_ERR_NULL;
    }
    if (pos < 1 || pos > list->length) {
        return DS_ERR_RANGE;
    }
    *out = list->data[pos - 1];
    return DS_OK;
}

/**
 * @brief 按学号顺序查找。
 *
 * @param list 顺序表。
 * @param id   学号字符串。
 * @param pos  命中时写入 1 基位置；可以为 NULL。
 * @param out  命中时写入记录副本；可以为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；未找到返回 DS_ERR_NOT_FOUND。
 *
 * 学号在 f1.txt 中唯一，因此找到第一个即可返回。最坏时间复杂度 O(n)。
 */
DsStatus seqlist_find_by_id(const SeqList *list, const char *id, size_t *pos,
                            Student *out)
{
    size_t i;

    if (list == NULL || id == NULL) {
        return DS_ERR_NULL;
    }

    for (i = 0; i < list->length; ++i) {
        if (strcmp(list->data[i].ID, id) == 0) {
            if (pos != NULL) {
                *pos = i + 1;
            }
            if (out != NULL) {
                *out = list->data[i];
            }
            return DS_OK;
        }
    }
    return DS_ERR_NOT_FOUND;
}

/**
 * @brief 求指定学号记录的前驱。
 *
 * @param list 顺序表。
 * @param id   目标学号。
 * @param out  接收前驱记录；不能为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；目标不存在返回
 *         DS_ERR_NOT_FOUND；目标在第一个位置（无前驱）返回 DS_ERR_RANGE。
 *
 * 先找到目标的位置 pos，前驱就是 data[pos-2]。时间复杂度 O(n)。
 */
DsStatus seqlist_predecessor(const SeqList *list, const char *id, Student *out)
{
    size_t pos;
    Student ignored;
    DsStatus status;

    if (list == NULL || id == NULL || out == NULL) {
        return DS_ERR_NULL;
    }

    status = seqlist_find_by_id(list, id, &pos, &ignored);
    if (status != DS_OK) {
        return status;
    }
    if (pos == 1) {
        return DS_ERR_RANGE;
    }
    *out = list->data[pos - 2];
    return DS_OK;
}

/**
 * @brief 按加权成绩从高到低排序。
 *
 * @param list 顺序表；为 NULL 或元素少于 2 个时直接返回。
 *
 * 直接调用标准库 qsort，比较函数由 student.c 提供。qsort 的平均
 * 时间复杂度 O(n log n)，最坏 O(n^2)；空间复杂度 O(log n) 递归栈。
 */
void seqlist_sort_by_weight_desc(SeqList *list)
{
    if (list == NULL || list->length < 2) {
        return;
    }
    qsort(list->data, list->length, sizeof(Student), student_compare_weight_desc);
}

/**
 * @brief 按学号从小到大排序。
 *
 * @param list 顺序表；为 NULL 或元素少于 2 个时直接返回。
 *
 * 时间复杂度 O(n log n)（qsort 平均情况）。
 */
void seqlist_sort_by_id(SeqList *list)
{
    if (list == NULL || list->length < 2) {
        return;
    }
    qsort(list->data, list->length, sizeof(Student), student_compare_id);
}

/**
 * @brief 分页打印顺序表内容。
 *
 * @param list      顺序表；为 NULL 时直接返回。
 * @param page_size 每页条数；0 表示不分页。
 *
 * 具体表格格式由 student_print_list 统一负责，顺序表模块只负责把
 * 连续数组和长度传过去。
 */
void seqlist_print(const SeqList *list, size_t page_size)
{
    if (list == NULL) {
        return;
    }
    student_print_list(list->data, list->length, page_size);
}
