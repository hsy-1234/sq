/**
 * @file    ds_graph.h
 * @brief   带权图：邻接矩阵 + 邻接表双存储。
 *
 * 同时维护邻接矩阵（用于 Dijkstra、Prim 等稠密算法）和邻接表
 * （用于 DFS、BFS 与稀疏图输出）。支持有向图和无向图。
 * 顶点用 0 基下标编号，可附带名字（用于武汉地铁等应用）。
 */
#ifndef DS_GRAPH_H
#define DS_GRAPH_H

#include <stddef.h>
#include <stdbool.h>
#include "ds/ds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DS_GRAPH_INF 1e30        /**< 表示不存在的边。 */
#define DS_GRAPH_NAME_SIZE 32    /**< 顶点名字最大字节数（含 '\0'）。 */

/** 邻接表边结点。 */
typedef struct GraphArc {
    int to;                  /**< 目标顶点下标。 */
    double weight;           /**< 边权。 */
    struct GraphArc *next;   /**< 下一条边。 */
} GraphArc;

/** 图对象。 */
typedef struct Graph {
    int capacity;            /**< 最大顶点数。 */
    int vertex_count;        /**< 当前顶点数。 */
    int edge_count;          /**< 当前边数（无向图一条边只计一次）。 */
    bool directed;           /**< 是否为有向图。 */
    double *matrix;          /**< capacity * capacity 的邻接矩阵。 */
    GraphArc **adj;          /**< 长度为 capacity 的邻接表头指针数组。 */
    char (*names)[DS_GRAPH_NAME_SIZE]; /**< 顶点名字数组。 */
} Graph;

/** 一条边，用于 Prim / Kruskal 输出。 */
typedef struct GraphEdge {
    int u;
    int v;
    double weight;
} GraphEdge;

/** 顶点访问函数。 */
typedef void (*GraphVisitFn)(int vertex, void *context);

/**
 * @brief 初始化图。
 * @param graph    图对象。
 * @param capacity 最大顶点数（必须大于 0）。
 * @param directed true 为有向图，false 为无向图。
 */
DsStatus graph_init(Graph *graph, int capacity, bool directed);

/** @brief 释放图内部所有存储，可重复调用。 */
void graph_destroy(Graph *graph);

/** @brief 清空顶点和边，保留容量。 */
void graph_clear(Graph *graph);

/** @brief 添加顶点，返回顶点下标；失败返回 -1。 */
int graph_add_vertex(Graph *graph, const char *name);

/** @brief 按名字查找顶点，未找到返回 -1。 */
int graph_find_vertex(const Graph *graph, const char *name);

/** @brief 返回顶点名字；下标非法返回空串。 */
const char *graph_vertex_name(const Graph *graph, int vertex);

/**
 * @brief 添加一条边；已存在则更新权值。
 * @param weight 必须 >= 0。无向图会自动添加反向边。
 */
DsStatus graph_add_edge(Graph *graph, int u, int v, double weight);

/** @brief 删除一条边；不存在返回 DS_ERR_NOT_FOUND。 */
DsStatus graph_remove_edge(Graph *graph, int u, int v);

/** @brief 查询边是否存在；weight 可为 NULL。 */
bool graph_has_edge(const Graph *graph, int u, int v, double *weight);

/** @brief 输出邻接矩阵。 */
void graph_print_matrix(const Graph *graph);

/** @brief 输出邻接表。 */
void graph_print_adjlist(const Graph *graph);

/** @brief 从 start 开始深度优先遍历。 */
DsStatus graph_dfs(const Graph *graph, int start, GraphVisitFn visit, void *context);

/** @brief 从 start 开始广度优先遍历。 */
DsStatus graph_bfs(const Graph *graph, int start, GraphVisitFn visit, void *context);

/**
 * @brief Dijkstra 单源最短路径（要求边权非负）。
 * @param dist 输出数组，长度至少 vertex_count，存放最短距离。
 * @param prev 输出数组，长度至少 vertex_count，存放前驱顶点；-1 表示无前驱。
 */
DsStatus graph_dijkstra(const Graph *graph, int source, double *dist, int *prev);

/** @brief 输出 source 到 target 的路径与长度。 */
void graph_print_path(const Graph *graph, const int *prev, int source, int target);

/**
 * @brief Prim 最小生成树（仅无向图）。
 * @param mst       输出边数组，长度至少 vertex_count - 1。
 * @param mst_count 返回边数。
 * @param total     返回总权值。
 * @return 图连通返回 DS_OK；不连通返回 DS_ERR。
 */
DsStatus graph_prim(const Graph *graph, int start, GraphEdge *mst,
                    int *mst_count, double *total);

/**
 * @brief Kruskal 最小生成树（仅无向图）。
 * @param mst       输出边数组，长度至少 vertex_count - 1。
 * @param mst_count 返回边数。
 * @param total     返回总权值。
 */
DsStatus graph_kruskal(const Graph *graph, GraphEdge *mst,
                       int *mst_count, double *total);

/** @brief 判断无向图是否连通；有向图按弱连通处理。 */
bool graph_is_connected(const Graph *graph);

/**
 * @brief 从文本文件加载图。
 *
 * 文件格式（# 开头为注释，空行忽略）：
 * @code
 * 7 9 0                 # 顶点数 边数 是否有向(0/1)
 * 0
 * 1
 * ...
 * 6
 * 0 1 28
 * 0 5 10
 * ...
 * @endcode
 * 顶点名字可以包含中文和空格，占一行。加载前 graph 必须未初始化。
 */
DsStatus graph_load_file(Graph *graph, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* DS_GRAPH_H */
