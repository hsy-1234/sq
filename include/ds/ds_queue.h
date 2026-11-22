/**
 * @file    ds_queue.h
 * @brief   循环队列，元素类型为 int。
 *
 * 采用“牺牲一个存储单元”的经典写法区分队空与队满，
 * 实际最多存放 capacity - 1 个元素。
 */
#ifndef DS_QUEUE_H
#define DS_QUEUE_H

#include <stddef.h>
#include <stdbool.h>
#include "ds/ds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IntQueue {
    int *data;       /**< 存储区，容量为 capacity。 */
    size_t capacity; /**< 环形缓冲区槽位数。 */
    size_t front;    /**< 队头下标。 */
    size_t count;    /**< 当前元素个数。 */
} IntQueue;

/** @brief 初始化队列；capacity 为 0 时使用默认容量 16。 */
DsStatus queue_init(IntQueue *queue, size_t capacity);

/** @brief 释放存储区，可重复调用。 */
void queue_destroy(IntQueue *queue);

/** @brief 清空队列，保留容量。 */
void queue_clear(IntQueue *queue);

/** @brief 入队；队满返回 DS_ERR_FULL。 */
DsStatus queue_enqueue(IntQueue *queue, int value);

/** @brief 出队；队空返回 DS_ERR_EMPTY。 */
DsStatus queue_dequeue(IntQueue *queue, int *out);

/** @brief 取队头元素但不出队。 */
DsStatus queue_front(const IntQueue *queue, int *out);

/** @brief 队列是否为空。 */
bool queue_is_empty(const IntQueue *queue);

/** @brief 元素个数。 */
size_t queue_size(const IntQueue *queue);

#ifdef __cplusplus
}
#endif

#endif /* DS_QUEUE_H */
