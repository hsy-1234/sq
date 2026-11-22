/**
 * @file    main.c
 * @brief   题目一：线性表（学生信息管理，带头结点单链表）。
 *
 * 本程序把 data/f1.txt 中的全部学生记录装入单链表，并提供：
 *   - 尾插法 / 头插法建表；
 *   - 分页输出全部记录；
 *   - 按位置插入、求指定学号的前驱；
 *   - 按位置删除、按学号删除；
 *   - 按加权成绩降序排序；
 *   - 保存为 f1.txt 格式；
 *   - 顺序表与链表的建表、排序耗时及复杂度对比。
 *
 * 位置约定：菜单中的“第 i 个”统一使用 1 基下标，即第 1 个元素
 * 对应链表中的第一个数据结点。头结点只是哨兵，不参与计数。
 */

#include "ds/ds_console.h"
#include "ds/ds_input.h"
#include "ds/ds_linklist.h"
#include "ds/ds_memory.h"
#include "ds/ds_menu.h"
#include "ds/ds_path.h"
#include "ds/ds_seqlist.h"
#include "ds/ds_types.h"
#include "ds/ds_util.h"
#include "ds/student.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/** 原始数据文件；相对于工程根目录。 */
#define LAB01_DATA_PATH "data/f1.txt"

/** 默认保存路径，避免直接覆盖原始数据。 */
#define LAB01_SAVE_PATH "data/f1_lab01_out.txt"

/** 输出时的每页条数。 */
#define LAB01_PAGE_SIZE 20

/** 应用上下文：整个程序只维护一条学生链表。 */
typedef struct Lab01App {
    LinkList students;
} Lab01App;

/**
 * @brief 读取一个合法的 5 位数字学号。
 *
 * @param prompt 输入提示。
 * @param id     输出缓冲区，大小固定为 STUDENT_ID_SIZE。
 * @return 读取成功返回 true；输入结束（EOF）返回 false。
 *
 * 具体合法性规则由 student_valid_id 统一维护，交互层只负责提示和重试，
 * 从而与文件解析共用同一套校验标准。
 */
static bool read_id(const char *prompt, char *id)
{
    char line[32];

    for (;;) {
        char *text;

        if (ds_input_line(prompt, line, sizeof(line)) != DS_OK) {
            return false;
        }
        text = ds_str_trim(line);
        if (student_valid_id(text)) {
            ds_str_copy(id, STUDENT_ID_SIZE, text);
            return true;
        }
        ds_console_error("学号必须是 5 位数字，请重新输入。");
    }
}

/**
 * @brief 读取一个合法的学生姓名。
 *
 * @param name 输出缓冲区，大小固定为 STUDENT_NAME_SIZE。
 * @return 成功返回 true；EOF 返回 false。
 *
 * 姓名规则（非空、长度、禁止空白）由 student_valid_name 统一维护。
 */
static bool read_name(char *name)
{
    char line[64];

    for (;;) {
        char *text;

        if (ds_input_line("请输入姓名（不含空格）: ", line, sizeof(line)) != DS_OK) {
            return false;
        }
        text = ds_str_trim(line);
        if (student_valid_name(text)) {
            ds_str_copy(name, STUDENT_NAME_SIZE, text);
            return true;
        }
        ds_console_error("姓名需为 1-%d 个非空白字符。", STUDENT_NAME_SIZE - 1);
    }
}

/**
 * @brief 读取一个合法的 2 位专业编号。
 *
 * @param major 输出缓冲区，大小固定为 STUDENT_MAJOR_SIZE。
 * @return 成功返回 true；EOF 返回 false。
 */
static bool read_major(char *major)
{
    char line[16];

    for (;;) {
        char *text;

        if (ds_input_line("请输入专业编号（2 位数字）: ", line, sizeof(line)) != DS_OK) {
            return false;
        }
        text = ds_str_trim(line);
        if (student_valid_major(text)) {
            ds_str_copy(major, STUDENT_MAJOR_SIZE, text);
            return true;
        }
        ds_console_error("专业编号必须是 2 位数字，例如 13。");
    }
}

/**
 * @brief 依次读取 8 门课程成绩。
 *
 * @param student 学生记录，成绩写入其字段。
 * @return 成功返回 true；EOF 返回 false。
 *
 * 成绩通过 ds_input_int_range 读取，自动限制在 0..100，并在非法输入时
 * 重新提示。字段顺序必须与 f1.txt 一致：通识 3 门、公共基础 3 门、
 * 专业主干 2 门。
 */
static bool read_scores(Student *student)
{
    static const char *const labels[8] = {
        "通识1", "通识2", "通识3",
        "公共基础1", "公共基础2", "公共基础3",
        "专业主干1", "专业主干2"
    };
    int scores[8];
    size_t i;

    for (i = 0; i < DS_ARRAY_LEN(labels); ++i) {
        char prompt[64];

        snprintf(prompt, sizeof(prompt), "请输入%s成绩（0-100）: ", labels[i]);
        if (ds_input_int_range(prompt, 0, 100, &scores[i]) != DS_OK) {
            return false;
        }
    }

    student->GEC1 = (short)scores[0];
    student->GEC2 = (short)scores[1];
    student->GEC3 = (short)scores[2];
    student->PEC1 = (short)scores[3];
    student->PEC2 = (short)scores[4];
    student->PEC3 = (short)scores[5];
    student->CaIC1 = (short)scores[6];
    student->CaIC2 = (short)scores[7];
    return true;
}

/**
 * @brief 交互式录入一条新学生记录。
 *
 * @param list 当前链表，用于检查学号是否重复。
 * @param out  输出学生记录。
 * @return 成功返回 true；EOF 返回 false。
 *
 * 学号重复时只重新输入学号，不丢弃已经录入的其他字段；这样比整条记录
 * 重录更友好。所有字段读完后统一计算加权成绩。
 */
static bool input_new_student(const LinkList *list, Student *out)
{
    if (list == NULL || out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    for (;;) {
        if (!read_id("请输入学号（5 位数字）: ", out->ID)) {
            return false;
        }
        if (linklist_find_node(list, out->ID) == NULL) {
            break;
        }
        ds_console_error("学号 %s 已存在，请重新输入。", out->ID);
    }

    if (!read_name(out->name)) {
        return false;
    }
    if (!read_major(out->pro_number)) {
        return false;
    }
    if (!read_scores(out)) {
        return false;
    }

    student_update_weight(out);
    return true;
}

/**
 * @brief 从 data/f1.txt 重新建立链表。
 *
 * @param app         应用上下文。
 * @param head_insert true 使用头插法，false 使用尾插法。
 * @param out_count   可选输出参数，返回记录条数。
 * @return DS_OK；文件读取或结点分配失败时返回对应状态码。
 *
 * 先完整读取文件，再清空旧链表，避免文件读取失败时破坏当前数据。
 * 文件读取由 student_load_file 完成，批量建链由 linklist_assign 完成；
 * 本函数只负责选择头插/尾插模式并传递结果。头插法按文件顺序逐个头插，
 * 因此最终链表顺序与文件顺序相反；尾插法保持文件顺序。
 *
 * linklist_assign 内部先建新链再替换旧链，因此读取或建链失败时，
 * 原来的链表不会被破坏。时间复杂度 O(n)，空间复杂度 O(n)。
 */
static DsStatus rebuild_list(Lab01App *app, bool head_insert, size_t *out_count)
{
    Student *items = NULL;
    size_t count = 0;
    DsStatus status;

    if (app == NULL) {
        return DS_ERR_NULL;
    }

    status = student_load_file(LAB01_DATA_PATH, &items, &count);
    if (status != DS_OK) {
        return status;
    }

    status = linklist_assign(&app->students, items, count, head_insert);
    ds_free((void **)&items);
    if (status != DS_OK) {
        return status;
    }
    if (out_count != NULL) {
        *out_count = count;
    }
    return DS_OK;
}

/**
 * @brief 输出链表的首条记录，便于确认建表结果。
 *
 * @param list 链表。
 */
static void print_first_record(const LinkList *list)
{
    Student first;

    if (linklist_get(list, 1, &first) == DS_OK) {
        printf("首条记录：\n");
        student_print(&first);
    }
}

/** @brief 菜单功能：尾插法建表。 */
static void action_build_tail(void *context)
{
    Lab01App *app = (Lab01App *)context;
    size_t count = 0;
    DsStatus status;

    if (app == NULL) {
        return;
    }

    status = rebuild_list(app, false, &count);
    if (status != DS_OK) {
        ds_console_error("尾插法建表失败：%s。", ds_status_text(status));
        return;
    }
    if (count == 0) {
        ds_console_info("文件中没有有效记录。");
        return;
    }

    ds_console_success("尾插法建表完成，共 %llu 条记录。",
                       (unsigned long long)count);
    print_first_record(&app->students);
}

/** @brief 菜单功能：头插法建表。 */
static void action_build_head(void *context)
{
    Lab01App *app = (Lab01App *)context;
    size_t count = 0;
    DsStatus status;

    if (app == NULL) {
        return;
    }

    status = rebuild_list(app, true, &count);
    if (status != DS_OK) {
        ds_console_error("头插法建表失败：%s。", ds_status_text(status));
        return;
    }
    if (count == 0) {
        ds_console_info("文件中没有有效记录。");
        return;
    }

    ds_console_success("头插法建表完成，共 %llu 条记录。",
                       (unsigned long long)count);
    ds_console_info("头插法会使链表顺序与文件顺序相反。");
    print_first_record(&app->students);
}

/** @brief 菜单功能：分页输出全部学生。 */
static void action_print(void *context)
{
    Lab01App *app = (Lab01App *)context;

    if (app != NULL) {
        linklist_print(&app->students, LAB01_PAGE_SIZE);
    }
}

/** @brief 菜单功能：在第 i 个位置插入一条新记录。 */
static void action_insert(void *context)
{
    Lab01App *app = (Lab01App *)context;
    Student student;
    DsStatus status;
    char prompt[96];
    int max_position;
    int position = 0;

    if (app == NULL) {
        return;
    }
    if (app->students.length >= (size_t)INT_MAX) {
        ds_console_error("链表长度超出可表示范围，无法继续插入。");
        return;
    }

    max_position = (int)app->students.length + 1;
    snprintf(prompt, sizeof(prompt), "请输入插入位置（1-%d）: ", max_position);
    if (ds_input_int_range(prompt, 1, max_position, &position) != DS_OK) {
        return;
    }
    if (!input_new_student(&app->students, &student)) {
        return;
    }

    status = linklist_insert(&app->students, (size_t)position, student);
    if (status != DS_OK) {
        ds_console_error("插入失败：%s。", ds_status_text(status));
        return;
    }

    ds_console_success("已在第 %d 个位置插入学生，当前共 %llu 条记录。",
                       position, (unsigned long long)app->students.length);
    student_print(&student);
}

/** @brief 菜单功能：求指定学号学生的前驱。 */
static void action_predecessor(void *context)
{
    Lab01App *app = (Lab01App *)context;
    Student predecessor;
    DsStatus status;
    char id[STUDENT_ID_SIZE];

    if (app == NULL) {
        return;
    }
    if (app->students.length == 0) {
        ds_console_info("链表为空，没有前驱可求。");
        return;
    }
    if (!read_id("请输入要查询的学号: ", id)) {
        return;
    }

    status = linklist_predecessor(&app->students, id, &predecessor);
    if (status == DS_ERR_RANGE) {
        ds_console_info("学号为 %s 的学生是首元素，没有前驱。", id);
    } else if (status == DS_ERR_NOT_FOUND) {
        ds_console_error("未找到学号为 %s 的学生。", id);
    } else if (status != DS_OK) {
        ds_console_error("求前驱失败：%s。", ds_status_text(status));
    } else {
        printf("学号 %s 的前驱为：\n", id);
        student_print(&predecessor);
    }
}

/** @brief 菜单功能：按位置删除学生。 */
static void action_delete_at(void *context)
{
    Lab01App *app = (Lab01App *)context;
    Student removed;
    DsStatus status;
    char prompt[80];
    int position = 0;

    if (app == NULL) {
        return;
    }
    if (app->students.length == 0) {
        ds_console_info("链表为空，没有记录可删除。");
        return;
    }
    if (app->students.length > (size_t)INT_MAX) {
        ds_console_error("链表长度超出可表示范围。");
        return;
    }

    snprintf(prompt, sizeof(prompt), "请输入删除位置（1-%llu）: ",
             (unsigned long long)app->students.length);
    if (ds_input_int_range(prompt, 1, (int)app->students.length, &position) != DS_OK) {
        return;
    }

    status = linklist_delete_at(&app->students, (size_t)position, &removed);
    if (status != DS_OK) {
        ds_console_error("按位置删除失败：%s。", ds_status_text(status));
        return;
    }

    ds_console_success("已删除第 %d 个学生，剩余 %llu 条记录。",
                       position, (unsigned long long)app->students.length);
    student_print(&removed);
}

/** @brief 菜单功能：按学号删除学生。 */
static void action_delete_by_id(void *context)
{
    Lab01App *app = (Lab01App *)context;
    Student removed;
    DsStatus status;
    char id[STUDENT_ID_SIZE];

    if (app == NULL) {
        return;
    }
    if (app->students.length == 0) {
        ds_console_info("链表为空，没有记录可删除。");
        return;
    }
    if (!read_id("请输入要删除的学号: ", id)) {
        return;
    }

    status = linklist_delete_by_id(&app->students, id, &removed);
    if (status == DS_ERR_NOT_FOUND) {
        ds_console_error("未找到学号为 %s 的学生。", id);
    } else if (status != DS_OK) {
        ds_console_error("按学号删除失败：%s。", ds_status_text(status));
    } else {
        ds_console_success("已删除学号为 %s 的学生，剩余 %llu 条记录。",
                           id, (unsigned long long)app->students.length);
        student_print(&removed);
    }
}

/** @brief 菜单功能：按加权成绩降序排序。 */
static void action_sort(void *context)
{
    Lab01App *app = (Lab01App *)context;
    Student first;
    Student last;
    double elapsed;

    if (app == NULL) {
        return;
    }
    if (app->students.length < 2) {
        ds_console_info("记录少于 2 条，无需排序。");
        return;
    }

    elapsed = ds_clock_ms();
    linklist_sort_by_weight_desc(&app->students);
    elapsed = ds_clock_ms() - elapsed;

    ds_console_success("按加权成绩降序排序完成，用时 %.3f ms。", elapsed);
    if (linklist_get(&app->students, 1, &first) == DS_OK) {
        printf("第 1 名：\n");
        student_print(&first);
    }
    if (linklist_get(&app->students, app->students.length, &last) == DS_OK) {
        printf("第 %llu 名：\n", (unsigned long long)app->students.length);
        student_print(&last);
    }
}

/** @brief 菜单功能：保存当前链表。 */
static void action_save(void *context)
{
    Lab01App *app = (Lab01App *)context;
    char path[260];
    char *text;
    const char *target;
    DsStatus status;

    if (app == NULL) {
        return;
    }

    if (ds_input_line("请输入保存路径（直接回车使用 data/f1_lab01_out.txt）: ",
                      path, sizeof(path)) != DS_OK) {
        return;
    }
    text = ds_str_trim(path);
    target = (text[0] != '\0') ? text : LAB01_SAVE_PATH;

    status = linklist_save_file(&app->students, target);
    if (status == DS_OK) {
        ds_console_success("已保存 %llu 条记录到 %s。",
                           (unsigned long long)app->students.length, target);
    } else {
        ds_console_error("保存失败：%s。", ds_status_text(status));
    }
}

/**
 * @brief 输出一行对比表格。
 *
 * @param label     第一列文字。
 * @param seq_text  顺序表一列文字。
 * @param list_text 链表一列文字。
 *
 * 使用 ds_console_print_cell 按显示宽度补空格，中文列名也能对齐。
 */
static void print_compare_row(const char *label, const char *seq_text,
                              const char *list_text)
{
    ds_console_print_cell(label, 18, DS_ALIGN_LEFT);
    ds_console_print_cell(seq_text, 16, DS_ALIGN_RIGHT);
    ds_console_print_cell(list_text, 16, DS_ALIGN_RIGHT);
    putchar('\n');
}

/**
 * @brief 菜单功能：顺序表与链表的实现对比。
 *
 * 用同一批 f1.txt 数据分别装入顺序表和链表，记录建表、排序耗时，并
 * 检查排序后的记录数和首名加权成绩是否一致。对比重点不是绝对毫秒数
 * （数据规模只有 4615 条），而是两种存储结构在操作方式上的差异：
 *   - 顺序表支持 O(1) 随机访问，插入/删除平均需要搬移一半元素；
 *   - 链表按位置访问需要 O(n) 遍历，已知前驱时插入/删除只需改指针。
 *
 * 两边都使用库提供的批量建表接口（seqlist_assign / linklist_assign），
 * 因而建表都是 O(n)，避免把“逐条尾插 O(n^2)”的实现细节重复写在应用层。
 */
static void action_compare(void *context)
{
    Student *items = NULL;
    size_t count = 0;
    SeqList seq;
    LinkList linked;
    DsStatus status;
    double started;
    double seq_build = 0.0;
    double list_build = 0.0;
    double seq_sort = 0.0;
    double list_sort = 0.0;
    bool seq_ready = false;
    bool list_ready = false;
    char seq_buffer[32];
    char list_buffer[32];
    float difference;

    DS_UNUSED(context);
    memset(&seq, 0, sizeof(seq));
    memset(&linked, 0, sizeof(linked));

    ds_console_subtitle("顺序表与链表：同一批数据的实现对比");
    status = student_load_file(LAB01_DATA_PATH, &items, &count);
    if (status != DS_OK) {
        ds_console_error("读取数据失败：%s。", ds_status_text(status));
        goto cleanup;
    }
    if (count == 0) {
        ds_console_info("文件中没有有效记录，无法对比。");
        goto cleanup;
    }

    /* 1. 顺序表：一次性拷贝，O(n)。 */
    status = seqlist_init(&seq, count);
    if (status != DS_OK) {
        ds_console_error("初始化顺序表失败：%s。", ds_status_text(status));
        goto cleanup;
    }
    seq_ready = true;
    started = ds_clock_ms();
    status = seqlist_assign(&seq, items, count);
    seq_build = ds_clock_ms() - started;
    if (status != DS_OK) {
        ds_console_error("顺序表建表失败：%s。", ds_status_text(status));
        goto cleanup;
    }

    /* 2. 链表：批量尾插，内部维护尾指针，O(n)。 */
    status = linklist_init(&linked);
    if (status != DS_OK) {
        ds_console_error("初始化链表失败：%s。", ds_status_text(status));
        goto cleanup;
    }
    list_ready = true;
    started = ds_clock_ms();
    status = linklist_assign(&linked, items, count, false);
    list_build = ds_clock_ms() - started;
    if (status != DS_OK) {
        ds_console_error("链表建表失败：%s。", ds_status_text(status));
        goto cleanup;
    }

    /* 3. 两种结构使用同一个比较函数，排序结果应完全一致。 */
    started = ds_clock_ms();
    seqlist_sort_by_weight_desc(&seq);
    seq_sort = ds_clock_ms() - started;

    started = ds_clock_ms();
    linklist_sort_by_weight_desc(&linked);
    list_sort = ds_clock_ms() - started;

    if (seq.length != linked.length || linked.head->next == NULL) {
        ds_console_error("两种结构的记录数不一致，对比失败。");
        goto cleanup;
    }
    difference = seq.data[0].weights - linked.head->next->data.weights;
    if (difference < -0.001f || difference > 0.001f) {
        ds_console_error("两种结构的首名加权成绩不一致，对比失败。");
        goto cleanup;
    }

    /* 4. 输出对比表。毫秒列用固定 3 位小数，便于观察相对差异。 */
    print_compare_row("操作", "顺序表", "链表");
    ds_console_rule('-', 50);
    snprintf(seq_buffer, sizeof(seq_buffer), "%.3f", seq_build);
    snprintf(list_buffer, sizeof(list_buffer), "%.3f", list_build);
    print_compare_row("建表耗时(ms)", seq_buffer, list_buffer);
    snprintf(seq_buffer, sizeof(seq_buffer), "%.3f", seq_sort);
    snprintf(list_buffer, sizeof(list_buffer), "%.3f", list_sort);
    print_compare_row("排序耗时(ms)", seq_buffer, list_buffer);
    print_compare_row("按位置访问", "O(1)", "O(n)");
    print_compare_row("插入/删除", "O(n)", "O(n)");
    print_compare_row("额外空间", "O(n)", "O(n)");

    printf("两种结构均为 %llu 条记录，排序后首名加权成绩一致。\n",
           (unsigned long long)seq.length);
    printf("顺序表第 1 名：\n");
    student_print(&seq.data[0]);
    printf("链表第 1 名：\n");
    student_print(&linked.head->next->data);

cleanup:
    if (list_ready) {
        linklist_destroy(&linked);
    }
    if (seq_ready) {
        seqlist_destroy(&seq);
    }
    ds_free((void **)&items);
}

/** 菜单项表。所有功能均已实现，不再保留 NULL 动作。 */
static DsMenuItem items[] = {
    { 1, "建立链表（尾插法，读取 data/f1.txt）", action_build_tail },
    { 2, "建立链表（头插法，结果与文件顺序相反）", action_build_head },
    { 3, "输出全部学生", action_print },
    { 4, "在第 i 个位置插入学生", action_insert },
    { 5, "求指定学号的前驱", action_predecessor },
    { 6, "按位置删除学生", action_delete_at },
    { 7, "按学号删除学生", action_delete_by_id },
    { 8, "按加权成绩降序排序", action_sort },
    { 9, "保存到文件", action_save },
    { 10, "顺序表与链表实现对比", action_compare }
};

static DsMenu menu = {
    "题目一：线性表（学生信息管理）",
    items,
    DS_ARRAY_LEN(items)
};

/**
 * @brief 程序入口。
 *
 * 初始化控制台和空链表后进入菜单循环；退出时释放全部结点并恢复
 * 控制台代码页，保证不留下内存或终端状态问题。
 */
int main(void)
{
    Lab01App app;
    DsStatus status;

    ds_console_init();

    /*
     * 用户可能直接在 build 或 build/bin 下双击 exe；此时当前目录不是
     * 工程根目录，data/f1.txt 会找不到。先统一切换到包含 data 的
     * 工程根目录，后续菜单中的相对路径就与启动位置无关了。
     */
    status = ds_path_set_project_root();
    if (status != DS_OK) {
        ds_console_error("无法定位工程根目录：%s。请从工程目录内运行程序。",
                         ds_status_text(status));
        ds_console_restore();
        return 1;
    }

    status = linklist_init(&app.students);
    if (status != DS_OK) {
        ds_console_error("初始化链表失败：%s。", ds_status_text(status));
        ds_console_restore();
        return 1;
    }

    ds_menu_run(&menu, &app);

    linklist_destroy(&app.students);
    ds_console_restore();
    return 0;
}
