/**
 * @file    main.c
 * @brief   题目三：图（邻接矩阵 + 邻接表、遍历、最短路径、最小生成树）。
 *
 * 程序从 data/graph_*.txt 加载自建图，所有图算法都由 ds_graph 模块
 * 完成，本文件只负责选择图、读取起点和展示结果。
 *
 * 菜单功能：
 *   1. 加载示例图或指定图文件；
 *   2. 输出邻接矩阵；
 *   3. 输出邻接表；
 *   4. 深度优先遍历（DFS）；
 *   5. 广度优先遍历（BFS）；
 *   6. Dijkstra 单源最短路径；
 *   7. Prim 最小生成树；
 *   8. Kruskal 最小生成树；
 *   9. 判断连通性。
 *
 * 约定：
 *   - 顶点编号为 0 基，输入起点时直接输入顶点编号；
 *   - Prim / Kruskal 只适用于无向图，选择有向图时会给出明确提示；
 *   - 每次重新加载图都会先释放旧图，失败时保留原有图不变。
 */

#include "ds/ds_console.h"
#include "ds/ds_graph.h"
#include "ds/ds_input.h"
#include "ds/ds_memory.h"
#include "ds/ds_menu.h"
#include "ds/ds_path.h"
#include "ds/ds_types.h"
#include "ds/ds_util.h"

#include <stdio.h>
#include <string.h>

/** 应用上下文：一张图 + 是否已成功加载。 */
typedef struct Lab03App {
    Graph graph;
    bool loaded;
} Lab03App;

/** 示例图条目。 */
typedef struct GraphPreset {
    const char *name;
    const char *path;
} GraphPreset;

/** 三个自建示例图，分别侧重遍历、最小生成树和最短路径。 */
static const GraphPreset presets[] = {
    { "5 结点无向图（DFS/BFS 示例）", "data/graph_bfs.txt" },
    { "7 结点带权无向图（Prim/Kruskal 示例）", "data/graph_mst.txt" },
    { "7 结点有向带权图（Dijkstra 示例）", "data/graph_dijkstra.txt" }
};

/** @brief 若图尚未加载，输出提示并返回 false。 */
static bool ensure_loaded(const Lab03App *app)
{
    if (app == NULL || !app->loaded) {
        ds_console_error("当前还没有加载图，请先执行功能 1。");
        return false;
    }
    return true;
}

/** @brief 按编号列出全部顶点，供用户选择起点。 */
static void print_vertex_list(const Graph *graph)
{
    int i;

    printf("可选顶点：\n");
    for (i = 0; i < graph->vertex_count; ++i) {
        printf("  %2d. %s\n", i, graph_vertex_name(graph, i));
    }
}

/**
 * @brief 读取一个合法顶点编号。
 *
 * 先列出顶点，再调用 ds_input_int_range 限制在 [0, vertex_count-1]。
 */
static bool read_vertex(const Graph *graph, const char *prompt, int *out_vertex)
{
    int value;

    if (graph == NULL || out_vertex == NULL || graph->vertex_count <= 0) {
        return false;
    }

    print_vertex_list(graph);
    if (ds_input_int_range(prompt, 0, graph->vertex_count - 1, &value) != DS_OK) {
        return false;
    }

    *out_vertex = value;
    return true;
}

/**
 * @brief 从文件加载图，成功后才替换旧图。
 *
 * graph_load_file 要求目标对象未初始化，因此这里使用临时 Graph，
 * 成功后再销毁旧图并整体赋值。这样加载失败时用户原来的图仍然可用。
 */
static DsStatus load_graph(Lab03App *app, const char *path)
{
    Graph next;
    DsStatus status;

    if (app == NULL || path == NULL) {
        return DS_ERR_NULL;
    }

    memset(&next, 0, sizeof(next));
    status = graph_load_file(&next, path);
    if (status != DS_OK) {
        /* 失败路径可能已经分配过部分内存，destroy 可安全处理。 */
        graph_destroy(&next);
        return status;
    }

    if (app->loaded) {
        graph_destroy(&app->graph);
    }
    app->graph = next;
    app->loaded = true;
    return DS_OK;
}

/**
 * @brief 菜单 1：选择示例图或输入自定义图文件。
 */
static void action_load(void *context)
{
    Lab03App *app = (Lab03App *)context;
    char path[DS_PATH_MAX];
    char *text;
    DsStatus status;
    int choice;
    size_t i;

    if (app == NULL) {
        return;
    }

    ds_console_subtitle("选择要加载的图");
    for (i = 0; i < DS_ARRAY_LEN(presets); ++i) {
        printf("  %llu. %s\n", (unsigned long long)(i + 1), presets[i].name);
    }
    printf("  %llu. 手动输入图文件路径\n", (unsigned long long)(DS_ARRAY_LEN(presets) + 1));
    printf("  0. 返回\n");

    if (ds_input_int_range("请选择", 0, (int)DS_ARRAY_LEN(presets) + 1,
                           &choice) != DS_OK) {
        return;
    }
    if (choice == 0) {
        return;
    }

    if (choice >= 1 && choice <= (int)DS_ARRAY_LEN(presets)) {
        status = load_graph(app, presets[choice - 1].path);
    } else {
        if (ds_input_line("图文件路径（直接回车使用 data/graph_mst.txt）: ",
                          path, sizeof(path)) != DS_OK) {
            return;
        }
        text = ds_str_trim(path);
        status = load_graph(app, text[0] != '\0' ? text : "data/graph_mst.txt");
    }

    if (status != DS_OK) {
        ds_console_error("加载图失败：%s。", ds_status_text(status));
        return;
    }

    ds_console_success("加载完成：%d 个顶点，%d 条边，%s图。",
                       app->graph.vertex_count, app->graph.edge_count,
                       app->graph.directed ? "有向" : "无向");
    if (graph_is_connected(&app->graph)) {
        ds_console_info("当前图%s连通。",
                        app->graph.directed ? "弱" : "");
    } else {
        ds_console_info("当前图不连通。");
    }
}

/** @brief 菜单 2：输出邻接矩阵。 */
static void action_matrix(void *context)
{
    Lab03App *app = (Lab03App *)context;

    if (ensure_loaded(app)) {
        graph_print_matrix(&app->graph);
    }
}

/** @brief 菜单 3：输出邻接表。 */
static void action_adjlist(void *context)
{
    Lab03App *app = (Lab03App *)context;

    if (ensure_loaded(app)) {
        graph_print_adjlist(&app->graph);
    }
}

/** 遍历访问器上下文：图对象 + 已访问顶点数。 */
typedef struct VisitState {
    const Graph *graph;
    size_t count;
} VisitState;

/** @brief 打印一个顶点，并在顶点之间输出 " -> "。 */
static void print_vertex_visit(int vertex, void *context)
{
    VisitState *state = (VisitState *)context;

    if (state == NULL) {
        return;
    }
    if (state->count > 0) {
        printf(" -> ");
    }
    printf("%d(%s)", vertex, graph_vertex_name(state->graph, vertex));
    ++state->count;
}

/**
 * @brief DFS / BFS 的公共流程。
 *
 * 只从用户指定起点出发，因此不连通图或有向图可能访问不到全部顶点；
 * 访问结束后根据实际访问数给出提示。
 */
static void run_traversal(Lab03App *app, bool depth_first)
{
    VisitState state;
    DsStatus status;
    int start;

    if (!ensure_loaded(app)) {
        return;
    }
    if (!read_vertex(&app->graph, "请输入遍历起点", &start)) {
        return;
    }

    state.graph = &app->graph;
    state.count = 0;

    printf("%s 序列：", depth_first ? "DFS" : "BFS");
    if (depth_first) {
        status = graph_dfs(&app->graph, start, print_vertex_visit, &state);
    } else {
        status = graph_bfs(&app->graph, start, print_vertex_visit, &state);
    }
    printf("\n");

    if (status != DS_OK) {
        ds_console_error("遍历失败：%s。", ds_status_text(status));
        return;
    }

    ds_console_success("共访问 %llu 个顶点。", (unsigned long long)state.count);
    if (state.count < (size_t)app->graph.vertex_count) {
        ds_console_info("从该起点不能到达全部顶点（图不连通或为有向图）。");
    }
}

/** @brief 菜单 4：DFS。 */
static void action_dfs(void *context)
{
    run_traversal((Lab03App *)context, true);
}

/** @brief 菜单 5：BFS。 */
static void action_bfs(void *context)
{
    run_traversal((Lab03App *)context, false);
}

/**
 * @brief 菜单 6：Dijkstra 单源最短路径。
 *
 * 先输出所有目标的最短距离，再逐个调用 graph_print_path 输出完整
 * 顶点序列；不可达顶点单独标记，不打印虚假路径。
 */
static void action_dijkstra(void *context)
{
    Lab03App *app = (Lab03App *)context;
    double *dist;
    int *prev;
    int source;
    int i;
    DsStatus status;

    if (!ensure_loaded(app)) {
        return;
    }
    if (!read_vertex(&app->graph, "请输入源点", &source)) {
        return;
    }

    dist = (double *)ds_malloc((size_t)app->graph.vertex_count * sizeof(double));
    prev = (int *)ds_malloc((size_t)app->graph.vertex_count * sizeof(int));

    status = graph_dijkstra(&app->graph, source, dist, prev);
    if (status != DS_OK) {
        ds_console_error("Dijkstra 计算失败：%s。", ds_status_text(status));
        ds_free((void **)&dist);
        ds_free((void **)&prev);
        return;
    }

    ds_console_subtitle("最短路径结果");
    printf("源点：%d(%s)\n", source, graph_vertex_name(&app->graph, source));
    for (i = 0; i < app->graph.vertex_count; ++i) {
        printf("到 %d(%s)：", i, graph_vertex_name(&app->graph, i));
        if (dist[i] >= DS_GRAPH_INF / 2.0) {
            printf("不可达\n");
        } else {
            printf("最短距离 %.2f\n", dist[i]);
            graph_print_path(&app->graph, prev, source, i);
        }
    }

    ds_free((void **)&dist);
    ds_free((void **)&prev);
}

/** @brief 输出一条最小生成树边。 */
static void print_mst_edge(const Graph *graph, const GraphEdge *edge)
{
    printf("  %d(%s) -- %d(%s) : %.2f\n",
           edge->u, graph_vertex_name(graph, edge->u),
           edge->v, graph_vertex_name(graph, edge->v), edge->weight);
}

/** @brief 菜单 7：Prim 最小生成树。 */
static void action_prim(void *context)
{
    Lab03App *app = (Lab03App *)context;
    GraphEdge *mst;
    int mst_count = 0;
    double total = 0.0;
    int start;
    int i;
    DsStatus status;

    if (!ensure_loaded(app)) {
        return;
    }
    if (app->graph.directed) {
        ds_console_error("Prim 算法只适用于无向图，请先加载无向图。");
        return;
    }
    if (!read_vertex(&app->graph, "请输入 Prim 起点", &start)) {
        return;
    }

    mst = (GraphEdge *)ds_malloc((size_t)app->graph.vertex_count * sizeof(GraphEdge));
    status = graph_prim(&app->graph, start, mst, &mst_count, &total);
    if (status != DS_OK) {
        if (status == DS_ERR) {
            ds_console_error("图不连通，无法生成覆盖全部顶点的最小生成树。");
        } else {
            ds_console_error("Prim 计算失败：%s。", ds_status_text(status));
        }
        ds_free((void **)&mst);
        return;
    }

    ds_console_subtitle("Prim 最小生成树");
    for (i = 0; i < mst_count; ++i) {
        print_mst_edge(&app->graph, &mst[i]);
    }
    ds_console_success("共 %d 条边，总权值 %.2f。", mst_count, total);
    ds_free((void **)&mst);
}

/** @brief 菜单 8：Kruskal 最小生成树。 */
static void action_kruskal(void *context)
{
    Lab03App *app = (Lab03App *)context;
    GraphEdge *mst;
    int mst_count = 0;
    double total = 0.0;
    int i;
    DsStatus status;

    if (!ensure_loaded(app)) {
        return;
    }
    if (app->graph.directed) {
        ds_console_error("Kruskal 算法只适用于无向图，请先加载无向图。");
        return;
    }

    mst = (GraphEdge *)ds_malloc((size_t)app->graph.vertex_count * sizeof(GraphEdge));
    status = graph_kruskal(&app->graph, mst, &mst_count, &total);
    if (status != DS_OK) {
        if (status == DS_ERR) {
            ds_console_error("图不连通，无法生成覆盖全部顶点的最小生成树。");
        } else {
            ds_console_error("Kruskal 计算失败：%s。", ds_status_text(status));
        }
        ds_free((void **)&mst);
        return;
    }

    ds_console_subtitle("Kruskal 最小生成树");
    for (i = 0; i < mst_count; ++i) {
        print_mst_edge(&app->graph, &mst[i]);
    }
    ds_console_success("共 %d 条边，总权值 %.2f。", mst_count, total);
    ds_free((void **)&mst);
}

/** @brief 菜单 9：判断连通性。 */
static void action_connected(void *context)
{
    Lab03App *app = (Lab03App *)context;

    if (!ensure_loaded(app)) {
        return;
    }

    if (graph_is_connected(&app->graph)) {
        ds_console_success("图%s连通。", app->graph.directed ? "弱" : "");
    } else {
        ds_console_info("图不连通。");
    }
}

/** 菜单项表。 */
static DsMenuItem items[] = {
    { 1, "加载示例图 / 指定图文件", action_load },
    { 2, "输出邻接矩阵", action_matrix },
    { 3, "输出邻接表", action_adjlist },
    { 4, "深度优先遍历（DFS）", action_dfs },
    { 5, "广度优先遍历（BFS）", action_bfs },
    { 6, "Dijkstra 单源最短路径", action_dijkstra },
    { 7, "Prim 最小生成树", action_prim },
    { 8, "Kruskal 最小生成树", action_kruskal },
    { 9, "判断图的连通性", action_connected }
};

static DsMenu menu = {
    "题目三：图",
    items,
    DS_ARRAY_LEN(items)
};

/**
 * @brief 程序入口。
 *
 * 先切换工程根目录，保证 data/graph_*.txt 的相对路径在 build 或
 * build/bin 下也能使用；退出时释放图。
 */
int main(void)
{
    Lab03App app;
    DsStatus status;

    memset(&app, 0, sizeof(app));
    ds_console_init();

    status = ds_path_set_project_root();
    if (status != DS_OK) {
        ds_console_error("无法定位工程根目录：%s。请从工程目录内运行程序。",
                         ds_status_text(status));
        ds_console_restore();
        return 1;
    }

    ds_menu_run(&menu, &app);

    if (app.loaded) {
        graph_destroy(&app.graph);
    }
    ds_console_restore();
    return 0;
}
