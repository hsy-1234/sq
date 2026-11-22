/**
 * @file    main.c
 * @brief   题目二：二叉树（菜单骨架，功能待实现）。
 *
 * 计划实现：括号表示法建树、链式存储、括号输出、
 * 先/中/后/层次遍历、求双亲、求高度、求任意两结点路径。
 */

#include "ds/ds_console.h"
#include "ds/ds_menu.h"
#include "ds/ds_types.h"

static DsMenuItem items[] = {
    { 1, "输入括号表示法并建树", NULL },
    { 2, "输出括号表示法", NULL },
    { 3, "先序遍历", NULL },
    { 4, "中序遍历", NULL },
    { 5, "后序遍历", NULL },
    { 6, "层次遍历", NULL },
    { 7, "求树高、结点数、叶子数", NULL },
    { 8, "求指定结点的双亲", NULL },
    { 9, "求任意两结点之间的路径", NULL },
    { 10, "交换所有结点的左右子树", NULL }
};

static DsMenu menu = {
    "题目二：二叉树",
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
