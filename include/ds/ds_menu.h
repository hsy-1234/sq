/**
 * @file    ds_menu.h
 * @brief   可复用的控制台菜单框架。
 *
 * 用法示例：
 * @code
 * static void show_all(void *ctx) { ... }
 * static DsMenuItem items[] = {
 *     { 1, "显示全部学生", show_all },
 *     { 2, "排序",         NULL     }, // NULL 表示尚未实现
 * };
 * static DsMenu menu = { "学生管理系统", items, DS_ARRAY_LEN(items) };
 * ds_menu_run(&menu, &app);
 * @endcode
 */
#ifndef DS_MENU_H
#define DS_MENU_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 菜单项处理函数；context 由 ds_menu_run 原样传入。 */
typedef void (*DsMenuAction)(void *context);

/** 一个菜单项。id 为 0 时保留给“退出”，不要使用。 */
typedef struct DsMenuItem {
    int id;
    const char *text;
    DsMenuAction action;
} DsMenuItem;

/** 一个完整菜单。 */
typedef struct DsMenu {
    const char *title;
    DsMenuItem *items;
    size_t count;
} DsMenu;

/**
 * @brief 进入菜单循环，直到用户选择 0 或输入结束。
 * @param menu    菜单定义。
 * @param context 传给每个处理函数的上下文指针，可为 NULL。
 */
void ds_menu_run(const DsMenu *menu, void *context);

#ifdef __cplusplus
}
#endif

#endif /* DS_MENU_H */
