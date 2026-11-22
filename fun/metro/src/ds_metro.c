/**
 * @file    ds_metro.c
 * @brief   武汉轨道交通网：对象生命周期、查询、票价、统计与图算法适配。
 *
 * 本文件负责：
 *   - 解析全量文本数据，建立物理站点、线路、区段、停靠点和线路-站点节点；
 *   - 由节点建立时间图、距离图、换乘图和物理站点图；
 *   - 查询、统计、票价、线路/站点/换乘/邻接表输出；
 *   - DFS/BFS/MST 复用基础库 ds_graph；
 *   - 三种出行目标分别调用 Dijkstra，再统一重算里程、时间、换乘和票价。
 *
 * 数据格式与字段含义见 fun/metro/data/README.md。
 */
#include "ds/ds_metro.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ds/ds_memory.h"
#include "ds/ds_util.h"

/* ------------------------------------------------------------------------- */
/* 内部工具                                                                   */
/* ------------------------------------------------------------------------- */

const char *metro_line_name(const MetroNetwork *network, int line)
{
    if (network == NULL || line < 0 || (size_t)line >= network->line_count) {
        return "(未知线路)";
    }
    return network->lines[line].name;
}

const char *metro_station_name(const MetroNetwork *network, int station)
{
    if (network == NULL || station < 0 ||
        (size_t)station >= network->station_count) {
        return "(未知站点)";
    }
    return network->stations[station].name;
}

/** @brief 把 graph_dfs/graph_bfs 的顶点回调转成地铁站点回调。 */
typedef struct MetroVisitBridge {
    const MetroNetwork *network;
    MetroStationVisitFn visit;
    void *context;
} MetroVisitBridge;

static void metro_visit_bridge(int vertex, void *context)
{
    MetroVisitBridge *bridge = (MetroVisitBridge *)context;

    if (bridge != NULL && bridge->visit != NULL) {
        bridge->visit(bridge->network, vertex, bridge->context);
    }
}

/* ------------------------------------------------------------------------- */
/* 加载器内部工具                                                             */
/* ------------------------------------------------------------------------- */

/**
 * @brief 保证动态数组容量足够。
 *
 * 这里不返回错误码：ds_realloc() 在内存不足时会直接终止程序，因此只
 * 需要关心“容量是否够”这一件事。容量按 2 倍增长，避免逐条 realloc。
 */
static void metro_grow_array(void **data, size_t *capacity,
                             size_t needed, size_t element_size)
{
    size_t new_capacity;

    if (needed <= *capacity) {
        return;
    }
    new_capacity = (*capacity == 0) ? 16 : *capacity;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    *data = ds_realloc(*data, new_capacity * element_size);
    *capacity = new_capacity;
}

/** @brief 从一行中取出下一个空白分隔的 token，并原地写入 '\0'。 */
static char *metro_next_token(char **cursor)
{
    char *start;
    char *p;

    if (cursor == NULL || *cursor == NULL) {
        return NULL;
    }
    p = *cursor;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p == '\0') {
        *cursor = p;
        return NULL;
    }
    start = p;
    while (*p != '\0' && !isspace((unsigned char)*p)) {
        ++p;
    }
    if (*p != '\0') {
        *p = '\0';
        ++p;
    }
    *cursor = p;
    return start;
}

/** @brief 严格解析非负 double，拒绝空串、尾随字符、NaN 和无穷大。 */
static bool metro_parse_double(const char *token, double *value)
{
    char *end = NULL;
    double parsed;

    if (token == NULL || value == NULL) {
        return false;
    }
    parsed = strtod(token, &end);
    if (end == token || *end != '\0' || !isfinite(parsed) || parsed < 0.0) {
        return false;
    }
    *value = parsed;
    return true;
}

/** @brief 严格解析 int，拒绝空串、尾随字符和溢出。 */
static bool metro_parse_int(const char *token, int *value)
{
    char *end = NULL;
    long parsed;

    if (token == NULL || value == NULL) {
        return false;
    }
    parsed = strtol(token, &end, 10);
    if (end == token || *end != '\0' || parsed < 0 || parsed > 2147483647L) {
        return false;
    }
    *value = (int)parsed;
    return true;
}

/** @brief 按编号查找线路；未找到返回 -1。 */
static int metro_find_line_index(const MetroNetwork *network, const char *id)
{
    size_t i;

    if (network == NULL || id == NULL) {
        return -1;
    }
    for (i = 0; i < network->line_count; ++i) {
        if (strcmp(network->lines[i].id, id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief 追加一个物理站点。
 * @return 新站点下标；编号重复、字段超长或容量异常返回 -1。
 */
static int metro_add_station(MetroNetwork *network,
                             const char *id,
                             const char *name)
{
    MetroStation *station;

    if (network == NULL || id == NULL || name == NULL ||
        id[0] == '\0' || name[0] == '\0' ||
        strlen(id) >= METRO_ID_SIZE || strlen(name) >= METRO_NAME_SIZE) {
        return -1;
    }
    if (metro_find_station_by_id(network, id) >= 0) {
        return -1;
    }

    metro_grow_array((void **)&network->stations, &network->station_capacity,
                     network->station_count + 1, sizeof(*network->stations));
    station = &network->stations[network->station_count];
    memset(station, 0, sizeof(*station));
    ds_str_copy(station->id, sizeof(station->id), id);
    ds_str_copy(station->name, sizeof(station->name), name);
    station->transfer_minutes = METRO_DEFAULT_TRANSFER_MINUTES;
    return (int)network->station_count++;
}

/** @brief 追加一条线路；编号重复或字段超长返回 -1。 */
static int metro_add_line(MetroNetwork *network,
                          const char *id,
                          const char *name)
{
    MetroLine *line;

    if (network == NULL || id == NULL || name == NULL ||
        id[0] == '\0' || name[0] == '\0' ||
        strlen(id) >= METRO_ID_SIZE || strlen(name) >= METRO_LINE_NAME_SIZE) {
        return -1;
    }
    if (metro_find_line_index(network, id) >= 0) {
        return -1;
    }

    metro_grow_array((void **)&network->lines, &network->line_capacity,
                     network->line_count + 1, sizeof(*network->lines));
    line = &network->lines[network->line_count];
    memset(line, 0, sizeof(*line));
    ds_str_copy(line->id, sizeof(line->id), id);
    ds_str_copy(line->name, sizeof(line->name), name);
    return (int)network->line_count++;
}

/** @brief 追加一个区段，并把它挂到所属线路上。 */
static DsStatus metro_add_segment(MetroNetwork *network,
                                  int line,
                                  int segment_id,
                                  bool loop,
                                  size_t *out_index)
{
    MetroSegment *segment;

    if (network == NULL || out_index == NULL ||
        line < 0 || (size_t)line >= network->line_count || segment_id <= 0) {
        return DS_ERR_RANGE;
    }

    metro_grow_array((void **)&network->segments, &network->segment_capacity,
                     network->segment_count + 1, sizeof(*network->segments));
    segment = &network->segments[network->segment_count];
    segment->line = line;
    segment->segment_id = segment_id;
    segment->loop = loop;
    segment->first_stop = network->stop_count;
    segment->stop_count = 0;

    ++network->lines[line].segment_count;
    *out_index = network->segment_count++;
    return DS_OK;
}

/** @brief 向当前区段追加一个停靠点。 */
static DsStatus metro_add_stop(MetroNetwork *network,
                               size_t segment_index,
                               int station,
                               double km_to_next,
                               double minutes_to_next)
{
    MetroStop *stop;

    if (network == NULL || segment_index >= network->segment_count ||
        station < 0 || (size_t)station >= network->station_count ||
        !isfinite(km_to_next) || km_to_next < 0.0 ||
        !isfinite(minutes_to_next) || minutes_to_next < 0.0) {
        return DS_ERR_RANGE;
    }

    metro_grow_array((void **)&network->stops, &network->stop_capacity,
                     network->stop_count + 1, sizeof(*network->stops));
    stop = &network->stops[network->stop_count];
    stop->station = station;
    stop->km_to_next = km_to_next;
    stop->minutes_to_next = minutes_to_next;
    ++network->stop_count;
    ++network->segments[segment_index].stop_count;
    return DS_OK;
}

/** @brief 读一行并去掉行尾换行；超长行会被判定为格式错误。 */
static DsStatus metro_read_line(FILE *fp, char *buffer, size_t size,
                                int *line_number)
{
    size_t length;

    if (fgets(buffer, (int)size, fp) == NULL) {
        return DS_ERR_IO;
    }
    ++*line_number;
    length = strlen(buffer);
    if (length == size - 1 && buffer[length - 1] != '\n' && !feof(fp)) {
        int ch;
        while ((ch = fgetc(fp)) != '\n' && ch != EOF) {
            /* 丢弃超长行的剩余部分，保证下一次读取从新行开始。 */
        }
        return DS_ERR_FORMAT;
    }
    ds_str_trim(buffer);
    return DS_OK;
}

/**
 * @brief 第一遍扫描：只收集 STATION 和 LINE。
 *
 * 先收集所有站点和线路，第二遍再处理 SEG/STOP/TRANSFER，这样记录
 * 顺序不必严格依赖“站点在前、线路在后”的写法。
 */
static DsStatus metro_parse_stations_and_lines(MetroNetwork *network,
                                               FILE *fp,
                                               int *line_number)
{
    char buffer[1024];
    bool first_line = true;

    for (;;) {
        DsStatus status = metro_read_line(fp, buffer, sizeof(buffer), line_number);
        char *cursor;
        char *command;
        char *id;
        char *name;

        if (status == DS_ERR_IO && feof(fp)) {
            return DS_OK;
        }
        if (status != DS_OK) {
            return status;
        }
        if (first_line) {
            first_line = false;
            if (strncmp(buffer, "\xEF\xBB\xBF", 3) == 0) {
                memmove(buffer, buffer + 3, strlen(buffer + 3) + 1);
            }
        }
        if (buffer[0] == '\0' || buffer[0] == '#') {
            continue;
        }

        cursor = buffer;
        command = metro_next_token(&cursor);
        if (command == NULL) {
            continue;
        }
        if (strcmp(command, "STATION") == 0) {
            id = metro_next_token(&cursor);
            name = cursor;
            ds_str_trim(name);
            if (id == NULL || name[0] == '\0' ||
                metro_add_station(network, id, name) < 0) {
                return DS_ERR_FORMAT;
            }
        } else if (strcmp(command, "LINE") == 0) {
            id = metro_next_token(&cursor);
            name = cursor;
            ds_str_trim(name);
            if (id == NULL || name[0] == '\0' ||
                metro_add_line(network, id, name) < 0) {
                return DS_ERR_FORMAT;
            }
        }
        /* SEG/STOP/TRANSFER 留给第二遍。 */
    }
}

/** @brief 判断同一条线路中是否已经存在相同编号的区段。 */
static bool metro_segment_exists(const MetroNetwork *network,
                                 int line,
                                 int segment_id)
{
    size_t i;

    for (i = 0; i < network->segment_count; ++i) {
        if (network->segments[i].line == line &&
            network->segments[i].segment_id == segment_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 第二遍扫描：收集 SEG/STOP/TRANSFER。
 *
 * STOP 必须紧跟在当前 SEG 之后，且线路编号必须与 SEG 一致；这样
 * 可以避免“停靠点挂错区段”这类很难从统计数字看出来的错误。
 */
static DsStatus metro_parse_segments_and_stops(MetroNetwork *network,
                                               FILE *fp,
                                               int *line_number)
{
    char buffer[1024];
    size_t current_segment = (size_t)-1;

    for (;;) {
        DsStatus status = metro_read_line(fp, buffer, sizeof(buffer), line_number);
        char *cursor;
        char *command;

        if (status == DS_ERR_IO && feof(fp)) {
            return DS_OK;
        }
        if (status != DS_OK) {
            return status;
        }
        if (buffer[0] == '\0' || buffer[0] == '#') {
            continue;
        }

        cursor = buffer;
        command = metro_next_token(&cursor);
        if (command == NULL) {
            continue;
        }

        if (strcmp(command, "SEG") == 0) {
            char *line_id = metro_next_token(&cursor);
            char *segment_token = metro_next_token(&cursor);
            char *loop_token = metro_next_token(&cursor);
            int line;
            int segment_id;
            int loop_value;

            if (line_id == NULL || segment_token == NULL || loop_token == NULL ||
                !metro_parse_int(segment_token, &segment_id) ||
                segment_id <= 0 ||
                !metro_parse_int(loop_token, &loop_value) ||
                (loop_value != 0 && loop_value != 1)) {
                return DS_ERR_FORMAT;
            }
            line = metro_find_line_index(network, line_id);
            if (line < 0 || metro_segment_exists(network, line, segment_id)) {
                return DS_ERR_FORMAT;
            }
            status = metro_add_segment(network, line, segment_id,
                                       loop_value == 1, &current_segment);
            if (status != DS_OK) {
                return status;
            }
        } else if (strcmp(command, "STOP") == 0) {
            char *line_id = metro_next_token(&cursor);
            char *station_id = metro_next_token(&cursor);
            char *km_token = metro_next_token(&cursor);
            char *minute_token = metro_next_token(&cursor);
            int line;
            int station;
            double km;
            double minutes;

            if (current_segment == (size_t)-1 ||
                line_id == NULL || station_id == NULL ||
                km_token == NULL || minute_token == NULL ||
                !metro_parse_double(km_token, &km) ||
                !metro_parse_double(minute_token, &minutes)) {
                return DS_ERR_FORMAT;
            }
            line = metro_find_line_index(network, line_id);
            station = metro_find_station_by_id(network, station_id);
            if (line < 0 || station < 0 ||
                network->segments[current_segment].line != line) {
                return DS_ERR_FORMAT;
            }
            status = metro_add_stop(network, current_segment,
                                    station, km, minutes);
            if (status != DS_OK) {
                return status;
            }
        } else if (strcmp(command, "TRANSFER") == 0) {
            char *station_id = metro_next_token(&cursor);
            char *minute_token = metro_next_token(&cursor);
            int station;
            double minutes;

            if (station_id == NULL || minute_token == NULL ||
                !metro_parse_double(minute_token, &minutes) ||
                minutes <= 0.0) {
                return DS_ERR_FORMAT;
            }
            station = metro_find_station_by_id(network, station_id);
            if (station < 0) {
                return DS_ERR_FORMAT;
            }
            network->stations[station].transfer_minutes = minutes;
        }
    }
}

/**
 * @brief 为每条线路的每个物理站点建立节点，并填充站点反向索引。
 *
 * node_lookup 使用“线路下标 * 站点数 + 站点下标”的一维布局，避免
 * 为每个站点单独分配一个小数组。节点编号同时是四张图的顶点编号。
 */
static DsStatus metro_build_nodes(MetroNetwork *network)
{
    size_t lookup_count;
    size_t i;

    if (network == NULL || network->station_count == 0 ||
        network->line_count == 0 || network->stop_count == 0) {
        return DS_ERR_FORMAT;
    }
    lookup_count = network->line_count * network->station_count;
    if (lookup_count / network->line_count != network->station_count) {
        return DS_ERR_MEMORY;
    }

    network->node_lookup = (int *)ds_malloc(lookup_count * sizeof(int));
    for (i = 0; i < lookup_count; ++i) {
        network->node_lookup[i] = -1;
    }

    for (i = 0; i < network->segment_count; ++i) {
        const MetroSegment *segment = &network->segments[i];
        size_t k;

        for (k = 0; k < segment->stop_count; ++k) {
            const MetroStop *stop = &network->stops[segment->first_stop + k];
            size_t key = (size_t)segment->line * network->station_count +
                         (size_t)stop->station;

            if (network->node_lookup[key] < 0) {
                MetroNode *node;

                metro_grow_array((void **)&network->nodes,
                                 &network->node_capacity,
                                 network->node_count + 1,
                                 sizeof(*network->nodes));
                node = &network->nodes[network->node_count];
                node->station = stop->station;
                node->line = segment->line;
                network->node_lookup[key] = (int)network->node_count;
                ++network->node_count;
                ++network->lines[segment->line].station_count;
            }
        }
    }

    /* 根据节点反向填充“一个站点由哪些线路经过”。 */
    for (i = 0; i < network->node_count; ++i) {
        const MetroNode *node = &network->nodes[i];
        MetroStation *station = &network->stations[node->station];
        int k;
        bool exists = false;

        for (k = 0; k < station->line_count; ++k) {
            if (station->lines[k] == node->line) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            if (station->line_count >= METRO_MAX_LINES_PER_STATION) {
                return DS_ERR_FORMAT;
            }
            station->lines[station->line_count++] = node->line;
        }
    }

    /* 线路总里程包含普通区段和环线的闭环边。 */
    for (i = 0; i < network->line_count; ++i) {
        network->lines[i].total_km = 0.0;
    }
    for (i = 0; i < network->segment_count; ++i) {
        const MetroSegment *segment = &network->segments[i];
        size_t k;

        for (k = 0; k < segment->stop_count; ++k) {
            network->lines[segment->line].total_km +=
                network->stops[segment->first_stop + k].km_to_next;
        }
    }

    for (i = 0; i < network->station_count; ++i) {
        network->stations[i].is_transfer =
            network->stations[i].line_count >= 2;
    }
    return DS_OK;
}

/**
 * @brief 添加一条无向边；若已有更短边则保留更短者。
 *
 * 物理站点图会把多条线路经过的同一对相邻站合并成一条边，因此不能
 * 让后写入的较长距离覆盖前面的较短距离。
 */
static DsStatus metro_add_shorter_edge(Graph *graph, int u, int v, double weight)
{
    double old_weight = 0.0;

    if (graph == NULL || u == v) {
        return DS_OK;
    }
    if (graph_has_edge(graph, u, v, &old_weight) && old_weight <= weight) {
        return DS_OK;
    }
    return graph_add_edge(graph, u, v, weight);
}

/** @brief 建立四张图：时间、距离、换乘次数、物理站点。 */
static DsStatus metro_build_graphs(MetroNetwork *network)
{
    int node_count;
    int station_count;
    int i;
    DsStatus status;

    if (network == NULL || network->node_count == 0 ||
        network->station_count == 0 || network->node_lookup == NULL) {
        return DS_ERR_FORMAT;
    }
    node_count = (int)network->node_count;
    station_count = (int)network->station_count;

    status = graph_init(&network->time_graph, node_count, false);
    if (status != DS_OK) {
        return status;
    }
    status = graph_init(&network->distance_graph, node_count, false);
    if (status != DS_OK) {
        return status;
    }
    status = graph_init(&network->transfer_graph, node_count, false);
    if (status != DS_OK) {
        return status;
    }
    status = graph_init(&network->station_graph, station_count, false);
    if (status != DS_OK) {
        return status;
    }

    for (i = 0; i < node_count; ++i) {
        const MetroNode *node = &network->nodes[i];
        char vertex_name[DS_GRAPH_NAME_SIZE];

        snprintf(vertex_name, sizeof(vertex_name), "%s%s",
                 network->lines[node->line].id,
                 network->stations[node->station].id);
        if (graph_add_vertex(&network->time_graph, vertex_name) < 0 ||
            graph_add_vertex(&network->distance_graph, vertex_name) < 0 ||
            graph_add_vertex(&network->transfer_graph, vertex_name) < 0) {
            return DS_ERR_MEMORY;
        }
    }
    for (i = 0; i < station_count; ++i) {
        if (graph_add_vertex(&network->station_graph,
                             network->stations[i].id) < 0) {
            return DS_ERR_MEMORY;
        }
    }

    /*
     * 乘车边：同一线路相邻停靠点。
     * 普通区段最后一站没有下一条边；环线最后一站接回第一站。
     */
    for (i = 0; i < (int)network->segment_count; ++i) {
        const MetroSegment *segment = &network->segments[i];
        size_t k;

        for (k = 0; k < segment->stop_count; ++k) {
            const MetroStop *stop = &network->stops[segment->first_stop + k];
            const MetroStop *next = NULL;
            int u;
            int v;

            if (k + 1 < segment->stop_count) {
                next = &network->stops[segment->first_stop + k + 1];
            } else if (segment->loop && segment->stop_count > 1) {
                next = &network->stops[segment->first_stop];
            }
            if (next == NULL) {
                continue;
            }

            u = network->node_lookup[(size_t)segment->line *
                                     network->station_count + stop->station];
            v = network->node_lookup[(size_t)segment->line *
                                     network->station_count + next->station];
            if (u < 0 || v < 0) {
                return DS_ERR_FORMAT;
            }

            status = metro_add_shorter_edge(&network->time_graph, u, v,
                                            stop->minutes_to_next);
            if (status != DS_OK) {
                return status;
            }
            status = metro_add_shorter_edge(&network->distance_graph, u, v,
                                            stop->km_to_next);
            if (status != DS_OK) {
                return status;
            }
            status = metro_add_shorter_edge(&network->transfer_graph, u, v, 0.0);
            if (status != DS_OK) {
                return status;
            }
            status = metro_add_shorter_edge(&network->station_graph,
                                            stop->station, next->station,
                                            stop->km_to_next);
            if (status != DS_OK) {
                return status;
            }
        }
    }

    /*
     * 换乘边：同一物理站点、不同线路的节点两两相连。
     * 时间图用站点自己的换乘时间，距离图为 0，换乘图每条边计 1 次。
     */
    for (i = 0; i < station_count; ++i) {
        const MetroStation *station = &network->stations[i];
        int a;
        int b;

        for (a = 0; a < station->line_count; ++a) {
            for (b = a + 1; b < station->line_count; ++b) {
                int u = network->node_lookup[(size_t)station->lines[a] *
                                             network->station_count + i];
                int v = network->node_lookup[(size_t)station->lines[b] *
                                             network->station_count + i];

                if (u < 0 || v < 0) {
                    return DS_ERR_FORMAT;
                }
                status = metro_add_shorter_edge(&network->time_graph, u, v,
                                                station->transfer_minutes);
                if (status != DS_OK) {
                    return status;
                }
                status = metro_add_shorter_edge(&network->distance_graph,
                                                u, v, 0.0);
                if (status != DS_OK) {
                    return status;
                }
                status = metro_add_shorter_edge(&network->transfer_graph,
                                                u, v, 1.0);
                if (status != DS_OK) {
                    return status;
                }
            }
        }
    }
    return DS_OK;
}

/* ------------------------------------------------------------------------- */
/* 生命周期                                                                   */
/* ------------------------------------------------------------------------- */

void metro_network_init(MetroNetwork *network)
{
    if (network == NULL) {
        return;
    }
    memset(network, 0, sizeof(*network));
}

void metro_network_destroy(MetroNetwork *network)
{
    if (network == NULL) {
        return;
    }

    ds_free((void **)&network->stations);
    ds_free((void **)&network->lines);
    ds_free((void **)&network->segments);
    ds_free((void **)&network->stops);
    ds_free((void **)&network->nodes);
    ds_free((void **)&network->node_lookup);

    graph_destroy(&network->time_graph);
    graph_destroy(&network->distance_graph);
    graph_destroy(&network->transfer_graph);
    graph_destroy(&network->station_graph);

    memset(network, 0, sizeof(*network));
}

DsStatus metro_load(MetroNetwork *network, const char *path)
{
    MetroNetwork next;
    FILE *fp;
    int line_number = 0;
    DsStatus status;

    if (network == NULL || path == NULL) {
        return DS_ERR_NULL;
    }

    metro_network_init(&next);
    fp = fopen(path, "r");
    if (fp == NULL) {
        return DS_ERR_IO;
    }

    /*
     * 两遍扫描共用同一个文件句柄：第一遍收集站点和线路，第二遍挂接
     * 区段、停靠点和换乘信息。这样数据文件不必严格按“先站点后线路”
     * 排列，维护时更不容易因为调整顺序而破坏解析。
     */
    status = metro_parse_stations_and_lines(&next, fp, &line_number);
    if (status == DS_OK) {
        rewind(fp);
        line_number = 0;
        status = metro_parse_segments_and_stops(&next, fp, &line_number);
    }
    fclose(fp);
    if (status != DS_OK) {
        metro_network_destroy(&next);
        return status;
    }

    status = metro_build_nodes(&next);
    if (status != DS_OK) {
        metro_network_destroy(&next);
        return status;
    }
    status = metro_build_graphs(&next);
    if (status != DS_OK) {
        metro_network_destroy(&next);
        return status;
    }

    next.loaded = true;
    status = metro_validate(&next);
    if (status != DS_OK) {
        metro_network_destroy(&next);
        return status;
    }

    /*
     * 只有全部解析和校验成功后才替换旧网络，保证加载失败时调用方
     * 仍然可以继续使用上一次成功加载的数据。
     */
    metro_network_destroy(network);
    *network = next;
    return DS_OK;
}

/* ------------------------------------------------------------------------- */
/* 站点/线路查询                                                              */
/* ------------------------------------------------------------------------- */

int metro_find_station(const MetroNetwork *network, const char *name)
{
    size_t i;

    if (network == NULL || name == NULL) {
        return -1;
    }
    for (i = 0; i < network->station_count; ++i) {
        if (strcmp(network->stations[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int metro_find_station_by_id(const MetroNetwork *network, const char *id)
{
    size_t i;

    if (network == NULL || id == NULL) {
        return -1;
    }
    for (i = 0; i < network->station_count; ++i) {
        if (strcmp(network->stations[i].id, id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

DsStatus metro_get_stats(const MetroNetwork *network, MetroStats *stats)
{
    size_t i;

    if (network == NULL || stats == NULL) {
        return DS_ERR_NULL;
    }

    memset(stats, 0, sizeof(*stats));
    if (!network->loaded) {
        return DS_ERR_EMPTY;
    }

    stats->line_count = network->line_count;
    stats->station_count = network->station_count;
    stats->segment_count = network->segment_count;
    stats->stop_count = network->stop_count;
    stats->node_count = network->node_count;

    for (i = 0; i < network->station_count; ++i) {
        if (network->stations[i].is_transfer) {
            ++stats->transfer_station_count;
        }
    }
    for (i = 0; i < network->line_count; ++i) {
        stats->total_line_km += network->lines[i].total_km;
    }

    stats->station_graph_connected =
        graph_is_connected(&network->station_graph);
    return DS_OK;
}

void metro_print_stats(const MetroNetwork *network)
{
    MetroStats stats;
    DsStatus status;

    if (network == NULL || !network->loaded) {
        printf("尚未加载地铁数据。\n");
        return;
    }

    status = metro_get_stats(network, &stats);
    if (status != DS_OK) {
        printf("统计失败：%s\n", ds_status_text(status));
        return;
    }

    printf("运营线路数：%zu\n", stats.line_count);
    printf("物理站点数：%zu\n", stats.station_count);
    printf("换乘站数  ：%zu\n", stats.transfer_station_count);
    printf("区段数    ：%zu\n", stats.segment_count);
    printf("停靠点记录：%zu\n", stats.stop_count);
    printf("线路-站点节点数：%zu\n", stats.node_count);
    printf("线路总里程：%.2f km\n", stats.total_line_km);
    printf("物理站点图连通：%s\n", stats.station_graph_connected ? "是" : "否");
}

DsStatus metro_print_lines(const MetroNetwork *network)
{
    size_t i;

    if (network == NULL) {
        return DS_ERR_NULL;
    }
    if (!network->loaded) {
        return DS_ERR_EMPTY;
    }

    printf("%-6s %-18s %8s %8s %10s\n",
           "编号", "线路", "站点数", "区段数", "里程(km)");
    printf("------------------------------------------------------------\n");
    for (i = 0; i < network->line_count; ++i) {
        const MetroLine *line = &network->lines[i];
        printf("%-6s %-18s %8d %8d %10.2f\n",
               line->id, line->name,
               line->station_count, line->segment_count, line->total_km);
    }
    return DS_OK;
}

DsStatus metro_print_line_detail(const MetroNetwork *network, int line)
{
    size_t i;
    double cumulative = 0.0;
    bool found = false;

    if (network == NULL) {
        return DS_ERR_NULL;
    }
    if (!network->loaded) {
        return DS_ERR_EMPTY;
    }
    if (line < 0 || (size_t)line >= network->line_count) {
        return DS_ERR_RANGE;
    }

    printf("线路：%s（%s）\n", network->lines[line].name,
           network->lines[line].id);
    printf("站点数：%d，区段数：%d，总里程：%.2f km\n",
           network->lines[line].station_count,
           network->lines[line].segment_count,
           network->lines[line].total_km);

    for (i = 0; i < network->segment_count; ++i) {
        const MetroSegment *segment = &network->segments[i];
        size_t k;

        if (segment->line != line) {
            continue;
        }
        found = true;
        printf("\n  区段 %d（%s）：\n", segment->segment_id,
               segment->loop ? "环线" : "普通");

        for (k = 0; k < segment->stop_count; ++k) {
            const MetroStop *stop = &network->stops[segment->first_stop + k];
            printf("  %2zu. %s", k + 1,
                   metro_station_name(network, stop->station));

            if (k + 1 < segment->stop_count) {
                printf("  --%.2f km / %.1f min-->",
                       stop->km_to_next, stop->minutes_to_next);
            } else if (segment->loop) {
                printf("  (环线回到起点)");
            }
            printf("\n");
            cumulative += stop->km_to_next;
        }
    }

    if (!found) {
        printf("该线路没有区段记录。\n");
    } else {
        printf("\n累计里程：%.2f km\n", cumulative);
    }
    return DS_OK;
}

DsStatus metro_print_station(const MetroNetwork *network, int station)
{
    size_t n;
    const MetroStation *s;

    if (network == NULL) {
        return DS_ERR_NULL;
    }
    if (!network->loaded) {
        return DS_ERR_EMPTY;
    }
    if (station < 0 || (size_t)station >= network->station_count) {
        return DS_ERR_RANGE;
    }

    s = &network->stations[station];
    printf("站点：%s（%s）\n", s->name, s->id);
    printf("换乘站：%s，换乘时间：%.1f 分钟\n",
           s->is_transfer ? "是" : "否", s->transfer_minutes);
    printf("所属线路：");
    for (n = 0; n < (size_t)s->line_count; ++n) {
        if (n > 0) {
            printf("、");
        }
        printf("%s", metro_line_name(network, s->lines[n]));
    }
    printf("\n");

    for (n = 0; n < network->node_count; ++n) {
        const MetroNode *node = &network->nodes[n];
        const GraphArc *arc;

        if (node->station != station) {
            continue;
        }
        printf("  %s 相邻节点：", metro_line_name(network, node->line));
        arc = network->time_graph.adj[n];
        if (arc == NULL) {
            printf("无");
        }
        while (arc != NULL) {
            double km = 0.0;

            if (network->distance_graph.matrix != NULL &&
                arc->to >= 0 &&
                arc->to < network->distance_graph.capacity) {
                km = network->distance_graph.matrix[
                    (size_t)n * (size_t)network->distance_graph.capacity +
                    (size_t)arc->to];
            }
            printf(" %s(%s) %.1fkm/%.1fmin",
                   metro_station_name(network, network->nodes[arc->to].station),
                   metro_line_name(network, network->nodes[arc->to].line),
                   km, arc->weight);
            arc = arc->next;
        }
        printf("\n");
    }
    return DS_OK;
}

DsStatus metro_print_transfers(const MetroNetwork *network)
{
    size_t i;
    size_t count = 0;

    if (network == NULL) {
        return DS_ERR_NULL;
    }
    if (!network->loaded) {
        return DS_ERR_EMPTY;
    }

    printf("%-12s %-8s %-6s %s\n", "换乘站", "编号", "线路数", "线路");
    printf("------------------------------------------------------------\n");
    for (i = 0; i < network->station_count; ++i) {
        const MetroStation *s = &network->stations[i];
        int k;

        if (!s->is_transfer) {
            continue;
        }
        ++count;
        printf("%-12s %-8s %-6d ", s->name, s->id, s->line_count);
        for (k = 0; k < s->line_count; ++k) {
            if (k > 0) {
                printf("、");
            }
            printf("%s", metro_line_name(network, s->lines[k]));
        }
        printf("\n");
    }
    printf("\n共 %zu 个换乘站。\n", count);
    return DS_OK;
}

DsStatus metro_print_adjlist(const MetroNetwork *network)
{
    size_t i;

    if (network == NULL) {
        return DS_ERR_NULL;
    }
    if (!network->loaded) {
        return DS_ERR_EMPTY;
    }

    for (i = 0; i < network->node_count; ++i) {
        const MetroNode *node = &network->nodes[i];
        const GraphArc *arc;

        printf("[%3zu] %s（%s）", i,
               metro_station_name(network, node->station),
               metro_line_name(network, node->line));
        arc = network->time_graph.adj[i];
        while (arc != NULL) {
            printf(" -> [%d]%s(%s)", arc->to,
                   metro_station_name(network, network->nodes[arc->to].station),
                   metro_line_name(network, network->nodes[arc->to].line));
            arc = arc->next;
        }
        printf("\n");
    }
    return DS_OK;
}

/* ------------------------------------------------------------------------- */
/* 票价与路径                                                                 */
/* ------------------------------------------------------------------------- */

int metro_fare(double distance_km)
{
    if (!isfinite(distance_km) || distance_km < 0.0) {
        return -1;
    }

    /* 同站进出、4 km 以内均为 2 元。 */
    if (distance_km <= 4.0) {
        return 2;
    }
    if (distance_km <= 12.0) {
        /* 4.0、8.0、12.0 是边界，减一个极小量避免边界多收 1 元。 */
        return 3 + (int)((distance_km - 4.0 - 1e-9) / 4.0);
    }
    if (distance_km <= 24.0) {
        return 5 + (int)((distance_km - 12.0 - 1e-9) / 6.0);
    }
    if (distance_km <= 40.0) {
        return 7 + (int)((distance_km - 24.0 - 1e-9) / 8.0);
    }
    if (distance_km <= 50.0) {
        return 9;
    }
    return 9 + (int)((distance_km - 50.0 + 1e-9) / 20.0);
}

void metro_path_init(MetroPath *path)
{
    if (path != NULL) {
        memset(path, 0, sizeof(*path));
        path->fare = -1;
    }
}

void metro_path_destroy(MetroPath *path)
{
    if (path == NULL) {
        return;
    }
    ds_free((void **)&path->nodes);
    memset(path, 0, sizeof(*path));
    path->fare = -1;
}

/**
 * @brief 从 prev 数组回溯 source->target 的节点路径。
 *
 * prev 是由 graph_dijkstra() 生成的“前驱”数组。回溯时从 target 出发
 * 走到 source，得到的是反向序列，因此最后要原地反转。只有真正走到
 * source 才算成功，否则说明 target 不可达或 prev 已损坏。
 */
static bool metro_reconstruct_path(const int *prev,
                                   int source,
                                   int target,
                                   int *buffer,
                                   size_t node_count,
                                   size_t *path_length)
{
    size_t length = 0;
    int node = target;

    while (node != -1 && length < node_count) {
        buffer[length++] = node;
        if (node == source) {
            break;
        }
        node = prev[node];
    }
    if (length == 0 || buffer[length - 1] != source) {
        return false;
    }

    for (size_t i = 0; i < length / 2; ++i) {
        int tmp = buffer[i];
        buffer[i] = buffer[length - 1 - i];
        buffer[length - 1 - i] = tmp;
    }
    *path_length = length;
    return true;
}

/**
 * @brief 按节点路径重新计算里程、时间和换乘次数。
 *
 * 不直接使用 Dijkstra 的 dist：不同优化目标选的是不同的图，只有
 * 重新在 time_graph / distance_graph 上逐边累加，才能保证输出给用户
 * 的四个指标互相自洽。换乘边表现为“同一物理站点、不同线路”。
 */
static void metro_measure_path(const MetroNetwork *network,
                               const int *nodes,
                               size_t node_count,
                               double *distance_km,
                               double *time_min,
                               int *transfers)
{
    size_t i;

    *distance_km = 0.0;
    *time_min = 0.0;
    *transfers = 0;
    for (i = 1; i < node_count; ++i) {
        int u = nodes[i - 1];
        int v = nodes[i];
        double weight = 0.0;

        if (graph_has_edge(&network->distance_graph, u, v, &weight)) {
            *distance_km += weight;
        }
        if (graph_has_edge(&network->time_graph, u, v, &weight)) {
            *time_min += weight;
        }
        if (network->nodes[u].station == network->nodes[v].station &&
            network->nodes[u].line != network->nodes[v].line) {
            ++*transfers;
        }
    }
}

/** @brief 判断候选路径是否优于当前最优路径（含确定性的平局规则）。 */
static bool metro_candidate_better(MetroCriterion criterion,
                                   double primary,
                                   double time_min,
                                   double distance_km,
                                   size_t length,
                                   double best_primary,
                                   double best_time,
                                   double best_distance,
                                   size_t best_length)
{
    const double eps = 1e-9;

    if (primary < best_primary - eps) {
        return true;
    }
    if (primary > best_primary + eps) {
        return false;
    }

    /*
     * 主目标相同后按“时间 -> 里程 -> 节点数”打破平局，保证同一份
     * 数据、同一次查询的结果稳定，不会因邻接表顺序不同而跳变。
     */
    if (criterion != METRO_FASTEST && time_min < best_time - eps) {
        return true;
    }
    if (criterion != METRO_FASTEST && time_min > best_time + eps) {
        return false;
    }
    if (criterion != METRO_SHORTEST && distance_km < best_distance - eps) {
        return true;
    }
    if (criterion != METRO_SHORTEST && distance_km > best_distance + eps) {
        return false;
    }
    return length < best_length;
}

DsStatus metro_plan(const MetroNetwork *network,
                    int from,
                    int to,
                    MetroCriterion criterion,
                    MetroPath *path)
{
    const Graph *graph;
    double *dist = NULL;
    int *prev = NULL;
    int *candidate = NULL;
    int *best = NULL;
    size_t best_length = 0;
    double best_primary = METRO_INF;
    double best_time = METRO_INF;
    double best_distance = METRO_INF;
    int best_transfers = 0;
    size_t s;
    size_t t;

    if (network == NULL || path == NULL) {
        return DS_ERR_NULL;
    }
    if (!network->loaded) {
        return DS_ERR_EMPTY;
    }
    if (from < 0 || to < 0 ||
        (size_t)from >= network->station_count ||
        (size_t)to >= network->station_count) {
        return DS_ERR_RANGE;
    }
    if (criterion < METRO_FASTEST || criterion > METRO_FEWEST_TRANSFERS) {
        return DS_ERR_RANGE;
    }

    switch (criterion) {
    case METRO_FASTEST:
        graph = &network->time_graph;
        break;
    case METRO_SHORTEST:
        graph = &network->distance_graph;
        break;
    case METRO_FEWEST_TRANSFERS:
    default:
        graph = &network->transfer_graph;
        break;
    }

    dist = (double *)ds_malloc(network->node_count * sizeof(*dist));
    prev = (int *)ds_malloc(network->node_count * sizeof(*prev));
    candidate = (int *)ds_malloc(network->node_count * sizeof(*candidate));
    best = (int *)ds_malloc(network->node_count * sizeof(*best));

    /*
     * 一个物理站点可能对应多条线路上的多个节点，所以不能只从一个
     * 节点出发。这里枚举所有起点节点，分别做一次 Dijkstra，再在所有
     * 终点节点中选最优组合。线路数很少（最多 8 条/站），代价可接受。
     */
    for (s = 0; s < network->node_count; ++s) {
        DsStatus status;

        if (network->nodes[s].station != from) {
            continue;
        }
        status = graph_dijkstra(graph, (int)s, dist, prev);
        if (status != DS_OK) {
            continue;
        }

        for (t = 0; t < network->node_count; ++t) {
            double primary;
            double distance_km;
            double time_min;
            int transfers;
            size_t length;

            if (network->nodes[t].station != to ||
                dist[t] >= DS_GRAPH_INF / 2.0) {
                continue;
            }
            if (!metro_reconstruct_path(prev, (int)s, (int)t, candidate,
                                        network->node_count, &length)) {
                continue;
            }

            metro_measure_path(network, candidate, length,
                               &distance_km, &time_min, &transfers);
            if (criterion == METRO_FEWEST_TRANSFERS) {
                primary = (double)transfers;
            } else if (criterion == METRO_FASTEST) {
                primary = time_min;
            } else {
                primary = distance_km;
            }

            if (metro_candidate_better(criterion, primary, time_min,
                                       distance_km, length, best_primary,
                                       best_time, best_distance, best_length)) {
                memcpy(best, candidate, length * sizeof(*best));
                best_length = length;
                best_primary = primary;
                best_time = time_min;
                best_distance = distance_km;
                best_transfers = transfers;
            }
        }
    }

    ds_free((void **)&dist);
    ds_free((void **)&prev);
    ds_free((void **)&candidate);

    if (best_length == 0) {
        ds_free((void **)&best);
        return DS_ERR_NOT_FOUND;
    }

    path->nodes = (int *)ds_malloc(best_length * sizeof(*path->nodes));
    memcpy(path->nodes, best, best_length * sizeof(*path->nodes));
    path->node_count = best_length;
    path->distance_km = best_distance;
    path->time_min = best_time;
    path->transfers = best_transfers;
    path->fare = metro_fare(best_distance);

    ds_free((void **)&best);
    return DS_OK;
}

void metro_print_path(const MetroNetwork *network, const MetroPath *path)
{
    size_t i;

    if (network == NULL || path == NULL) {
        printf("路径为空。\n");
        return;
    }
    if (path->nodes == NULL || path->node_count == 0) {
        printf("未找到可行路径。\n");
        return;
    }

    printf("路径：");
    for (i = 0; i < path->node_count; ++i) {
        int node_index = path->nodes[i];
        const MetroNode *node;

        if (node_index < 0 || (size_t)node_index >= network->node_count) {
            continue;
        }
        node = &network->nodes[node_index];
        if (i > 0) {
            int prev_index = path->nodes[i - 1];
            if (prev_index >= 0 && (size_t)prev_index < network->node_count &&
                network->nodes[prev_index].line != node->line) {
                printf("  [换乘]  ");
            } else {
                printf(" -> ");
            }
        }
        printf("%s(%s)", metro_station_name(network, node->station),
               metro_line_name(network, node->line));
    }
    printf("\n");
    printf("里程：%.2f km，时间：%.1f 分钟，换乘：%d 次，票价：",
           path->distance_km, path->time_min, path->transfers);
    if (path->fare < 0) {
        printf("未知\n");
    } else {
        printf("%d 元\n", path->fare);
    }
}

/* ------------------------------------------------------------------------- */
/* 图算法                                                                     */
/* ------------------------------------------------------------------------- */

DsStatus metro_dfs(const MetroNetwork *network,
                   int start,
                   MetroStationVisitFn visit,
                   void *context)
{
    MetroVisitBridge bridge;

    if (network == NULL) {
        return DS_ERR_NULL;
    }
    if (!network->loaded || network->station_graph.vertex_count == 0) {
        return DS_ERR_EMPTY;
    }
    if (start < 0 || (size_t)start >= network->station_count) {
        return DS_ERR_RANGE;
    }

    bridge.network = network;
    bridge.visit = visit;
    bridge.context = context;
    return graph_dfs(&network->station_graph, start,
                     metro_visit_bridge, &bridge);
}

DsStatus metro_bfs(const MetroNetwork *network,
                   int start,
                   MetroStationVisitFn visit,
                   void *context)
{
    MetroVisitBridge bridge;

    if (network == NULL) {
        return DS_ERR_NULL;
    }
    if (!network->loaded || network->station_graph.vertex_count == 0) {
        return DS_ERR_EMPTY;
    }
    if (start < 0 || (size_t)start >= network->station_count) {
        return DS_ERR_RANGE;
    }

    bridge.network = network;
    bridge.visit = visit;
    bridge.context = context;
    return graph_bfs(&network->station_graph, start,
                     metro_visit_bridge, &bridge);
}

DsStatus metro_mst(const MetroNetwork *network,
                   bool use_prim,
                   GraphEdge *edges,
                   int *edge_count,
                   double *total_weight)
{
    if (network == NULL || edges == NULL ||
        edge_count == NULL || total_weight == NULL) {
        return DS_ERR_NULL;
    }
    if (!network->loaded || network->station_count == 0) {
        return DS_ERR_EMPTY;
    }
    if (network->station_count < 2) {
        *edge_count = 0;
        *total_weight = 0.0;
        return DS_OK;
    }

    if (use_prim) {
        return graph_prim(&network->station_graph, 0,
                          edges, edge_count, total_weight);
    }
    return graph_kruskal(&network->station_graph,
                         edges, edge_count, total_weight);
}

DsStatus metro_validate(const MetroNetwork *network)
{
    size_t i;

    if (network == NULL) {
        return DS_ERR_NULL;
    }
    if (!network->loaded) {
        return DS_ERR_EMPTY;
    }
    if (network->line_count == 0 || network->station_count == 0 ||
        network->segment_count == 0 || network->stop_count == 0 ||
        network->node_count == 0) {
        return DS_ERR_FORMAT;
    }
    if (network->node_lookup == NULL) {
        return DS_ERR_FORMAT;
    }
    if (network->time_graph.vertex_count != (int)network->node_count ||
        network->distance_graph.vertex_count != (int)network->node_count ||
        network->transfer_graph.vertex_count != (int)network->node_count ||
        network->station_graph.vertex_count != (int)network->station_count) {
        return DS_ERR_FORMAT;
    }

    for (i = 0; i < network->line_count; ++i) {
        const MetroLine *line = &network->lines[i];

        if (line->station_count < 2 || line->segment_count < 1) {
            return DS_ERR_FORMAT;
        }
        if (line->total_km < 0.0 || !isfinite(line->total_km)) {
            return DS_ERR_FORMAT;
        }
    }

    for (i = 0; i < network->station_count; ++i) {
        const MetroStation *station = &network->stations[i];

        if (station->line_count < 1 ||
            station->line_count > METRO_MAX_LINES_PER_STATION) {
            return DS_ERR_FORMAT;
        }
        if (station->is_transfer != (station->line_count >= 2)) {
            return DS_ERR_FORMAT;
        }
    }

    for (i = 0; i < network->segment_count; ++i) {
        const MetroSegment *segment = &network->segments[i];
        size_t k;

        if (segment->line < 0 ||
            (size_t)segment->line >= network->line_count) {
            return DS_ERR_FORMAT;
        }
        if (segment->stop_count < 2 ||
            segment->first_stop + segment->stop_count > network->stop_count) {
            return DS_ERR_FORMAT;
        }
        for (k = 0; k < segment->stop_count; ++k) {
            const MetroStop *stop = &network->stops[segment->first_stop + k];

            if (stop->station < 0 ||
                (size_t)stop->station >= network->station_count) {
                return DS_ERR_FORMAT;
            }
            if (stop->km_to_next < 0.0 || !isfinite(stop->km_to_next) ||
                stop->minutes_to_next < 0.0 || !isfinite(stop->minutes_to_next)) {
                return DS_ERR_FORMAT;
            }
        }
    }

    /* 区段内禁止重复站点（环线的首尾闭合由 loop 标记，不重复存首站）。 */
    for (i = 0; i < network->segment_count; ++i) {
        const MetroSegment *segment = &network->segments[i];
        size_t a;
        size_t b;

        for (a = 0; a < segment->stop_count; ++a) {
            int station_a = network->stops[segment->first_stop + a].station;
            for (b = a + 1; b < segment->stop_count; ++b) {
                int station_b = network->stops[segment->first_stop + b].station;
                if (station_a == station_b) {
                    return DS_ERR_FORMAT;
                }
            }
        }
    }

    for (i = 0; i < network->node_count; ++i) {
        const MetroNode *node = &network->nodes[i];

        if (node->station < 0 ||
            (size_t)node->station >= network->station_count ||
            node->line < 0 || (size_t)node->line >= network->line_count) {
            return DS_ERR_FORMAT;
        }
        if (network->node_lookup[(size_t)node->line * network->station_count +
                                 (size_t)node->station] != (int)i) {
            return DS_ERR_FORMAT;
        }
    }

    if (!graph_is_connected(&network->station_graph)) {
        return DS_ERR_STATE;
    }
    return DS_OK;
}
