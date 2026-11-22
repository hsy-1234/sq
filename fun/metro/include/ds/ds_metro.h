/**
 * @file    ds_metro.h
 * @brief   趣味题：武汉轨道交通网的数据模型与查询接口。
 *
 * 本模块位于 fun/metro 目录，与四道基础题隔离，不参与 build/libds.a 的
 * 编译。它复用基础库中的 Graph：
 *
 *   - time_graph      线路-站点图，边权为乘车分钟数 + 换乘分钟数；
 *   - distance_graph  线路-站点图，边权为乘车公里数；
 *   - transfer_graph  线路-站点图，乘车边为 0，换乘边为 1；
 *   - station_graph   物理站点图，用于 DFS/BFS、连通性判断和 MST。
 *
 * 设计要点：
 *   1. “站点”和“线路-站点节点”分开。洪山广场是一个物理站点，但
 *      (2 号线, 洪山广场) 与 (4 号线, 洪山广场) 是两个图节点，中间用
 *      换乘边连接。这样换乘次数、换乘时间和线路信息才不会丢失。
 *   2. 物理站点图把同一站点的不同线路合并成一个节点，适合遍历和 MST。
 *   3. 顶点名使用 S001、L02S031 这类短编号，中文名保存在 MetroStation /
 *      MetroLine 数组里，避免 Graph 的 32 字节顶点名被中文站名截断。
 *
 * 数据文件格式见 fun/metro/data/README.md。
 */
#ifndef DS_METRO_H
#define DS_METRO_H

#include <stddef.h>
#include <stdbool.h>
#include "ds/ds_types.h"
#include "ds/ds_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 站点/线路编号缓冲区大小（含 '\0'）。 */
#define METRO_ID_SIZE 8

/** 站点中文名缓冲区大小（含 '\0'）。 */
#define METRO_NAME_SIZE 64

/** 线路中文名缓冲区大小（含 '\0'）。 */
#define METRO_LINE_NAME_SIZE 32

/** 一个站点最多直接关联的线路数；武汉地铁目前远小于该值。 */
#define METRO_MAX_LINES_PER_STATION 8

/** 默认换乘步行/等待时间（分钟）。 */
#define METRO_DEFAULT_TRANSFER_MINUTES 4.0

/** 表示不可达的距离/时间。 */
#define METRO_INF 1e30

/** 一个物理站点。 */
typedef struct MetroStation {
    char id[METRO_ID_SIZE];                 /**< 稳定编号，如 S031。 */
    char name[METRO_NAME_SIZE];             /**< 中文站名。 */
    int line_count;                         /**< 经过该站的线路数。 */
    int lines[METRO_MAX_LINES_PER_STATION]; /**< 线路下标（MetroNetwork.lines）。 */
    double transfer_minutes;                /**< 换乘步行/等待时间。 */
    bool is_transfer;                       /**< line_count >= 2 时为 true。 */
} MetroStation;

/** 一条运营线路的汇总信息。 */
typedef struct MetroLine {
    char id[METRO_ID_SIZE];                 /**< 线路编号，如 L02。 */
    char name[METRO_LINE_NAME_SIZE];        /**< 线路名，如 2号线。 */
    int station_count;                      /**< 不重复的站点数。 */
    int segment_count;                      /**< 区段数；支线/主线可各占一段。 */
    double total_km;                        /**< 各区段里程之和。 */
} MetroLine;

/**
 * @brief 一条连续区段。
 *
 * 普通线路通常只有一个区段；有支线的线路可以有多个区段。环线通过
 * loop = true 表示最后一站与第一站相连。
 */
typedef struct MetroSegment {
    int line;              /**< 所属线路下标。 */
    int segment_id;        /**< 文件中的区段编号（从 1 开始）。 */
    bool loop;             /**< true 表示环线，最后一站接回第一站。 */
    size_t first_stop;     /**< 在 MetroNetwork.stops 中的起始下标。 */
    size_t stop_count;     /**< 本区段站点数。 */
} MetroSegment;

/** 区段中的一个停靠点。 */
typedef struct MetroStop {
    int station;               /**< 物理站点下标。 */
    double km_to_next;         /**< 到下一站的公里数；普通区段末站为 0。 */
    double minutes_to_next;    /**< 到下一站的运行分钟数；末站为 0。 */
} MetroStop;

/** 线路-站点图中的一个节点。 */
typedef struct MetroNode {
    int station;   /**< 物理站点下标。 */
    int line;      /**< 线路下标。 */
} MetroNode;

/** 出行方案的优化目标。 */
typedef enum MetroCriterion {
    METRO_FASTEST = 0,          /**< 总时间最少（含换乘）。 */
    METRO_SHORTEST = 1,         /**< 总里程最短，用于计算票价。 */
    METRO_FEWEST_TRANSFERS = 2  /**< 换乘次数最少。 */
} MetroCriterion;

/**
 * @brief 一次查询得到的路径。
 *
 * nodes 保存线路-站点节点下标，而不是物理站点下标。这样相邻两个节点
 * 的 line 不同时，就能识别为一次换乘。
 */
typedef struct MetroPath {
    int *nodes;            /**< 线路-站点节点序列。 */
    size_t node_count;     /**< 节点数。 */
    double distance_km;    /**< 总里程。 */
    double time_min;       /**< 总时间（含换乘）。 */
    int transfers;         /**< 换乘次数。 */
    int fare;              /**< 按本次路径里程计算的票价；-1 表示未知。 */
} MetroPath;

/** 统计信息。 */
typedef struct MetroStats {
    size_t line_count;                 /**< 线路数。 */
    size_t station_count;              /**< 物理站点数。 */
    size_t transfer_station_count;     /**< 换乘站数。 */
    size_t segment_count;              /**< 区段数。 */
    size_t stop_count;                 /**< 停靠点记录数。 */
    size_t node_count;                 /**< 线路-站点节点数。 */
    double total_line_km;              /**< 各线路里程之和。 */
    bool station_graph_connected;      /**< 物理站点图是否连通。 */
} MetroStats;

/**
 * @brief 地铁网络对象。
 *
 * 调用 metro_network_init() 后使用；metro_network_destroy() 释放内部
 * 所有动态内存，可重复调用。
 */
typedef struct MetroNetwork {
    MetroStation *stations;       /**< 物理站点数组。 */
    size_t station_count;         /**< 当前站点数。 */
    size_t station_capacity;      /**< 站点数组容量。 */

    MetroLine *lines;             /**< 线路数组。 */
    size_t line_count;            /**< 当前线路数。 */
    size_t line_capacity;         /**< 线路数组容量。 */

    MetroSegment *segments;       /**< 区段数组。 */
    size_t segment_count;         /**< 当前区段数。 */
    size_t segment_capacity;      /**< 区段数组容量。 */

    MetroStop *stops;             /**< 停靠点数组，按区段连续存放。 */
    size_t stop_count;            /**< 当前停靠点数。 */
    size_t stop_capacity;         /**< 停靠点数组容量。 */

    MetroNode *nodes;             /**< 线路-站点节点数组。 */
    size_t node_count;            /**< 当前节点数。 */
    size_t node_capacity;         /**< 节点数组容量。 */

    /**
     * 线路-站点节点查找表，长度为 line_count * station_count。
     * node_lookup[line * station_count + station] 为节点下标，-1 表示不存在。
     */
    int *node_lookup;

    Graph time_graph;             /**< 时间图，边权：分钟。 */
    Graph distance_graph;         /**< 距离图，边权：公里。 */
    Graph transfer_graph;         /**< 换乘图，乘车 0、换乘 1。 */
    Graph station_graph;          /**< 物理站点图，用于遍历/MST。 */

    bool loaded;                  /**< 数据是否已成功加载。 */
} MetroNetwork;

/** 访问物理站点时的回调。 */
typedef void (*MetroStationVisitFn)(const MetroNetwork *network,
                                    int station,
                                    void *context);

/* ------------------------------------------------------------------------- */
/* 生命周期                                                                   */
/* ------------------------------------------------------------------------- */

/** @brief 把网络对象清零，之后才能调用 metro_load()。 */
void metro_network_init(MetroNetwork *network);

/** @brief 释放网络内部所有数组和图；可重复调用。 */
void metro_network_destroy(MetroNetwork *network);

/**
 * @brief 从自定义文本文件加载武汉地铁数据。
 *
 * 加载采用“先构建临时网络，成功后再替换旧网络”的方式：两遍扫描
 * STATION/LINE 与 SEG/STOP/TRANSFER，随后建立线路-站点节点和四张图；
 * 任何一步失败都会释放临时网络，旧网络保持不变。
 *
 * @param network 目标网络对象。
 * @param path    数据文件路径，例如 fun/metro/data/wuhan_metro.txt。
 * @return DS_OK；参数错误返回 DS_ERR_NULL；文件打不开返回 DS_ERR_IO；
 *         格式或数据校验失败返回 DS_ERR_FORMAT / DS_ERR_STATE。
 */
DsStatus metro_load(MetroNetwork *network, const char *path);

/* ------------------------------------------------------------------------- */
/* 站点/线路查询                                                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief 按中文站名精确查找物理站点。
 * @return 站点下标；未找到或参数非法返回 -1。
 */
int metro_find_station(const MetroNetwork *network, const char *name);

/**
 * @brief 按站点编号查找物理站点。
 * @return 站点下标；未找到或参数非法返回 -1。
 */
int metro_find_station_by_id(const MetroNetwork *network, const char *id);

/** @brief 返回站点名；下标非法时返回 "(未知站点)"，不会返回 NULL。 */
const char *metro_station_name(const MetroNetwork *network, int station);

/** @brief 返回线路名；下标非法时返回 "(未知线路)"，不会返回 NULL。 */
const char *metro_line_name(const MetroNetwork *network, int line);

/** @brief 汇总线路、站点、换乘、节点和连通性统计。 */
DsStatus metro_get_stats(const MetroNetwork *network, MetroStats *stats);

/** @brief 以表格形式输出统计信息。 */
void metro_print_stats(const MetroNetwork *network);

/** @brief 输出所有线路概览。 */
DsStatus metro_print_lines(const MetroNetwork *network);

/** @brief 输出指定线路的站序、站间距和累计里程。 */
DsStatus metro_print_line_detail(const MetroNetwork *network, int line);

/** @brief 输出指定物理站点的所属线路和相邻站信息。 */
DsStatus metro_print_station(const MetroNetwork *network, int station);

/** @brief 输出所有换乘站。 */
DsStatus metro_print_transfers(const MetroNetwork *network);

/**
 * @brief 输出线路-站点图的邻接表。
 *
 * 每行格式：节点下标、站名、线路名，以及所有相邻节点。
 */
DsStatus metro_print_adjlist(const MetroNetwork *network);

/* ------------------------------------------------------------------------- */
/* 票价与路径                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief 按武汉地铁里程分段规则计算票价。
 *
 * 规则：0–4 km 2 元；4–8 km 3 元；8–12 km 4 元；12–18 km 5 元；
 * 18–24 km 6 元；24–32 km 7 元；32–40 km 8 元；40–50 km 9 元；
 * 50 km 以上每 20 km 加 1 元。同站进出按 2 元处理。
 *
 * @param distance_km 最短路径里程，必须 >= 0。
 * @return 票价；参数非法返回 -1。
 */
int metro_fare(double distance_km);

/** @brief 初始化路径对象。 */
void metro_path_init(MetroPath *path);

/** @brief 释放路径中的节点数组，可重复调用。 */
void metro_path_destroy(MetroPath *path);

/**
 * @brief 计算两站之间的出行方案。
 *
 * @param network   已加载的网络。
 * @param from      起点物理站点下标。
 * @param to        终点物理站点下标。
 * @param criterion 优化目标。
 * @param path      输出路径；调用前应先 metro_path_init()。
 * @return DS_OK；未加载返回 DS_ERR_EMPTY；站点越界返回 DS_ERR_RANGE；
 *         两站不连通返回 DS_ERR_NOT_FOUND。
 */
DsStatus metro_plan(const MetroNetwork *network,
                    int from,
                    int to,
                    MetroCriterion criterion,
                    MetroPath *path);

/** @brief 按“站名（线路） -> ...”格式输出路径及距离、时间、票价。 */
void metro_print_path(const MetroNetwork *network, const MetroPath *path);

/* ------------------------------------------------------------------------- */
/* 图算法                                                                     */
/* ------------------------------------------------------------------------- */

/**
 * @brief 在物理站点图上做深度优先遍历。
 * @param start   起点物理站点下标。
 * @param visit   访问回调，可为 NULL（只遍历不输出）。
 * @param context 原样传给回调。
 */
DsStatus metro_dfs(const MetroNetwork *network,
                   int start,
                   MetroStationVisitFn visit,
                   void *context);

/** @brief 在物理站点图上做广度优先遍历。 */
DsStatus metro_bfs(const MetroNetwork *network,
                   int start,
                   MetroStationVisitFn visit,
                   void *context);

/**
 * @brief 在物理站点图上求最小生成树。
 *
 * @param use_prim    true 用 Prim，false 用 Kruskal。
 * @param edges       输出边数组，长度至少 station_count - 1。
 * @param edge_count  输出边数。
 * @param total_weight 输出总权值（公里）。
 */
DsStatus metro_mst(const MetroNetwork *network,
                   bool use_prim,
                   GraphEdge *edges,
                   int *edge_count,
                   double *total_weight);

/**
 * @brief 校验网络结构是否自洽。
 *
 * 检查线路/区段/站点/节点数量关系、站序中的重复站、换乘标记、
 * 物理站点图连通性等。成功返回 DS_OK，否则返回具体错误码。
 */
DsStatus metro_validate(const MetroNetwork *network);

#ifdef __cplusplus
}
#endif

#endif /* DS_METRO_H */
