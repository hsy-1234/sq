/**
 * @file    fun/metro/tests/test_metro.c
 * @brief   武汉地铁模块的回归测试。
 *
 * 覆盖范围：
 *   - 票价分段边界；
 *   - 空网络的生命周期和错误码；
 *   - 全量数据加载后的线路/站点/换乘统计；
 *   - 最快、最短、最少换乘三种路径方案；
 *   - 同站进出；
 *   - DFS/BFS 连通性；
 *   - Prim/Kruskal 生成树边数与总权值。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ds/ds_path.h"
#include "ds/ds_metro.h"

static int failures = 0;

static void check_true(int condition, const char *message)
{
    if (!condition) {
        printf("  [失败] %s\n", message);
        ++failures;
    }
}

static void check_int(int actual, int expected, const char *message)
{
    if (actual != expected) {
        printf("  [失败] %s：期望 %d，实际 %d\n", message, expected, actual);
        ++failures;
    }
}

static void check_double(double actual, double expected, double tolerance,
                         const char *message)
{
    if (fabs(actual - expected) > tolerance) {
        printf("  [失败] %s：期望 %.4f，实际 %.4f\n",
               message, expected, actual);
        ++failures;
    }
}

static void check_status(DsStatus actual, DsStatus expected, const char *message)
{
    if (actual != expected) {
        printf("  [失败] %s：期望 %s，实际 %s\n",
               message, ds_status_text(expected), ds_status_text(actual));
        ++failures;
    }
}

static void test_fare(void)
{
    printf("票价边界测试：\n");
    check_int(metro_fare(0.0), 2, "0 km");
    check_int(metro_fare(4.0), 2, "4 km");
    check_int(metro_fare(4.01), 3, "4.01 km");
    check_int(metro_fare(8.0), 3, "8 km");
    check_int(metro_fare(8.01), 4, "8.01 km");
    check_int(metro_fare(12.0), 4, "12 km");
    check_int(metro_fare(12.01), 5, "12.01 km");
    check_int(metro_fare(18.0), 5, "18 km");
    check_int(metro_fare(18.01), 6, "18.01 km");
    check_int(metro_fare(24.0), 6, "24 km");
    check_int(metro_fare(24.01), 7, "24.01 km");
    check_int(metro_fare(32.0), 7, "32 km");
    check_int(metro_fare(32.01), 8, "32.01 km");
    check_int(metro_fare(40.0), 8, "40 km");
    check_int(metro_fare(40.01), 9, "40.01 km");
    check_int(metro_fare(50.0), 9, "50 km");
    check_int(metro_fare(50.01), 9, "50.01 km");
    check_int(metro_fare(70.0), 10, "70 km");
    check_int(metro_fare(70.01), 10, "70.01 km");
    check_int(metro_fare(90.0), 11, "90 km");
    check_int(metro_fare(-0.1), -1, "负里程");
}

static void test_empty_network(void)
{
    MetroNetwork network;
    MetroStats stats;
    MetroPath path;
    GraphEdge edges[4];
    int edge_count = 0;
    double total = 0.0;

    printf("空网络生命周期测试：\n");
    metro_network_init(&network);
    check_true(!network.loaded, "初始化后 loaded=false");
    check_int(metro_find_station(&network, "洪山广场"), -1, "空网络查站");
    check_status(metro_get_stats(&network, &stats), DS_ERR_EMPTY, "空网络统计");
    check_status(metro_print_lines(&network), DS_ERR_EMPTY, "空网络线路输出");
    check_status(metro_dfs(&network, 0, NULL, NULL), DS_ERR_EMPTY, "空网络 DFS");
    check_status(metro_mst(&network, true, edges, &edge_count, &total),
                 DS_ERR_EMPTY, "空网络 MST");
    check_status(metro_validate(&network), DS_ERR_EMPTY, "空网络校验");

    metro_path_init(&path);
    check_int(path.fare, -1, "路径初始票价未知");
    check_status(metro_plan(&network, 0, 1, METRO_FASTEST, &path),
                 DS_ERR_EMPTY, "空网络路径规划");
    metro_path_destroy(&path);

    check_status(metro_load(NULL, "fun/metro/data/wuhan_metro.txt"),
                 DS_ERR_NULL, "空网络对象加载");
    check_status(metro_load(&network, NULL), DS_ERR_NULL, "空路径加载");
    check_status(metro_load(&network, "fun/metro/data/not_exists.txt"),
                 DS_ERR_IO, "不存在文件加载");
    check_true(!network.loaded, "加载失败后仍为未加载");

    metro_network_destroy(&network);
    metro_network_destroy(&network); /* 可重复释放。 */
    check_true(!network.loaded, "释放后 loaded=false");
}

static void test_full_load(MetroNetwork *network)
{
    MetroStats stats;
    DsStatus status;

    printf("全量数据加载测试：\n");
    metro_network_init(network);
    status = metro_load(network, "fun/metro/data/wuhan_metro.txt");
    check_status(status, DS_OK, "加载全量数据");
    if (status != DS_OK) {
        return;
    }

    check_true(network->loaded, "loaded=true");
    check_int((int)network->line_count, 13, "线路数");
    check_int((int)network->station_count, 298, "物理站点数");
    check_int((int)network->segment_count, 13, "区段数");
    check_int((int)network->stop_count, 352, "停靠点记录数");
    check_int((int)network->node_count, 352, "线路-站点节点数");

    status = metro_get_stats(network, &stats);
    check_status(status, DS_OK, "统计接口");
    check_int((int)stats.transfer_station_count, 49, "换乘站数");
    check_double(stats.total_line_km, 575.2, 0.2, "线路总里程");
    check_true(stats.station_graph_connected, "物理站点图连通");

    check_true(metro_find_station(network, "洪山广场") >= 0, "按中文名查站");
    check_true(metro_find_station_by_id(network, "S050") >= 0, "按编号查站");
    check_int(metro_find_station(network, "不存在的站"), -1, "查不存在的站");

    {
        size_t i;
        int loop_count = 0;
        for (i = 0; i < network->segment_count; ++i) {
            if (network->segments[i].loop) {
                ++loop_count;
            }
        }
        check_int(loop_count, 1, "环线区段数");
    }

    check_status(metro_validate(network), DS_OK, "全量数据校验");

    /* 失败重载不能破坏已经加载的数据。 */
    check_status(metro_load(network, "fun/metro/data/not_exists.txt"),
                 DS_ERR_IO, "失败重载返回 IO 错误");
    check_true(network->loaded, "失败重载后仍保持已加载");
    check_int((int)network->station_count, 298, "失败重载后站点数不变");
}

static void check_path_endpoints(const MetroNetwork *network,
                                 const MetroPath *path,
                                 int from,
                                 int to,
                                 const char *label)
{
    int first_node;
    int last_node;

    check_true(path->nodes != NULL && path->node_count >= 1, label);
    if (path->nodes == NULL || path->node_count == 0) {
        return;
    }
    first_node = path->nodes[0];
    last_node = path->nodes[path->node_count - 1];
    check_int(network->nodes[first_node].station, from, "路径起点");
    check_int(network->nodes[last_node].station, to, "路径终点");
    check_true(path->distance_km > 0.0, "路径里程为正");
    check_true(path->time_min > 0.0, "路径时间为正");
    check_true(path->fare >= 2, "票价为有效值");
}

static void test_paths(MetroNetwork *network)
{
    MetroPath fastest;
    MetroPath shortest;
    MetroPath fewest;
    MetroPath same;
    MetroPath hub;
    int from = metro_find_station(network, "宏图大道");
    int to = metro_find_station(network, "武昌火车站");
    int same_station = metro_find_station(network, "洪山广场");
    DsStatus status;

    printf("路径规划测试：\n");
    check_true(from >= 0 && to >= 0 && same_station >= 0, "测试站点存在");
    if (from < 0 || to < 0 || same_station < 0) {
        return;
    }

    metro_path_init(&fastest);
    metro_path_init(&shortest);
    metro_path_init(&fewest);
    metro_path_init(&same);
    metro_path_init(&hub);

    status = metro_plan(network, from, to, METRO_FASTEST, &fastest);
    check_status(status, DS_OK, "最快路径");
    if (status == DS_OK) {
        check_path_endpoints(network, &fastest, from, to, "最快路径结构");
    }

    status = metro_plan(network, from, to, METRO_SHORTEST, &shortest);
    check_status(status, DS_OK, "最短路径");
    if (status == DS_OK) {
        check_path_endpoints(network, &shortest, from, to, "最短路径结构");
        check_int(shortest.fare, metro_fare(shortest.distance_km),
                  "票价与最短里程一致");
    }

    status = metro_plan(network, from, to, METRO_FEWEST_TRANSFERS, &fewest);
    check_status(status, DS_OK, "最少换乘路径");
    if (status == DS_OK) {
        check_path_endpoints(network, &fewest, from, to, "少换乘路径结构");
    }

    if (fastest.nodes != NULL && shortest.nodes != NULL) {
        check_true(shortest.distance_km <= fastest.distance_km + 1e-6,
                   "最短方案里程不大于最快方案");
    }
    if (fastest.nodes != NULL && fewest.nodes != NULL) {
        check_true(fewest.transfers <= fastest.transfers,
                   "少换乘方案换乘次数不大于最快方案");
    }

    /* 换乘枢纽站有多个线路节点，验证多源枚举不会漏掉可行组合。 */
    status = metro_plan(network, same_station, to, METRO_FASTEST, &hub);
    check_status(status, DS_OK, "从换乘枢纽出发");
    if (status == DS_OK) {
        check_path_endpoints(network, &hub, same_station, to,
                             "枢纽路径结构");
    }

    check_status(metro_plan(network, -1, to, METRO_FASTEST, &hub),
                 DS_ERR_RANGE, "起点越界");
    check_status(metro_plan(network, from, to, (MetroCriterion)99, &hub),
                 DS_ERR_RANGE, "无效优化目标");

    status = metro_plan(network, same_station, same_station,
                        METRO_FASTEST, &same);
    check_status(status, DS_OK, "同站路径");
    if (status == DS_OK) {
        check_int((int)same.node_count, 1, "同站路径只有一个节点");
        check_double(same.distance_km, 0.0, 1e-9, "同站里程为 0");
        check_double(same.time_min, 0.0, 1e-9, "同站时间为 0");
        check_int(same.fare, 2, "同站进出票价 2 元");
    }

    metro_path_destroy(&fastest);
    metro_path_destroy(&shortest);
    metro_path_destroy(&fewest);
    metro_path_destroy(&same);
    metro_path_destroy(&hub);
}

static void count_visit(const MetroNetwork *network, int station, void *context)
{
    size_t *count = (size_t *)context;

    (void)network;
    (void)station;
    ++*count;
}

static void test_traversal_and_mst(MetroNetwork *network)
{
    GraphEdge *prim_edges;
    GraphEdge *kruskal_edges;
    int prim_count = 0;
    int kruskal_count = 0;
    double prim_total = 0.0;
    double kruskal_total = 0.0;
    size_t dfs_count = 0;
    size_t bfs_count = 0;
    int start = metro_find_station(network, "洪山广场");

    printf("遍历与生成树测试：\n");
    check_true(start >= 0, "遍历起点存在");
    if (start < 0) {
        return;
    }

    check_status(metro_dfs(network, start, count_visit, &dfs_count),
                 DS_OK, "DFS");
    check_status(metro_bfs(network, start, count_visit, &bfs_count),
                 DS_OK, "BFS");
    check_int((int)dfs_count, 298, "DFS 访问全部站点");
    check_int((int)bfs_count, 298, "BFS 访问全部站点");

    prim_edges = (GraphEdge *)malloc(network->station_count * sizeof(*prim_edges));
    kruskal_edges = (GraphEdge *)malloc(network->station_count * sizeof(*kruskal_edges));
    check_true(prim_edges != NULL && kruskal_edges != NULL, "生成树缓冲区");
    if (prim_edges == NULL || kruskal_edges == NULL) {
        free(prim_edges);
        free(kruskal_edges);
        return;
    }

    check_status(metro_mst(network, true, prim_edges,
                           &prim_count, &prim_total), DS_OK, "Prim");
    check_status(metro_mst(network, false, kruskal_edges,
                           &kruskal_count, &kruskal_total), DS_OK, "Kruskal");
    check_int(prim_count, 297, "Prim 边数");
    check_int(kruskal_count, 297, "Kruskal 边数");
    check_double(prim_total, kruskal_total, 1e-6, "Prim/Kruskal 总权值一致");
    check_true(prim_total > 0.0, "生成树总权值为正");

    free(prim_edges);
    free(kruskal_edges);
}

int main(void)
{
    MetroNetwork network;
    DsStatus path_status;

    printf("=== 武汉地铁模块测试 ===\n");
    path_status = ds_path_set_project_root();
    if (path_status != DS_OK) {
        printf("无法定位工程根目录：%s\n", ds_status_text(path_status));
        return 1;
    }

    test_fare();
    test_empty_network();
    test_full_load(&network);
    if (network.loaded) {
        test_paths(&network);
        test_traversal_and_mst(&network);
    }
    metro_network_destroy(&network);

    if (failures == 0) {
        printf("\n全部测试通过。\n");
        return 0;
    }
    printf("\n共 %d 项失败。\n", failures);
    return 1;
}
