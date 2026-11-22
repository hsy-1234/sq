/**
 * @file    fun/metro/main.c
 * @brief   武汉轨道交通查询系统（趣味题）的菜单入口。
 *
 * 菜单分层：线路、站点、遍历、路径、MST、对比、统计各有子菜单；
 * 所有动作都走 ds_metro.h 中的接口，不直接操作图。数据文件由
 * metro_load() 解析为线路-站点双节点图和四张辅助图，路径查询按
 * “最快 / 最短 / 最少换乘”三种目标分别调用 Dijkstra。
 *
 * 趣味题与四道基础题隔离：本文件不加入 build/libds.a，只链接基础库。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ds/ds_console.h"
#include "ds/ds_input.h"
#include "ds/ds_memory.h"
#include "ds/ds_menu.h"
#include "ds/ds_path.h"
#include "ds/ds_util.h"
#include "ds/ds_metro.h"

#define METRO_DEFAULT_DATA "fun/metro/data/wuhan_metro.txt"
#define METRO_COMPARE_FILE "fun/metro/data/metro_compare.txt"

/** 菜单回调共享的上下文。 */
typedef struct MetroApp {
    MetroNetwork network;
    char data_path[DS_PATH_MAX];
} MetroApp;

/** 遍历回调上下文：统计实际访问的站点数。 */
typedef struct MetroVisitContext {
    size_t visited;
} MetroVisitContext;

/* ------------------------------------------------------------------------- */
/* 小工具                                                                     */
/* ------------------------------------------------------------------------- */

static void metro_report_status(DsStatus status, const char *what)
{
    if (status == DS_OK) {
        ds_console_success("%s完成。", what);
    } else {
        ds_console_error("%s失败：%s", what, ds_status_text(status));
    }
}

/** @brief 读入站名并精确查找；失败时输出提示并返回 -1。 */
static int read_station(const MetroApp *app, const char *prompt)
{
    char name[METRO_NAME_SIZE];
    int station;

    if (ds_input_line(prompt, name, sizeof(name)) != DS_OK) {
        return -1;
    }
    ds_str_trim(name);
    station = metro_find_station(&app->network, name);
    if (station < 0) {
        ds_console_error("没有找到站点“%s”。", name);
    }
    return station;
}

/** @brief DFS/BFS 的站点访问回调。 */
static void print_visited_station(const MetroNetwork *network,
                                  int station,
                                  void *context)
{
    MetroVisitContext *visit_context = (MetroVisitContext *)context;

    printf("[%3d] %s\n", station,
           (network != NULL && station >= 0 &&
            (size_t)station < network->station_count)
               ? network->stations[station].name
               : "(未知站点)");
    if (visit_context != NULL) {
        ++visit_context->visited;
    }
}

/** @brief 输出数据文件格式说明。 */
static void show_data_format(void)
{
    ds_console_subtitle("数据文件格式");
    printf("文件：%s\n\n", METRO_DEFAULT_DATA);
    printf("STATION  <站编号> <中文站名>\n");
    printf("LINE     <线路编号> <线路名>\n");
    printf("SEG      <线路编号> <区段号> <0普通|1环线>\n");
    printf("STOP     <线路编号> <站编号> <到下一站km> <到下一站min>\n");
    printf("TRANSFER <站编号> <换乘分钟>\n\n");
    printf("说明：\n");
    printf("  1. 同一站编号出现在多条线路中，自动视为换乘站；\n");
    printf("  2. SEG 后面的 STOP 属于该区段，普通区段末站的 km/min 写 0；\n");
    printf("  3. 环线最后一段的末站通过 SEG 的 loop=1 接回首站；\n");
    printf("  4. 支持 # 注释和空行，中文站名可含空格；\n");
    printf("  5. TRANSFER 可覆盖该站默认 4.0 分钟换乘时间。\n");
}

/* ------------------------------------------------------------------------- */
/* 加载与线路信息                                                             */
/* ------------------------------------------------------------------------- */

static void action_load(void *context)
{
    MetroApp *app = (MetroApp *)context;
    DsStatus status;

    printf("数据文件：%s\n", app->data_path);
    status = metro_load(&app->network, app->data_path);
    metro_report_status(status, "加载地铁数据");
    if (status == DS_OK) {
        metro_print_stats(&app->network);
    }
}

static void action_lines_all(void *context)
{
    MetroApp *app = (MetroApp *)context;

    if (!app->network.loaded) {
        ds_console_error("请先加载数据。");
        return;
    }
    metro_report_status(metro_print_lines(&app->network), "输出线路信息");
}

static void action_line_detail(void *context)
{
    MetroApp *app = (MetroApp *)context;
    int line;

    if (!app->network.loaded) {
        ds_console_error("请先加载数据。");
        return;
    }
    if (metro_print_lines(&app->network) != DS_OK) {
        return;
    }
    if (ds_input_int_range("线路下标（从 0 开始）：", 0,
                           (int)app->network.line_count - 1, &line) != DS_OK) {
        return;
    }
    metro_report_status(metro_print_line_detail(&app->network, line),
                        "输出线路详情");
}

static void action_lines(void *context)
{
    DsMenuItem items[] = {
        {1, "所有线路概览", action_lines_all},
        {2, "指定线路站序与站间距", action_line_detail},
    };
    DsMenu menu = {"线路信息", items, DS_ARRAY_LEN(items)};
    ds_menu_run(&menu, context);
}

/* ------------------------------------------------------------------------- */
/* 站点与换乘                                                                 */
/* ------------------------------------------------------------------------- */

static void action_station_query(void *context)
{
    MetroApp *app = (MetroApp *)context;
    int station;

    if (!app->network.loaded) {
        ds_console_error("请先加载数据。");
        return;
    }
    station = read_station(app, "站名：");
    if (station >= 0) {
        metro_report_status(metro_print_station(&app->network, station),
                            "输出站点信息");
    }
}

static void action_transfers(void *context)
{
    MetroApp *app = (MetroApp *)context;

    if (!app->network.loaded) {
        ds_console_error("请先加载数据。");
        return;
    }
    metro_report_status(metro_print_transfers(&app->network), "输出换乘站");
}

static void action_adjlist(void *context)
{
    MetroApp *app = (MetroApp *)context;

    if (!app->network.loaded) {
        ds_console_error("请先加载数据。");
        return;
    }
    metro_report_status(metro_print_adjlist(&app->network),
                        "输出线路-站点邻接表");
}

static void action_stations(void *context)
{
    DsMenuItem items[] = {
        {1, "查询站点与相邻站", action_station_query},
        {2, "所有换乘站", action_transfers},
        {3, "线路-站点邻接表", action_adjlist},
    };
    DsMenu menu = {"站点与换乘", items, DS_ARRAY_LEN(items)};
    ds_menu_run(&menu, context);
}

/* ------------------------------------------------------------------------- */
/* DFS / BFS                                                                  */
/* ------------------------------------------------------------------------- */

static void run_traversal(MetroApp *app, bool use_dfs)
{
    MetroVisitContext visit_context = {0};
    DsStatus status;
    int start;

    if (!app->network.loaded) {
        ds_console_error("请先加载数据。");
        return;
    }
    start = read_station(app, "遍历起点站名：");
    if (start < 0) {
        return;
    }

    ds_console_subtitle(use_dfs ? "深度优先遍历（DFS）" : "广度优先遍历（BFS）");
    if (use_dfs) {
        status = metro_dfs(&app->network, start,
                           print_visited_station, &visit_context);
    } else {
        status = metro_bfs(&app->network, start,
                           print_visited_station, &visit_context);
    }
    if (status != DS_OK) {
        ds_console_error("遍历失败：%s", ds_status_text(status));
        return;
    }

    printf("\n共访问 %zu 个站点。\n", visit_context.visited);
    if (visit_context.visited < app->network.station_count) {
        ds_console_info("访问数小于站点总数，图可能不连通。");
    }
}

static void action_dfs(void *context)
{
    run_traversal((MetroApp *)context, true);
}

static void action_bfs(void *context)
{
    run_traversal((MetroApp *)context, false);
}

static void action_traversal(void *context)
{
    DsMenuItem items[] = {
        {1, "DFS 深度优先遍历", action_dfs},
        {2, "BFS 广度优先遍历", action_bfs},
    };
    DsMenu menu = {"图的遍历", items, DS_ARRAY_LEN(items)};
    ds_menu_run(&menu, context);
}

/* ------------------------------------------------------------------------- */
/* 出行方案                                                                   */
/* ------------------------------------------------------------------------- */

static void run_plan(MetroApp *app, MetroCriterion criterion)
{
    MetroPath path;
    DsStatus status;
    int from;
    int to;

    if (!app->network.loaded) {
        ds_console_error("请先加载数据。");
        return;
    }
    from = read_station(app, "起点站名：");
    if (from < 0) {
        return;
    }
    to = read_station(app, "终点站名：");
    if (to < 0) {
        return;
    }

    metro_path_init(&path);
    status = metro_plan(&app->network, from, to, criterion, &path);
    if (status != DS_OK) {
        ds_console_error("路径规划失败：%s", ds_status_text(status));
    } else {
        metro_print_path(&app->network, &path);
    }
    metro_path_destroy(&path);
}

static void action_plan_fastest(void *context)
{
    run_plan((MetroApp *)context, METRO_FASTEST);
}

static void action_plan_shortest(void *context)
{
    run_plan((MetroApp *)context, METRO_SHORTEST);
}

static void action_plan_fewest(void *context)
{
    run_plan((MetroApp *)context, METRO_FEWEST_TRANSFERS);
}

static void action_plan(void *context)
{
    DsMenuItem items[] = {
        {1, "最快到达（时间最少）", action_plan_fastest},
        {2, "里程最短（用于票价）", action_plan_shortest},
        {3, "换乘最少", action_plan_fewest},
    };
    DsMenu menu = {"出行方案", items, DS_ARRAY_LEN(items)};
    ds_menu_run(&menu, context);
}

/* ------------------------------------------------------------------------- */
/* 最小生成树                                                                 */
/* ------------------------------------------------------------------------- */

static void action_mst(void *context)
{
    MetroApp *app = (MetroApp *)context;
    GraphEdge *edges;
    int edge_count = 0;
    double total = 0.0;
    bool use_prim;
    DsStatus status;
    int choice;
    int i;

    if (!app->network.loaded) {
        ds_console_error("请先加载数据。");
        return;
    }
    if (app->network.station_count < 2) {
        ds_console_info("站点少于 2 个，不需要生成树。");
        return;
    }
    if (ds_input_int_range("1 = Prim，2 = Kruskal：", 1, 2, &choice) != DS_OK) {
        return;
    }
    use_prim = (choice == 1);

    edges = (GraphEdge *)ds_malloc(app->network.station_count * sizeof(*edges));
    status = metro_mst(&app->network, use_prim, edges, &edge_count, &total);
    if (status != DS_OK) {
        ds_console_error("最小生成树失败：%s", ds_status_text(status));
        ds_free((void **)&edges);
        return;
    }

    ds_console_subtitle(use_prim ? "Prim 最小生成树" : "Kruskal 最小生成树");
    for (i = 0; i < edge_count; ++i) {
        printf("  %s -- %s  %.2f km\n",
               metro_station_name(&app->network, edges[i].u),
               metro_station_name(&app->network, edges[i].v),
               edges[i].weight);
    }
    printf("共 %d 条边，总权值 %.2f km。\n", edge_count, total);
    ds_free((void **)&edges);
}

/* ------------------------------------------------------------------------- */
/* 导航对比与统计                                                             */
/* ------------------------------------------------------------------------- */

static void action_compare(void *context)
{
    FILE *fp;
    char line[512];
    (void)context;

    fp = fopen(METRO_COMPARE_FILE, "r");
    if (fp == NULL) {
        ds_console_info("对比数据文件尚未准备：%s", METRO_COMPARE_FILE);
        ds_console_info("建议字段：起点,终点,地铁里程,地铁时间,地铁票价,APP里程,APP时间,APP票价,备注");
        return;
    }

    ds_console_subtitle("地铁方案与导航软件对比");
    while (fgets(line, sizeof(line), fp) != NULL) {
        fputs(line, stdout);
    }
    fclose(fp);
}

static void action_stats(void *context)
{
    MetroApp *app = (MetroApp *)context;

    if (!app->network.loaded) {
        ds_console_error("请先加载数据。");
        return;
    }
    metro_print_stats(&app->network);
}

static void action_validate(void *context)
{
    MetroApp *app = (MetroApp *)context;
    DsStatus status;

    if (!app->network.loaded) {
        ds_console_error("请先加载数据。");
        return;
    }
    status = metro_validate(&app->network);
    metro_report_status(status, "校验网络");
}

static void action_format(void *context)
{
    (void)context;
    show_data_format();
}

static void action_stats_menu(void *context)
{
    DsMenuItem items[] = {
        {1, "网络统计", action_stats},
        {2, "结构校验", action_validate},
        {3, "数据文件格式说明", action_format},
    };
    DsMenu menu = {"统计与校验", items, DS_ARRAY_LEN(items)};
    ds_menu_run(&menu, context);
}

/* ------------------------------------------------------------------------- */
/* 主菜单                                                                     */
/* ------------------------------------------------------------------------- */

int main(void)
{
    MetroApp app;
    DsStatus path_status;
    DsMenuItem items[] = {
        {1, "加载地铁数据", action_load},
        {2, "线路信息", action_lines},
        {3, "站点与换乘", action_stations},
        {4, "图的遍历（DFS/BFS）", action_traversal},
        {5, "出行方案（最快/最短/少换乘）", action_plan},
        {6, "最小生成树（Prim/Kruskal）", action_mst},
        {7, "与导航软件对比", action_compare},
        {8, "统计与校验", action_stats_menu},
    };
    DsMenu menu = {"武汉轨道交通查询系统（趣味题）", items, DS_ARRAY_LEN(items)};

    ds_console_init();
    path_status = ds_path_set_project_root();
    if (path_status != DS_OK) {
        ds_console_error("无法定位工程根目录：%s", ds_status_text(path_status));
        ds_console_restore();
        return 1;
    }

    memset(&app, 0, sizeof(app));
    ds_str_copy(app.data_path, sizeof(app.data_path), METRO_DEFAULT_DATA);
    metro_network_init(&app.network);

    ds_console_title("武汉轨道交通查询系统");
    ds_console_info("数据：13 条线路、298 座物理站点；支持票价、遍历、三种路径方案和 MST。");
    ds_console_info("首次使用请选择 1 加载数据。");
    ds_console_pause("按回车进入主菜单...");

    ds_menu_run(&menu, &app);

    metro_network_destroy(&app.network);
    ds_console_restore();
    return 0;
}
