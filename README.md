# 数据结构与算法课程设计（C11 + MinGW-w64）

本工程是《数据结构与算法分析》课程设计的基础代码库。所有数据结构以静态库
`build/libds.a` 的形式提供，四道题目各自有独立的菜单程序，后续只需在
`apps/labXX/main.c` 中填写业务回调。

## 1. 目录结构

```
sq/
├─ include/ds/          公共头文件（ADT 接口与注释）
├─ src/                 基础库实现
├─ apps/
│  ├─ lab01/            题目一：线性表（已实现）
│  ├─ lab02/            题目二：二叉树（已实现）
│  ├─ lab03/            题目三：图（已实现）
│  ├─ lab04/            题目四：查找与排序（已实现）
│  └─ smoke/            基础库自检程序
├─ data/
│  ├─ f1.txt            4615 条学生记录
│  ├─ graph_bfs.txt     5 结点无向图（DFS/BFS 示例）
│  ├─ graph_mst.txt     7 结点带权无向图（Prim/Kruskal 示例）
│  └─ graph_dijkstra.txt 7 结点有向带权图（Dijkstra 示例）
├─ build/               编译产物（自动生成，已加入 .gitignore）
├─ Makefile
└─ .vscode/             VS Code 构建、调试与 IntelliSense 配置
```

## 2. 环境要求

- Windows 10/11
- MinGW-w64 GCC 8.1.0 或更高版本（当前工程使用
  `D:\MinGW64\x86_64-8.1.0-release-posix-sjlj-rt_v6-rev0\mingw64`）
- `mingw32-make`（随 MinGW 提供）
- VS Code + C/C++ 扩展（`ms-vscode.cpptools`）

确认工具链：

```powershell
gcc --version
mingw32-make --version
gdb --version
```

本工程没有使用 CMake，也不需要 MSVC 或 `make`。

## 3. 编译与运行

在工程根目录执行：

```powershell
mingw32-make all          # 编译静态库和 5 个可执行文件
mingw32-make run-smoke    # 运行基础库自检（应全部通过）
mingw32-make run-lab01    # 运行题目一菜单
mingw32-make run-lab02    # 运行题目二菜单
mingw32-make run-lab03    # 运行题目三菜单
mingw32-make run-lab04    # 运行题目四菜单
mingw32-make clean        # 删除 build 目录
```

可执行文件位于：

```
build/libds.a
build/bin/smoke.exe
build/bin/lab01.exe
build/bin/lab02.exe
build/bin/lab03.exe
build/bin/lab04.exe
```

> 程序启动时会自动定位工程根目录并切换过去，因此从工程根目录、`build` 或
> `build/bin` 运行都能正确读取 `data/`。VS Code 的 `tasks.json` 和
> `launch.json` 也已把工作目录设为 `${workspaceFolder}`。
>
> 题目四需要复现同一组随机样本时，可设置环境变量 `LAB04_SEED`（例如
> `$env:LAB04_SEED=12345`）；不设置则使用当前时间作种子。

### VS Code 操作

- `Ctrl+Shift+B`：执行默认构建任务“构建全部”。
- `Ctrl+Shift+P` → `Tasks: Run Task`：运行自检或任一题目。
- `F5`：从 `.vscode/launch.json` 选择调试配置。
- `C/C++` 扩展的 IntelliSense 配置在 `.vscode/c_cpp_properties.json`。

## 4. 基础库模块

| 头文件 | 模块 | 主要内容 |
| --- | --- | --- |
| `ds_types.h` | 公共类型 | `DsStatus` 状态码、`DsCompareFn`、通用宏 |
| `ds_util.h` | 工具 | 字符串、计时、随机数、洗牌 |
| `ds_memory.h` | 内存 | 带失败检查的 `malloc/calloc/realloc/free` |
| `ds_console.h` | 控制台 | UTF-8 初始化、清屏、标题、分页、对齐输出 |
| `ds_input.h` | 输入 | 整行读取、整数/浮点/是非题校验 |
| `ds_menu.h` | 菜单 | 通用控制台菜单框架 |
| `ds_path.h` | 路径 | 定位工程根目录并切换工作目录，保证 `data/` 相对路径可用 |
| `student.h` | 学生记录 | f1.txt 解析/保存、加权成绩、比较函数、表格输出 |
| `ds_seqlist.h` | 顺序表 | 动态数组，1 基位置，插入/删除/查找/前驱/排序 |
| `ds_linklist.h` | 单链表 | 带头结点，头插/尾插、插入/删除/查找/逆置/排序 |
| `ds_stack.h` | 顺序栈 | `int` 元素，动态扩容 |
| `ds_queue.h` | 循环队列 | 牺牲一个槽位区分空/满 |
| `ds_btree.h` | 二叉树 | 括号表示法建树、四种遍历、双亲、高度、路径、交换子树 |
| `ds_graph.h` | 图 | 邻接矩阵 + 邻接表、DFS/BFS、Dijkstra、Prim、Kruskal |
| `ds_hash.h` | 哈希表 | 学号哈希、链地址法、ASL 统计 |
| `ds_sort.h` | 排序 | 冒泡/插入/选择/希尔/快速/归并/堆，带比较与移动统计 |
| `ds_search.h` | 查找 | 顺序、非递归折半、递归折半，带比较次数 |

约定：

- 所有可能失败的接口返回 `DsStatus`，`DS_OK == 0`。
- 顺序表/链表的位置参数统一使用 **1 基**下标。
- 链表带 **头结点**，`head->next` 是第一个数据结点。
- 图顶点使用 **0 基**下标；无向图的一条边只计一次。
- 排序/查找使用 `void * + 元素大小 + 比较函数`，可直接用于 `Student` 或其他类型。
- 各题目启动时调用 `ds_path_set_project_root()`，从工程根目录、`build` 或
  `build/bin` 运行都能正确解析 `data/` 下的相对路径。

## 5. 数据文件格式

### f1.txt

每行 11 个字段，空格分隔：

```
学号 姓名 专业编号 通识1 通识2 通识3 公共基础1 公共基础2 公共基础3 专业主干1 专业主干2
```

加权成绩：

```
weights = 0.8 × (公共基础1 + 公共基础2 + 公共基础3)
        + 1.2 × (专业主干1 + 专业主干2)
```

满分为 480。加载器会跳过空行、`#` 注释行和格式不合法的行，并在标准错误
输出汇总警告。

### 图文件

```
顶点数 边数 是否有向(0/1)
顶点0名字
顶点1名字
...
u v w
...
```

`#` 开头为注释，空行忽略。示例见 `data/graph_*.txt`。

## 6. 当前状态

- [x] 基础库 16 个模块实现完毕，`build/libds.a` 编译通过。
- [x] 自检程序覆盖学生文件、顺序表、链表、栈、队列、二叉树、图、哈希、
  7 种排序、3 种查找；当前全部通过。
- [x] 四道题的主程序与 VS Code 构建/调试配置。
- [x] 题目一：头插/尾插建表、输出、按位插入、求前驱、按位/按学号删除、
  加权降序排序、保存、顺序表/链表对比。
- [x] 题目二：括号表示法建树/输出、四种遍历、树高/结点/叶子统计、
  求双亲、任意两结点路径（功能 10“交换左右子树”暂不开放）。
- [x] 题目三：加载/查看图、邻接矩阵/邻接表、DFS/BFS、Dijkstra、
  Prim/Kruskal、连通性判断。
- [x] 题目四：随机抽取 100 条、冒泡/快速排序、顺序/折半/哈希查找、
  ASL 成功/失败统计、7 种排序算法性能对比。
- [ ] 课程设计报告。

## 7. 自检结果摘要

`build/bin/smoke.exe` 会输出逐项 `[ OK ]`，并给出若干关键数据：

- f1.txt：4615 条记录。
- 顺序表/链表：插入、删除、前驱、按加权降序排序均正确。
- 二叉树 `A(B(D,E),C(F,G))`：先序 `ABDECFG`、中序 `DBEAFCG`、
  后序 `DEBFGCA`、层次 `ABCDEFG`；D 到 F 路径 `D-B-A-C-F`。
- 图：DFS `01342`、BFS `01234`；Prim/Kruskal 总权 92；
  Dijkstra 0→4 = 10、0→6 = 16。
- 哈希表：1009 桶，装填因子 4.5738，ASL 成功约 2.90。
- 100 条样本：7 种排序均正确，顺序/折半查找均命中。
