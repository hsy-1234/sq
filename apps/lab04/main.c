/**
 * @file    main.c
 * @brief   题目四：查找与排序（菜单骨架，功能待实现）。
 *
 * 计划实现：随机抽取 100 条学生记录，至少两种排序、
 * 两种比较查找和哈希查找，输出 ASL 成功/失败。
 */

#include "ds/ds_console.h"
#include "ds/ds_menu.h"
#include "ds/ds_types.h"

static DsMenuItem items[] = {
    { 1, "随机抽取 100 条学生记录", NULL },
    { 2, "冒泡排序", NULL },
    { 3, "快速排序", NULL },
    { 4, "顺序查找", NULL },
    { 5, "折半查找", NULL },
    { 6, "哈希查找", NULL },
    { 7, "输出 ASL（成功/失败）", NULL },
    { 8, "多种算法性能对比", NULL }
};

static DsMenu menu = {
    "题目四：查找与排序",
    items,
    DS_ARRAY_LEN(items)
};

int main(void)
{
    ds_console_init();
    ds_menu_run(&menu, NULL);
    ds_console_restore();
    return 0;
}
