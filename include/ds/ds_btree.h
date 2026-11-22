/**
 * @file    ds_btree.h
 * @brief   二叉链表与括号表示法建树。
 *
 * 支持教材常见的括号表示法，例如 "A(B(D,E),C(F,G))"。
 * 结点数据为单个字符；右孩子存在而左孩子为空时写作 "A(,B)"。
 */
#ifndef DS_BTREE_H
#define DS_BTREE_H

#include <stddef.h>
#include <stdio.h>
#include "ds/ds_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 二叉树结点。 */
typedef struct BTNode {
    char data;  //结点数据
    struct BTNode *left;
    struct BTNode *right;
} BTNode;

/** 遍历访问函数。 */
typedef void (*BTVisitFn)(const BTNode *node, void *context);

/**
 * @brief 由括号表示法创建二叉树。
 * @param text 例如 "A(B(D,E),C(F,G))"。
 * @return 根结点；空串或格式错误返回 NULL。
 */
BTNode *btree_create_from_bracket(const char *text);

/** @brief 释放整棵树，可传 NULL。 */
void btree_destroy(BTNode *root);

/** @brief 按数据值查找结点，未找到返回 NULL。 */
BTNode *btree_find(BTNode *root, char value);

/** @brief 求某结点的双亲；根结点或未找到返回 NULL。 */
BTNode *btree_parent(BTNode *root, char value);

/** @brief 树高；空树为 0，叶子为 1。 */
int btree_height(const BTNode *root);

/** @brief 结点总数。 */
int btree_node_count(const BTNode *root);

/** @brief 叶子结点数。 */
int btree_leaf_count(const BTNode *root);

/** @brief 先序遍历。 */
void btree_preorder(const BTNode *root, BTVisitFn visit, void *context);

/** @brief 中序遍历。 */
void btree_inorder(const BTNode *root, BTVisitFn visit, void *context);

/** @brief 后序遍历。 */
void btree_postorder(const BTNode *root, BTVisitFn visit, void *context);

/** @brief 层次遍历。 */
void btree_levelorder(const BTNode *root, BTVisitFn visit, void *context);

/**
 * @brief 求从根到指定结点的路径。（求结点高度时可用此函数，路径长度即高度。）
 * @param path      输出缓冲区，存放结点数据序列（不含 '\0'）。
 * @param path_size 缓冲区字节数。
 * @param length    返回路径上的结点个数。
 */
DsStatus btree_path_to(const BTNode *root, char value,
                       char *path, size_t path_size, int *length);

/**
 * @brief 求两个结点之间的路径（包含两个端点）。
 *
 * 结果形如 D,B,A,C,F，表示从 D 出发向上到 A，再向下到 F。
 */
DsStatus btree_path_between(const BTNode *root, char a, char b,
                            char *path, size_t path_size, int *length);

/** @brief 交换每个结点的左右子树。 */
void btree_swap_children(BTNode *root);

/** @brief 以括号表示法输出到指定文件流。 */
void btree_print_bracket(FILE *stream, const BTNode *root);

#ifdef __cplusplus
}
#endif

#endif /* DS_BTREE_H */
