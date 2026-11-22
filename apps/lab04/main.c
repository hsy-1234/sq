/**
 * @file    main.c
 * @brief   题目四：查找与排序（100 条学生记录的多种算法对比）。
 *
 * 程序从 data/f1.txt 加载全部学生，随机抽取 100 条作为样本，并提供：
 *   - 冒泡排序、快速排序（按加权成绩降序）；
 *   - 顺序查找、折半查找（按学号）；
 *   - 链地址法哈希查找；
 *   - 顺序 / 折半 / 哈希三种查找的 ASL 成功与失败统计；
 *   - 7 种排序算法的比较次数、移动次数和耗时对比。
 *
 * 设计约定：
 *   - sample 保持随机顺序，用于顺序查找；
 *   - by_id 是样本按学号升序的副本，专门用于折半查找；
 *   - by_weight 是样本按加权降序的副本，排序功能不会破坏原始样本；
 *   - 哈希表在每次重新抽样后重建，保证三个查找结构的数据一致。
 */

#include "ds/ds_console.h"
#include "ds/ds_hash.h"
#include "ds/ds_input.h"
#include "ds/ds_memory.h"
#include "ds/ds_menu.h"
#include "ds/ds_path.h"
#include "ds/ds_search.h"
#include "ds/ds_sort.h"
#include "ds/ds_types.h"
#include "ds/ds_util.h"
#include "ds/student.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** 题目要求的样本规模。 */
#define LAB04_SAMPLE_SIZE 100

/** 哈希桶数：略大于样本数，装填因子接近 1。 */
#define LAB04_HASH_CAPACITY 101

/** 排序后展示前多少名。 */
#define LAB04_TOP_COUNT 10

/** ASL 失败统计使用的虚构学号个数。 */
#define LAB04_FAIL_COUNT 100

/** 应用上下文。 */
typedef struct Lab04App {
    Student *all;                    /**< f1.txt 全量记录。 */
    size_t all_count;                /**< 全量记录数。 */

    Student sample[LAB04_SAMPLE_SIZE];    /**< 随机样本，保持随机顺序。 */
    size_t sample_count;                  /**< 样本条数。 */

    Student by_id[LAB04_SAMPLE_SIZE];     /**< 按学号升序的副本。 */
    bool id_sorted;                       /**< by_id 是否已排序。 */

    Student by_weight[LAB04_SAMPLE_SIZE]; /**< 按加权成绩降序的副本。 */
    bool weight_sorted;                   /**< by_weight 是否已排序。 */

    StudentHash hash;                     /**< 样本的学号哈希表。 */
    bool hash_ready;                      /**< 哈希表是否可用。 */
} Lab04App;

/** @brief 若尚未抽样，输出提示并返回 false。 */
static bool ensure_sample(const Lab04App *app)
{
    if (app == NULL || app->sample_count == 0) {
        ds_console_error("还没有抽取样本，请先执行功能 1。");
        return false;
    }
    return true;
}

/** @brief 若哈希表未构建，输出提示并返回 false。 */
static bool ensure_hash(const Lab04App *app)
{
    if (!ensure_sample(app)) {
        return false;
    }
    if (!app->hash_ready) {
        ds_console_error("哈希表尚未构建成功，请重新执行功能 1。");
        return false;
    }
    return true;
}

/**
 * @brief 读取并校验 5 位学号，失败时循环重试。
 */
static bool read_id(char *id, size_t size)
{
    char *text;

    if (id == NULL || size < STUDENT_ID_SIZE) {
        return false;
    }

    for (;;) {
        if (ds_input_line("请输入 5 位学号: ", id, size) != DS_OK) {
            return false;
        }
        text = ds_str_trim(id);
        if (text != id) {
            memmove(id, text, strlen(text) + 1);
        }
        if (student_valid_id(id)) {
            return true;
        }
        ds_console_error("学号必须恰好是 5 位数字，请重新输入。");
    }
}

/**
 * @brief 建立按学号升序的副本，供折半查找使用。
 *
 * 使用库里的快速排序并忽略统计信息，避免在应用层再写一套排序。
 */
static void build_id_index(Lab04App *app)
{
    memcpy(app->by_id, app->sample, app->sample_count * sizeof(Student));
    ds_sort_quick(app->by_id, app->sample_count, sizeof(Student),
                  student_compare_id, NULL);
    app->id_sorted = ds_sort_is_sorted(app->by_id, app->sample_count,
                                       sizeof(Student), student_compare_id);
}

/**
 * @brief 重建样本的哈希表。
 *
 * 先销毁旧表，再按样本顺序插入；任何一步失败都保持 hash_ready=false，
 * 不影响顺序查找和折半查找继续使用。
 */
static DsStatus build_hash(Lab04App *app)
{
    size_t i;
    DsStatus status;

    if (app->hash_ready) {
        student_hash_destroy(&app->hash);
        app->hash_ready = false;
    }

    status = student_hash_init(&app->hash, LAB04_HASH_CAPACITY, NULL);
    if (status != DS_OK) {
        return status;
    }

    for (i = 0; i < app->sample_count; ++i) {
        status = student_hash_insert(&app->hash, app->sample[i]);
        if (status != DS_OK) {
            student_hash_destroy(&app->hash);
            return status;
        }
    }

    app->hash_ready = true;
    return DS_OK;
}

/**
 * @brief 选择随机种子。
 *
 * 默认使用当前时间；测试或写报告时可通过环境变量 LAB04_SEED 固定
 * 种子，便于复现同一组样本。这个入口不影响正常交互。
 */
static unsigned int choose_seed(void)
{
    const char *text = getenv("LAB04_SEED");

    if (text != NULL && *text != '\0') {
        return (unsigned int)strtoul(text, NULL, 10);
    }
    return (unsigned int)time(NULL);
}

/** @brief 打印排序后的前若干名。 */
static void print_top(const Student *items, size_t count)
{
    size_t limit = count < LAB04_TOP_COUNT ? count : LAB04_TOP_COUNT;
    size_t i;

    student_print_header();
    for (i = 0; i < limit; ++i) {
        student_print(&items[i]);
    }
    if (count > limit) {
        ds_console_info("仅显示前 %llu 名，共 %llu 条。",
                        (unsigned long long)limit, (unsigned long long)count);
    }
}

/**
 * @brief 菜单 1：随机抽取 100 条记录并重建查找结构。
 */
static void action_draw(void *context)
{
    Lab04App *app = (Lab04App *)context;
    size_t *indices;
    size_t i;
    unsigned int seed;
    DsStatus status;

    if (app == NULL) {
        return;
    }
    if (app->all_count < LAB04_SAMPLE_SIZE) {
        ds_console_error("f1.txt 只有 %llu 条记录，不足 100 条。",
                         (unsigned long long)app->all_count);
        return;
    }

    seed = choose_seed();
    ds_random_seed(seed);

    /*
     * 对下标数组洗牌，而不是直接洗牌 all：
     * 这样每次抽样都从原始文件顺序出发，固定 LAB04_SEED 时重复抽样
     * 得到同一组记录，也不会破坏 all 的原始顺序。
     */
    indices = (size_t *)ds_malloc(app->all_count * sizeof(size_t));
    for (i = 0; i < app->all_count; ++i) {
        indices[i] = i;
    }
    ds_shuffle(indices, app->all_count, sizeof(size_t));
    for (i = 0; i < LAB04_SAMPLE_SIZE; ++i) {
        app->sample[i] = app->all[indices[i]];
    }
    ds_free((void **)&indices);

    app->sample_count = LAB04_SAMPLE_SIZE;
    app->weight_sorted = false;

    build_id_index(app);
    status = build_hash(app);
    if (status != DS_OK) {
        ds_console_error("构建哈希表失败：%s。顺序/折半查找仍可使用。",
                         ds_status_text(status));
    }

    ds_console_success("已随机抽取 %llu 条记录（随机种子 %u）。",
                       (unsigned long long)app->sample_count, seed);
    printf("样本首条记录：\n");
    student_print_header();
    student_print(&app->sample[0]);
}

/**
 * @brief 菜单 2/3：按加权成绩降序排序样本副本。
 *
 * @param app   应用上下文。
 * @param quick true 使用快速排序，false 使用冒泡排序。
 */
static void sort_by_weight(Lab04App *app, bool quick)
{
    DsSortStats stats;
    const char *name = quick ? "快速排序" : "冒泡排序";

    if (!ensure_sample(app)) {
        return;
    }

    memcpy(app->by_weight, app->sample, app->sample_count * sizeof(Student));
    memset(&stats, 0, sizeof(stats));

    if (quick) {
        ds_sort_quick(app->by_weight, app->sample_count, sizeof(Student),
                      student_compare_weight_desc, &stats);
    } else {
        ds_sort_bubble(app->by_weight, app->sample_count, sizeof(Student),
                       student_compare_weight_desc, &stats);
    }

    if (!ds_sort_is_sorted(app->by_weight, app->sample_count, sizeof(Student),
                           student_compare_weight_desc)) {
        ds_console_error("%s结果未通过有序性校验。", name);
        return;
    }

    app->weight_sorted = true;
    ds_console_success("%s完成（按加权成绩降序）。", name);
    ds_sort_stats_print(name, &stats);
    ds_console_subtitle("加权成绩前 10 名");
    print_top(app->by_weight, app->sample_count);
}

/** @brief 菜单 2：冒泡排序。 */
static void action_bubble(void *context)
{
    sort_by_weight((Lab04App *)context, false);
}

/** @brief 菜单 3：快速排序。 */
static void action_quick(void *context)
{
    sort_by_weight((Lab04App *)context, true);
}

/**
 * @brief 输出一次查找成功的结果。
 */
static void print_found(const Student *student, size_t comparisons)
{
    ds_console_success("查找成功，比较次数 %llu。",
                       (unsigned long long)comparisons);
    student_print_header();
    student_print(student);
}

/** @brief 菜单 4：顺序查找（样本可以保持随机顺序）。 */
static void action_sequential(void *context)
{
    Lab04App *app = (Lab04App *)context;
    DsSearchResult result;
    char id[STUDENT_ID_SIZE];

    if (!ensure_sample(app) || !read_id(id, sizeof(id))) {
        return;
    }

    result = ds_search_sequential(id, app->sample, app->sample_count,
                                  sizeof(Student), student_compare_id_key);
    if (result.found) {
        print_found(&app->sample[result.index], result.comparisons);
    } else {
        ds_console_info("未找到学号 %s（比较 %llu 次）。", id,
                        (unsigned long long)result.comparisons);
    }
}

/** @brief 菜单 5：折半查找（使用按学号升序的副本）。 */
static void action_binary(void *context)
{
    Lab04App *app = (Lab04App *)context;
    DsSearchResult result;
    char id[STUDENT_ID_SIZE];

    if (!ensure_sample(app) || !read_id(id, sizeof(id))) {
        return;
    }
    if (!app->id_sorted) {
        build_id_index(app);
    }
    if (!app->id_sorted) {
        ds_console_error("学号索引排序失败，无法执行折半查找。");
        return;
    }

    result = ds_search_binary(id, app->by_id, app->sample_count,
                              sizeof(Student), student_compare_id_key);
    if (result.found) {
        print_found(&app->by_id[result.index], result.comparisons);
    } else {
        ds_console_info("未找到学号 %s（比较 %llu 次）。", id,
                        (unsigned long long)result.comparisons);
    }
}

/** @brief 菜单 6：哈希查找。 */
static void action_hash(void *context)
{
    Lab04App *app = (Lab04App *)context;
    Student found;
    size_t comparisons = 0;
    char id[STUDENT_ID_SIZE];
    DsStatus status;

    if (!ensure_hash(app) || !read_id(id, sizeof(id))) {
        return;
    }

    status = student_hash_search(&app->hash, id, &found, &comparisons);
    if (status == DS_OK) {
        print_found(&found, comparisons);
    } else if (status == DS_ERR_NOT_FOUND) {
        ds_console_info("未找到学号 %s（比较 %llu 次）。", id,
                        (unsigned long long)comparisons);
    } else {
        ds_console_error("哈希查找失败：%s。", ds_status_text(status));
    }
}

/**
 * @brief 计算顺序查找的成功 ASL（对样本中每个学号各查一次）。
 */
static double sequential_success_asl(const Lab04App *app)
{
    unsigned long long total = 0;
    size_t i;

    for (i = 0; i < app->sample_count; ++i) {
        DsSearchResult result = ds_search_sequential(
            app->sample[i].ID, app->sample, app->sample_count,
            sizeof(Student), student_compare_id_key);
        total += result.comparisons;
    }
    return (double)total / (double)app->sample_count;
}

/**
 * @brief 计算顺序查找的失败 ASL（使用 100 个不存在的学号）。
 */
static double sequential_failure_asl(const Lab04App *app)
{
    unsigned long long total = 0;
    size_t i;

    for (i = 0; i < LAB04_FAIL_COUNT; ++i) {
        char key[STUDENT_ID_SIZE];
        DsSearchResult result;

        snprintf(key, sizeof(key), "%05d", 90000 + (int)i);
        result = ds_search_sequential(key, app->sample, app->sample_count,
                                      sizeof(Student), student_compare_id_key);
        total += result.comparisons;
    }
    return (double)total / (double)LAB04_FAIL_COUNT;
}

/** @brief 计算折半查找的成功 ASL（按学号有序副本）。 */
static double binary_success_asl(const Lab04App *app)
{
    unsigned long long total = 0;
    size_t i;

    for (i = 0; i < app->sample_count; ++i) {
        DsSearchResult result = ds_search_binary(
            app->sample[i].ID, app->by_id, app->sample_count,
            sizeof(Student), student_compare_id_key);
        total += result.comparisons;
    }
    return (double)total / (double)app->sample_count;
}

/** @brief 计算折半查找的失败 ASL（使用 100 个不存在的学号）。 */
static double binary_failure_asl(const Lab04App *app)
{
    unsigned long long total = 0;
    size_t i;

    for (i = 0; i < LAB04_FAIL_COUNT; ++i) {
        char key[STUDENT_ID_SIZE];
        DsSearchResult result;

        snprintf(key, sizeof(key), "%05d", 90000 + (int)i);
        result = ds_search_binary(key, app->by_id, app->sample_count,
                                  sizeof(Student), student_compare_id_key);
        total += result.comparisons;
    }
    return (double)total / (double)LAB04_FAIL_COUNT;
}

/** @brief 计算哈希查找的成功 ASL（对样本中每个学号各查一次）。 */
static double hash_success_asl(const Lab04App *app)
{
    unsigned long long total = 0;
    size_t i;

    for (i = 0; i < app->sample_count; ++i) {
        size_t comparisons = 0;
        if (student_hash_search(&app->hash, app->sample[i].ID, NULL,
                                &comparisons) == DS_OK) {
            total += comparisons;
        }
    }
    return (double)total / (double)app->sample_count;
}

/** @brief 计算哈希查找的失败 ASL（使用 100 个不存在的学号）。 */
static double hash_failure_asl(const Lab04App *app)
{
    unsigned long long total = 0;
    size_t i;

    for (i = 0; i < LAB04_FAIL_COUNT; ++i) {
        char key[STUDENT_ID_SIZE];
        size_t comparisons = 0;

        snprintf(key, sizeof(key), "%05d", 90000 + (int)i);
        student_hash_search(&app->hash, key, NULL, &comparisons);
        total += comparisons;
    }
    return (double)total / (double)LAB04_FAIL_COUNT;
}

/** @brief 输出一行 ASL 表格。 */
static void print_asl_row(const char *method, double success, double failure,
                          const char *note)
{
    ds_console_print_cell(method, 16, DS_ALIGN_LEFT);
    printf("  %10.2f  %12.2f  %s\n", success, failure, note);
}

/** @brief 输出三种查找方法的 ASL 汇总。 */
static void print_asl_table(Lab04App *app)
{
    if (!ensure_hash(app)) {
        return;
    }
    if (!app->id_sorted) {
        build_id_index(app);
    }

    ds_console_subtitle("平均查找长度 ASL（样本 100 条）");
    ds_console_print_cell("查找方法", 16, DS_ALIGN_LEFT);
    printf("  ");
    ds_console_print_cell("ASL 成功", 10, DS_ALIGN_RIGHT);
    printf("  ");
    ds_console_print_cell("ASL 失败", 12, DS_ALIGN_RIGHT);
    printf("  说明\n");
    print_asl_row("顺序查找", sequential_success_asl(app),
                  sequential_failure_asl(app), "无序表，逐条比较");
    print_asl_row("折半查找", binary_success_asl(app),
                  binary_failure_asl(app), "按学号升序");
    print_asl_row("哈希查找", hash_success_asl(app),
                  hash_failure_asl(app), "101 桶，链地址法");
    printf("\n");
    student_hash_print_stats(&app->hash);
    ds_console_info("成功 ASL 对 100 个样本学号取平均；失败 ASL 对 100 个不存在的学号取平均。");
}

/** @brief 菜单 7：输出 ASL 成功/失败。 */
static void action_asl(void *context)
{
    print_asl_table((Lab04App *)context);
}

/**
 * @brief 菜单 8：7 种排序算法的性能对比。
 */
static void action_compare(void *context)
{
    typedef void (*SortFn)(void *, size_t, size_t, DsCompareFn, DsSortStats *);
    static const struct {
        const char *name;
        SortFn function;
    } algorithms[] = {
        { "冒泡排序", ds_sort_bubble },
        { "直接插入排序", ds_sort_insertion },
        { "简单选择排序", ds_sort_selection },
        { "希尔排序", ds_sort_shell },
        { "快速排序", ds_sort_quick },
        { "归并排序", ds_sort_merge },
        { "堆排序", ds_sort_heap }
    };
    Lab04App *app = (Lab04App *)context;
    Student work[LAB04_SAMPLE_SIZE];
    DsSortStats stats;
    size_t i;

    if (!ensure_sample(app)) {
        return;
    }

    ds_console_subtitle("7 种排序算法对比（100 条，按加权成绩降序）");
    for (i = 0; i < DS_ARRAY_LEN(algorithms); ++i) {
        memcpy(work, app->sample, app->sample_count * sizeof(Student));
        memset(&stats, 0, sizeof(stats));
        algorithms[i].function(work, app->sample_count, sizeof(Student),
                               student_compare_weight_desc, &stats);
        if (!ds_sort_is_sorted(work, app->sample_count, sizeof(Student),
                               student_compare_weight_desc)) {
            ds_console_error("%s结果未通过有序性校验。", algorithms[i].name);
            continue;
        }
        ds_sort_stats_print(algorithms[i].name, &stats);
    }
    ds_console_info("100 条数据规模较小，毫秒数仅供参考；比较/移动次数更能反映算法差异。");

    print_asl_table(app);
}

/** 菜单项表。 */
static DsMenuItem items[] = {
    { 1, "随机抽取 100 条学生记录", action_draw },
    { 2, "冒泡排序（加权降序）", action_bubble },
    { 3, "快速排序（加权降序）", action_quick },
    { 4, "顺序查找（按学号）", action_sequential },
    { 5, "折半查找（按学号）", action_binary },
    { 6, "哈希查找（按学号）", action_hash },
    { 7, "输出 ASL（成功/失败）", action_asl },
    { 8, "多种算法性能对比", action_compare }
};

static DsMenu menu = {
    "题目四：查找与排序",
    items,
    DS_ARRAY_LEN(items)
};

/**
 * @brief 程序入口。
 *
 * 启动时加载 f1.txt 全量数据；随机抽样由菜单 1 完成。退出时释放
 * 学生数组和哈希表。
 */
int main(void)
{
    Lab04App app;
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

    status = student_load_file("data/f1.txt", &app.all, &app.all_count);
    if (status != DS_OK) {
        ds_console_error("加载 data/f1.txt 失败：%s。", ds_status_text(status));
        ds_console_restore();
        return 1;
    }
    if (app.all_count < LAB04_SAMPLE_SIZE) {
        ds_console_error("学生记录不足 100 条，无法完成题目要求。");
        ds_free((void **)&app.all);
        ds_console_restore();
        return 1;
    }

    ds_menu_run(&menu, &app);

    student_hash_destroy(&app.hash);
    ds_free((void **)&app.all);
    ds_console_restore();
    return 0;
}
