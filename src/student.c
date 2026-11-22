/**
 * @file    student.c
 * @brief   学生记录的数据模型、文件读写与表格输出。
 *
 * f1.txt 每行 11 个字段，顺序固定：
 *
 *     学号 姓名 专业编号 通识1 通识2 通识3 公共基础1 公共基础2
 *     公共基础3 专业主干1 专业主干2
 *
 * 例如：
 *     00002 hqghum 13 85 79 90 88 76 81 92 95
 *
 * 加权成绩按题目给定公式计算：
 *
 *     weights = 0.8 * (公共基础1 + 公共基础2 + 公共基础3)
 *             + 1.2 * (专业主干1 + 专业主干2)
 *
 * 满分 0.8*300 + 1.2*200 = 480。通识课成绩只参与展示，不参与加权。
 * 结构体字段沿用骨架命名：GEC* 通识，PEC* 公共基础，CaIC* 专业主干。
 */
#include "ds/student.h"
#include "ds/ds_console.h"
#include "ds/ds_memory.h"
#include "ds/ds_util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 表头 12 列加列间空格后的总显示宽度，用于画分隔线。 */
#define STUDENT_TABLE_WIDTH 72

/**
 * @brief 按公式重新计算一名学生的加权成绩。
 *
 * @param student 学生记录；为 NULL 时直接返回。
 *
 * 先转成 double 再乘系数，最后存回 float 的 weights 字段，避免
 * “整数先相加再乘 0.8”时丢掉小数部分。
 */
void student_update_weight(Student *student)
{
    if (student == NULL) {
        return;
    }
    student->weights = (float)(0.8 * ((double)student->PEC1 + student->PEC2 +
                                      student->PEC3) +
                               1.2 * ((double)student->CaIC1 + student->CaIC2));
}

/**
 * @brief 读取加权成绩。
 *
 * @param student 学生记录；为 NULL 时返回 0。
 * @return weights 字段的值。
 *
 * 提供这个 getter 是为了让上层代码不必直接访问字段，也便于以后把
 * float 改成 double 时只改一处。
 */
float student_weight(const Student *student)
{
    return student != NULL ? student->weights : 0.0f;
}

/**
 * @brief 判断成绩是否在 0..100 内。
 *
 * @param score 成绩。
 * @return 合法返回 true。
 */
static bool score_in_range(int score)
{
    return score >= 0 && score <= 100;
}

/**
 * @brief 解析一行 f1.txt 格式的学生记录。
 *
 * @param line 以 '\0' 结尾的一行文本（可含首尾空白）。
 * @param out  接收解析结果；函数会先把它清零。
 * @return 解析成功返回 true；字段缺失、类型错误、成绩越界或行尾有垃圾
 *         返回 false。
 *
 * 解析步骤：
 *   1. 用 sscanf 的宽度限制读入 3 个字符串和 8 个整数，%n 记录已消费
 *      的字符数；
 *   2. sscanf 返回 11 表示 11 个字段都成功转换（%n 不计入返回值）；
 *   3. 跳过 %n 之后的空白，若还有非空白字符，说明行尾有多余内容，拒绝；
 *   4. 校验学号恰好 5 位、姓名/专业非空、8 门成绩都在 0..100；
 *   5. 拷贝到输出结构体并计算加权成绩。
 *
 * 宽度限制（%5s/%15s/%3s）与结构体数组大小严格对应，既防止缓冲区
 * 溢出，也保证超长字段会被拆成多个字段从而被拒绝。
 */
bool student_parse_line(const char *line, Student *out)
{
    char id[STUDENT_ID_SIZE];
    char name[STUDENT_NAME_SIZE];
    char major[STUDENT_MAJOR_SIZE];
    int g1, g2, g3;
    int p1, p2, p3;
    int c1, c2;
    int consumed = 0;
    int fields;

    if (line == NULL || out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    fields = sscanf(line, "%5s %15s %3s %d %d %d %d %d %d %d %d %n",
                    id, name, major, &g1, &g2, &g3, &p1, &p2, &p3, &c1, &c2,
                    &consumed);
    if (fields != 11) {
        return false;
    }
    /* %n 停在最后一个整数之后，跳过行尾空白后必须已经到 '\0'。 */
    while (line[consumed] != '\0' && isspace((unsigned char)line[consumed])) {
        ++consumed;
    }
    if (line[consumed] != '\0') {
        return false;
    }
    if (strlen(id) != 5 || name[0] == '\0' || major[0] == '\0') {
        return false;
    }
    if (!score_in_range(g1) || !score_in_range(g2) || !score_in_range(g3) ||
        !score_in_range(p1) || !score_in_range(p2) || !score_in_range(p3) ||
        !score_in_range(c1) || !score_in_range(c2)) {
        return false;
    }

    ds_str_copy(out->ID, sizeof(out->ID), id);
    ds_str_copy(out->name, sizeof(out->name), name);
    ds_str_copy(out->pro_number, sizeof(out->pro_number), major);
    out->GEC1 = (short)g1;
    out->GEC2 = (short)g2;
    out->GEC3 = (short)g3;
    out->PEC1 = (short)p1;
    out->PEC2 = (short)p2;
    out->PEC3 = (short)p3;
    out->CaIC1 = (short)c1;
    out->CaIC2 = (short)c2;
    student_update_weight(out);
    return true;
}

/**
 * @brief 打印一个左对齐表头单元格，并追加一个空格分隔列。
 *
 * @param text  表头文本。
 * @param width 显示宽度。
 */
static void print_header_cell(const char *text, int width)
{
    ds_console_print_cell(text, width, DS_ALIGN_LEFT);
    putchar(' ');
}

/**
 * @brief 打印一个右对齐的数字单元格，并追加一个空格分隔列。
 *
 * @param value 数字。
 * @param width 显示宽度。
 *
 * 先格式化成字符串，再复用 ds_console_print_cell，保证和表头使用同一
 * 套宽度计算逻辑。
 */
static void print_number_cell(int value, int width)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    ds_console_print_cell(buffer, width, DS_ALIGN_RIGHT);
    putchar(' ');
}

/**
 * @brief 打印学生表格的表头与分隔线。
 *
 * 列宽按内容估算：学号 6、姓名 10、专业 4、8 门成绩各 4、加权 8。
 * 中文列名按 2 列宽计算，因此“学号”“姓名”等能对齐。
 */
void student_print_header(void)
{
    print_header_cell("学号", 6);
    print_header_cell("姓名", 10);
    print_header_cell("专业", 4);
    print_header_cell("通1", 4);
    print_header_cell("通2", 4);
    print_header_cell("通3", 4);
    print_header_cell("公1", 4);
    print_header_cell("公2", 4);
    print_header_cell("公3", 4);
    print_header_cell("主1", 4);
    print_header_cell("主2", 4);
    ds_console_print_cell("加权", 8, DS_ALIGN_RIGHT);
    putchar('\n');
    ds_console_rule('-', STUDENT_TABLE_WIDTH);
}

/**
 * @brief 打印一名学生的完整记录。
 *
 * @param student 学生记录；为 NULL 时直接返回。
 *
 * 字符串左对齐，成绩和加权右对齐；加权保留两位小数。
 */
void student_print(const Student *student)
{
    char weight[16];

    if (student == NULL) {
        return;
    }

    ds_console_print_cell(student->ID, 6, DS_ALIGN_LEFT);
    putchar(' ');
    ds_console_print_cell(student->name, 10, DS_ALIGN_LEFT);
    putchar(' ');
    ds_console_print_cell(student->pro_number, 4, DS_ALIGN_LEFT);
    putchar(' ');
    print_number_cell(student->GEC1, 4);
    print_number_cell(student->GEC2, 4);
    print_number_cell(student->GEC3, 4);
    print_number_cell(student->PEC1, 4);
    print_number_cell(student->PEC2, 4);
    print_number_cell(student->PEC3, 4);
    print_number_cell(student->CaIC1, 4);
    print_number_cell(student->CaIC2, 4);
    snprintf(weight, sizeof(weight), "%.2f", (double)student->weights);
    ds_console_print_cell(weight, 8, DS_ALIGN_RIGHT);
    putchar('\n');
}

/**
 * @brief 分页打印学生记录列表。
 *
 * @param items     记录数组；为 NULL 或 count 为 0 时只打印提示。
 * @param count     记录条数。
 * @param page_size 每页条数；0 表示不分页。
 *
 * 每页打印完暂停并重新打印表头，避免滚动后看不到列名。最后一页不再
 * 暂停。页大小 26 与骨架程序的 DispList 保持一致。
 */
void student_print_list(const Student *items, size_t count, size_t page_size)
{
    size_t i;

    if (items == NULL || count == 0) {
        ds_console_info("没有可显示的学生记录。");
        return;
    }

    student_print_header();
    for (i = 0; i < count; ++i) {
        student_print(&items[i]);
        if (page_size > 0 && (i + 1) % page_size == 0 && i + 1 < count) {
            ds_console_pause("按回车键继续显示下一页...");
            student_print_header();
        }
    }
    printf("共 %llu 条记录。\n", (unsigned long long)count);
}

/**
 * @brief 按加权成绩降序比较两名学生。
 *
 * @param a 指向 Student 的指针。
 * @param b 指向 Student 的指针。
 * @return 负数表示 a 排在 b 前；正数表示 a 排在 b 后；0 表示完全相等。
 *
 * 权重相同时按学号升序，保证 qsort 结果稳定、可复现（qsort 本身不
 * 稳定，这里用学号作为第二关键字消除歧义）。
 */
int student_compare_weight_desc(const void *a, const void *b)
{
    const Student *left = (const Student *)a;
    const Student *right = (const Student *)b;

    if (left->weights > right->weights) {
        return -1;  //左边权重大，排在前面
    }
    if (left->weights < right->weights) {
        return 1;  //右边权重大，排在前面
    }
    return strcmp(left->ID, right->ID);  //权重相同，按学号升序
}

/**
 * @brief 按加权成绩升序比较两名学生。
 *
 * @param a 指向 Student 的指针。
 * @param b 指向 Student 的指针。
 * @return 负数/正数/0 的含义同 qsort 约定。
 *
 * 注意不要用“返回 desc 结果的相反数”实现：那样同分时学号顺序也会
 * 反过来，且 INT_MIN 取负会溢出。这里显式比较两个字段。
 */
int student_compare_weight_asc(const void *a, const void *b)
{
    const Student *left = (const Student *)a;
    const Student *right = (const Student *)b;

    if (left->weights < right->weights) {
        return -1;
    }
    if (left->weights > right->weights) {
        return 1;
    }
    return strcmp(left->ID, right->ID);
}

/**
 * @brief 按学号升序比较两名学生。
 *
 * @param a 指向 Student 的指针。
 * @param b 指向 Student 的指针。
 * @return strcmp 的结果。
 *
 * 学号是等长数字串，字典序与数值序一致，因此不需要转成整数。
 */
int student_compare_id(const void *a, const void *b)
{
    const Student *left = (const Student *)a;
    const Student *right = (const Student *)b;

    return strcmp(left->ID, right->ID);
}

/**
 * @brief 折半查找用的“关键字 vs 元素”比较函数。
 *
 * @param key     指向学号字符串的指针。
 * @param element 指向 Student 的指针。
 * @return key 与 element->ID 的 strcmp 结果。
 *
 * ds_search_binary 的约定是 cmp(key, element)，因此参数顺序不能反。
 */
int student_compare_id_key(const void *key, const void *element)
{
    const char *id = (const char *)key;
    const Student *student = (const Student *)element;

    return strcmp(id, student->ID);
}

/**
 * @brief 从文本文件加载全部学生记录。
 *
 * @param path      文件路径。
 * @param out_items 接收动态数组；成功时由调用者负责 ds_free。
 * @param out_count 接收记录条数。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；打不开文件返回 DS_ERR_IO；
 *         一条合法记录都没有返回 DS_ERR_FORMAT。
 *
 * 读取策略：
 *   - 初始分配 256 条，满了以后容量翻倍；
 *   - 跳过空行和以 '#' 开头的注释行；
 *   - 解析失败的行计入 invalid，最后在 stderr 给出警告，不中断加载；
 *   - 读完后把数组缩到恰好 count 条，避免长期占用多余内存。
 *
 * 限制：单行缓冲区为 256 字节；f1.txt 每行约 50 字节，足够。若某行
 * 超过 255 字节，fgets 会把它拆成两段，两段都会解析失败并计入警告。
 *
 * 时间复杂度 O(文件大小)，空间复杂度 O(count)。
 */
DsStatus student_load_file(const char *path, Student **out_items, size_t *out_count)
{
    FILE *file;
    char line[256];
    Student *items;
    size_t capacity = 256;
    size_t count = 0;
    size_t invalid = 0;

    if (path == NULL || out_items == NULL || out_count == NULL) {
        return DS_ERR_NULL;
    }
    *out_items = NULL;
    *out_count = 0;

    file = fopen(path, "r");
    if (file == NULL) {
        return DS_ERR_IO;
    }

    items = (Student *)ds_malloc(capacity * sizeof(Student));
    while (fgets(line, (int)sizeof(line), file) != NULL) {
        Student student;

        ds_str_trim(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (!student_parse_line(line, &student)) {
            ++invalid;
            continue;
        }
        if (count == capacity) {
            /* 翻倍扩容；ds_realloc 失败会直接终止，不会丢原数组。 */
            capacity *= 2;
            items = (Student *)ds_realloc(items, capacity * sizeof(Student));
        }
        items[count++] = student;
    }
    fclose(file);

    if (invalid > 0) {
        fprintf(stderr, "[警告] %s 中有 %llu 行格式不合法，已跳过。\n", path,
                (unsigned long long)invalid);
    }
    if (count == 0) {
        /* 文件存在但没有任何可用记录，按格式错误处理。 */
        ds_free((void **)&items);
        return DS_ERR_FORMAT;
    }

    /* 收缩到实际大小；即使 realloc 返回同一地址也没问题。 */
    items = (Student *)ds_realloc(items, count * sizeof(Student));
    *out_items = items;
    *out_count = count;
    return DS_OK;
}

/**
 * @brief 把学生数组写回 f1.txt 格式。
 *
 * @param path  目标文件路径。
 * @param items 学生数组；count 为 0 时可以为 NULL。
 * @param count 记录条数。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；打开/写入失败返回 DS_ERR_IO。
 *
 * 只写 11 个原始字段，不写 weights；下次加载时会按公式重新计算，避免
 * 文件里的加权值与成绩不一致。
 *
 * 时间复杂度 O(count)。
 */
DsStatus student_save_file(const char *path, const Student *items, size_t count)
{
    FILE *file;
    size_t i;

    if (path == NULL || (items == NULL && count > 0)) {
        return DS_ERR_NULL;
    }

    file = fopen(path, "w");
    if (file == NULL) {
        return DS_ERR_IO;
    }

    for (i = 0; i < count; ++i) {
        const Student *s = &items[i];
        /* short 提升为 int 再打印，避免 %hd 与类型不匹配。 */
        fprintf(file, "%s %s %s %d %d %d %d %d %d %d %d\n",
                s->ID, s->name, s->pro_number,
                (int)s->GEC1, (int)s->GEC2, (int)s->GEC3,
                (int)s->PEC1, (int)s->PEC2, (int)s->PEC3,
                (int)s->CaIC1, (int)s->CaIC2);
    }

    if (ferror(file)) {
        fclose(file);
        return DS_ERR_IO;
    }
    fclose(file);
    return DS_OK;
}
