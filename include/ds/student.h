/**
 * @file    student.h
 * @brief   学生记录类型与 f1.txt 数据文件读写。
 *
 * f1.txt 每行 11 个字段，空格分隔：
 *   学号(5位) 姓名 专业编号(2位)
 *   通识教育1 通识教育2 通识教育3
 *   公共基础1 公共基础2 公共基础3
 *   专业主干1 专业主干2
 *
 * 加权成绩公式（与课设骨架一致）：
 *   weights = 0.8 * (公共基础三门总分) + 1.2 * (专业主干两门总分)
 * 满分为 0.8 * 300 + 1.2 * 200 = 480。
 */
#ifndef DS_STUDENT_H
#define DS_STUDENT_H

#include <stddef.h>
#include "ds/ds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STUDENT_ID_SIZE 6    /**< 学号 5 位 + '\0'。 */
#define STUDENT_NAME_SIZE 16 /**< 姓名最长 15 字节 + '\0'。 */
#define STUDENT_MAJOR_SIZE 4 /**< 专业编号 2 位 + '\0'，留有余量。 */

/** 一条学生记录。字段名与课设提供的空架构保持一致。 */
typedef struct Student {
    char ID[STUDENT_ID_SIZE];       /**< 学号，例如 "00002"。 */
    char name[STUDENT_NAME_SIZE];   /**< 姓名。 */
    char pro_number[STUDENT_MAJOR_SIZE]; /**< 专业编号，例如 "13"。 */
    short GEC1, GEC2, GEC3;         /**< 三门通识教育课成绩。 */
    short PEC1, PEC2, PEC3;         /**< 三门公共基础课成绩。 */
    short CaIC1, CaIC2;             /**< 两门专业主干课成绩。 */
    float weights;                  /**< 加权成绩，排序依据。 */
} Student;

/** @brief 根据八门课成绩重新计算加权成绩。 */
void student_update_weight(Student *student);

/** @brief 返回加权成绩；student 为 NULL 时返回 0。 */
float student_weight(const Student *student);

/**
 * @brief 解析一行学生数据。
 * @param line 以 '\0' 结尾的一行文本。
 * @param out  解析结果。
 * @return 字段齐全且取值合法返回 true。
 */
bool student_parse_line(const char *line, Student *out);

/** @brief 输出表头（学号、姓名、专业、八门课、加权成绩）。 */
void student_print_header(void);

/** @brief 按统一列宽输出一条学生记录。 */
void student_print(const Student *student);

/**
 * @brief 分页输出学生数组。
 * @param items     数组首地址。
 * @param count     元素个数。
 * @param page_size 每页条数，0 表示不分页。
 */
void student_print_list(const Student *items, size_t count, size_t page_size);

/* ---- 比较函数，可直接传给 qsort / ds_sort_* / ds_search_* ---- */

/** @brief 按加权成绩从大到小比较（同分时按学号升序）。 */
int student_compare_weight_desc(const void *a, const void *b);

/** @brief 按加权成绩从小到大比较（同分时按学号升序）。 */
int student_compare_weight_asc(const void *a, const void *b);

/** @brief 按学号升序比较两个 Student。 */
int student_compare_id(const void *a, const void *b);

/**
 * @brief 以字符串学号为关键字，与 Student 比较。
 *
 * 供 ds_search_sequential / ds_search_binary 使用：
 * key 是 const char *，element 是 const Student *。
 */
int student_compare_id_key(const void *key, const void *element);

/**
 * @brief 从文本文件加载学生记录。
 *
 * 自动跳过空行和以 '#' 开头的注释行；格式不合法的行会被跳过，
 * 并在标准错误输出一条汇总警告。
 *
 * @param path      文件路径。
 * @param out_items 返回动态分配的数组，调用者用 ds_free 释放。
 * @param out_count 返回记录条数。
 */
DsStatus student_load_file(const char *path, Student **out_items, size_t *out_count);

/** @brief 把学生数组写回 f1.txt 格式（不含加权成绩列）。 */
DsStatus student_save_file(const char *path, const Student *items, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* DS_STUDENT_H */
