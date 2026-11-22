/**
 * @file    main.c
 * @brief   题目二：二叉树（括号表示法建树与遍历）。
 *
 * 程序用链式二叉链表存储一棵一般二叉树，结点值为单个字符。
 * 菜单功能：
 *   1. 输入括号表示法并建树；
 *   2. 输出当前树的括号表示法；
 *   3. 先序遍历；
 *   4. 中序遍历；
 *   5. 后序遍历；
 *   6. 层次遍历；
 *   7. 求树高、结点数、叶子数；
 *   8. 求指定结点的双亲；
 *   9. 求任意两结点之间的路径。
 *
 * 设计约定：
 *   - 建树、遍历、求高、求双亲、求路径等树算法全部由 ds_btree
 *     模块完成，本文件只负责菜单交互和结果展示；
 *   - 结点值必须互不相同，否则“按值查找双亲/路径”会产生歧义，
 *     因此建树时会做一次重复值检查；
 *   - 所有失败路径统一输出中文原因，不把库的错误码直接抛给用户。
 */

#include "ds/ds_btree.h"
#include "ds/ds_console.h"
#include "ds/ds_input.h"
#include "ds/ds_memory.h"
#include "ds/ds_menu.h"
#include "ds/ds_types.h"
#include "ds/ds_util.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** 默认示例树，用户直接回车时使用。 */
#define LAB02_DEFAULT_EXPR "A(B(D,E),C(F,G))"

/** 括号表达式缓冲区大小。 */
#define LAB02_EXPR_SIZE 256

/** 应用上下文：整个程序只维护一棵二叉树。 */
typedef struct Lab02App {
    BTNode *root;
} Lab02App;

/** 遍历输出上下文。 */
typedef struct PrintContext {
    int count;
} PrintContext;

/** 遍历函数指针，便于四个遍历菜单共用同一段展示代码。 */
typedef void (*BTreeTraversal)(const BTNode *, BTVisitFn, void *);

/**
 * @brief 检查当前是否已经建树。
 *
 * @param app 应用上下文。
 * @return 有树返回 true；空树输出提示并返回 false。
 */
static bool ensure_tree(const Lab02App *app)
{
    if (app->root == NULL) {
        ds_console_error("当前是空树，请先执行菜单 1 建树。");
        return false;
    }
    return true;
}

/**
 * @brief 读取一个括号表示法表达式。
 *
 * @param expression 输出缓冲区。
 * @param size       缓冲区字节数。
 * @return 读取成功返回 true；EOF 返回 false。
 *
 * 用户直接回车时使用默认示例，方便快速演示。输入过长时不会截断后
 * 静默建树，而是提示重新输入，避免用户以为建的是自己输入的树。
 */
static bool read_expression(char *expression, size_t size)
{
    char line[512];

    for (;;) {
        char *text;
        size_t length;

        if (ds_input_line("请输入括号表示法（直接回车使用示例 "
                          LAB02_DEFAULT_EXPR "）: ",
                          line, sizeof(line)) != DS_OK) {
            return false;
        }
        text = ds_str_trim(line);
        if (*text == '\0') {
            ds_str_copy(expression, size, LAB02_DEFAULT_EXPR);
            return true;
        }

        length = strlen(text);
        if (length >= size) {
            ds_console_error("表达式过长（最多 %d 个字符），请重新输入。",
                             (int)size - 1);
            continue;
        }
        memmove(expression, text, length + 1);
        return true;
    }
}

/**
 * @brief 读取一个单字符结点值。
 *
 * @param prompt 输入提示。
 * @param value  输出字符。
 * @return 读取成功返回 true；EOF 返回 false。
 *
 * 结点值只能是单个非空白字符；中文等多字节字符会被拒绝，因为本模块
 * 的 BTNode.data 是 char，按字节存储。
 */
static bool read_node_value(const char *prompt, char *value)
{
    char line[32];

    for (;;) {
        char *text;

        if (ds_input_line(prompt, line, sizeof(line)) != DS_OK) {
            return false;
        }
        text = ds_str_trim(line);
        if (strlen(text) == 1 && !isspace((unsigned char)text[0])) {
            *value = text[0];
            return true;
        }
        ds_console_error("结点值必须是单个非空白字符，请重新输入。");
    }
}

/**
 * @brief 遍历访问函数：按顺序输出结点值，结点之间用空格分隔。
 *
 * @param node    当前结点。
 * @param context PrintContext，用于记录已经输出的结点数。
 */
static void print_visit(const BTNode *node, void *context)
{
    PrintContext *printer = (PrintContext *)context;

    if (printer->count > 0) {
        putchar(' ');
    }
    putchar(node->data);
    ++printer->count;
}

/**
 * @brief 执行一种遍历并输出结果。
 *
 * @param app      应用上下文。
 * @param label    遍历名称。
 * @param traverse 具体的遍历函数（btree_preorder 等）。
 */
static void show_traversal(const Lab02App *app, const char *label,
                           BTreeTraversal traverse)
{
    PrintContext printer = { 0 };

    if (!ensure_tree(app)) {
        return;
    }

    ds_console_subtitle(label);
    traverse(app->root, print_visit, &printer);
    printf("\n共 %d 个结点。\n", printer.count);
}

/**
 * @brief 重复结点值检查的上下文。
 *
 * duplicate 为 '\0' 表示没有重复；否则保存第一次发现的重复字符。
 */
typedef struct LabelCheck {
    bool seen[256];
    char duplicate;
} LabelCheck;

/**
 * @brief 先序遍历回调：记录已经出现过的结点字符。
 *
 * @param node    当前结点。
 * @param context LabelCheck。
 */
static void check_label_visit(const BTNode *node, void *context)
{
    LabelCheck *check = (LabelCheck *)context;
    unsigned char index = (unsigned char)node->data;

    if (check->seen[index] && check->duplicate == '\0') {
        check->duplicate = node->data;
    }
    check->seen[index] = true;
}

/**
 * @brief 菜单 1：输入括号表示法并建树。
 *
 * 先建新树，确认语法和结点值都合法后，才销毁旧树并替换。这样即使
 * 输入错误，用户原来的树也不会丢失。
 */
static void action_build(void *context)
{
    Lab02App *app = (Lab02App *)context;
    char expression[LAB02_EXPR_SIZE];
    BTNode *new_root;
    LabelCheck check;
    int nodes;

    ds_console_subtitle("输入括号表示法并建树");
    printf("示例：A(B(D,E),C(F,G))\n");
    printf("约定：结点值为单个字符，且同一棵树内不能重复。\n");

    if (!read_expression(expression, sizeof(expression))) {
        return;
    }

    new_root = btree_create_from_bracket(expression);
    if (new_root == NULL) {
        ds_console_error("括号表达式格式错误，请检查括号、逗号和结点值。");
        return;
    }

    memset(&check, 0, sizeof(check));
    btree_preorder(new_root, check_label_visit, &check);
    if (check.duplicate != '\0') {
        btree_destroy(new_root);
        ds_console_error("结点值 '%c' 重复。请保证每个结点值唯一。",
                         check.duplicate);
        return;
    }

    btree_destroy(app->root);
    app->root = new_root;
    nodes = btree_node_count(app->root);

    ds_console_success("建树成功：共 %d 个结点，树高 %d。",
                       nodes, btree_height(app->root));
    printf("括号表示法：");
    btree_print_bracket(stdout, app->root);
    putchar('\n');
}

/**
 * @brief 菜单 2：输出当前树的括号表示法。
 */
static void action_show_bracket(void *context)
{
    Lab02App *app = (Lab02App *)context;

    if (!ensure_tree(app)) {
        return;
    }

    ds_console_subtitle("当前二叉树的括号表示法");
    btree_print_bracket(stdout, app->root);
    putchar('\n');
    printf("结点数：%d，树高：%d，叶子数：%d\n",
           btree_node_count(app->root),
           btree_height(app->root),
           btree_leaf_count(app->root));
}

/** @brief 菜单 3：先序遍历。 */
static void action_preorder(void *context)
{
    show_traversal((const Lab02App *)context, "先序遍历（根 -> 左 -> 右）",
                   btree_preorder);
}

/** @brief 菜单 4：中序遍历。 */
static void action_inorder(void *context)
{
    show_traversal((const Lab02App *)context, "中序遍历（左 -> 根 -> 右）",
                   btree_inorder);
}

/** @brief 菜单 5：后序遍历。 */
static void action_postorder(void *context)
{
    show_traversal((const Lab02App *)context, "后序遍历（左 -> 右 -> 根）",
                   btree_postorder);
}

/** @brief 菜单 6：层次遍历。 */
static void action_levelorder(void *context)
{
    show_traversal((const Lab02App *)context, "层次遍历（从上到下、从左到右）",
                   btree_levelorder);
}

/**
 * @brief 菜单 7：求树高、结点数和叶子数。
 */
static void action_stats(void *context)
{
    Lab02App *app = (Lab02App *)context;

    if (!ensure_tree(app)) {
        return;
    }

    ds_console_subtitle("二叉树的统计信息");
    printf("树高（根所在层为 1）：%d\n", btree_height(app->root));
    printf("结点总数：%d\n", btree_node_count(app->root));
    printf("叶子结点数：%d\n", btree_leaf_count(app->root));
}

/**
 * @brief 菜单 8：求指定结点的双亲。
 */
static void action_parent(void *context)
{
    Lab02App *app = (Lab02App *)context;
    BTNode *target;
    BTNode *parent;
    char value;

    if (!ensure_tree(app)) {
        return;
    }

    ds_console_subtitle("求指定结点的双亲");
    if (!read_node_value("请输入子结点字符: ", &value)) {
        return;
    }

    target = btree_find(app->root, value);
    if (target == NULL) {
        ds_console_error("树中不存在结点 '%c'。", value);
        return;
    }
    if (target == app->root) {
        ds_console_info("结点 '%c' 是根结点，没有双亲。", value);
        return;
    }

    parent = btree_parent(app->root, value);
    if (parent == NULL) {
        /* 理论上不会发生：目标存在且不是根，就一定有双亲。 */
        ds_console_error("求双亲失败：树结构异常。");
        return;
    }

    printf("结点 '%c' 的双亲是 '%c'（%s孩子）。\n",
           value, parent->data,
           parent->left == target ? "左" : "右");
}

/**
 * @brief 按路径顺序输出字符，结点之间用 " -> " 分隔。
 *
 * @param path   路径缓冲区，不要求以 '\0' 结尾。
 * @param length 路径上的结点数。
 */
static void print_path(const char *path, int length)
{
    int i;

    for (i = 0; i < length; ++i) {
        if (i > 0) {
            printf(" -> ");
        }
        putchar(path[i]);
    }
    putchar('\n');
}

/**
 * @brief 菜单 9：求任意两结点之间的路径。
 *
 * 路径长度最多为 2 * 树高 - 1 个结点（两个叶子分居左右子树的最坏
 * 情况），因此这里按 2 * 树高 + 1 分配缓冲区。
 */
static void action_path(void *context)
{
    Lab02App *app = (Lab02App *)context;
    char start;
    char end;
    char *path;
    int height;
    int length = 0;
    int capacity;
    DsStatus status;

    if (!ensure_tree(app)) {
        return;
    }

    ds_console_subtitle("求任意两结点之间的路径");
    if (!read_node_value("请输入起点结点字符: ", &start) ||
        !read_node_value("请输入终点结点字符: ", &end)) {
        return;
    }

    if (btree_find(app->root, start) == NULL) {
        ds_console_error("树中不存在起点结点 '%c'。", start);
        return;
    }
    if (btree_find(app->root, end) == NULL) {
        ds_console_error("树中不存在终点结点 '%c'。", end);
        return;
    }

    height = btree_height(app->root);
    capacity = height * 2 + 1;
    path = (char *)ds_malloc((size_t)capacity);

    status = btree_path_between(app->root, start, end,
                                path, (size_t)capacity, &length);
    if (status != DS_OK) {
        ds_console_error("求路径失败：%s", ds_status_text(status));
        ds_free((void **)&path);
        return;
    }

    printf("结点 '%c' 到 '%c' 的路径（%d 个结点，%d 条边）：\n",
           start, end, length, length - 1);
    print_path(path, length);
    ds_free((void **)&path);
}

/*
 * 功能 10“交换左右子树”暂不开放。保留原实现并用 #if 0 隔离，
 * 以后需要恢复时，只要去掉 #if 0 / #endif 并把菜单项加回即可。
 */
#if 0
/**
 * @brief 菜单 10：交换所有结点的左右子树。
 *
 * 交换后结点数和树高不变，但先序、中序、后序、层次序列都会改变，
 * 可以接着执行 3-6 观察镜像效果。
 */
static void action_swap(void *context)
{
    Lab02App *app = (Lab02App *)context;
    PrintContext printer = { 0 };

    if (!ensure_tree(app)) {
        return;
    }

    ds_console_subtitle("交换所有结点的左右子树");
    btree_swap_children(app->root);
    ds_console_success("交换完成：每个结点的左右孩子都已互换。");

    printf("交换后的括号表示法：");
    btree_print_bracket(stdout, app->root);
    putchar('\n');

    printf("交换后的先序序列：");
    btree_preorder(app->root, print_visit, &printer);
    putchar('\n');
}
#endif

/** 菜单项表。当前开放功能 1-9。 */
static DsMenuItem items[] = {
    { 1, "输入括号表示法并建树", action_build },
    { 2, "输出括号表示法", action_show_bracket },
    { 3, "先序遍历", action_preorder },
    { 4, "中序遍历", action_inorder },
    { 5, "后序遍历", action_postorder },
    { 6, "层次遍历", action_levelorder },
    { 7, "求树高、结点数、叶子数", action_stats },
    { 8, "求指定结点的双亲", action_parent },
    { 9, "求任意两结点之间的路径", action_path }
};

static DsMenu menu = {
    "题目二：二叉树",
    items,
    DS_ARRAY_LEN(items)
};

/**
 * @brief 程序入口。
 *
 * 初始化控制台和空树后进入菜单循环；退出时释放整棵树并恢复控制台
 * 代码页，保证不留下内存或终端状态问题。
 */
int main(void)
{
    Lab02App app;

    app.root = NULL;

    ds_console_init();
    ds_menu_run(&menu, &app);
    btree_destroy(app.root);
    ds_console_restore();
    return 0;
}
