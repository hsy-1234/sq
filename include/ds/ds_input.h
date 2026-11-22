/**
 * @file    ds_input.h
 * @brief   健壮的键盘输入函数。
 *
 * 所有函数都按“整行读取 + 解析 + 失败重试”的方式工作：
 * 用户输入非法内容时不会污染下一次输入，也不会让 scanf 遗留字符。
 */
#ifndef DS_INPUT_H
#define DS_INPUT_H

#include <stddef.h>
#include <stdbool.h>
#include "ds/ds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 读取一行文本（去掉行尾 \r\n）。
 * @param prompt 提示文字，可为 NULL。
 * @param buffer 输出缓冲区。
 * @param size   缓冲区字节数。
 * @return DS_OK；输入结束（EOF）返回 DS_ERR_IO。
 */
DsStatus ds_input_line(const char *prompt, char *buffer, size_t size);

/**
 * @brief 读取一个整数，输入非法时提示并重新输入。
 * @return DS_OK；EOF 返回 DS_ERR_IO。
 */
DsStatus ds_input_int(const char *prompt, int *value);

/**
 * @brief 读取 [min, max] 范围内的整数，越界时提示并重新输入。
 */
DsStatus ds_input_int_range(const char *prompt, int min, int max, int *value);

/** @brief 读取一个浮点数，输入非法时提示并重新输入。 */
DsStatus ds_input_double(const char *prompt, double *value);

/** @brief 读取 y/n 确认，接受 y、Y、n、N。 */
DsStatus ds_input_yes_no(const char *prompt, bool *yes);

#ifdef __cplusplus
}
#endif

#endif /* DS_INPUT_H */
