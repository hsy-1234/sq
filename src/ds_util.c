/**
 * @file    ds_util.c
 * @brief   跨模块通用工具：状态码文本、字符串、计时与随机数。
 *
 * 这些函数不依赖任何具体数据结构，放在单独模块里可以避免顺序表、
 * 链表、图等实现重复造轮子。字符串函数只处理以 '\0' 结尾的字节串，
 * 不做 Unicode 大小写转换；课设数据是 ASCII 学号/姓名，足够使用。
 */
#include "ds/ds_util.h"
#include "ds/ds_types.h"
#include "ds/ds_memory.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief 把 DsStatus 转成便于打印的中文说明。
 *
 * @param status 状态码。
 * @return 静态字符串，调用者不需要释放；未知状态返回“未知状态”。
 *
 * 用 switch 而不是字符串数组，避免枚举值不连续时数组越界。
 */
const char *ds_status_text(DsStatus status)
{
    switch (status) {
    case DS_OK:            return "成功";
    case DS_ERR:           return "操作失败";
    case DS_ERR_NULL:      return "空指针参数";
    case DS_ERR_MEMORY:    return "内存分配失败";
    case DS_ERR_RANGE:     return "位置或下标越界";
    case DS_ERR_EMPTY:     return "结构为空";
    case DS_ERR_FULL:      return "容量已满";
    case DS_ERR_NOT_FOUND: return "未找到目标";
    case DS_ERR_DUPLICATE: return "关键字重复";
    case DS_ERR_IO:        return "文件或输入输出错误";
    case DS_ERR_FORMAT:    return "数据格式错误";
    case DS_ERR_STATE:     return "当前状态不允许该操作";
    default:               return "未知状态";
    }
}

/**
 * @brief 安全的字符串复制，保证结果以 '\0' 结尾。
 *
 * @param dst      目标缓冲区。
 * @param dst_size 目标缓冲区字节数。
 * @param src      源字符串；为 NULL 时写入空串。
 *
 * 与 strncpy 的区别：无论源串多长，dst 最后一个字节一定是 '\0'，
 * 因此后续 strcmp/printf 不会越界读。dst 为 NULL 或大小为 0 时不做
 * 任何事。
 *
 * 复杂度 O(min(strlen(src), dst_size))。
 */
void ds_str_copy(char *dst, size_t dst_size, const char *src)
{
    size_t length;

    if (dst == NULL || dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    length = strlen(src);
    if (length >= dst_size) {
        /* 最多复制 dst_size - 1 个字符，留一个位置给 '\0'。 */
        length = dst_size - 1;
    }
    memcpy(dst, src, length);
    dst[length] = '\0';
}

/**
 * @brief 原地去掉字符串首尾空白。
 *
 * @param text 待处理字符串；为 NULL 时返回 NULL。
 * @return 传入的 text 指针，便于链式调用。
 *
 * 先找到第一个非空白字符 start，再从末尾向前找到最后一个非空白字符
 * end，截断尾部后用 memmove 把有效内容移到缓冲区开头。使用 memmove
 * 而不是 strcpy，是因为源和目标区域可能重叠。
 *
 * 复杂度 O(n)。
 */
char *ds_str_trim(char *text)
{
    char *start;
    char *end;

    if (text == NULL) {
        return NULL;
    }

    start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';

    if (start != text) {
        /* +1 把结尾的 '\0' 一起搬过去。 */
        memmove(text, start, (size_t)(end - start) + 1);
    }
    return text;
}

/**
 * @brief 忽略 ASCII 大小写比较两个字符串。
 *
 * @param a 字符串 a；NULL 视为空串。
 * @param b 字符串 b；NULL 视为空串。
 * @return 完全相等（忽略大小写）返回 true。
 *
 * 只对 ASCII 字母做 tolower；中文等多字节字符按原字节比较。
 * 复杂度 O(min(len(a), len(b)))。
 */
bool ds_str_equal_ci(const char *a, const char *b)
{
    if (a == NULL) {
        a = "";
    }
    if (b == NULL) {
        b = "";
    }
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

/**
 * @brief 把十进制字符串严格解析为 int。
 *
 * @param text  输入字符串，允许首尾空白。
 * @param value 接收结果；不能为 NULL。
 * @return 解析成功返回 true；空串、溢出、尾随垃圾返回 false。
 *
 * 先用 strtol 得到 long，再检查是否落在 INT_MIN..INT_MAX。这样在
 * 32 位 int、64 位 long 的 MinGW 上不会把超范围值悄悄截断。
 *
 * 复杂度 O(n)。
 */
bool ds_str_to_int(const char *text, int *value)
{
    char *end = NULL;
    long parsed;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return false;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }
    if (end == text) {
        /* 没有任何数字字符。 */
        return false;
    }
    while (*end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0') {
        /* 数字后面还有非空白字符，例如 "12abc"。 */
        return false;
    }

    *value = (int)parsed;
    return true;
}

/**
 * @brief 返回程序启动以来的 CPU 时间，单位毫秒。
 *
 * @return 毫秒数（双精度，便于统计短时间操作）。
 *
 * 使用 clock() 而不是墙钟时间，排序/查找性能对比时不受系统调度影响。
 * 不同平台的 CLOCKS_PER_SEC 可能不同，这里统一换算成毫秒。
 */
double ds_clock_ms(void)
{
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

/**
 * @brief 初始化随机数种子。
 *
 * @param seed 种子；传 0 表示用当前时间自动取种子。
 *
 * 固定种子可以让“随机抽取 100 条学生记录”的实验可复现；传 0 则每次
 * 运行结果不同，便于演示。
 */
void ds_random_seed(unsigned int seed)
{
    if (seed == 0u) {
        seed = (unsigned int)time(NULL);
    }
    srand(seed);
}

/**
 * @brief 返回 [min, max] 内的随机整数。
 *
 * @param min 下界（包含）。
 * @param max 上界（包含）。
 * @return 随机整数；max <= min 时直接返回 min。
 *
 * rand() % span 在 span 远小于 RAND_MAX 时偏差可以忽略；课设只用来
 * 抽取 100 条记录，不用于密码学场景。
 */
int ds_random_int(int min, int max)
{
    int span;

    if (max <= min) {
        return min;
    }
    span = max - min + 1;
    return min + rand() % span;
}

/**
 * @brief 原地随机打乱一个数组（Fisher-Yates 洗牌）。
 *
 * @param base  数组首地址。
 * @param count 元素个数。
 * @param size  单个元素字节数。
 *
 * 从最后一个元素开始，随机选择 [0, i] 中的一个位置与 i 交换。每个
 * 排列出现的概率相等，且不需要额外数组，只用 size 字节的临时缓冲区。
 *
 * 时间复杂度 O(n)，空间复杂度 O(size)。
 */
void ds_shuffle(void *base, size_t count, size_t size)
{
    unsigned char *bytes = (unsigned char *)base;
    unsigned char *temp;
    size_t i;

    if (base == NULL || count < 2 || size == 0) {
        return;
    }

    temp = (unsigned char *)ds_malloc(size);
    for (i = count - 1; i > 0; --i) {
        /* j 均匀落在 [0, i]，i 位置之后的元素已经固定。 */
        size_t j = (size_t)(rand() % (int)(i + 1));
        unsigned char *a = bytes + i * size;
        unsigned char *b = bytes + j * size;

        /* 用 memcpy 交换任意类型元素，避免按 int/结构体分别实现。 */
        memcpy(temp, a, size);
        memcpy(a, b, size);
        memcpy(b, temp, size);
    }
    ds_free((void **)&temp);
}
