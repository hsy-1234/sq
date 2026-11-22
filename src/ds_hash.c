/**
 * @file    ds_hash.c
 * @brief   学生哈希表（链地址法）的实现。
 *
 * 结构示意（capacity = 5）：
 *
 *     buckets[0] -> [00012] -> [00047] -> NULL
 *     buckets[1] -> NULL
 *     buckets[2] -> [00103] -> NULL
 *     buckets[3] -> NULL
 *     buckets[4] -> [00005] -> [00090] -> [00211] -> NULL
 *
 * 每个桶指向一条单链表，冲突元素头插到链表中。装填因子
 * alpha = count / capacity；链地址法允许 alpha > 1，本课设用 1009 个
 * 桶装 4615 条记录，alpha 约为 4.57，平均查找长度仍然很小。
 *
 * 学号是纯数字时直接用“数值 % capacity”，这样同一个学号一定落在
 * 同一个桶；非纯数字键退化为 djb2 字符串哈希，保证接口通用。
 */
#include "ds/ds_hash.h"
#include "ds/ds_memory.h"

#include <stdio.h>
#include <string.h>

#define HASH_DEFAULT_CAPACITY 101

/**
 * @brief 计算字符串键的桶下标。
 *
 * @param key      键字符串。
 * @param capacity 桶数。
 * @return 落在 [0, capacity-1] 的下标；参数非法时返回 0。
 *
 * 分两种情况：
 *   1. 纯数字键：边扫描边累加 value = value * 10 + digit，最后取模。
 *      这样 "00012" 和 "12" 会得到相同的桶，符合“学号按数值散列”的
 *      直觉；size_t 溢出会回绕，但模运算仍然稳定。
 *   2. 含非数字字符：使用 djb2（value = value * 33 + byte），实现简单、
 *      分布均匀，适合姓名等短字符串。
 *
 * 时间复杂度 O(len(key))。
 */
size_t ds_hash_id(const char *key, size_t capacity)
{
    size_t value = 0;
    const unsigned char *cursor;
    bool numeric = true;

    if (key == NULL || capacity == 0) {
        return 0;
    }
    if (key[0] == '\0') {
        return 0;
    }

    /* 第一遍先判断是否全为数字，同时把数值累加出来。 */
    for (cursor = (const unsigned char *)key; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            numeric = false;
            break;
        }
        value = value * 10 + (size_t)(*cursor - '0');
    }
    if (numeric) {
        return value % capacity;
    }

    /* 非纯数字：重新扫描，使用 djb2。 */
    value = 5381;
    for (cursor = (const unsigned char *)key; *cursor != '\0'; ++cursor) {
        value = value * 33 + (size_t)*cursor;
    }
    return value % capacity;
}

/**
 * @brief 初始化哈希表。
 *
 * @param table    待初始化的哈希表。
 * @param capacity 桶数；为 0 时使用默认值 101。
 * @param hash     哈希函数；为 NULL 时使用 ds_hash_id。
 * @return DS_OK；table 为 NULL 返回 DS_ERR_NULL。
 *
 * buckets 用 calloc 清零，因此每个桶初始都是空链。capacity 不强制
 * 取质数，但实际建表时建议用质数（如 1009）以获得更均匀的分布。
 *
 * 复杂度 O(capacity)。
 */
DsStatus student_hash_init(StudentHash *table, size_t capacity, DsHashFn hash)
{
    if (table == NULL) {
        return DS_ERR_NULL;
    }
    if (capacity == 0) {
        capacity = HASH_DEFAULT_CAPACITY;
    }

    table->buckets = (StudentHashNode **)ds_calloc(capacity,
                                                   sizeof(StudentHashNode *));
    table->capacity = capacity;
    table->count = 0;
    table->hash = hash != NULL ? hash : ds_hash_id;
    return DS_OK;
}

/**
 * @brief 释放所有链表结点和桶数组。
 *
 * @param table 哈希表；为 NULL 时直接返回。
 *
 * 逐桶遍历链表，先保存 next 再释放当前结点，避免释放后继续解引用。
 * 复杂度 O(capacity + count)。
 */
void student_hash_destroy(StudentHash *table)
{
    size_t i;

    if (table == NULL) {
        return;
    }

    if (table->buckets != NULL) {
        for (i = 0; i < table->capacity; ++i) {
            StudentHashNode *node = table->buckets[i];
            while (node != NULL) {
                StudentHashNode *next = node->next;
                ds_free((void **)&node);
                node = next;
            }
        }
    }
    ds_free((void **)&table->buckets);
    table->capacity = 0;
    table->count = 0;
}

/**
 * @brief 插入一条学生记录。
 *
 * @param table 哈希表。
 * @param value 待插入记录（按值拷贝）。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；学号已存在返回
 *         DS_ERR_DUPLICATE。
 *
 * 先计算桶下标，再遍历该桶检查学号是否重复。确认不重复后，把新结点
 * 头插到桶链表头部——头插是 O(1)，不需要维护尾指针。
 *
 * 平均时间复杂度 O(1 + alpha)，最坏 O(n)（所有键冲突）。
 */
DsStatus student_hash_insert(StudentHash *table, Student value)
{
    size_t index;
    StudentHashNode *node;

    if (table == NULL || table->buckets == NULL || table->hash == NULL) {
        return DS_ERR_NULL;
    }

    index = table->hash(value.ID, table->capacity);
    for (node = table->buckets[index]; node != NULL; node = node->next) {
        if (strcmp(node->data.ID, value.ID) == 0) {
            return DS_ERR_DUPLICATE;
        }
    }

    node = (StudentHashNode *)ds_malloc(sizeof(StudentHashNode));
    node->data = value;
    node->next = table->buckets[index];
    table->buckets[index] = node;
    ++table->count;
    return DS_OK;
}

/**
 * @brief 按学号查找学生记录。
 *
 * @param table       哈希表。
 * @param id          学号。
 * @param out         命中时写入记录副本；可以为 NULL。
 * @param comparisons 命中/未命中时写入比较次数；可以为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；未找到返回 DS_ERR_NOT_FOUND。
 *
 * comparisons 用于实验统计：命中时等于目标在链中的位置（1 基），
 * 未命中时等于整条链的长度。
 *
 * 平均时间复杂度 O(1 + alpha)。
 */
DsStatus student_hash_search(const StudentHash *table, const char *id,
                             Student *out, size_t *comparisons)
{
    size_t index;
    size_t count = 0;
    StudentHashNode *node;

    if (comparisons != NULL) {
        *comparisons = 0;
    }
    if (table == NULL || table->buckets == NULL || table->hash == NULL ||
        id == NULL) {
        return DS_ERR_NULL;
    }

    index = table->hash(id, table->capacity);
    for (node = table->buckets[index]; node != NULL; node = node->next) {
        ++count;
        if (strcmp(node->data.ID, id) == 0) {
            if (out != NULL) {
                *out = node->data;
            }
            if (comparisons != NULL) {
                *comparisons = count;
            }
            return DS_OK;
        }
    }

    if (comparisons != NULL) {
        *comparisons = count;
    }
    return DS_ERR_NOT_FOUND;
}

/**
 * @brief 按学号删除记录。
 *
 * @param table 哈希表。
 * @param id    学号。
 * @param out   接收被删除记录；可以为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；未找到返回 DS_ERR_NOT_FOUND。
 *
 * 用“二级指针”遍历：link 指向“当前结点的指针字段”，这样删除头结点
 * 和删除中间结点可以用同一套代码，不需要单独判断前驱。
 *
 * 平均时间复杂度 O(1 + alpha)。
 */
DsStatus student_hash_delete(StudentHash *table, const char *id, Student *out)
{
    size_t index;
    StudentHashNode **link;

    if (table == NULL || table->buckets == NULL || table->hash == NULL ||
        id == NULL) {
        return DS_ERR_NULL;
    }

    index = table->hash(id, table->capacity);
    link = &table->buckets[index];
    while (*link != NULL) {
        if (strcmp((*link)->data.ID, id) == 0) {
            StudentHashNode *target = *link;
            /* 让前驱的 next（或桶头指针）直接跳过 target。 */
            *link = target->next;
            if (out != NULL) {
                *out = target->data;
            }
            ds_free((void **)&target);
            --table->count;
            return DS_OK;
        }
        link = &(*link)->next;
    }
    return DS_ERR_NOT_FOUND;
}

/**
 * @brief 统计平均查找长度（ASL）和最长链。
 *
 * @param table       哈希表。
 * @param asl_success 接收成功 ASL；可以为 NULL。
 * @param asl_fail    接收失败 ASL；可以为 NULL。
 * @param max_chain   接收最长链长度；可以为 NULL。
 *
 * 设第 i 条链长度为 L_i，记录总数为 n，桶数为 m：
 *
 *   成功 ASL = Σ [L_i(L_i+1)/2] / n
 *
 * 因为链中第 1 个元素比较 1 次、第 2 个比较 2 次……第 L 个比较 L 次，
 * 一条链的总比较次数是 1+2+...+L = L(L+1)/2。
 *
 *   失败 ASL = Σ L_i / m
 *
 * 失败查找必须走完整条链才能确认不存在，因此每条链贡献 L_i 次比较。
 *
 * 时间复杂度 O(m + n)。
 */
void student_hash_stats(const StudentHash *table, double *asl_success,
                        double *asl_fail, size_t *max_chain)
{
    size_t i;
    size_t longest = 0;
    double success_sum = 0.0;
    double fail_sum = 0.0;

    if (asl_success != NULL) {
        *asl_success = 0.0;
    }
    if (asl_fail != NULL) {
        *asl_fail = 0.0;
    }
    if (max_chain != NULL) {
        *max_chain = 0;
    }
    if (table == NULL || table->buckets == NULL || table->capacity == 0) {
        return;
    }

    for (i = 0; i < table->capacity; ++i) {
        size_t length = 0;
        const StudentHashNode *node = table->buckets[i];
        while (node != NULL) {
            ++length;
            node = node->next;
        }
        success_sum += (double)length * (double)(length + 1) / 2.0;
        fail_sum += (double)length;
        if (length > longest) {
            longest = length;
        }
    }

    if (asl_success != NULL) {
        /* n == 0 时没有“成功查找”，按 0 处理，避免除零。 */
        *asl_success = table->count > 0 ? success_sum / (double)table->count : 0.0;
    }
    if (asl_fail != NULL) {
        *asl_fail = fail_sum / (double)table->capacity;
    }
    if (max_chain != NULL) {
        *max_chain = longest;
    }
}

/**
 * @brief 打印哈希表的容量、装填因子和 ASL。
 *
 * @param table 哈希表；为 NULL 时直接返回。
 *
 * 输出示例：
 *   哈希表：桶数=1009，记录数=4615，装填因子=4.5738
 *   ASL 成功=2.9042，ASL 失败=4.5738，最长链=6
 */
void student_hash_print_stats(const StudentHash *table)
{
    double asl_success;
    double asl_fail;
    size_t max_chain;

    if (table == NULL) {
        return;
    }
    student_hash_stats(table, &asl_success, &asl_fail, &max_chain);
    printf("哈希表：桶数=%llu，记录数=%llu，装填因子=%.4f\n",
           (unsigned long long)table->capacity,
           (unsigned long long)table->count,
           table->capacity > 0 ? (double)table->count / (double)table->capacity
                               : 0.0);
    printf("ASL 成功=%.4f，ASL 失败=%.4f，最长链=%llu\n", asl_success,
           asl_fail, (unsigned long long)max_chain);
}
