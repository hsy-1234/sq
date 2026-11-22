/**
 * @file    ds_queue.c
 * @brief   基于动态数组的循环队列。
 *
 * 数据布局（capacity = 8 为例）：
 *
 *     front          front+count
 *       |                 |
 *       v                 v
 *     [ a ][ b ][ c ][   ][   ][   ][   ][   ]
 *
 * 下标通过 % capacity 回绕，因此出队后空出的位置可以被后续入队复用。
 * 为了区分“队空”和“队满”，这里牺牲一个存储槽：
 *   - 队空：count == 0；
 *   - 队满：count == capacity - 1。
 * 因此实际最多存放 capacity - 1 个元素；capacity 至少为 2。
 */
#include "ds/ds_queue.h"
#include "ds/ds_memory.h"

#define QUEUE_DEFAULT_CAPACITY 16

/**
 * @brief 初始化循环队列。
 *
 * @param queue    待初始化的队列。
 * @param capacity 期望容量；小于 2 时使用默认值 16。
 * @return DS_OK；queue 为 NULL 时返回 DS_ERR_NULL。
 *
 * 复杂度：O(1)。
 */
DsStatus queue_init(IntQueue *queue, size_t capacity)
{
    if (queue == NULL) {
        return DS_ERR_NULL;
    }
    if (capacity < 2) {
        /* 至少保留一个空槽，否则无法区分队空和队满。 */
        capacity = QUEUE_DEFAULT_CAPACITY;
    }

    queue->data = (int *)ds_malloc(capacity * sizeof(int));
    queue->capacity = capacity;
    queue->front = 0;
    queue->count = 0;
    return DS_OK;
}

/**
 * @brief 释放队列内部数组，并把所有字段恢复为初始状态。
 *
 * @param queue 队列；为 NULL 时直接返回。
 *
 * 复杂度：O(1)。
 */
void queue_destroy(IntQueue *queue)
{
    if (queue == NULL) {
        return;
    }
    ds_free((void **)&queue->data);
    queue->capacity = 0;
    queue->front = 0;
    queue->count = 0;
}

/**
 * @brief 清空队列中的元素，但保留已分配的内存。
 *
 * @param queue 队列；为 NULL 时直接返回。
 *
 * 只重置 front 和 count，不移动数据，因此是 O(1)。
 */
void queue_clear(IntQueue *queue)
{
    if (queue != NULL) {
        queue->front = 0;
        queue->count = 0;
    }
}

/**
 * @brief 元素入队（追加到队尾）。
 *
 * @param queue 队列。
 * @param value 待入队整数。
 * @return DS_OK；队列为 NULL 返回 DS_ERR_NULL；队满返回 DS_ERR_FULL。
 *
 * 队尾下标 = (front + count) % capacity。由于最多只放 capacity-1 个
 * 元素，该下标不会与 front 重合。
 *
 * 复杂度：O(1)。
 */
DsStatus queue_enqueue(IntQueue *queue, int value)
{
    if (queue == NULL) {
        return DS_ERR_NULL;
    }
    if (queue->count >= queue->capacity - 1) {
        /* 留一个空槽，队满时不再写入。 */
        return DS_ERR_FULL;
    }

    queue->data[(queue->front + queue->count) % queue->capacity] = value;
    ++queue->count;
    return DS_OK;
}

/**
 * @brief 队头元素出队。
 *
 * @param queue 队列。
 * @param out   接收出队元素的指针；可以为 NULL，表示只删除不取值。
 * @return DS_OK；队列为 NULL 返回 DS_ERR_NULL；队空返回 DS_ERR_EMPTY。
 *
 * 出队只移动 front 下标，不搬移数组元素，因此是 O(1)。
 */
DsStatus queue_dequeue(IntQueue *queue, int *out)
{
    if (queue == NULL) {
        return DS_ERR_NULL;
    }
    if (queue->count == 0) {
        return DS_ERR_EMPTY;
    }

    if (out != NULL) {
        *out = queue->data[queue->front];
    }
    /* 回绕到数组开头，复用前面已经出队的位置。 */
    queue->front = (queue->front + 1) % queue->capacity;
    --queue->count;
    return DS_OK;
}

/**
 * @brief 读取队头元素，但不删除。
 *
 * @param queue 队列。
 * @param out   接收队头元素的指针；不能为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；队空返回 DS_ERR_EMPTY。
 *
 * 复杂度：O(1)。
 */
DsStatus queue_front(const IntQueue *queue, int *out)
{
    if (queue == NULL || out == NULL) {
        return DS_ERR_NULL;
    }
    if (queue->count == 0) {
        return DS_ERR_EMPTY;
    }
    *out = queue->data[queue->front];
    return DS_OK;
}

/**
 * @brief 判断队列是否为空；NULL 也视为空。
 *
 * 复杂度：O(1)。
 */
bool queue_is_empty(const IntQueue *queue)
{
    return queue == NULL || queue->count == 0;
}

/**
 * @brief 返回当前元素个数；NULL 返回 0。
 *
 * 复杂度：O(1)。
 */
size_t queue_size(const IntQueue *queue)
{
    return queue != NULL ? queue->count : 0;
}
