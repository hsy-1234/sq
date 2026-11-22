/**
 * @file    ds_console.h
 * @brief   控制台初始化、清屏、标题、分页与 UTF-8 对齐输出。
 *
 * Windows 控制台默认使用 GBK 代码页，而本工程源码统一为 UTF-8。
 * 程序启动时调用 ds_console_init() 把输入/输出代码页切换为 UTF-8，
 * 退出前调用 ds_console_restore() 恢复，避免中文乱码。
 */
#ifndef DS_CONSOLE_H
#define DS_CONSOLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 单元格对齐方式。 */
typedef enum DsAlign {
    DS_ALIGN_LEFT = 0,
    DS_ALIGN_RIGHT = 1
} DsAlign;

/** @brief 初始化控制台（UTF-8 代码页、ANSI 转义支持）。 */
void ds_console_init(void);

/** @brief 恢复控制台原有代码页。 */
void ds_console_restore(void);

/** @brief 清屏；输出被重定向时什么也不做。 */
void ds_console_clear(void);

/** @brief 输出一条分隔线，例如 ds_console_rule('=', 60)。 */
void ds_console_rule(char ch, int width);

/** @brief 输出带上下分隔线的标题。 */
void ds_console_title(const char *title);

/** @brief 输出二级标题。 */
void ds_console_subtitle(const char *title);

/**
 * @brief 暂停，等待用户按回车。
 * @param prompt 提示文字，可为 NULL（使用默认提示）。
 *
 * 当标准输入不是终端（例如重定向、自动化测试）时立即返回，避免卡死。
 */
void ds_console_pause(const char *prompt);

/**
 * @brief 计算 UTF-8 字符串在等宽终端中的显示宽度。
 *
 * ASCII 字符宽度为 1，常见中日韩字符宽度为 2，组合字符按 1 处理。
 */
int ds_console_text_width(const char *text);

/**
 * @brief 按显示宽度输出一个单元格，必要时补空格。
 * @param text  待输出文本，可为 NULL。
 * @param width 目标显示宽度（列数）。
 * @param align 左对齐或右对齐。
 */
void ds_console_print_cell(const char *text, int width, DsAlign align);

/** @brief 输出一行成功提示，自动换行。 */
void ds_console_success(const char *format, ...);

/** @brief 输出一行错误提示，自动换行。 */
void ds_console_error(const char *format, ...);

/** @brief 输出一行普通提示，自动换行。 */
void ds_console_info(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* DS_CONSOLE_H */
