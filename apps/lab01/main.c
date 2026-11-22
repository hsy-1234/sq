/**
 * @file    main.c
 * @brief   题目一：线性表（菜单骨架，功能待实现）。
 *
 * 计划实现：头插/尾插建表、输出、按位插入、求前驱、
 * 按位/按关键字删除、按加权成绩降序排序、保存文件。
 */

#include "ds/ds_console.h"
#include "ds/ds_menu.h"
#include "ds/ds_types.h"

static DsMenuItem items[] = {
    { 1, "建立链表（尾插法，读取 data/f1.txt）", NULL },
    { 2, "输出全部学生", NULL },
    { 3, "在第 i 个位置插入学生", NULL },
    { 4, "求指定学号的前驱", NULL },
    { 5, "按位置删除学生", NULL },
    { 6, "按学号删除学生", NULL },
    { 7, "按加权成绩降序排序", NULL },
    { 8, "保存到文件", NULL },
    { 9, "顺序表与链表实现对比", NULL }
};

static DsMenu menu = {
    "题目一：线性表",
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
