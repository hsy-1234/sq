/**
 * @file    main.c
 * @brief   题目三：图（菜单骨架，功能待实现）。
 *
 * 计划实现：邻接矩阵 + 邻接表、DFS/BFS、Dijkstra、
 * Prim 与 Kruskal 最小生成树。
 */

#include "ds/ds_console.h"
#include "ds/ds_menu.h"
#include "ds/ds_types.h"

static DsMenuItem items[] = {
    { 1, "从文件加载图", NULL },
    { 2, "输出邻接矩阵", NULL },
    { 3, "输出邻接表", NULL },
    { 4, "深度优先遍历（DFS）", NULL },
    { 5, "广度优先遍历（BFS）", NULL },
    { 6, "Dijkstra 单源最短路径", NULL },
    { 7, "Prim 最小生成树", NULL },
    { 8, "Kruskal 最小生成树", NULL },
    { 9, "判断图的连通性", NULL }
};

static DsMenu menu = {
    "题目三：图",
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
