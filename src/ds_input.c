/**
 * @file    ds_input.c
 * @brief   健壮的控制台输入封装。
 *
 * 所有输入都走“整行读取 + 解析 + 失败重试”的路径，而不是直接 scanf。
 * 这样可以避免 scanf 失败后把非法字符留在缓冲区、导致后续输入连环失败
 * 的经典问题。每个函数在参数合法时都会一直重试，直到用户输入正确值；
 * 只有遇到 EOF/IO 错误才返回 DS_ERR_IO。
 */
#include "ds/ds_input.h"
#include "ds/ds_console.h"
#include "ds/ds_util.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 读取一行文本，去掉行尾的 CR/LF。
 *
 * @param prompt 提示串；为 NULL 时不打印提示。
 * @param buffer 接收缓冲区。
 * @param size   缓冲区字节数。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；EOF/读错误返回 DS_ERR_IO。
 *
 * 关键点：
 *   - fgets 最多读 size-1 个字符；若没有读到 '\n'，说明这一行超长，
 *     必须把剩余字符丢弃，否则它们会被当成下一行输入。
 *   - Windows 文本行以 "\r\n" 结尾，因此先删 '\n' 再删 '\r'。
 *   - 丢弃超长行后调用 clearerr，清除可能因读到 EOF 而设置的错误标志。
 *
 * 时间复杂度 O(L)，L 为实际输入行长度。
 */
DsStatus ds_input_line(const char *prompt, char *buffer, size_t size)
{
    size_t length;
    bool complete_line;

    if (buffer == NULL || size == 0) {
        return DS_ERR_NULL;
    }
    if (prompt != NULL) {
        fputs(prompt, stdout);
    }
    fflush(stdout);

    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return DS_ERR_IO;
    }

    /* 是否读到了完整的一行：缓冲区里出现 '\n' 就说明没有截断。 */
    complete_line = strchr(buffer, '\n') != NULL;
    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
    }
    if (length > 0 && buffer[length - 1] == '\r') {
        buffer[--length] = '\0';
    }

    if (!complete_line) {
        int ch;
        /* 丢弃超长行剩余部分，保证下一次读取从新行开始。 */
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* 空循环体：这里只消费字符。 */
        }
        clearerr(stdin);
    }
    return DS_OK;
}

/**
 * @brief 读取一个整数，失败时提示并重试。
 *
 * @param prompt 提示串。
 * @param value  接收结果；不能为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；EOF 返回 DS_ERR_IO。
 *
 * 使用 ds_str_to_int 做严格解析：允许首尾空白，但不允许多余字符，
 * 并检查 long 是否落在 int 范围内。
 */
DsStatus ds_input_int(const char *prompt, int *value)
{
    char line[64];

    if (value == NULL) {
        return DS_ERR_NULL;
    }

    for (;;) {
        DsStatus status = ds_input_line(prompt, line, sizeof(line));
        if (status != DS_OK) {
            return status;
        }
        ds_str_trim(line);
        if (ds_str_to_int(line, value)) {
            return DS_OK;
        }
        ds_console_error("请输入合法的整数。");
    }
}

/**
 * @brief 读取 [min, max] 范围内的整数。
 *
 * @param prompt 提示串。
 * @param min    下界（包含）。
 * @param max    上界（包含）。
 * @param value  接收结果；不能为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；EOF 返回 DS_ERR_IO。
 *
 * 先复用 ds_input_int 做语法校验，再做范围校验；越界时提示合法区间
 * 并重新读取。
 */
DsStatus ds_input_int_range(const char *prompt, int min, int max, int *value)
{
    int parsed;

    if (value == NULL) {
        return DS_ERR_NULL;
    }

    for (;;) {
        DsStatus status = ds_input_int(prompt, &parsed);
        if (status != DS_OK) {
            return status;
        }
        if (parsed >= min && parsed <= max) {
            *value = parsed;
            return DS_OK;
        }
        ds_console_error("输入应在 %d 到 %d 之间，请重新输入。", min, max);
    }
}

/**
 * @brief 读取一个有限实数，失败时提示并重试。
 *
 * @param prompt 提示串。
 * @param value  接收结果；不能为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；EOF 返回 DS_ERR_IO。
 *
 * 用 strtod 解析并检查：
 *   - errno == ERANGE：上溢或下溢；
 *   - end == line：根本没有数字；
 *   - *end != '\0'：数字后面还有垃圾字符；
 *   - !isfinite(parsed)：拒绝 inf/nan。
 * 尾随空白会先被跳过，因此 "3.14   " 是合法输入。
 */
DsStatus ds_input_double(const char *prompt, double *value)
{
    char line[64];

    if (value == NULL) {
        return DS_ERR_NULL;
    }

    for (;;) {
        char *end = NULL;
        double parsed;
        DsStatus status = ds_input_line(prompt, line, sizeof(line));

        if (status != DS_OK) {
            return status;
        }
        ds_str_trim(line);
        if (line[0] == '\0') {
            ds_console_error("输入不能为空。");
            continue;
        }

        errno = 0;
        parsed = strtod(line, &end);
        while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) {
            ++end;
        }
        if (errno == ERANGE || end == line || end == NULL || *end != '\0' ||
            !isfinite(parsed)) {
            ds_console_error("请输入合法的实数。");
            continue;
        }
        *value = parsed;
        return DS_OK;
    }
}

/**
 * @brief 读取是非回答。
 *
 * @param prompt 提示串。
 * @param yes    接收结果；不能为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；EOF 返回 DS_ERR_IO。
 *
 * 接受 y/Y/yes/YES/是 和 n/N/no/NO/否；其他输入一律提示重试。
 * 英文比较使用 ds_str_equal_ci，中文“是/否”用 strcmp 精确比较。
 */
DsStatus ds_input_yes_no(const char *prompt, bool *yes)
{
    char line[16];

    if (yes == NULL) {
        return DS_ERR_NULL;
    }

    for (;;) {
        DsStatus status = ds_input_line(prompt, line, sizeof(line));
        if (status != DS_OK) {
            return status;
        }
        ds_str_trim(line);
        if (ds_str_equal_ci(line, "y") || ds_str_equal_ci(line, "yes") ||
            strcmp(line, "是") == 0) {
            *yes = true;
            return DS_OK;
        }
        if (ds_str_equal_ci(line, "n") || ds_str_equal_ci(line, "no") ||
            strcmp(line, "否") == 0) {
            *yes = false;
            return DS_OK;
        }
        ds_console_error("请输入 y 或 n。");
    }
}
