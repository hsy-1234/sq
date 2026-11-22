/**
 * @file    ds_graph.c
 * @brief   图的双表示实现：邻接矩阵 + 邻接表。
 *
 * 为什么同时维护两种结构？
 *   - 邻接矩阵 matrix[u][v] 可以 O(1) 判断边是否存在，Dijkstra、Prim、
 *     Kruskal 的边集扫描都直接用它；
 *   - 邻接表 adj[u] 只保存实际存在的边，DFS/BFS 遍历时不用扫描大量
 *     “不存在”的矩阵单元，输出也更紧凑。
 *
 * 数据布局与不变量：
 *   - matrix 是按行优先展开的 capacity × capacity 数组，下标换算为
 *     u * capacity + v；无边用 DS_GRAPH_INF（1e30）表示；
 *   - 对角线 matrix[i][i] = 0，表示“自己到自己”的距离，但不是一条边；
 *   - adj[u] 是按终点编号 to 升序排列的单链表，每个弧保存 to 和 weight；
 *   - 无向图的每条边在矩阵中写两次（u,v 和 v,u），在邻接表中也挂两个
 *     方向的弧；edge_count 只统计“一条无向边”一次；
 *   - 有向图只写 u->v 一个方向，edge_count 统计弧的条数；
 *   - 图不支持自环，graph_add_edge 会拒绝 u == v；也不支持负权，
 *     因为 Dijkstra 要求边权非负。
 *
 * 复杂度概览：
 *   加/删边 O(deg(u))（邻接表定位），判边 O(1)，
 *   DFS/BFS O(V+E)，Dijkstra/Prim O(V^2)，Kruskal O(E log E)。
 */
#include "ds/ds_graph.h"
#include "ds/ds_memory.h"
#include "ds/ds_util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 邻接矩阵下标宏。
 *
 * @param graph 图指针。
 * @param row   行（起点）。
 * @param col   列（终点）。
 * @return 矩阵元素的左值。
 *
 * 先转 size_t 再乘，避免 int 乘法在容量较大时溢出；仍然要求
 * capacity 在合理范围内（课程设计通常不超过几百个顶点）。
 */
#define MATRIX(graph, row, col) \
    ((graph)->matrix[(size_t)(row) * (size_t)(graph)->capacity + (size_t)(col)])

/**
 * @brief 判断顶点编号是否合法。
 *
 * @param graph  图指针，可以为 NULL。
 * @param vertex 顶点编号。
 * @return 图非空且 0 <= vertex < vertex_count 时返回 true。
 */
static bool vertex_valid(const Graph *graph, int vertex)
{
    return graph != NULL && vertex >= 0 && vertex < graph->vertex_count;
}

/**
 * @brief 从邻接表中删除一条弧 from -> to。
 *
 * @param graph 图指针。
 * @param from  起点。
 * @param to    终点。
 *
 * 使用“二级指针”遍历：link 指向当前链表指针（可能是 adj[from]，
 * 也可能是某个结点的 next 字段）。命中时 *link = target->next 即可
 * 把 target 从链中摘除，不需要单独保存前驱。
 *
 * 若弧不存在则什么也不做。时间复杂度 O(deg(from))。
 */
static void adj_remove(Graph *graph, int from, int to)
{
    GraphArc **link = &graph->adj[from];

    while (*link != NULL) {
        if ((*link)->to == to) {
            GraphArc *target = *link;
            *link = target->next;
            ds_free((void **)&target);
            return;
        }
        link = &(*link)->next;
    }
}

/**
 * @brief 向邻接表插入一条弧，保持按终点升序。
 *
 * @param graph  图指针。
 * @param from   起点。
 * @param to     终点。
 * @param weight 边权。
 *
 * 先走到第一个 to 比新弧大的位置（或链表尾），把新弧插在它前面。
 * 调用前必须确认该弧尚不存在；graph_add_edge 已经做了重复判断。
 *
 * 时间复杂度 O(deg(from))。
 */
static void adj_add(Graph *graph, int from, int to, double weight)
{
    GraphArc *arc = (GraphArc *)ds_malloc(sizeof(GraphArc));
    GraphArc **link = &graph->adj[from];

    arc->to = to;
    arc->weight = weight;
    while (*link != NULL && (*link)->to < to) {
        link = &(*link)->next;
    }
    arc->next = *link;
    *link = arc;
}

/**
 * @brief 初始化一个容量为 capacity 的图。
 *
 * @param graph    图对象；调用前应未初始化或已 destroy。
 * @param capacity 最大顶点数，必须 > 0。
 * @param directed true 表示有向图，false 表示无向图。
 * @return DS_OK；graph 为 NULL 返回 DS_ERR_NULL；capacity <= 0 返回
 *         DS_ERR_RANGE。
 *
 * 分配三块内存：
 *   - matrix：capacity^2 个 double；
 *   - adj：capacity 个链表头指针；
 *   - names：capacity 个固定长度字符串。
 *
 * 矩阵先全部填 INF，再把对角线改成 0；adj 和 names 由 ds_calloc
 * 清零。初始化后 vertex_count = edge_count = 0。
 *
 * 时间复杂度 O(capacity^2)，空间 O(capacity^2)。
 */
DsStatus graph_init(Graph *graph, int capacity, bool directed)
{
    int i;

    if (graph == NULL) {
        return DS_ERR_NULL;
    }
    if (capacity <= 0) {
        return DS_ERR_RANGE;
    }

    graph->capacity = capacity;
    graph->vertex_count = 0;
    graph->edge_count = 0;
    graph->directed = directed;
    graph->matrix = (double *)ds_malloc((size_t)capacity * (size_t)capacity *
                                        sizeof(double));
    graph->adj = (GraphArc **)ds_calloc((size_t)capacity, sizeof(GraphArc *));
    graph->names = (char (*)[DS_GRAPH_NAME_SIZE])ds_calloc(
        (size_t)capacity, DS_GRAPH_NAME_SIZE);

    for (i = 0; i < capacity * capacity; ++i) {
        graph->matrix[i] = DS_GRAPH_INF;
    }
    for (i = 0; i < capacity; ++i) {
        graph->matrix[(size_t)i * (size_t)capacity + (size_t)i] = 0.0;
    }
    return DS_OK;
}

/**
 * @brief 销毁图，释放矩阵、邻接表、顶点名和所有弧结点。
 *
 * @param graph 图对象；为 NULL 时直接返回。
 *
 * 先逐条释放 adj 中的弧，再释放三块主内存。ds_free 会把指针置 NULL，
 * 因此重复 destroy 是安全的。
 *
 * 时间复杂度 O(V+E)。
 */
void graph_destroy(Graph *graph)
{
    int i;

    if (graph == NULL) {
        return;
    }

    if (graph->adj != NULL) {
        for (i = 0; i < graph->capacity; ++i) {
            GraphArc *arc = graph->adj[i];
            while (arc != NULL) {
                GraphArc *next = arc->next;
                ds_free((void **)&arc);
                arc = next;
            }
        }
    }
    ds_free((void **)&graph->matrix);
    ds_free((void **)&graph->adj);
    ds_free((void **)&graph->names);
    graph->capacity = 0;
    graph->vertex_count = 0;
    graph->edge_count = 0;
    graph->directed = false;
}

/**
 * @brief 清空图的内容，但保留 capacity 和 directed 配置。
 *
 * @param graph 已初始化的图。
 *
 * 释放所有弧结点，把矩阵恢复为 INF、对角线恢复为 0，顶点名清零，
 * 顶点数和边数归零。清空后可以重新 add_vertex/add_edge，不需要重新
 * 分配矩阵。
 *
 * 时间复杂度 O(V^2 + E)，主要开销是重置矩阵。
 */
void graph_clear(Graph *graph)
{
    int i;

    if (graph == NULL) {
        return;
    }

    for (i = 0; i < graph->capacity; ++i) {
        GraphArc *arc = graph->adj != NULL ? graph->adj[i] : NULL;
        while (arc != NULL) {
            GraphArc *next = arc->next;
            ds_free((void **)&arc);
            arc = next;
        }
        if (graph->adj != NULL) {
            graph->adj[i] = NULL;
        }
    }

    for (i = 0; i < graph->capacity * graph->capacity; ++i) {
        graph->matrix[i] = DS_GRAPH_INF;
    }
    for (i = 0; i < graph->capacity; ++i) {
        graph->matrix[(size_t)i * (size_t)graph->capacity + (size_t)i] = 0.0;
    }
    if (graph->names != NULL) {
        memset(graph->names, 0, (size_t)graph->capacity * DS_GRAPH_NAME_SIZE);
    }
    graph->vertex_count = 0;
    graph->edge_count = 0;
}

/**
 * @brief 追加一个顶点。
 *
 * @param graph 已初始化的图。
 * @param name  顶点名；长度超过 DS_GRAPH_NAME_SIZE-1 会被截断。
 * @return 新顶点编号（0 基）；图满或参数非法返回 -1。
 *
 * 顶点按加入顺序编号，名称保存在 names[index]。本函数不检查重名；
 * 如果业务需要唯一名称，应在调用前用 graph_find_vertex 判断。
 *
 * 时间复杂度 O(1)（不计字符串复制长度）。
 */
int graph_add_vertex(Graph *graph, const char *name)
{
    int index;

    if (graph == NULL || name == NULL || graph->vertex_count >= graph->capacity) {
        return -1;
    }

    index = graph->vertex_count++;
    ds_str_copy(graph->names[index], DS_GRAPH_NAME_SIZE, name);
    return index;
}

/**
 * @brief 按名称查找顶点编号。
 *
 * @param graph 图指针。
 * @param name  顶点名。
 * @return 顶点编号；未找到或参数非法返回 -1。
 *
 * 线性扫描 names 数组，时间复杂度 O(V)。课程设计的顶点数较小，足够；
 * 若顶点很多，可以另建哈希表把查找降到 O(1)。
 */
int graph_find_vertex(const Graph *graph, const char *name)
{
    int i;

    if (graph == NULL || name == NULL) {
        return -1;
    }
    for (i = 0; i < graph->vertex_count; ++i) {
        if (strcmp(graph->names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 取顶点名称。
 *
 * @param graph  图指针。
 * @param vertex 顶点编号。
 * @return 顶点名；编号非法时返回空串 ""（不是 NULL），方便直接 printf。
 */
const char *graph_vertex_name(const Graph *graph, int vertex)
{
    if (!vertex_valid(graph, vertex)) {
        return "";
    }
    return graph->names[vertex];
}

/**
 * @brief 添加一条边（或更新已存在边的权值）。
 *
 * @param graph  已初始化的图。
 * @param u      起点编号。
 * @param v      终点编号。
 * @param weight 边权，必须 >= 0 且有限。
 * @return DS_OK；参数/编号/自环/负权/非有限权返回错误码。
 *
 * 处理流程：
 *   1. 校验 u、v 合法且 u != v（不支持自环）；
 *   2. 拒绝负权、NaN、Inf（Dijkstra 和 MST 的前提）；
 *   3. 若矩阵中已有边，只更新权值，edge_count 不变；
 *      否则在矩阵和邻接表中都插入，edge_count 加一；
 *   4. 无向图同时维护 v->u 的矩阵元素和邻接弧。
 *
 * 重复调用同一对顶点是“更新权值”，不会产生重边。时间复杂度
 * O(deg(u) + deg(v))。
 */
DsStatus graph_add_edge(Graph *graph, int u, int v, double weight)
{
    bool exists;

    if (graph == NULL) {
        return DS_ERR_NULL;
    }
    if (!vertex_valid(graph, u) || !vertex_valid(graph, v) || u == v) {
        return DS_ERR_RANGE;
    }
    if (weight < 0.0 || !isfinite(weight)) {
        return DS_ERR_RANGE;
    }

    exists = MATRIX(graph, u, v) != DS_GRAPH_INF;
    MATRIX(graph, u, v) = weight;
    if (!graph->directed) {
        MATRIX(graph, v, u) = weight;
    }

    if (exists) {
        /* 已存在：只改邻接表中对应弧的 weight，不改变链表结构。 */
        GraphArc *arc;
        for (arc = graph->adj[u]; arc != NULL; arc = arc->next) {
            if (arc->to == v) {
                arc->weight = weight;
                break;
            }
        }
        if (!graph->directed) {
            for (arc = graph->adj[v]; arc != NULL; arc = arc->next) {
                if (arc->to == u) {
                    arc->weight = weight;
                    break;
                }
            }
        }
    } else {
        adj_add(graph, u, v, weight);
        if (!graph->directed) {
            adj_add(graph, v, u, weight);
        }
        ++graph->edge_count;
    }
    return DS_OK;
}

/**
 * @brief 删除一条边。
 *
 * @param graph 已初始化的图。
 * @param u     起点编号。
 * @param v     终点编号。
 * @return DS_OK；参数非法返回 DS_ERR_NULL/DS_ERR_RANGE；边不存在返回
 *         DS_ERR_NOT_FOUND。
 *
 * 自环不是合法边，调用方不应传入 u == v；本函数假设图状态一致
 * （矩阵和邻接表同步）。删除无向边时两个方向都要删，edge_count 只减一。
 *
 * 时间复杂度 O(deg(u) + deg(v))。
 */
DsStatus graph_remove_edge(Graph *graph, int u, int v)
{
    if (graph == NULL) {
        return DS_ERR_NULL;
    }
    if (!vertex_valid(graph, u) || !vertex_valid(graph, v)) {
        return DS_ERR_RANGE;
    }
    if (MATRIX(graph, u, v) == DS_GRAPH_INF) {
        return DS_ERR_NOT_FOUND;
    }

    MATRIX(graph, u, v) = DS_GRAPH_INF;
    adj_remove(graph, u, v);
    if (!graph->directed) {
        MATRIX(graph, v, u) = DS_GRAPH_INF;
        adj_remove(graph, v, u);
    }
    --graph->edge_count;
    return DS_OK;
}

/**
 * @brief 判断边是否存在，并可取出权值。
 *
 * @param graph  图指针。
 * @param u      起点。
 * @param v      终点。
 * @param weight 可选输出；非 NULL 且边存在时写入权值。
 * @return 存在返回 true；不存在或顶点非法返回 false。
 *
 * 直接查矩阵，O(1)。注意对角线 matrix[u][u] = 0 表示“自身到自身”，
 * 不是一条实际边；业务层查询边时应避免 u == v。
 */
bool graph_has_edge(const Graph *graph, int u, int v, double *weight)
{
    if (!vertex_valid(graph, u) || !vertex_valid(graph, v)) {
        return false;
    }
    if (MATRIX(graph, u, v) == DS_GRAPH_INF) {
        return false;
    }
    if (weight != NULL) {
        *weight = MATRIX(graph, u, v);
    }
    return true;
}

/**
 * @brief 以表格形式输出邻接矩阵。
 *
 * @param graph 图指针。
 *
 * 先列出“编号 = 名称”，再输出列号和每行数据；无边显示 INF，其余
 * 保留两位小数。主要用于实验报告中的结构展示和调试。
 *
 * 时间复杂度 O(V^2)，输出量也是 O(V^2)。
 */
void graph_print_matrix(const Graph *graph)
{
    int i;
    int j;

    if (graph == NULL) {
        return;
    }

    printf("邻接矩阵（INF 表示无边）：\n");
    for (i = 0; i < graph->vertex_count; ++i) {
        printf("  %d = %s\n", i, graph->names[i]);
    }
    printf("      ");
    for (j = 0; j < graph->vertex_count; ++j) {
        printf("%8d", j);
    }
    putchar('\n');

    for (i = 0; i < graph->vertex_count; ++i) {
        printf("%4d  ", i);
        for (j = 0; j < graph->vertex_count; ++j) {
            double value = MATRIX(graph, i, j);
            if (value == DS_GRAPH_INF) {
                printf("%8s", "INF");
            } else {
                printf("%8.2f", value);
            }
        }
        putchar('\n');
    }
}

/**
 * @brief 输出邻接表。
 *
 * @param graph 图指针。
 *
 * 每个顶点一行：`编号(名称): -> 终点(权值) -> ...`。由于 adj 按终点
 * 升序维护，输出顺序稳定，便于对比实验。
 *
 * 时间复杂度 O(V+E)。
 */
void graph_print_adjlist(const Graph *graph)
{
    int i;

    if (graph == NULL) {
        return;
    }

    printf("邻接表：\n");
    for (i = 0; i < graph->vertex_count; ++i) {
        const GraphArc *arc;
        printf("  %d(%s):", i, graph->names[i]);
        for (arc = graph->adj[i]; arc != NULL; arc = arc->next) {
            printf(" -> %d(%.2f)", arc->to, arc->weight);
        }
        putchar('\n');
    }
}

/**
 * @brief DFS 的递归访问函数。
 *
 * @param graph   图指针。
 * @param vertex  当前顶点。
 * @param visited 访问标记数组。
 * @param visit   访问回调，可以为 NULL。
 * @param context 透传给回调的上下文。
 *
 * 进入顶点时先标记 visited 再回调，因此即使图中有环也不会无限递归。
 * 然后按邻接表顺序递归所有未访问的邻居。
 */
static void dfs_visit(const Graph *graph, int vertex, bool *visited,
                      GraphVisitFn visit, void *context)
{
    const GraphArc *arc;

    visited[vertex] = true;
    if (visit != NULL) {
        visit(vertex, context);
    }
    for (arc = graph->adj[vertex]; arc != NULL; arc = arc->next) {
        if (!visited[arc->to]) {
            dfs_visit(graph, arc->to, visited, visit, context);
        }
    }
}

/**
 * @brief 从 start 出发做深度优先遍历。
 *
 * @param graph   图指针。
 * @param start   起点编号。
 * @param visit   访问回调，可以为 NULL。
 * @param context 透传给回调的上下文。
 * @return DS_OK；graph 为 NULL 返回 DS_ERR_NULL；start 非法返回
 *         DS_ERR_RANGE。
 *
 * 只遍历 start 所在的连通分量（有向图则是 start 能到达的部分）；
 * 若要遍历整个非连通图，需要对每个未访问顶点再次调用。
 *
 * 时间复杂度 O(V+E)，空间 O(V)（visited + 递归栈，最坏 O(V)）。
 */
DsStatus graph_dfs(const Graph *graph, int start, GraphVisitFn visit,
                   void *context)
{
    bool *visited;

    if (graph == NULL) {
        return DS_ERR_NULL;
    }
    if (!vertex_valid(graph, start)) {
        return DS_ERR_RANGE;
    }

    visited = (bool *)ds_calloc((size_t)graph->vertex_count, sizeof(bool));
    dfs_visit(graph, start, visited, visit, context);
    ds_free((void **)&visited);
    return DS_OK;
}

/**
 * @brief 从 start 出发做广度优先遍历。
 *
 * @param graph   图指针。
 * @param start   起点编号。
 * @param visit   访问回调，可以为 NULL。
 * @param context 透传给回调的上下文。
 * @return DS_OK；graph 为 NULL 返回 DS_ERR_NULL；start 非法返回
 *         DS_ERR_RANGE。
 *
 * 用数组模拟队列：
 *   - 起点入队并立即标记 visited；
 *   - 每次出队一个顶点，访问后把它所有未访问的邻居标记并入队；
 *   - 每个顶点最多入队一次，因此队列容量取 V 就足够。
 *
 * 这样得到的访问顺序就是按“边数”分层的 BFS 序。时间复杂度 O(V+E)，
 * 空间 O(V)。
 */
DsStatus graph_bfs(const Graph *graph, int start, GraphVisitFn visit,
                   void *context)
{
    bool *visited;
    int *queue;
    int head = 0;
    int tail = 0;

    if (graph == NULL) {
        return DS_ERR_NULL;
    }
    if (!vertex_valid(graph, start)) {
        return DS_ERR_RANGE;
    }

    visited = (bool *)ds_calloc((size_t)graph->vertex_count, sizeof(bool));
    queue = (int *)ds_malloc((size_t)graph->vertex_count * sizeof(int));

    visited[start] = true;
    queue[tail++] = start;
    while (head < tail) {
        int vertex = queue[head++];
        const GraphArc *arc;

        if (visit != NULL) {
            visit(vertex, context);
        }
        for (arc = graph->adj[vertex]; arc != NULL; arc = arc->next) {
            if (!visited[arc->to]) {
                visited[arc->to] = true;
                queue[tail++] = arc->to;
            }
        }
    }

    ds_free((void **)&visited);
    ds_free((void **)&queue);
    return DS_OK;
}

/**
 * @brief Dijkstra 单源最短路径（邻接矩阵 + 线性选点）。
 *
 * @param graph  图指针，边权必须非负。
 * @param source 源点编号。
 * @param dist   输出数组，长度至少 V；存放 source 到各点的最短距离。
 * @param prev   输出数组，长度至少 V；prev[i] 是路径上 i 的前驱，
 *               源点为 -1，不可达点也为 -1。
 * @return DS_OK；参数非法返回 DS_ERR_NULL/DS_ERR_RANGE。
 *
 * 算法步骤：
 *   1. dist 全部初始化为 INF，prev 全部为 -1，dist[source] = 0；
 *   2. 重复 V 次：
 *        a. 在未访问顶点中选 dist 最小的 best；
 *        b. 若不存在（剩下都不可达）则提前结束；
 *        c. 标记 best 已确定，松弛它所有未访问的邻居：
 *             if (dist[best] + w < dist[i]) 更新 dist[i] 和 prev[i]；
 *   3. 结束后 dist 即最短距离，沿 prev 反向回溯得到路径。
 *
 * 因为每次线性扫描选点，时间复杂度 O(V^2)，适合稠密图；若用二叉堆
 * 可降到 O((V+E) log V)，但课程设计规模下没有必要。
 */
DsStatus graph_dijkstra(const Graph *graph, int source, double *dist, int *prev)
{
    bool *visited;
    int i;
    int iteration;
    int count;

    if (graph == NULL || dist == NULL || prev == NULL) {
        return DS_ERR_NULL;
    }
    if (!vertex_valid(graph, source)) {
        return DS_ERR_RANGE;
    }

    count = graph->vertex_count;
    visited = (bool *)ds_calloc((size_t)count, sizeof(bool));
    for (i = 0; i < count; ++i) {
        dist[i] = DS_GRAPH_INF;
        prev[i] = -1;
    }
    dist[source] = 0.0;

    for (iteration = 0; iteration < count; ++iteration) {
        int best = -1;
        double best_distance = DS_GRAPH_INF;

        /* 线性扫描：在所有未确定顶点中找 dist 最小者。 */
        for (i = 0; i < count; ++i) {
            if (!visited[i] && dist[i] < best_distance) {
                best_distance = dist[i];
                best = i;
            }
        }
        if (best < 0) {
            /* 剩余顶点都不可达，继续循环没有意义。 */
            break;
        }
        visited[best] = true;

        /* 松弛：只更新尚未确定的邻居，已确定的顶点距离不会再变小。 */
        for (i = 0; i < count; ++i) {
            double weight = MATRIX(graph, best, i);
            if (!visited[i] && weight != DS_GRAPH_INF &&
                dist[best] + weight < dist[i]) {
                dist[i] = dist[best] + weight;
                prev[i] = best;
            }
        }
    }

    ds_free((void **)&visited);
    return DS_OK;
}

/**
 * @brief 根据 prev 数组输出 source 到 target 的路径和总长度。
 *
 * @param graph  图指针。
 * @param prev   graph_dijkstra 生成的 prev 数组。
 * @param source 源点。
 * @param target 终点。
 *
 * prev 记录的是“每个顶点在最短路径上的前驱”，所以从 target 开始
 * 不断 prev[node] 就能回到 source，得到的是反向路径；输出时从数组
 * 末尾向前打印即可。
 *
 * 总长度在打印过程中累加 MATRIX(path[i], path[i-1])，正好对应正向
 * 路径上的每条边。不可达时输出提示。
 *
 * 时间复杂度 O(V)，空间 O(V)。
 */
void graph_print_path(const Graph *graph, const int *prev, int source,
                      int target)
{
    int *path;
    int length = 0;
    int node;
    int i;
    double total = 0.0;

    if (graph == NULL || prev == NULL || !vertex_valid(graph, source) ||
        !vertex_valid(graph, target)) {
        printf("路径参数不合法。\n");
        return;
    }

    path = (int *)ds_malloc((size_t)graph->vertex_count * sizeof(int));
    node = target;
    while (node != -1) {
        path[length++] = node;
        if (node == source) {
            break;
        }
        node = prev[node];
    }

    /* 回溯必须最终到达 source；否则说明 target 不可达。 */
    if (length == 0 || path[length - 1] != source) {
        printf("从 %d(%s) 到 %d(%s) 不可达。\n", source,
               graph->names[source], target, graph->names[target]);
        ds_free((void **)&path);
        return;
    }

    printf("路径：");
    for (i = length - 1; i >= 0; --i) {
        printf("%d(%s)", path[i], graph->names[path[i]]);
        if (i > 0) {
            printf(" -> ");
            /*
             * path 是反向的：path[i] 在正向路径上位于 path[i-1] 之前，
             * 所以边是 path[i] -> path[i-1]。
             */
            total += MATRIX(graph, path[i], path[i - 1]);
        }
    }
    printf("\n长度：%.2f\n", total);
    ds_free((void **)&path);
}

/**
 * @brief Kruskal 的边比较函数：按权值升序，权值相同按 u、v 升序。
 *
 * @param a 指向 GraphEdge 的指针。
 * @param b 指向 GraphEdge 的指针。
 * @return 负数/0/正数，含义同 qsort。
 *
 * 加入 u、v 的次级比较只是为了让输出顺序稳定，不影响最小生成树
 * 的总权值。
 */
static int compare_edges(const void *a, const void *b)
{
    const GraphEdge *left = (const GraphEdge *)a;
    const GraphEdge *right = (const GraphEdge *)b;

    if (left->weight < right->weight) {
        return -1;
    }
    if (left->weight > right->weight) {
        return 1;
    }
    if (left->u != right->u) {
        return left->u - right->u;
    }
    return left->v - right->v;
}

/**
 * @brief Prim 最小生成树（邻接矩阵，O(V^2)）。
 *
 * @param graph     无向连通图。
 * @param start     起点编号。
 * @param mst       输出边数组，容量至少 V-1；V<=1 时可为 NULL。
 * @param mst_count 输出生成树边数。
 * @param total     输出总权值。
 * @return DS_OK；有向图返回 DS_ERR_STATE；参数非法返回 DS_ERR_NULL/
 *         DS_ERR_RANGE；图不连通返回 DS_ERR。
 *
 * 维护三个数组：
 *   visited[v]  是否已加入生成树；
 *   lowcost[v]  v 到当前生成树的最小边权；
 *   parent[v]   lowcost[v] 对应的树内端点。
 *
 * 每轮在未访问顶点中选 lowcost 最小者 best，把 (parent[best], best)
 * 加入 MST，然后用 best 的邻接边更新其他未访问顶点的 lowcost。
 *
 * 时间复杂度 O(V^2)，空间 O(V)。图不连通时无法选出 V-1 条边，
 * 返回 DS_ERR，此时 mst_count/total 不保证有效。
 */
DsStatus graph_prim(const Graph *graph, int start, GraphEdge *mst,
                    int *mst_count, double *total)
{
    bool *visited;
    double *lowcost;
    int *parent;
    int count;
    int i;
    int added = 0;
    double sum = 0.0;

    if (graph == NULL || mst_count == NULL || total == NULL) {
        return DS_ERR_NULL;
    }
    if (graph->directed) {
        return DS_ERR_STATE;
    }
    if (!vertex_valid(graph, start)) {
        return DS_ERR_RANGE;
    }

    count = graph->vertex_count;
    if (count > 1 && mst == NULL) {
        return DS_ERR_NULL;
    }
    if (count <= 1) {
        *mst_count = 0;
        *total = 0.0;
        return DS_OK;
    }

    visited = (bool *)ds_calloc((size_t)count, sizeof(bool));
    lowcost = (double *)ds_malloc((size_t)count * sizeof(double));
    parent = (int *)ds_malloc((size_t)count * sizeof(int));

    /* 初始时生成树只有 start，lowcost[v] 就是 start 到 v 的边权。 */
    for (i = 0; i < count; ++i) {
        lowcost[i] = MATRIX(graph, start, i);
        parent[i] = start;
    }
    lowcost[start] = 0.0;
    visited[start] = true;

    for (i = 1; i < count; ++i) {
        int best = -1;
        double best_cost = DS_GRAPH_INF;
        int v;

        /* 选一条连接树内外的最小边。 */
        for (v = 0; v < count; ++v) {
            if (!visited[v] && lowcost[v] < best_cost) {
                best_cost = lowcost[v];
                best = v;
            }
        }
        if (best < 0) {
            /* 还有顶点没加入，但已经没有边相连：图不连通。 */
            ds_free((void **)&visited);
            ds_free((void **)&lowcost);
            ds_free((void **)&parent);
            return DS_ERR;
        }

        visited[best] = true;
        mst[added].u = parent[best];
        mst[added].v = best;
        mst[added].weight = best_cost;
        ++added;
        sum += best_cost;

        /* 新顶点加入后，用它更新其他未访问顶点的最小入边。 */
        for (v = 0; v < count; ++v) {
            double weight = MATRIX(graph, best, v);
            if (!visited[v] && weight < lowcost[v]) {
                lowcost[v] = weight;
                parent[v] = best;
            }
        }
    }

    ds_free((void **)&visited);
    ds_free((void **)&lowcost);
    ds_free((void **)&parent);
    *mst_count = added;
    *total = sum;
    return DS_OK;
}

/**
 * @brief 并查集的查找（路径减半）。
 *
 * @param parent 并查集父数组。
 * @param vertex 待查顶点。
 * @return 所在集合的根。
 *
 * 循环中把当前结点指向祖父结点，压缩路径，使后续查找更快。
 * 时间复杂度近似 O(α(V))。
 */
static int union_find_root(int *parent, int vertex)
{
    while (parent[vertex] != vertex) {
        parent[vertex] = parent[parent[vertex]];
        vertex = parent[vertex];
    }
    return vertex;
}

/**
 * @brief Kruskal 最小生成树（边集排序 + 并查集）。
 *
 * @param graph     无向图。
 * @param mst       输出边数组，容量至少 V-1；V<=1 时可为 NULL。
 * @param mst_count 输出生成树边数。
 * @param total     输出总权值。
 * @return DS_OK；有向图返回 DS_ERR_STATE；参数非法返回 DS_ERR_NULL；
 *         图不连通返回 DS_ERR。
 *
 * 步骤：
 *   1. 扫描矩阵上三角，收集所有实际存在的边（无向边只收一次）；
 *   2. 按权值升序排序；
 *   3. 初始化并查集，每个顶点自成一棵树；
 *   4. 依次取最小边，若两个端点不在同一集合就合并并加入 MST；
 *      已加入 V-1 条边即可提前结束；
 *   5. 若结束后不足 V-1 条，说明图不连通。
 *
 * 时间复杂度 O(E log E)，主要来自排序；空间 O(E+V)。
 *
 * 注意：边数组按 graph->edge_count 预分配，依赖“矩阵与 edge_count
 * 一致”的不变量。正常通过 graph_add_edge 构建的图满足该条件。
 */
DsStatus graph_kruskal(const Graph *graph, GraphEdge *mst, int *mst_count,
                       double *total)
{
    GraphEdge *edges;
    int *parent;
    int count;
    int edge_total;
    int i;
    int added = 0;
    double sum = 0.0;

    if (graph == NULL || mst_count == NULL || total == NULL) {
        return DS_ERR_NULL;
    }
    if (graph->directed) {
        return DS_ERR_STATE;
    }

    count = graph->vertex_count;
    if (count > 1 && mst == NULL) {
        return DS_ERR_NULL;
    }
    if (count <= 1) {
        *mst_count = 0;
        *total = 0.0;
        return DS_OK;
    }

    edge_total = graph->edge_count;
    edges = (GraphEdge *)ds_malloc((size_t)(edge_total > 0 ? edge_total : 1) *
                                   sizeof(GraphEdge));
    {
        int u;
        int v;
        int index = 0;
        for (u = 0; u < count; ++u) {
            for (v = u + 1; v < count; ++v) {
                double weight = MATRIX(graph, u, v);
                if (weight != DS_GRAPH_INF) {
                    edges[index].u = u;
                    edges[index].v = v;
                    edges[index].weight = weight;
                    ++index;
                }
            }
        }
        edge_total = index;
    }

    qsort(edges, (size_t)edge_total, sizeof(GraphEdge), compare_edges);
    parent = (int *)ds_malloc((size_t)count * sizeof(int));
    for (i = 0; i < count; ++i) {
        parent[i] = i;
    }

    for (i = 0; i < edge_total && added < count - 1; ++i) {
        int root_u = union_find_root(parent, edges[i].u);
        int root_v = union_find_root(parent, edges[i].v);

        if (root_u != root_v) {
            parent[root_u] = root_v;
            mst[added++] = edges[i];
            sum += edges[i].weight;
        }
    }

    ds_free((void **)&edges);
    ds_free((void **)&parent);

    if (added != count - 1) {
        return DS_ERR;
    }
    *mst_count = added;
    *total = sum;
    return DS_OK;
}

/**
 * @brief 判断图是否连通。
 *
 * @param graph 图指针。
 * @return 连通返回 true；空图返回 false；单顶点图返回 true。
 *
 * 对无向图就是普通连通性；对有向图，这里按“弱连通”处理：只要把
 * 有向边看成无向边后整个图连通，就返回 true。因此 BFS 扩展邻居时
 * 同时检查 matrix[vertex][neighbor] 和 matrix[neighbor][vertex]。
 *
 * 直接用矩阵扫描邻居，时间复杂度 O(V^2)，空间 O(V)。
 */
bool graph_is_connected(const Graph *graph)
{
    bool *visited;
    int *queue;
    int head = 0;
    int tail = 0;
    int count;
    int reached = 0;

    if (graph == NULL || graph->vertex_count == 0) {
        return false;
    }
    if (graph->vertex_count == 1) {
        return true;
    }

    count = graph->vertex_count;
    visited = (bool *)ds_calloc((size_t)count, sizeof(bool));
    queue = (int *)ds_malloc((size_t)count * sizeof(int));

    visited[0] = true;
    queue[tail++] = 0;
    while (head < tail) {
        int vertex = queue[head++];
        int neighbor;

        ++reached;
        for (neighbor = 0; neighbor < count; ++neighbor) {
            bool linked = MATRIX(graph, vertex, neighbor) != DS_GRAPH_INF ||
                          MATRIX(graph, neighbor, vertex) != DS_GRAPH_INF;
            if (!visited[neighbor] && linked) {
                visited[neighbor] = true;
                queue[tail++] = neighbor;
            }
        }
    }

    ds_free((void **)&visited);
    ds_free((void **)&queue);
    return reached == count;
}

/**
 * @brief 从文件中读一行有效数据（跳过空行和 # 注释）。
 *
 * @param file   已打开的文件。
 * @param buffer 输出缓冲区。
 * @param size   缓冲区字节数。
 * @return 读到有效行返回 true；文件结束返回 false。
 *
 * 行首/行尾空白由 ds_str_trim 去掉；空行和以 '#' 开头的行会被跳过。
 * 若一行超过缓冲区，fgets 会分多次返回，调用方按普通行处理；因此
 * 顶点名和边数据都应保持在一行 255 字节以内。
 */
static bool read_data_line(FILE *file, char *buffer, size_t size)
{
    while (fgets(buffer, (int)size, file) != NULL) {
        ds_str_trim(buffer);
        if (buffer[0] == '\0' || buffer[0] == '#') {
            continue;
        }
        return true;
    }
    return false;
}

/**
 * @brief 从文本文件加载图。
 *
 * @param graph 图对象；调用前应未初始化或已 destroy。
 * @param path  文件路径。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；打开失败返回 DS_ERR_IO；
 *         格式/顶点/边数据错误返回 DS_ERR_FORMAT。
 *
 * 文件格式：
 *
 *     n m directed
 *     name_0
 *     name_1
 *     ...
 *     name_{n-1}
 *     u v w
 *     ...
 *
 * 其中 directed 非 0 表示有向图；顶点名可以包含空格（整行读取）；
 * 空行和 # 注释行会被跳过。
 *
 * 读取过程中任何一步失败都会关闭文件并 graph_destroy，避免留下半成品。
 * 成功返回时 graph 已按文件内容初始化，调用方负责最终 graph_destroy。
 *
 * 时间复杂度 O(n^2 + m * deg)，主要开销是矩阵初始化和建边。
 */
DsStatus graph_load_file(Graph *graph, const char *path)
{
    FILE *file;
    char line[256];
    int vertex_count;
    int edge_count;
    int directed;
    int i;
    DsStatus status;

    if (graph == NULL || path == NULL) {
        return DS_ERR_NULL;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return DS_ERR_IO;
    }
    if (!read_data_line(file, line, sizeof(line)) ||
        sscanf(line, "%d %d %d", &vertex_count, &edge_count, &directed) != 3 ||
        vertex_count <= 0 || edge_count < 0) {
        fclose(file);
        return DS_ERR_FORMAT;
    }

    status = graph_init(graph, vertex_count, directed != 0);
    if (status != DS_OK) {
        fclose(file);
        return status;
    }

    /* 先读 n 个顶点名，按文件顺序编号 0..n-1。 */
    for (i = 0; i < vertex_count; ++i) {
        if (!read_data_line(file, line, sizeof(line))) {
            fclose(file);
            graph_destroy(graph);
            return DS_ERR_FORMAT;
        }
        if (graph_add_vertex(graph, line) < 0) {
            fclose(file);
            graph_destroy(graph);
            return DS_ERR_FORMAT;
        }
    }

    /* 再读 m 条边；graph_add_edge 会校验编号、自环和权值。 */
    for (i = 0; i < edge_count; ++i) {
        int u;
        int v;
        double weight;
        if (!read_data_line(file, line, sizeof(line)) ||
            sscanf(line, "%d %d %lf", &u, &v, &weight) != 3) {
            fclose(file);
            graph_destroy(graph);
            return DS_ERR_FORMAT;
        }
        if (graph_add_edge(graph, u, v, weight) != DS_OK) {
            fclose(file);
            graph_destroy(graph);
            return DS_ERR_FORMAT;
        }
    }

    fclose(file);
    return DS_OK;
}
