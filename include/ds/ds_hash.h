/**
 * @file    ds_hash.h
 * @brief   以学号为关键字的链地址法哈希表。
 *
 * 哈希函数可替换；默认 ds_hash_id 把 5 位学号转成整数后取模。
 * 链地址法天然处理“冲突”和“溢出”，并可直接统计 ASL。
 */
#ifndef DS_HASH_H
#define DS_HASH_H

#include <stddef.h>
#include "ds/ds_types.h"
#include "ds/student.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 哈希函数：把关键字映射到 [0, capacity) 的下标。 */
typedef size_t (*DsHashFn)(const char *key, size_t capacity);

/** 哈希桶结点。 */
typedef struct StudentHashNode {
    Student data;
    struct StudentHashNode *next;
} StudentHashNode;

/** 链地址法哈希表。 */
typedef struct StudentHash {
    StudentHashNode **buckets; /**< 桶数组。 */
    size_t capacity;           /**< 桶数。 */
    size_t count;              /**< 已存记录数。 */
    DsHashFn hash;             /**< 哈希函数。 */
} StudentHash;

/** @brief 默认哈希函数：学号取模；非纯数字关键字使用 djb2。 */
size_t ds_hash_id(const char *key, size_t capacity);

/** @brief 初始化哈希表；capacity 为 0 时使用默认值 101。 */
DsStatus student_hash_init(StudentHash *table, size_t capacity, DsHashFn hash);

/** @brief 释放所有结点，可重复调用。 */
void student_hash_destroy(StudentHash *table);

/** @brief 插入记录；学号重复返回 DS_ERR_DUPLICATE。 */
DsStatus student_hash_insert(StudentHash *table, Student value);

/**
 * @brief 按学号查找。
 * @param out         找到时返回记录，可为 NULL。
 * @param comparisons 返回比较次数，可为 NULL。
 */
DsStatus student_hash_search(const StudentHash *table, const char *id,
                             Student *out, size_t *comparisons);

/** @brief 按学号删除；out 可为 NULL。 */
DsStatus student_hash_delete(StudentHash *table, const char *id, Student *out);

/**
 * @brief 统计平均查找长度。
 * @param asl_success 成功查找的 ASL：Σ len_i(len_i+1)/2 / count。
 * @param asl_fail    失败查找的 ASL：Σ len_i / capacity。
 * @param max_chain   最长链长度，可为 NULL。
 */
void student_hash_stats(const StudentHash *table, double *asl_success,
                        double *asl_fail, size_t *max_chain);

/** @brief 输出装填因子、ASL、最长链等统计信息。 */
void student_hash_print_stats(const StudentHash *table);

#ifdef __cplusplus
}
#endif

#endif /* DS_HASH_H */
