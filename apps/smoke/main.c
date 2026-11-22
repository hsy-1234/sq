/**
 * @file    main.c
 * @brief   基础库自检程序：逐项验证各模块，全部通过返回 0。
 *
 * 运行方式（工程根目录）：
 *   build\bin\smoke.exe
 * 或：
 *   mingw32-make run-smoke
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ds/ds_types.h"
#include "ds/ds_util.h"
#include "ds/ds_memory.h"
#include "ds/ds_console.h"
#include "ds/student.h"
#include "ds/ds_seqlist.h"
#include "ds/ds_linklist.h"
#include "ds/ds_stack.h"
#include "ds/ds_queue.h"
#include "ds/ds_btree.h"
#include "ds/ds_graph.h"
#include "ds/ds_hash.h"
#include "ds/ds_sort.h"
#include "ds/ds_search.h"

static int failures = 0;

static void check(int condition, const char *message)
{
    if (condition) {
        printf("[ OK ] %s\n", message);
    } else {
        printf("[FAIL] %s\n", message);
        ++failures;
    }
}

static void check_status(DsStatus actual, DsStatus expected, const char *message)
{
    if (actual == expected) {
        printf("[ OK ] %s\n", message);
    } else {
        printf("[FAIL] %s（期望 %d，实际 %d：%s）\n", message,
               (int)expected, (int)actual, ds_status_text(actual));
        ++failures;
    }
}

static void check_double(double actual, double expected, const char *message)
{
    if (fabs(actual - expected) < 1e-9) {
        printf("[ OK ] %s（%.4f）\n", message, actual);
    } else {
        printf("[FAIL] %s（期望 %.4f，实际 %.4f）\n", message, expected, actual);
        ++failures;
    }
}

/* ------------------------------------------------------------------------- */
/* 遍历访问器                                                                 */
/* ------------------------------------------------------------------------- */

static char seq[128];
static size_t seq_len = 0;

static void seq_reset(void)
{
    seq_len = 0;
    seq[0] = '\0';
}

static void seq_tree_char(const BTNode *node, void *context)
{
    DS_UNUSED(context);
    if (node != NULL && seq_len + 1 < sizeof(seq)) {
        seq[seq_len++] = node->data;
        seq[seq_len] = '\0';
    }
}

static void seq_vertex(int vertex, void *context)
{
    DS_UNUSED(context);
    if (seq_len + 1 < sizeof(seq)) {
        seq[seq_len++] = (char)('0' + vertex);
        seq[seq_len] = '\0';
    }
}

/* ------------------------------------------------------------------------- */
/* 各模块测试                                                                 */
/* ------------------------------------------------------------------------- */

static void test_students(Student **out_students, size_t *out_count)
{
    Student *students = NULL;
    size_t count = 0;
    DsStatus status;

    ds_console_subtitle("1. 学生数据文件");
    status = student_load_file("data/f1.txt", &students, &count);
    check_status(status, DS_OK, "加载 data/f1.txt");
    check(count == 4615, "记录条数应为 4615");
    if (status != DS_OK || count == 0) {
        *out_students = students;
        *out_count = count;
        return;
    }

    check(strlen(students[0].ID) == 5, "首条记录学号为 5 位");
    check(students[0].weights >= 0.0f && students[0].weights <= 480.0f,
          "首条记录加权成绩在 [0,480] 内");
    printf("       首条：%s %s 专业%s 加权%.2f\n",
           students[0].ID, students[0].name, students[0].pro_number,
           students[0].weights);

    *out_students = students;
    *out_count = count;
}

static void test_seqlist(const Student *students, size_t count)
{
    SeqList list;
    Student value;
    size_t pos = 0;

    ds_console_subtitle("2. 顺序表");
    check_status(seqlist_init(&list, 8), DS_OK, "初始化顺序表");
    check_status(seqlist_assign(&list, students, count), DS_OK, "装入全部学生");
    check(list.length == count, "顺序表长度与文件一致");

    check_status(seqlist_find_by_id(&list, students[100].ID, &pos, &value),
                 DS_OK, "按学号查找第 101 条");
    check(pos == 101, "第 101 条的位置应为 101");
    check(strcmp(value.ID, students[100].ID) == 0, "查找结果学号一致");

    check_status(seqlist_predecessor(&list, students[100].ID, &value),
                 DS_OK, "求第 101 条的前驱");
    check(strcmp(value.ID, students[99].ID) == 0, "前驱应为第 100 条");

    check_status(seqlist_insert(&list, 1, students[0]), DS_OK, "在第 1 位插入");
    check(list.length == count + 1, "插入后长度加一");
    check_status(seqlist_get(&list, 1, &value), DS_OK, "读取第 1 位");
    check(strcmp(value.ID, students[0].ID) == 0, "插入位置正确");

    check_status(seqlist_delete_at(&list, 1, &value), DS_OK, "删除第 1 位");
    check(list.length == count, "删除后长度恢复");

    check_status(seqlist_delete_by_id(&list, students[200].ID, &value),
                 DS_OK, "按学号删除第 201 条");
    check(list.length == count - 1, "按学号删除后长度减一");
    check_status(seqlist_find_by_id(&list, students[200].ID, &pos, NULL),
                 DS_ERR_NOT_FOUND, "被删除学号已不存在");

    check_status(seqlist_assign(&list, students, count), DS_OK, "重新装入全部学生");
    seqlist_sort_by_weight_desc(&list);
    check(ds_sort_is_sorted(list.data, list.length, sizeof(Student),
                            student_compare_weight_desc),
          "顺序表按加权成绩降序排列");
    printf("       最高加权：%.2f，最低加权：%.2f\n",
           list.data[0].weights, list.data[list.length - 1].weights);

    seqlist_destroy(&list);
    check(list.data == NULL && list.length == 0, "销毁顺序表");
}

static void test_linklist(const Student *students, size_t count)
{
    LinkList list;
    Student value;
    size_t loaded = 0;
    LNode *node;

    ds_console_subtitle("3. 单链表");
    check_status(linklist_init(&list), DS_OK, "初始化带头结点单链表");
    check_status(linklist_load_file(&list, "data/f1.txt", &loaded), DS_OK,
                 "尾插法建表");
    check(loaded == count && list.length == count, "链表长度与文件一致");

    node = linklist_find_node(&list, students[300].ID);
    check(node != NULL && strcmp(node->data.ID, students[300].ID) == 0,
          "按学号查找第 301 条");

    check_status(linklist_predecessor(&list, students[300].ID, &value),
                 DS_OK, "求第 301 条的前驱");
    check(strcmp(value.ID, students[299].ID) == 0, "前驱应为第 300 条");

    check_status(linklist_insert(&list, 1, students[0]), DS_OK, "在第 1 位插入");
    check(list.length == count + 1, "插入后链表长度加一");
    check_status(linklist_get(&list, 1, &value), DS_OK, "读取第 1 位");
    check(strcmp(value.ID, students[0].ID) == 0, "插入位置正确");
    check_status(linklist_delete_at(&list, 1, &value), DS_OK, "删除第 1 位");

    check_status(linklist_delete_by_id(&list, students[400].ID, &value),
                 DS_OK, "按学号删除第 401 条");
    check(list.length == count - 1, "按学号删除后长度减一");

    linklist_sort_by_weight_desc(&list);
    check(linklist_get(&list, 1, &value) == DS_OK, "排序后读取首结点");
    check(value.weights >= list.head->next->next->data.weights,
          "链表按加权成绩降序排列");
    printf("       最高加权：%.2f\n", value.weights);

    linklist_reverse(&list);
    check(list.length == count - 1, "逆置不改变链表长度");
    check(linklist_get(&list, 1, &value) == DS_OK, "逆置后读取首结点");
    printf("       逆置后首结点：%s\n", value.ID);

    linklist_destroy(&list);
    check(list.head == NULL && list.length == 0, "销毁链表");
}

static void test_stack_queue(void)
{
    IntStack stack;
    IntQueue queue;
    int value = 0;

    ds_console_subtitle("4. 栈与队列");
    check_status(stack_init(&stack, 4), DS_OK, "初始化栈");
    check_status(stack_push(&stack, 1), DS_OK, "入栈 1");
    check_status(stack_push(&stack, 2), DS_OK, "入栈 2");
    check_status(stack_push(&stack, 3), DS_OK, "入栈 3");
    check_status(stack_push(&stack, 4), DS_OK, "入栈 4");
    check(stack_size(&stack) == 4, "栈大小为 4");
    check_status(stack_pop(&stack, &value), DS_OK, "出栈");
    check(value == 4, "后进先出：先出 4");
    check_status(stack_peek(&stack, &value), DS_OK, "读栈顶");
    check(value == 3, "栈顶为 3");
    stack_clear(&stack);
    check(stack_is_empty(&stack), "清空后栈为空");
    check_status(stack_pop(&stack, &value), DS_ERR_EMPTY, "空栈出栈报错");
    stack_destroy(&stack);

    check_status(queue_init(&queue, 4), DS_OK, "初始化循环队列");
    check_status(queue_enqueue(&queue, 1), DS_OK, "入队 1");
    check_status(queue_enqueue(&queue, 2), DS_OK, "入队 2");
    check_status(queue_enqueue(&queue, 3), DS_OK, "入队 3");
    check_status(queue_enqueue(&queue, 4), DS_ERR_FULL, "队满时入队报错");
    check_status(queue_dequeue(&queue, &value), DS_OK, "出队");
    check(value == 1, "先进先出：先出 1");
    check_status(queue_enqueue(&queue, 4), DS_OK, "出队后重新入队 4");
    check_status(queue_dequeue(&queue, &value), DS_OK, "出队");
    check(value == 2, "第二个出队元素为 2");
    check_status(queue_dequeue(&queue, &value), DS_OK, "出队");
    check(value == 3, "第三个出队元素为 3");
    check_status(queue_dequeue(&queue, &value), DS_OK, "出队");
    check(value == 4, "第四个出队元素为 4");
    check(queue_is_empty(&queue), "队列已空");
    queue_destroy(&queue);
}

static void test_btree(void)
{
    BTNode *root;
    BTNode *parent;
    char path[32];
    int length = 0;
    FILE *temp;

    ds_console_subtitle("5. 二叉树");
    root = btree_create_from_bracket("A(B(D,E),C(F,G))");
    check(root != NULL, "括号表示法建树");
    if (root == NULL) {
        return;
    }

    check(btree_height(root) == 3, "树高为 3");
    check(btree_node_count(root) == 7, "结点数为 7");
    check(btree_leaf_count(root) == 4, "叶子数为 4");

    parent = btree_parent(root, 'E');
    check(parent != NULL && parent->data == 'B', "E 的双亲为 B");
    check(btree_parent(root, 'A') == NULL, "根结点没有双亲");

    seq_reset();
    btree_preorder(root, seq_tree_char, NULL);
    check(strcmp(seq, "ABDECFG") == 0, "先序遍历 ABDECFG");
    seq_reset();
    btree_inorder(root, seq_tree_char, NULL);
    check(strcmp(seq, "DBEAFCG") == 0, "中序遍历 DBEAFCG");
    seq_reset();
    btree_postorder(root, seq_tree_char, NULL);
    check(strcmp(seq, "DEBFGCA") == 0, "后序遍历 DEBFGCA");
    seq_reset();
    btree_levelorder(root, seq_tree_char, NULL);
    check(strcmp(seq, "ABCDEFG") == 0, "层次遍历 ABCDEFG");

    check_status(btree_path_to(root, 'E', path, sizeof(path), &length), DS_OK,
                 "求根到 E 的路径");
    path[length] = '\0';
    check(length == 3 && strcmp(path, "ABE") == 0, "路径为 A-B-E");

    check_status(btree_path_between(root, 'D', 'F', path, sizeof(path), &length),
                 DS_OK, "求 D 到 F 的路径");
    path[length] = '\0';
    check(length == 5 && strcmp(path, "DBACF") == 0, "路径为 D-B-A-C-F");

    temp = fopen("btree_test.tmp", "w+");
    if (temp != NULL) {
        btree_print_bracket(temp, root);
        rewind(temp);
        if (fgets(path, (int)sizeof(path), temp) != NULL) {
            path[strcspn(path, "\r\n")] = '\0';
            check(strcmp(path, "A(B(D,E),C(F,G))") == 0, "括号表示法输出");
        } else {
            check(0, "读取括号表示法输出");
        }
        fclose(temp);
        remove("btree_test.tmp");
    } else {
        check(0, "创建括号表示法临时文件");
    }

    btree_swap_children(root);
    seq_reset();
    btree_preorder(root, seq_tree_char, NULL);
    check(strcmp(seq, "ACGFBED") == 0, "交换左右子树后先序 ACGFBED");

    btree_destroy(root);
}

static void test_graph(void)
{
    Graph graph;
    GraphEdge mst[16];
    int mst_count = 0;
    double total = 0.0;
    double dist[7];
    int prev[7];
    int i;

    ds_console_subtitle("6. 图：遍历");
    check_status(graph_load_file(&graph, "data/graph_bfs.txt"), DS_OK,
                 "加载 5 结点无向图");
    check(graph.vertex_count == 5 && graph.edge_count == 5, "顶点数/边数正确");
    check(graph_is_connected(&graph), "无向图连通");

    seq_reset();
    check_status(graph_dfs(&graph, 0, seq_vertex, NULL), DS_OK, "DFS");
    check(strcmp(seq, "01342") == 0, "DFS 序列 01342");
    seq_reset();
    check_status(graph_bfs(&graph, 0, seq_vertex, NULL), DS_OK, "BFS");
    check(strcmp(seq, "01234") == 0, "BFS 序列 01234");
    graph_destroy(&graph);

    ds_console_subtitle("7. 图：最小生成树");
    check_status(graph_load_file(&graph, "data/graph_mst.txt"), DS_OK,
                 "加载 7 结点带权无向图");
    check_status(graph_prim(&graph, 0, mst, &mst_count, &total), DS_OK, "Prim");
    check(mst_count == 6, "Prim 选出 6 条边");
    check_double(total, 92.0, "Prim 总权为 92");
    check_status(graph_kruskal(&graph, mst, &mst_count, &total), DS_OK, "Kruskal");
    check(mst_count == 6, "Kruskal 选出 6 条边");
    check_double(total, 92.0, "Kruskal 总权为 92");
    graph_destroy(&graph);

    ds_console_subtitle("8. 图：Dijkstra 最短路径");
    check_status(graph_load_file(&graph, "data/graph_dijkstra.txt"), DS_OK,
                 "加载 7 结点有向带权图");
    check_status(graph_dijkstra(&graph, 0, dist, prev), DS_OK, "Dijkstra");
    check_double(dist[4], 10.0, "0 到 4 的最短距离为 10");
    check_double(dist[6], 16.0, "0 到 6 的最短距离为 16");
    printf("       0 -> 4：");
    graph_print_path(&graph, prev, 0, 4);
    printf("       0 -> 6：");
    graph_print_path(&graph, prev, 0, 6);
    for (i = 0; i < graph.vertex_count; ++i) {
        if (dist[i] < DS_GRAPH_INF / 2.0) {
            printf("       dist[%d] = %.2f\n", i, dist[i]);
        }
    }
    graph_destroy(&graph);
}

static void test_hash(const Student *students, size_t count)
{
    StudentHash table;
    Student value;
    size_t comparisons = 0;
    size_t max_chain = 0;
    double asl_success = 0.0;
    double asl_fail = 0.0;
    size_t i;
    size_t inserted = 0;
    DsStatus status;

    ds_console_subtitle("9. 哈希表");
    check_status(student_hash_init(&table, 1009, NULL), DS_OK,
                 "初始化 1009 个桶的哈希表");
    for (i = 0; i < count; ++i) {
        status = student_hash_insert(&table, students[i]);
        if (status != DS_OK) {
            printf("       插入失败：%s（%s）\n", students[i].ID,
                   ds_status_text(status));
            break;
        }
        ++inserted;
    }
    check(inserted == count && table.count == count, "全部学生插入哈希表");

    check_status(student_hash_search(&table, students[123].ID, &value,
                                     &comparisons), DS_OK, "按学号查找");
    check(strcmp(value.ID, students[123].ID) == 0, "哈希查找结果正确");
    check(comparisons >= 1, "查找比较次数已统计");

    check_status(student_hash_search(&table, "99999", NULL, NULL),
                 DS_ERR_NOT_FOUND, "不存在的学号返回未找到");
    check_status(student_hash_insert(&table, students[0]), DS_ERR_DUPLICATE,
                 "重复插入返回重复错误");

    student_hash_stats(&table, &asl_success, &asl_fail, &max_chain);
    check(asl_success > 1.0 && asl_success < 10.0, "ASL 成功值合理");
    check(asl_fail > 0.0, "ASL 失败值大于 0");
    check(max_chain >= 1, "最长链已统计");
    student_hash_print_stats(&table);

    check_status(student_hash_delete(&table, students[0].ID, &value), DS_OK,
                 "删除首条记录");
    check(table.count == count - 1, "删除后记录数减一");
    check_status(student_hash_search(&table, students[0].ID, NULL, NULL),
                 DS_ERR_NOT_FOUND, "已删除学号找不到");
    student_hash_destroy(&table);
}

static void test_sort_search(const Student *students, size_t count)
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
    Student sample[100];
    Student work[100];
    Student by_id[100];
    DsSortStats stats;
    DsSearchResult result;
    const char *key;
    size_t i;

    ds_console_subtitle("10. 排序与查找（100 条随机样本）");
    check(count >= 100, "学生总数不少于 100");
    if (count < 100) {
        return;
    }
    memcpy(sample, students, sizeof(sample));
    ds_random_seed(20260701u);
    ds_shuffle(sample, 100, sizeof(Student));

    for (i = 0; i < DS_ARRAY_LEN(algorithms); ++i) {
        memcpy(work, sample, sizeof(work));
        memset(&stats, 0, sizeof(stats));
        algorithms[i].function(work, 100, sizeof(Student),
                               student_compare_weight_desc, &stats);
        check(ds_sort_is_sorted(work, 100, sizeof(Student),
                                student_compare_weight_desc),
              algorithms[i].name);
        ds_sort_stats_print(algorithms[i].name, &stats);
    }

    memcpy(by_id, students, sizeof(by_id));
    qsort(by_id, 100, sizeof(Student), student_compare_id);
    key = by_id[50].ID;

    result = ds_search_sequential(key, by_id, 100, sizeof(Student),
                                  student_compare_id_key);
    check(result.found && strcmp(by_id[result.index].ID, key) == 0,
          "顺序查找命中");
    printf("       顺序查找比较次数：%llu\n",
           (unsigned long long)result.comparisons);

    result = ds_search_binary(key, by_id, 100, sizeof(Student),
                              student_compare_id_key);
    check(result.found && strcmp(by_id[result.index].ID, key) == 0,
          "非递归折半查找命中");
    check(result.comparisons <= 7, "折半查找比较次数不超过 7");
    printf("       折半查找比较次数：%llu\n",
           (unsigned long long)result.comparisons);

    result = ds_search_binary_recursive(key, by_id, 100, sizeof(Student),
                                        student_compare_id_key);
    check(result.found && strcmp(by_id[result.index].ID, key) == 0,
          "递归折半查找命中");
    check(result.comparisons <= 7, "递归折半比较次数不超过 7");

    result = ds_search_sequential("99999", by_id, 100, sizeof(Student),
                                  student_compare_id_key);
    check(!result.found, "顺序查找未命中");
    result = ds_search_binary("99999", by_id, 100, sizeof(Student),
                              student_compare_id_key);
    check(!result.found, "折半查找未命中");
}

int main(void)
{
    Student *students = NULL;
    size_t count = 0;

    ds_console_init();
    ds_console_title("数据结构基础库自检");

    test_students(&students, &count);
    if (students != NULL) {
        test_seqlist(students, count);
        test_linklist(students, count);
        test_stack_queue();
        test_btree();
        test_graph();
        test_hash(students, count);
        test_sort_search(students, count);
        ds_free((void **)&students);
    }

    ds_console_rule('=', 60);
    if (failures == 0) {
        printf("全部测试通过。\n");
    } else {
        printf("共 %d 项测试失败。\n", failures);
    }
    ds_console_restore();
    return failures == 0 ? 0 : 1;
}
