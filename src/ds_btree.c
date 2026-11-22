/**
 * @file    ds_btree.c
 * @brief   链式二叉树的建树、遍历、路径与形态操作。
 *
 * 结点定义：
 *
 *     typedef struct BTNode {
 *         char data;
 *         struct BTNode *left, *right;
 *     } BTNode;
 *
 * 本模块处理的是“一般二叉树”，不是二叉排序树，因此查找、求双亲都
 * 需要遍历整棵树，不能按大小关系剪枝。
 *
 * 括号表示法的文法（空白可出现在任意 token 之间）：
 *
 *     tree  := node
 *     node  := char [ '(' [ tree ] [ ',' [ tree ] ] ')' ]
 *
 * 例：
 *     A(B(D,E),C(F,G))   完全二叉树
 *     A(B)               只有左孩子
 *     A(,B)              只有右孩子
 *     A()                叶子
 *
 * 递归函数的复杂度均为 O(n)，n 为结点数；层次遍历额外需要 O(n) 的
 * 指针队列。
 */
#include "ds/ds_btree.h"
#include "ds/ds_memory.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 分配并初始化一个二叉树结点。
 *
 * @param value 结点字符。
 * @return 新结点指针，左右孩子均为 NULL。
 *
 * 复杂度 O(1)。
 */
static BTNode *btree_new_node(char value)
{
    BTNode *node = (BTNode *)ds_malloc(sizeof(BTNode));  //创建新结点，分配内存

    node->data = value;
    node->left = NULL;
    node->right = NULL;
    return node;
}

/**
 * @brief 跳过空白字符。
 *
 * @param cursor 指向当前字符指针的指针；函数会推进 *cursor。
 *
 * 支持空格、制表符、回车、换行，方便用户把括号表达式写成多行。
 */
static void skip_spaces(const char **cursor)
{
    while (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' ||
           **cursor == '\n') {
        ++(*cursor);  // 跳过空白
    }
}

/**
 * @brief 递归解析一个结点及其子树。
 *
 * @param cursor 指向当前字符指针的指针。
 * @return 解析出的子树根；语法错误返回 NULL。
 *
 * 解析流程：
 *   1. 跳过空白，读一个非分隔字符作为结点值；
 *   2. 若后面紧跟 '('，说明有孩子：
 *        - '(' 后不是 ',' 或 ')'，解析左子树；
 *        - 若遇到 ','，跳过并解析右子树（右子树可以为空）；
 *        - 最后必须遇到 ')'，否则语法错误；
 *   3. 没有 '(' 时当前结点就是叶子。
 *
 * 出错时会销毁已经建好的部分子树，避免内存泄漏。
 */
static BTNode *parse_node(const char **cursor)
{
    BTNode *node;
    char value;

    skip_spaces(cursor);
    value = **cursor;
    if (value == '\0' || value == '(' || value == ')' || value == ',') {
        /* 这些字符不能作为结点值，说明缺少结点。 */
        return NULL;
    }
    ++(*cursor);
    node = btree_new_node(value);

    skip_spaces(cursor);
    if (**cursor == '(') {
        ++(*cursor);
        skip_spaces(cursor);

        /* 左子树：下一个 token 不是 ',' 或 ')' 时存在。 */
        if (**cursor != ',' && **cursor != ')') {
            node->left = parse_node(cursor);
            if (node->left == NULL) {
                btree_destroy(node);
                return NULL;
            }
        }
        skip_spaces(cursor);

        /* 右子树：只有出现 ',' 才可能存在。 */
        if (**cursor == ',') {
            ++(*cursor);
            skip_spaces(cursor);
            if (**cursor != ')') {
                node->right = parse_node(cursor);
                if (node->right == NULL) {
                    btree_destroy(node);
                    return NULL;
                }
            }
            skip_spaces(cursor);
        }

        /* 孩子部分必须以 ')' 收尾。 */
        if (**cursor != ')') {
            btree_destroy(node);
            return NULL;
        }
        ++(*cursor);
    }
    return node;
}

/**
 * @brief 从括号表示法字符串创建二叉树。
 *
 * @param text 括号表达式；为 NULL 返回 NULL。
 * @return 树根；语法错误或空串返回 NULL。
 *
 * 解析完一棵树后还会跳过尾部空白并检查是否到达 '\0'，防止
 * "A(B) C(D)" 这种“多棵树粘在一起”的输入被静默接受。
 *
 * 时间复杂度 O(n)，空间复杂度 O(h)（递归栈，h 为树高）。
 */
BTNode *btree_create_from_bracket(const char *text)
{
    const char *cursor;
    BTNode *root;  //根节点

    if (text == NULL) {
        return NULL;  // 参数非法，返回 NULL
    }

    cursor = text;  //传参，指针指向括号表达式的首字符
    root = parse_node(&cursor);  //解析整棵树，返回根结点
    if (root == NULL) {
        return NULL;  // 语法错误或空串，返回 NULL
    }
    skip_spaces(&cursor);
    if (*cursor != '\0') {
        /* 第一棵树之后还有多余字符：整体视为非法输入。 */
        btree_destroy(root);
        return NULL;
    }
    return root;  // 返回根结点
}

/**
 * @brief 后序销毁整棵树。
 *
 * @param root 树根；为 NULL 时直接返回。
 *
 * 必须先递归销毁左右子树，再释放当前结点；否则会丢失孩子指针。
 * 时间复杂度 O(n)，递归深度 O(h)。
 */
void btree_destroy(BTNode *root)
{
    if (root == NULL) {
        return;
    }
    btree_destroy(root->left);  //递归销毁左子树
    btree_destroy(root->right);  //递归销毁右子树
    ds_free((void **)&root);  //释放当前结点并置 NULL，避免悬空指针
}

/**
 * @brief 在一般二叉树中查找指定字符。
 *
 * @param root  树根。
 * @param value 目标字符。
 * @return 命中返回结点指针；未找到返回 NULL。
 *
 * 先比较根，再查左子树，最后查右子树。因为不是二叉排序树，不能根据
 * 字符大小排除任何分支。最坏时间复杂度 O(n)。
 */
BTNode *btree_find(BTNode *root, char value)
{
    BTNode *found;

    if (root == NULL) {
        return NULL;
    }
    if (root->data == value) {
        return root;
    }
    found = btree_find(root->left, value);
    if (found != NULL) {
        return found;
    }
    return btree_find(root->right, value);
}

/**
 * @brief 求指定结点的双亲。
 *
 * @param root  树根。
 * @param value 孩子结点的字符。
 * @return 双亲结点指针；根结点或未找到时返回 NULL。
 *
 * 判断当前结点是否是目标结点的双亲，如果是就返回；否则递归到左右
 * 子树继续找。注意根没有双亲，返回值 NULL 同时表示“根”和“不存在”，
 * 调用方需要先用 btree_find 确认目标存在。
 *
 * 最坏时间复杂度 O(n)。
 */
BTNode *btree_parent(BTNode *root, char value)
{
    BTNode *found;

    if (root == NULL) {
        return NULL;
    }
    if ((root->left != NULL && root->left->data == value) ||
        (root->right != NULL && root->right->data == value)) {
        return root;  //在当前结点找到了目标结点的双亲，返回当前结点
    }
    found = btree_parent(root->left, value);
    if (found != NULL) {
        return found;  //在左子树找到了目标结点的双亲，返回找到的结点
    }
    return btree_parent(root->right, value);  //在右子树找到了目标结点的双亲，返回找到的结点
}

/**
 * @brief 求二叉树的高度。
 *
 * @param root 树根。
 * @return 空树为 0，只有根为 1，其余为 max(左高, 右高) + 1。
 *
 * 后序遍历思想：先拿到左右子树高度，再合并。时间复杂度 O(n)。
 */
int btree_height(const BTNode *root)
{
    int left_height;
    int right_height;

    if (root == NULL) {
        return 0;
    }
    left_height = btree_height(root->left);
    right_height = btree_height(root->right);
    return 1 + (left_height > right_height ? left_height : right_height);
}

/**
 * @brief 统计结点总数。
 *
 * @param root 树根。
 * @return 结点个数；空树为 0。
 *
 * 递归式：count(root) = 1 + count(left) + count(right)。
 * 时间复杂度 O(n)。
 */
int btree_node_count(const BTNode *root)
{
    if (root == NULL) {
        return 0;
    }
    return 1 + btree_node_count(root->left) + btree_node_count(root->right);
}

/**
 * @brief 统计叶子结点数。
 *
 * @param root 树根。
 * @return 叶子个数；空树为 0。
 *
 * 叶子定义：左右孩子都为 NULL。时间复杂度 O(n)。
 */
int btree_leaf_count(const BTNode *root)
{
    if (root == NULL) {
        return 0;
    }
    if (root->left == NULL && root->right == NULL) {
        return 1;
    }
    return btree_leaf_count(root->left) + btree_leaf_count(root->right);
}

/**
 * @brief 先序遍历：根 -> 左 -> 右。
 *
 * @param root    树根。
 * @param visit   访问回调；可以为 NULL（只遍历不做事）。
 * @param context 透传给回调的上下文。
 *
 * 时间复杂度 O(n)，递归深度 O(h)。
 */
void btree_preorder(const BTNode *root, BTVisitFn visit, void *context)
{
    if (root == NULL) {
        return;
    }
    if (visit != NULL) {
        visit(root, context);
    }
    btree_preorder(root->left, visit, context);
    btree_preorder(root->right, visit, context);
}

/**
 * @brief 中序遍历：左 -> 根 -> 右。
 *
 * @param root    树根。
 * @param visit   访问回调；可以为 NULL。
 * @param context 透传给回调的上下文。
 *
 * 对二叉排序树，中序序列是递增序列；本模块处理一般二叉树，只保证
 * 遍历顺序符合定义。
 */
void btree_inorder(const BTNode *root, BTVisitFn visit, void *context)
{
    if (root == NULL) {
        return;
    }
    btree_inorder(root->left, visit, context);
    if (visit != NULL) {
        visit(root, context);
    }
    btree_inorder(root->right, visit, context);
}

/**
 * @brief 后序遍历：左 -> 右 -> 根。
 *
 * @param root    树根。
 * @param visit   访问回调；可以为 NULL。
 * @param context 透传给回调的上下文。
 *
 * 后序常用于“先处理孩子再处理父结点”的场景，例如销毁树、计算表达式。
 */
void btree_postorder(const BTNode *root, BTVisitFn visit, void *context)
{
    if (root == NULL) {
        return;
    }
    btree_postorder(root->left, visit, context);
    btree_postorder(root->right, visit, context);
    if (visit != NULL) {
        visit(root, context);
    }
}

/**
 * @brief 层次遍历：从上到下、从左到右。
 *
 * @param root    树根。
 * @param visit   访问回调；可以为 NULL。
 * @param context 透传给回调的上下文。
 *
 * 用数组模拟队列：
 *   - head 指向下一个出队元素，tail 指向下一个入队位置；
 *   - 根先入队；
 *   - 每次出队一个结点，访问后把非空左右孩子依次入队。
 *
 * 队列容量取结点数 + 1，保证最坏情况下（满二叉树最后一层）也不会
 * 溢出。时间复杂度 O(n)，空间复杂度 O(n)。
 */
void btree_levelorder(const BTNode *root, BTVisitFn visit, void *context)
{
    BTNode **queue;
    size_t capacity;
    size_t head = 0;
    size_t tail = 0;
    int count;
    size_t i;

    if (root == NULL) {
        return;
    }

    count = btree_node_count(root);
    capacity = (size_t)count + 1;
    queue = (BTNode **)ds_malloc(capacity * sizeof(BTNode *));
    queue[tail++] = (BTNode *)root;

    while (head < tail) {
        BTNode *node = queue[head++];
        if (visit != NULL) {
            visit(node, context);
        }
        if (node->left != NULL) {
            queue[tail++] = node->left;
        }
        if (node->right != NULL) {
            queue[tail++] = node->right;
        }
    }

    /* 释放前清空指针，便于调试时一眼看出队列已失效。 */
    for (i = 0; i < tail; ++i) {
        queue[i] = NULL;
    }
    ds_free((void **)&queue);
}

/**
 * @brief 路径搜索的上下文。
 *
 * path     调用者提供的输出缓冲区；
 * capacity 缓冲区字节数；
 * length   命中时写入路径长度（字符数，不含 '\0'）；
 * overflow 缓冲区不够时置 true，用于区分“没找到”和“路径太长”。
 */
typedef struct PathContext {
    char *path;
    size_t capacity;
    int length;
    bool overflow;
} PathContext;

/**
 * @brief 递归查找从当前结点到目标结点的路径。
 *
 * @param node    当前结点。
 * @param value   目标字符。
 * @param depth   当前结点在路径中的下标。
 * @param context 路径上下文。
 * @return 命中返回 true；未找到或缓冲区不足返回 false。
 *
 * 采用先序遍历：进入结点时把 node->data 写入 path[depth]，若命中就
 * 记录长度；否则继续查左右子树。找到后立即回溯，不再访问其他分支。
 *
 * 注意 path 不写 '\0'，调用者通过 length 判断有效长度。
 */
static bool path_search(const BTNode *node, char value, int depth,
                        PathContext *context)
{
    if (node == NULL) {
        return false;
    }
    if ((size_t)depth >= context->capacity) {
        /* 路径长度超过缓冲区，记录溢出标志。 */
        context->overflow = true;
        return false;
    }

    context->path[depth] = node->data;
    if (node->data == value) {
        context->length = depth + 1;
        return true;
    }
    if (path_search(node->left, value, depth + 1, context)) {
        return true;
    }
    return path_search(node->right, value, depth + 1, context);
}

/**
 * @brief 求根到指定结点的路径。
 *
 * @param root      树根。
 * @param value     目标字符。
 * @param path      输出缓冲区。
 * @param path_size 缓冲区字节数。
 * @param length    输出路径长度（字符数）。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；目标不存在返回
 *         DS_ERR_NOT_FOUND；缓冲区不足返回 DS_ERR_RANGE。
 *
 * 路径按“根 -> ... -> 目标”的顺序写入，例如 A(B(D,E),C(F,G)) 中
 * 根到 E 的输出为 "ABE"，length = 3。
 */
DsStatus btree_path_to(const BTNode *root, char value, char *path,
                       size_t path_size, int *length)
{
    PathContext context;

    if (root == NULL || path == NULL || length == NULL || path_size == 0) {
        return DS_ERR_NULL;
    }

    context.path = path;
    context.capacity = path_size;
    context.length = 0;
    context.overflow = false;

    if (!path_search(root, value, 0, &context)) {
        return context.overflow ? DS_ERR_RANGE : DS_ERR_NOT_FOUND;
    }
    *length = context.length;
    return DS_OK;
}

/**
 * @brief 求任意两个结点之间的路径。
 *
 * @param root      树根。
 * @param a         起点字符。
 * @param b         终点字符。
 * @param path      输出缓冲区。
 * @param path_size 缓冲区字节数。
 * @param length    输出路径长度（字符数）。
 * @return DS_OK；参数非法返回 DS_ERR_NULL；结点不存在返回
 *         DS_ERR_NOT_FOUND；缓冲区不足返回 DS_ERR_RANGE。
 *
 * 算法：
 *   1. 分别求根到 a、根到 b 的路径 path_a、path_b；
 *   2. 从头比较两条路径，找到最后一个公共结点（最近公共祖先 LCA）；
 *   3. 从 a 回溯到 LCA（包含 LCA），再沿 path_b 走到 b（不重复 LCA）。
 *
 * 例：a=D，b=F，path_a=A,B,D，path_b=A,C,F，公共前缀长度 1（A），
 * 输出 D,B,A,C,F。
 *
 * 时间复杂度 O(n)，空间复杂度 O(h)。
 */
DsStatus btree_path_between(const BTNode *root, char a, char b, char *path,
                            size_t path_size, int *length)
{
    int height;
    char *path_a;
    char *path_b;
    int length_a = 0;
    int length_b = 0;
    int common = 0;
    int i;
    int index = 0;
    DsStatus status;

    if (root == NULL || path == NULL || length == NULL) {
        return DS_ERR_NULL;
    }

    /* 任何根到结点的路径长度都不会超过树高，+1 留给结束符。 */
    height = btree_height(root);
    path_a = (char *)ds_malloc((size_t)height + 1);
    path_b = (char *)ds_malloc((size_t)height + 1);

    status = btree_path_to(root, a, path_a, (size_t)height + 1, &length_a);
    if (status == DS_OK) {
        status = btree_path_to(root, b, path_b, (size_t)height + 1, &length_b);
    }
    if (status != DS_OK) {
        ds_free((void **)&path_a);
        ds_free((void **)&path_b);
        return status;
    }

    /* 找最长公共前缀；common 是第一个不同字符的下标。 */
    while (common < length_a && common < length_b &&
           path_a[common] == path_b[common]) {
        ++common;
    }

    /*
     * 输出长度 = a 到 LCA 的边数 + LCA 到 b 的边数 + 1 个结点。
     * 即 length_a - common + length_b - common + 1。
     */
    if ((size_t)(length_a + length_b - 2 * common + 1) > path_size) {
        ds_free((void **)&path_a);
        ds_free((void **)&path_b);
        return DS_ERR_RANGE;
    }

    /* 从 a 向上走到 LCA（common-1 是 LCA 的下标）。 */
    for (i = length_a - 1; i >= common - 1; --i) {
        path[index++] = path_a[i];
    }
    /* 从 LCA 的下一个结点向下走到 b。 */
    for (i = common; i < length_b; ++i) {
        path[index++] = path_b[i];
    }

    ds_free((void **)&path_a);
    ds_free((void **)&path_b);
    *length = index;
    return DS_OK;
}

/**
 * @brief 递归交换每个结点的左右子树。
 *
 * @param root 树根；为 NULL 时直接返回。
 *
 * 先交换当前结点的两个孩子，再递归处理左右子树。交换后整棵树的
 * 先序/中序/后序序列都会变化，但结点数和高度不变。
 *
 * 时间复杂度 O(n)。
 */
void btree_swap_children(BTNode *root)
{
    BTNode *temp;

    if (root == NULL) {
        return;
    }
    temp = root->left;
    root->left = root->right;
    root->right = temp;
    btree_swap_children(root->left);
    btree_swap_children(root->right);
}

/**
 * @brief 按括号表示法输出二叉树。
 *
 * @param stream 输出文件流；为 NULL 时直接返回。
 * @param root   树根；为 NULL 时什么也不输出。
 *
 * 输出规则：
 *   - 叶子只输出结点字符；
 *   - 只有左孩子：A(B)；
 *   - 只有右孩子：A(,B)（逗号前留空，表示左孩子为空）；
 *   - 两个孩子：A(B,C)。
 *
 * 时间复杂度 O(n)。
 */
void btree_print_bracket(FILE *stream, const BTNode *root)
{
    if (stream == NULL || root == NULL) {
        return;
    }

    fputc(root->data, stream);
    if (root->left != NULL || root->right != NULL) {
        fputc('(', stream);
        if (root->left != NULL) {
            btree_print_bracket(stream, root->left);
        }
        if (root->right != NULL) {
            fputc(',', stream);
            btree_print_bracket(stream, root->right);
        }
        fputc(')', stream);
    }
}
