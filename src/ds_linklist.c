/**
 * @file    ds_linklist.c
 * @brief   带头结点的单链表实现（学生数据）。
 *
 * 数据布局：
 *
 *     list->head -> [哑结点] -> [第 1 个元素] -> ... -> [第 n 个元素] -> NULL
 *
 * 设计要点：
 *   - 哑结点（dummy head）不保存学生数据，所有插入/删除都能统一处理，
 *     不需要为“在第一个元素前操作”单独写分支；
 *   - list->length 只统计真正的数据结点，不包含哑结点；
 *   - 对外位置一律从 1 开始，内部指针从 head->next 开始走；
 *   - 链表不保证按学号或成绩有序，查找是 O(n) 的线性扫描；
 *   - 只有排序函数会重排结点，排序后 length 不变。
 *
 * 复杂度概览：
 *   头插 O(1)，尾插/按位插入/按位删除/取元素 O(n)，
 *   按学号查找/删除/求前驱 O(n)，逆置 O(n)，排序 O(n log n)。
 */
#include "ds/ds_linklist.h"
#include "ds/ds_console.h"
#include "ds/ds_memory.h"
#include "ds/ds_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 创建一个链表结点。
 *
 * @param value 要保存的学生记录（按值复制）。
 * @return 新结点；next 初始化为 NULL。
 *
 * 使用 ds_malloc 分配，失败时由内存模块统一报错并终止程序。
 * 复杂度 O(1)。
 */
static LNode *linklist_new_node(Student value)
{
    LNode *node = (LNode *)ds_malloc(sizeof(LNode));

    node->data = value;
    node->next = NULL;
    return node;
}

/**
 * @brief 初始化空链表。
 *
 * @param list 待初始化的链表对象。
 * @return DS_OK；list 为 NULL 时返回 DS_ERR_NULL。
 *
 * 分配一个哑结点，并把 length 置 0。调用前 list 必须是未初始化或已经
 * 销毁的状态；对同一个对象重复 init 会覆盖 head 指针并泄漏旧链表。
 *
 * 时间复杂度 O(1)。
 */
DsStatus linklist_init(LinkList *list)
{
    if (list == NULL) {
        return DS_ERR_NULL;
    }

    list->head = (LNode *)ds_calloc(1, sizeof(LNode));
    list->length = 0;
    return DS_OK;
}

/**
 * @brief 销毁链表，释放所有数据结点和哑结点。
 *
 * @param list 链表对象；为 NULL 时直接返回。
 *
 * 先调用 clear 释放数据结点，再释放哑结点。ds_free 会把 head 置 NULL，
 * 因此销毁后再次 destroy 是安全的。
 *
 * 时间复杂度 O(n)。
 */
void linklist_destroy(LinkList *list)
{
    if (list == NULL) {
        return;
    }
    linklist_clear(list);  //释放所有数据结点
    ds_free((void **)&list->head);
    list->length = 0;
}

/**
 * @brief 清空链表，但保留哑结点。
 *
 * @param list 链表对象；为 NULL 或未初始化时直接返回。
 *
 * 从 head->next 开始逐个释放数据结点，最后把 head->next 置 NULL、
 * length 置 0。清空后链表仍可继续插入。
 *
 * 时间复杂度 O(n)。
 */
void linklist_clear(LinkList *list)
{
    LNode *current;

    if (list == NULL || list->head == NULL) {
        return;
    }

    current = list->head->next;
    while (current != NULL) {
        LNode *next = current->next;
        ds_free((void **)&current);
        current = next;
    }
    list->head->next = NULL;
    list->length = 0;
}

/**
 * @brief 尾插法：把学生追加到链表末尾。
 *
 * @param list  链表对象。
 * @param value 学生记录。
 * @return DS_OK；list 未初始化返回 DS_ERR_NULL。
 *
 * 因为没有维护尾指针，需要从头走到最后一个结点，再挂上新结点。
 * 连续追加 n 条记录的总时间是 O(n^2)；如果以后需要频繁尾插，可以在
 * LinkList 中增加 tail 指针把单次尾插优化到 O(1)。
 *
 * 时间复杂度 O(n)。
 */
DsStatus linklist_push_back(LinkList *list, Student value)
{
    LNode *node;
    LNode *tail;

    if (list == NULL || list->head == NULL) {
        return DS_ERR_NULL;
    }

    node = linklist_new_node(value);
    tail = list->head;  //哑结点是第 0 个结点，tail->next 是首元素
    while (tail->next != NULL) {
        tail = tail->next;  //走到最后一个结点
    }
    tail->next = node;  //挂上新结点
    ++list->length;  //更新长度
    return DS_OK;
}

/**
 * @brief 头插法：把学生插入到第一个数据结点之前。
 *
 * @param list  链表对象。
 * @param value 学生记录。
 * @return DS_OK；list 未初始化返回 DS_ERR_NULL。
 *
 * 新结点的 next 指向原来的首结点，哑结点的 next 指向新结点。
 * 时间复杂度 O(1)。
 */
DsStatus linklist_push_front(LinkList *list, Student value)
{
    LNode *node;

    if (list == NULL || list->head == NULL) {
        return DS_ERR_NULL;
    }

    node = linklist_new_node(value);
    node->next = list->head->next;  //插入结点指向的下一个结点是原来的首结点
    list->head->next = node;  //哑结点的 next 指向新结点
    ++list->length;  //更新长度
    return DS_OK;
}

/**
 * @brief 用数组批量重建链表。
 *
 * @param list        已初始化的链表。
 * @param items       学生数组；count 为 0 时可为 NULL。
 * @param count       元素个数。
 * @param head_insert true 头插，false 尾插。
 * @return DS_OK；list 未初始化或 items 非法返回 DS_ERR_NULL。
 *
 * 与逐条调用 push_back 不同，本函数只在尾插模式下维护一个 tail 指针，
 * 因此不会每插入一个元素都从头走到表尾，整体为 O(count)。
 *
 * 实现采用“先建新链、后换旧链”的顺序：循环期间旧链表完全不动，
 * 新链全部串好后才调用 clear 替换。这样即使构建过程失败，调用者
 * 原来的数据仍然可用（内存分配失败由 ds_malloc 统一终止程序）。
 */
DsStatus linklist_assign(LinkList *list, const Student *items, size_t count,
                         bool head_insert)
{
    LNode *new_head = NULL;
    LNode *tail = NULL;
    size_t i;

    if (list == NULL || list->head == NULL || (items == NULL && count > 0)) {
        return DS_ERR_NULL;
    }

    for (i = 0; i < count; ++i) {
        LNode *node = linklist_new_node(items[i]);

        if (head_insert) {
            node->next = new_head;  //新结点成为新的首结点
            new_head = node;
        } else {
            if (tail != NULL) {
                tail->next = node;  //接到当前尾结点之后
            } else {
                new_head = node;    //第一个结点同时是首结点
            }
            tail = node;            //更新尾指针，保证尾插 O(1)
        }
    }

    linklist_clear(list);           //新链已就绪，安全替换旧数据
    list->head->next = new_head;
    list->length = count;
    return DS_OK;
}

/**
 * @brief 在第 pos 个位置插入学生。
 *
 * @param list  链表对象。
 * @param pos   插入位置，合法范围 [1, length + 1]。
 * @param value 学生记录。
 * @return DS_OK；list 未初始化返回 DS_ERR_NULL；pos 越界返回
 *         DS_ERR_RANGE。
 *
 * 先走到第 pos-1 个结点（previous），再执行标准链表插入：
 *
 *     node->next = previous->next;
 *     previous->next = node;
 *
 * pos = 1 时 previous 就是哑结点，因此不需要特判。
 *
 * 时间复杂度 O(pos)，最坏 O(n)。
 */
DsStatus linklist_insert(LinkList *list, size_t pos, Student value)
{
    LNode *previous;
    LNode *node;
    size_t i;

    if (list == NULL || list->head == NULL) {
        return DS_ERR_NULL;  //链表未初始化
    }
    if (pos < 1 || pos > list->length + 1) {
        return DS_ERR_RANGE;  //pos 越界
    }

    previous = list->head;
    for (i = 1; i < pos; ++i) {
        previous = previous->next;  //找到第 pos-1 个结点
    }
    //下面是标准链表插入操作
    node = linklist_new_node(value);
    node->next = previous->next;
    previous->next = node;
    ++list->length;
    return DS_OK;
}

/**
 * @brief 删除第 pos 个结点。
 *
 * @param list 链表对象。
 * @param pos  删除位置，合法范围 [1, length]。
 * @param out  可选输出参数；非 NULL 时把被删除的学生复制到 *out。
 * @return DS_OK；list 未初始化返回 DS_ERR_NULL；pos 越界返回
 *         DS_ERR_RANGE。
 *
 * 先找到第 pos-1 个结点 previous，令 target = previous->next，
 * 再执行 previous->next = target->next，最后释放 target。
 *
 * 时间复杂度 O(pos)，最坏 O(n)。
 */
DsStatus linklist_delete_at(LinkList *list, size_t pos, Student *out)
{
    LNode *previous;
    LNode *target;
    size_t i;

    if (list == NULL || list->head == NULL) {
        return DS_ERR_NULL;  //链表未初始化
    }
    if (pos < 1 || pos > list->length) {
        return DS_ERR_RANGE;  //pos 越界
    }

    previous = list->head;
    for (i = 1; i < pos; ++i) {
        previous = previous->next;  //找到第 pos-1 个结点
    }
    target = previous->next;  //第 pos 个结点
    previous->next = target->next;  //前驱结点指向 target 的下一个结点

    if (out != NULL) {
        *out = target->data;
    }
    ds_free((void **)&target);  //释放被删除结点
    --list->length;
    return DS_OK;
}

/**
 * @brief 按学号删除结点。
 *
 * @param list 链表对象。
 * @param id   目标学号字符串。
 * @param out  可选输出参数；非 NULL 时返回被删除的学生。
 * @return DS_OK；list/id 为 NULL 返回 DS_ERR_NULL；未找到返回
 *         DS_ERR_NOT_FOUND。
 *
 * 用 previous/current 两个指针同步前进。命中后让 previous 绕过
 * current，再释放 current，因此删除首结点也无需特判（previous 是哑结点）。
 * 若链表中有重复学号，只删除第一个匹配项。
 *
 * 时间复杂度 O(n)。
 */
DsStatus linklist_delete_by_id(LinkList *list, const char *id, Student *out)
{
    LNode *previous;
    LNode *current;

    if (list == NULL || list->head == NULL || id == NULL) {
        return DS_ERR_NULL;
    }

    previous = list->head;
    current = previous->next;
    while (current != NULL) {
        if (strcmp(current->data.ID, id) == 0) {
            previous->next = current->next;
            if (out != NULL) {
                *out = current->data;
            }
            ds_free((void **)&current);
            --list->length;
            return DS_OK;
        }
        previous = current;
        current = current->next;
    }
    return DS_ERR_NOT_FOUND;
}

/**
 * @brief 取第 pos 个学生（不删除）。
 *
 * @param list 链表对象。
 * @param pos  位置，合法范围 [1, length]。
 * @param out  输出参数，不能为 NULL。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；pos 越界返回 DS_ERR_RANGE。
 *
 * 时间复杂度 O(pos)，最坏 O(n)。
 */
DsStatus linklist_get(const LinkList *list, size_t pos, Student *out)
{
    LNode *current;
    size_t i;

    if (list == NULL || list->head == NULL || out == NULL) {
        return DS_ERR_NULL;
    }
    if (pos < 1 || pos > list->length) {
        return DS_ERR_RANGE;
    }

    current = list->head->next;
    for (i = 1; i < pos; ++i) {
        current = current->next;
    }
    *out = current->data;
    return DS_OK;
}

/**
 * @brief 按学号查找结点指针。
 *
 * @param list 链表对象。
 * @param id   目标学号。
 * @return 命中返回结点指针；未找到或参数非法返回 NULL。
 *
 * 返回的是链表内部指针，调用方可以读取，但不应自行 free；若随后删除
 * 该结点，指针会失效。
 *
 * 时间复杂度 O(n)。
 */
LNode *linklist_find_node(const LinkList *list, const char *id)
{
    LNode *current;

    if (list == NULL || list->head == NULL || id == NULL) {
        return NULL;
    }

    current = list->head->next;
    while (current != NULL) {
        if (strcmp(current->data.ID, id) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * @brief 按学号查找并复制学生记录。
 *
 * @param list 链表对象。
 * @param id   目标学号。
 * @param out  可选输出参数；非 NULL 时写入命中的学生。
 * @return DS_OK；未找到返回 DS_ERR_NOT_FOUND。
 *
 * 复用 linklist_find_node，返回值语义与“查找结点”一致，但只暴露数据
 * 副本，调用方不能通过它修改链表。
 *
 * 时间复杂度 O(n)。
 */
DsStatus linklist_find_by_id(const LinkList *list, const char *id, Student *out)
{
    LNode *node = linklist_find_node(list, id);

    if (node == NULL) {
        return DS_ERR_NOT_FOUND;
    }
    if (out != NULL) {
        *out = node->data;
    }
    return DS_OK;
}

/**
 * @brief 求指定学号学生的前驱。
 *
 * @param list 链表对象。
 * @param id   目标学号。
 * @param out  输出参数，不能为 NULL。
 * @return DS_OK；空表返回 DS_ERR_EMPTY；目标是首结点（没有前驱）返回
 *         DS_ERR_RANGE；目标不存在返回 DS_ERR_NOT_FOUND。
 *
 * 采用“超前一步”的双指针：
 *   previous 指向 current 的前一个结点，初始为第一个数据结点；
 *   current  从第二个结点开始。
 * 这样命中 current 时 previous 正好是前驱。循环结束后单独检查首结点，
 * 因为首结点没有前驱，必须返回 RANGE 而不是 NOT_FOUND。
 *
 * 时间复杂度 O(n)。
 */
DsStatus linklist_predecessor(const LinkList *list, const char *id, Student *out)
{
    LNode *previous;
    LNode *current;

    if (list == NULL || list->head == NULL || id == NULL || out == NULL) {
        return DS_ERR_NULL;  //参数非法
    }

    previous = list->head->next;  //第一个数据结点
    if (previous == NULL) {
        return DS_ERR_EMPTY;  //链表为空，没有前驱
    }
    current = previous->next;  //第二个数据结点
    while (current != NULL) {
        if (strcmp(current->data.ID, id) == 0) {
            *out = previous->data;  //如果找到，返回前驱
            return DS_OK;
        }
        previous = current;  //没有找到，向后推进
        current = current->next;
    }

    /* 循环只覆盖第 2 个及之后的结点，首结点要在这里单独判断。 */
    if (strcmp(list->head->next->data.ID, id) == 0) {
        return DS_ERR_RANGE;
    }
    return DS_ERR_NOT_FOUND;
}

/**
 * @brief 原地逆置链表（不分配新结点）。
 *
 * @param list 链表对象；为 NULL 或未初始化时直接返回。
 *
 * 标准三指针法：
 *   previous 指向已逆置部分的头；
 *   current  指向待处理结点；
 *   next     暂存 current->next，防止断链后丢失后半段。
 *
 * 每处理一个结点就把它插到 previous 前面，最后让哑结点指向新的头。
 * 时间复杂度 O(n)，额外空间 O(1)。
 */
void linklist_reverse(LinkList *list)
{
    LNode *previous = NULL;
    LNode *current;
    LNode *next;

    if (list == NULL || list->head == NULL) {
        return;
    }

    current = list->head->next;
    while (current != NULL) {
        next = current->next;
        current->next = previous;
        previous = current;
        current = next;
    }
    list->head->next = previous;
}

/**
 * @brief qsort 的比较回调：按加权成绩降序，同分按学号升序。
 *
 * @param a 指向 LNode* 元素的指针。
 * @param b 指向 LNode* 元素的指针。
 * @return 负数表示 a 排在 b 前，正数表示 a 排在 b 后，0 表示相等。
 *
 * qsort 传给比较函数的是“数组元素”的地址，而数组元素本身是 LNode*，
 * 所以要先解引用成 LNode*，再取 ->data 交给学生比较函数。
 */
static int compare_node_weight_desc(const void *a, const void *b)
{
    const LNode *left = *(const LNode *const *)a;  //解引用成 LNode*
    const LNode *right = *(const LNode *const *)b;

    return student_compare_weight_desc(&left->data, &right->data);  //调用学生比较函数
}

/**
 * @brief 按加权成绩降序重排链表。
 *
 * @param list 链表对象；为空或长度小于 2 时直接返回。
 *
 * 链表本身不支持随机访问，直接用 qsort 需要 O(n^2) 的定位；这里采用
 * “指针数组 + qsort + 重连”：
 *   1. 分配长度为 n 的 LNode* 数组，按原顺序收集每个结点的地址；
 *   2. 对指针数组排序，只移动指针，不移动 Student 大对象；
 *   3. 按排序后的顺序重新设置 next，并让哑结点指向新的首结点。
 *
 * 时间复杂度 O(n log n)，额外空间 O(n)。
 */
void linklist_sort_by_weight_desc(LinkList *list)
{
    LNode **nodes;
    LNode *current;
    size_t i;

    if (list == NULL || list->head == NULL || list->length < 2) {
        return;  //链表为空或长度小于 2，无需排序
    }

    nodes = (LNode **)ds_malloc(list->length * sizeof(LNode *));
    current = list->head->next;
    for (i = 0; i < list->length; ++i) {  //把每个结点的地址收集到数组中
        nodes[i] = current;
        current = current->next;  
    }
    // 对指针数组排序，按加权成绩降序，同分按学号升序
    qsort(nodes, list->length, sizeof(LNode *), compare_node_weight_desc);

    list->head->next = nodes[0];
    for (i = 0; i + 1 < list->length; ++i) {
        nodes[i]->next = nodes[i + 1];
    }
    nodes[list->length - 1]->next = NULL;
    ds_free((void **)&nodes);
}

/**
 * @brief 分页输出链表中的学生。
 *
 * @param list      链表对象。
 * @param page_size 每页条数；为 0 表示不分页。
 *
 * 每页先打印表头，输出 page_size 条后暂停，按回车继续并重新打印表头。
 * 最后一条记录之后不再暂停，避免用户需要多按一次回车。
 *
 * 时间复杂度 O(n)，输出本身是主要开销。
 */
void linklist_print(const LinkList *list, size_t page_size)
{
    LNode *current;
    size_t index = 0;

    if (list == NULL || list->head == NULL || list->length == 0) {
        ds_console_info("链表为空，没有可显示的学生记录。");
        return;
    }

    student_print_header();
    current = list->head->next;
    while (current != NULL) {
        student_print(&current->data);
        ++index;
        if (page_size > 0 && index % page_size == 0 && current->next != NULL) {
            ds_console_pause("按回车键继续显示下一页...");
            student_print_header();
        }
        current = current->next;
    }
    printf("共 %llu 条记录。\n", (unsigned long long)list->length);
}

/**
 * @brief 从数据文件批量追加学生到链表。
 *
 * @param list      已初始化的链表。
 * @param path      数据文件路径。
 * @param out_count 可选输出参数；成功时写入追加的条数。
 * @return DS_OK；文件/格式错误返回 student_load_file 的状态；链表参数
 *         非法返回 DS_ERR_NULL。
 *
 * 先让 student_load_file 把整个文件读入动态数组，再逐条尾插。这样解析
 * 逻辑只写一份，顺序表和链表共用；代价是加载阶段额外需要 O(n) 内存。
 *
 * 如果中途分配失败，已经追加的结点会留在链表中，函数返回错误；调用方
 * 可以选择 clear 后重试。
 *
 * 时间复杂度 O(n^2)（每次尾插都要走到表尾）。
 */
DsStatus linklist_load_file(LinkList *list, const char *path, size_t *out_count)
{
    Student *items = NULL;
    size_t count = 0;
    size_t i;
    DsStatus status;

    if (list == NULL || path == NULL) {
        return DS_ERR_NULL;
    }
    if (out_count != NULL) {
        *out_count = 0;
    }

    status = student_load_file(path, &items, &count);
    if (status != DS_OK) {
        return status;
    }

    for (i = 0; i < count; ++i) {
        status = linklist_push_back(list, items[i]);
        if (status != DS_OK) {
            break;
        }
    }
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
 * @brief 把链表中的学生写回数据文件。
 *
 * @param list 链表对象。
 * @param path 输出文件路径。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；打开/写入失败返回 DS_ERR_IO。
 *
 * 输出格式与 f1.txt 一致：11 个字段，空格分隔，每行一条。成绩字段在
 * Student 中是 short，用 int 强制转换后交给 fprintf，避免可变参数提升
 * 带来的类型不匹配。
 *
 * 时间复杂度 O(n)。
 */
DsStatus linklist_save_file(const LinkList *list, const char *path)
{
    FILE *file;
    LNode *current;

    if (list == NULL || list->head == NULL || path == NULL) {
        return DS_ERR_NULL;
    }

    file = fopen(path, "w");
    if (file == NULL) {
        return DS_ERR_IO;
    }

    current = list->head->next;
    while (current != NULL) {
        const Student *s = &current->data;
        fprintf(file, "%s %s %s %d %d %d %d %d %d %d %d\n",
                s->ID, s->name, s->pro_number,
                (int)s->GEC1, (int)s->GEC2, (int)s->GEC3,
                (int)s->PEC1, (int)s->PEC2, (int)s->PEC3,
                (int)s->CaIC1, (int)s->CaIC2);
        current = current->next;
    }

    if (ferror(file)) {
        fclose(file);
        return DS_ERR_IO;
    }
    fclose(file);
    return DS_OK;
}
