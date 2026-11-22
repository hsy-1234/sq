# 趣味题：武汉轨道交通查询系统

本目录是趣味题“武汉市轨道交通网”的独立工程，和四道基础题隔离：

- 源码在 `fun/metro/`，不进入 `src/*.c`，也不加入 `build/libds.a`；
- 复用基础库的 `Graph`、`ds_menu`、`ds_console`、`ds_input`、`ds_path`；
- 根 Makefile 单独编译，输出 `build/bin/metro.exe` 和
  `build/bin/test_metro.exe`。

算法与数据结构的详细说明见 [docs/algorithms.md](docs/algorithms.md)。

## 1. 当前阶段

“解析建图 + 查询算法”阶段已经完成：

- [x] 数据模型：物理站点、线路、区段、停靠点、线路-站点节点；
- [x] 四张图的字段：时间图、距离图、换乘图、物理站点图；
- [x] 两遍扫描解析 `STATION/LINE/SEG/STOP/TRANSFER`，支持环线闭环边；
- [x] 网络生命周期、站点查找、票价函数、统计；
- [x] 线路概览、线路详情、站点信息、换乘站、线路-站点邻接表输出；
- [x] DFS/BFS/MST 对基础库的适配层；
- [x] 分层菜单、数据格式说明、导航对比文件读取；
- [x] 三种 `metro_plan()`：最快、最短、最少换乘；
- [x] 回归测试：票价边界、空网络、全量加载、三种路径、同站进出、遍历、MST；
- [x] 全量 13 条线路数据（298 站 / 352 个停靠点 / 49 个换乘站，含 12 号线环线）。

下一阶段：

- [ ] 补充与导航软件的真实对比数据；
- [ ] 补充更多换乘站、环线、支线的边界测试；
- [ ] 把测试结果整理进课程设计报告。

## 2. 目录

```text
fun/metro/
├─ include/ds/ds_metro.h     数据模型与接口声明
├─ src/ds_metro.c            解析建图、查询、票价、路径、统计、图算法适配
├─ docs/algorithms.md        算法与数据结构详解
├─ main.c                    分层菜单入口
├─ tests/test_metro.c        回归测试（加载/路径/遍历/MST）
└─ data/
   ├─ wuhan_metro.txt         全量 13 条线路数据
   ├─ wuhan_metro_stations.csv 站点坐标表
   ├─ wuhan_metro_lines.csv   线路汇总表
   ├─ wuhan_metro.sample.txt  2 号线 + 4 号线样例
   ├─ metro_compare.txt       导航对比表（占位）
   └─ README.md               数据格式说明
```

## 3. 构建与运行

在工程根目录执行：

```powershell
mingw32-make metro          # 编译 build/bin/metro.exe
mingw32-make run-metro      # 运行菜单
mingw32-make metro-test     # 编译并运行单元测试
mingw32-make run-metro-test # 只运行已编译的单元测试
mingw32-make all            # 基础题 + 地铁题一起构建
```

也可以直接运行：

```powershell
build\bin\metro.exe
build\bin\test_metro.exe
```

## 4. 建模说明

### 4.1 线路-站点双节点图

普通图如果只把“洪山广场”当一个顶点，就无法区分乘客在 2 号线还是 4 号线。
本工程把 `(线路, 站点)` 作为一个节点：

```text
(2号线, 洪山广场) --乘车边--> (2号线, 中南路)
(2号线, 洪山广场) --换乘边--> (4号线, 洪山广场)
```

这样：

- 乘车边带里程和运行时间；
- 换乘边带换乘时间；
- 路径中线路发生变化，就是一次换乘；
- 票价按本次返回路径的里程计算（不是另行求一次全网最短里程）。

### 4.2 四张图

| 图 | 用途 | 边权 |
| --- | --- | --- |
| `time_graph` | 最快到达 | 乘车分钟 + 换乘分钟 |
| `distance_graph` | 最短里程（票价按所选方案里程） | 公里 |
| `transfer_graph` | 最少换乘 | 乘车 0、换乘 1 |
| `station_graph` | DFS/BFS/MST | 站间里程 |

### 4.3 编号规则

- 物理站点：`S001`、`S002`……；
- 线路：`L01`、`L02`……；
- 图顶点：`L02S031`，表示 2 号线的 S031 站；
- 中文名放在 `MetroStation.name` / `MetroLine.name`，不放进图顶点名，
  避免 `Graph` 的 32 字节顶点名截断。

## 5. 当前测试结果

全量数据加载后的统计：

- 13 条线路、298 座物理站点、49 个换乘站；
- 13 个区段、352 条停靠点记录、352 个线路-站点节点；
- 线路总里程 575.20 km，物理站点图连通。

路径示例（宏图大道 → 武昌火车站）：

| 目标 | 里程 | 时间 | 换乘 | 票价 |
| --- | ---: | ---: | ---: | ---: |
| 最快到达 | 19.01 km | 38.0 min | 1 次 | 6 元 |
| 里程最短 | 18.72 km | 49.0 min | 4 次 | 6 元 |
| 换乘最少 | 19.01 km | 38.0 min | 1 次 | 6 元 |

`test_metro.exe` 覆盖全量加载、三种路径选择、同站进出、DFS/BFS
（均访问 298 站）和 Prim/Kruskal（均 297 条边、总权值一致）。

## 6. 下一阶段计划

1. 用真实导航结果填充 `metro_compare.txt`，做“地铁方案 vs APP”对比；
2. 补充更多换乘站、环线、支线的边界测试；
3. 把建模、核心代码、测试数据和运行截图整理进课程设计报告。
