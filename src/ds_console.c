/**
 * @file    ds_console.c
 * @brief   Windows 控制台 UTF-8/颜色/对齐的薄封装。
 *
 * 课设运行在中文 Windows 的 cmd/PowerShell 中，默认代码页通常是 GBK，
 * 直接 printf UTF-8 源码里的中文会变成乱码。本模块在启动时把输入/
 * 输出代码页切换为 UTF-8，并尽量开启 ANSI 转义序列支持，从而让
 * "\033[31m" 这样的颜色码和清屏码可用。退出时 restore 会恢复原代码页。
 *
 * 设计原则：
 *   - 所有“终端专属”操作都先用 _isatty 判断；输出被重定向到文件时
 *     不写 ANSI 转义序列，避免日志里出现乱码控制字符。
 *   - 宽度计算按 Unicode 码点估算：CJK、全角字符、emoji 算 2 列，
 *     其余算 1 列。课设表格里的中文标题/姓名因此可以对齐。
 *   - 本模块不负责换行以外的排版；表格列宽由 student.c 决定。
 */
#include "ds/ds_console.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

#define DS_CONSOLE_DEFAULT_WIDTH 60

/* 防止 ds_console_init 被重复调用覆盖掉最初保存的代码页。 */
static bool g_console_ready = false;
#ifdef _WIN32
static UINT g_saved_output_cp = 0;
static UINT g_saved_input_cp = 0;
#endif

/**
 * @brief 判断标准流是否连接到交互式终端。
 *
 * @param stream 标准输入或输出流。
 * @return 是终端返回 true；重定向到文件/管道返回 false。
 *
 * Windows 用 _isatty(_fileno(...))，POSIX 用 isatty(fileno(...))。
 */
static bool stream_is_tty(FILE *stream)
{
#ifdef _WIN32
    return _isatty(_fileno(stream)) != 0;
#else
    return isatty(fileno(stream)) != 0;
#endif
}

/**
 * @brief 初始化控制台：切换 UTF-8 代码页并开启 ANSI 转义。
 *
 * 只在 Windows 下做实际工作；重复调用是安全的。若 GetConsoleMode
 * 失败（例如输出被重定向），只是不开启 ANSI，不影响程序继续运行。
 */
void ds_console_init(void)
{
#ifdef _WIN32
    HANDLE output;
    DWORD mode;

    if (g_console_ready) {
        return;
    }
    /* 保存原代码页，供 ds_console_restore 恢复。 */
    g_saved_output_cp = GetConsoleOutputCP();
    g_saved_input_cp = GetConsoleCP();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &mode)) {
        /* 保留原有模式，只额外打开虚拟终端序列。 */
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    g_console_ready = true;
#endif
}

/**
 * @brief 恢复 ds_console_init 保存的代码页。
 *
 * 未初始化或已经恢复过时直接返回。程序正常退出前调用即可；即使不调用，
 * 进程结束时 Windows 也会回收控制台状态。
 */
void ds_console_restore(void)
{
#ifdef _WIN32
    if (!g_console_ready) {
        return;
    }
    if (g_saved_output_cp != 0) {
        SetConsoleOutputCP(g_saved_output_cp);
    }
    if (g_saved_input_cp != 0) {
        SetConsoleCP(g_saved_input_cp);
    }
    g_console_ready = false;
#endif
}

/**
 * @brief 清屏并把光标移到左上角。
 *
 * 仅当 stdout 是终端时输出 ANSI 序列 "\033[2J\033[H"；重定向到文件
 * 时什么也不做，避免日志被控制字符污染。
 */
void ds_console_clear(void)
{
    if (!stream_is_tty(stdout)) {
        return;
    }
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

/**
 * @brief 打印一条由 ch 组成的水平分隔线。
 *
 * @param ch    分隔字符，例如 '=' 或 '-'。
 * @param width 字符个数；<= 0 时使用默认宽度 60。
 */
void ds_console_rule(char ch, int width)
{
    int i;

    if (width <= 0) {
        width = DS_CONSOLE_DEFAULT_WIDTH;
    }
    for (i = 0; i < width; ++i) {
        putchar(ch);
    }
    putchar('\n');
}

/**
 * @brief 打印带上下边框的标题。
 *
 * @param title 标题文本；为 NULL 时只打印两条分隔线。
 */
void ds_console_title(const char *title)
{
    ds_console_rule('=', DS_CONSOLE_DEFAULT_WIDTH);
    if (title != NULL) {
        printf("%s\n", title);
    }
    ds_console_rule('=', DS_CONSOLE_DEFAULT_WIDTH);
}

/**
 * @brief 打印小节标题，格式为 "--- 标题 ---"。
 *
 * @param title 小节标题；为 NULL 时按空串处理。
 */
void ds_console_subtitle(const char *title)
{
    if (title == NULL) {
        title = "";
    }
    printf("--- %s ---\n", title);
}

/**
 * @brief 暂停，等待用户按回车。
 *
 * @param prompt 提示文本；为 NULL 时使用“按回车键继续...”。
 *
 * 如果 stdin 不是终端（例如自动化测试把输入重定向），直接返回，避免
 * 程序永远等不到回车。读取失败时 clearerr，防止 EOF 标志影响后续输入。
 */
void ds_console_pause(const char *prompt)
{
    char buffer[32];

    if (!stream_is_tty(stdin)) {
        return;
    }
    fflush(stdout);
    if (prompt == NULL) {
        prompt = "按回车键继续...";
    }
    fputs(prompt, stdout);
    fflush(stdout);
    if (fgets(buffer, (int)sizeof(buffer), stdin) == NULL) {
        clearerr(stdin);
    }
}

/**
 * @brief 解码一个 UTF-8 码点。
 *
 * @param text       指向当前字节的指针。
 * @param code_point 输出码点。
 * @return 消耗的字节数（1..4）。
 *
 * 只做结构合法性检查（首字节前缀、后续字节 10xxxxxx），不检查代理区
 * 和 Unicode 上限。遇到非法序列时按单字节处理并返回 1，保证扫描一定
 * 前进，不会死循环。
 */
static int utf8_decode(const unsigned char *text, unsigned int *code_point)
{
    if (text[0] < 0x80u) {
        *code_point = text[0];
        return 1;
    }
    if ((text[0] & 0xE0u) == 0xC0u && text[1] != '\0' &&
        (text[1] & 0xC0u) == 0x80u) {
        *code_point = ((unsigned int)(text[0] & 0x1Fu) << 6) |
                      (unsigned int)(text[1] & 0x3Fu);
        return 2;
    }
    if ((text[0] & 0xF0u) == 0xE0u && text[1] != '\0' && text[2] != '\0' &&
        (text[1] & 0xC0u) == 0x80u && (text[2] & 0xC0u) == 0x80u) {
        *code_point = ((unsigned int)(text[0] & 0x0Fu) << 12) |
                      ((unsigned int)(text[1] & 0x3Fu) << 6) |
                      (unsigned int)(text[2] & 0x3Fu);
        return 3;
    }
    if ((text[0] & 0xF8u) == 0xF0u && text[1] != '\0' && text[2] != '\0' &&
        text[3] != '\0' && (text[1] & 0xC0u) == 0x80u &&
        (text[2] & 0xC0u) == 0x80u && (text[3] & 0xC0u) == 0x80u) {
        *code_point = ((unsigned int)(text[0] & 0x07u) << 18) |
                      ((unsigned int)(text[1] & 0x3Fu) << 12) |
                      ((unsigned int)(text[2] & 0x3Fu) << 6) |
                      (unsigned int)(text[3] & 0x3Fu);
        return 4;
    }
    /* 非法首字节：原样返回，至少消耗一个字节。 */
    *code_point = text[0];
    return 1;
}

/**
 * @brief 估算一个 Unicode 码点在终端中占用的列数。
 *
 * @param cp Unicode 码点。
 * @return 0（控制字符）、2（CJK/全角/emoji）或 1（其他）。
 *
 * 这不是完整的 wcwidth 实现，但覆盖了课设会用到的中文、全角标点和
 * 常见 emoji。组合字符没有特殊处理（按 1 列计），不影响表格主体。
 */
static int code_point_width(unsigned int cp)
{
    if (cp == 0) {
        return 0;
    }
    if (cp < 0x20u || (cp >= 0x7Fu && cp < 0xA0u)) {
        /* C0/C1 控制字符不占显示列。 */
        return 0;
    }
    if ((cp >= 0x1100u && cp <= 0x115Fu) ||
        (cp >= 0x2E80u && cp <= 0xA4CFu && cp != 0x303Fu) ||
        (cp >= 0xAC00u && cp <= 0xD7A3u) ||
        (cp >= 0xF900u && cp <= 0xFAFFu) ||
        (cp >= 0xFE10u && cp <= 0xFE19u) ||
        (cp >= 0xFE30u && cp <= 0xFE6Fu) ||
        (cp >= 0xFF00u && cp <= 0xFF60u) ||
        (cp >= 0xFFE0u && cp <= 0xFFE6u) ||
        (cp >= 0x1F300u && cp <= 0x1F64Fu) ||
        (cp >= 0x1F900u && cp <= 0x1F9FFu) ||
        (cp >= 0x20000u && cp <= 0x2FFFDu) ||
        (cp >= 0x30000u && cp <= 0x3FFFDu)) {
        return 2;
    }
    return 1;
}

/**
 * @brief 计算字符串在终端中的显示宽度。
 *
 * @param text UTF-8 字符串；为 NULL 时返回 0。
 * @return 显示列数。
 *
 * 逐码点解码并累加宽度，时间复杂度 O(strlen(text))。
 */
int ds_console_text_width(const char *text)
{
    const unsigned char *cursor;
    int width = 0;

    if (text == NULL) {
        return 0;
    }

    cursor = (const unsigned char *)text;
    while (*cursor != '\0') {
        unsigned int cp;
        int length = utf8_decode(cursor, &cp);
        width += code_point_width(cp);
        cursor += length;
    }
    return width;
}

/**
 * @brief 按指定显示宽度打印一个表格单元格。
 *
 * @param text  单元格内容；为 NULL 时按空串处理。
 * @param width 目标显示宽度；<= 0 时直接输出原文。
 * @param align 左对齐或右对齐。
 *
 * 先算 text 的显示宽度，再补空格。若 text 本身比 width 宽，padding
 * 为 0，不截断内容——表格可能因此错位，但不会丢数据。
 */
void ds_console_print_cell(const char *text, int width, DsAlign align)
{
    int text_width;
    int padding;
    int i;

    if (text == NULL) {
        text = "";
    }
    if (width <= 0) {
        fputs(text, stdout);
        return;
    }

    text_width = ds_console_text_width(text);
    padding = width - text_width;
    if (padding < 0) {
        padding = 0;
    }

    if (align == DS_ALIGN_RIGHT) {
        for (i = 0; i < padding; ++i) {
            putchar(' ');
        }
        fputs(text, stdout);
    } else {
        fputs(text, stdout);
        for (i = 0; i < padding; ++i) {
            putchar(' ');
        }
    }
}

/**
 * @brief 统一输出带颜色前缀的消息。
 *
 * @param prefix 前缀，例如 "[成功] "。
 * @param color  ANSI 颜色码。
 * @param format printf 格式串。
 * @param args   可变参数列表。
 *
 * 只有 stdout 是终端时才输出颜色，重定向到文件时只保留前缀和正文。
 * 输出结束后恢复默认颜色，避免影响后续文本。
 */
static void console_message(const char *prefix, const char *color,
                            const char *format, va_list args)
{
    bool tty = stream_is_tty(stdout);

    if (tty) {
        fputs(color, stdout);
    }
    fputs(prefix, stdout);
    vprintf(format, args);
    if (tty) {
        fputs("\033[0m", stdout);
    }
    putchar('\n');
}

/**
 * @brief 打印绿色成功消息。
 *
 * @param format printf 格式串；为 NULL 时直接返回。
 * @param ...    格式参数。
 */
void ds_console_success(const char *format, ...)
{
    va_list args;

    if (format == NULL) {
        return;
    }
    va_start(args, format);
    console_message("[成功] ", "\033[32m", format, args);
    va_end(args);
}

/**
 * @brief 打印红色错误消息。
 *
 * @param format printf 格式串；为 NULL 时直接返回。
 * @param ...    格式参数。
 */
void ds_console_error(const char *format, ...)
{
    va_list args;

    if (format == NULL) {
        return;
    }
    va_start(args, format);
    console_message("[错误] ", "\033[31m", format, args);
    va_end(args);
}

/**
 * @brief 打印青色提示消息。
 *
 * @param format printf 格式串；为 NULL 时直接返回。
 * @param ...    格式参数。
 */
void ds_console_info(const char *format, ...)
{
    va_list args;

    if (format == NULL) {
        return;
    }
    va_start(args, format);
    console_message("[提示] ", "\033[36m", format, args);
    va_end(args);
}
