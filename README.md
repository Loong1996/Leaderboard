# RankList

`RankList` 是一个面向 `C++` 业务开发者的轻量级实时排行榜项目，核心组件为 [`include/TVectorLeaderboard.h`](/D:/Code/RankList/include/TVectorLeaderboard.h)。它基于有序 `vector` 维护榜单顺序，适合“读多写少、按名次访问频繁、榜单规模较大”的业务场景，例如积分榜、战力榜、关卡榜、活动榜。

项目当前提供：

- 一个可直接集成的模板类 `TVectorLeaderboard<TKey, TValue, TCompare>`
- 一个示例程序 [`src/main.cpp`](/D:/Code/RankList/src/main.cpp)
- 一套覆盖基础行为、边界条件和基准测试的测试程序 [`test/BenchmarkLeaderboard.cpp`](/D:/Code/RankList/test/BenchmarkLeaderboard.cpp)

## 适用场景

适合：

- 需要按排名分页拉榜、取 TopN、按名次读取单个条目
- 榜单规则固定，排序字段明确
- 业务层通常能拿到玩家最新分数，或同时缓存旧值
- 希望用单头文件快速接入，不引入额外依赖

不适合：

- 超高频写入且每次更新都需要严格亚毫秒级
- 需要并发读写安全
- 需要按 `key` 做大量高频随机查询，但业务层拿不到 `value`
- 需要复杂索引结构，例如同时按多个维度做独立查询

## 快速开始

### 1. 引入头文件

```cpp
#include "TVectorLeaderboard.h"
```

### 2. 创建一个简单分数榜

```cpp
#include <cstdint>
#include <cstdio>

#include "TVectorLeaderboard.h"

int main()
{
    TVectorLeaderboard<uint64_t, int64_t> board;

    board.UpdateEntry(1001, 500);
    board.UpdateEntry(1002, 800);
    board.UpdateEntry(1003, 300);
    board.UpdateEntry(1004, 700);

    board.ForeachEntries(1, 10, [](uint32_t rank, const auto& node)
    {
        std::printf("rank=%u key=%llu score=%lld\n",
            rank,
            static_cast<unsigned long long>(node.key),
            static_cast<long long>(node.value));
    });

    uint32_t rank = board.GetRank(1002, 800);
    std::printf("player 1002 rank = %u\n", rank);
    return 0;
}
```

默认比较器是 `std::greater<TValue>`，即数值越大排名越高。

### 3. 创建一个限长 Top 榜

```cpp
TVectorLeaderboard<uint64_t, int64_t> board(1000); // 只保留前 1000 名

uint32_t rank = board.UpdateEntry(2001, 9500);
if (rank == 0)
{
    // 未进入榜单
}
```

当设置了 `MaxSize` 后，排名落在榜单尾部之外的条目不会被保留，接口返回 `0` 表示未上榜。

### 4. 自定义多字段排序

```cpp
struct RankData
{
    int64_t score;
    int64_t fightPower;
    int64_t timestamp;
};

struct RankCompare
{
    bool operator()(const RankData& lhs, const RankData& rhs) const
    {
        if (lhs.score != rhs.score)
        {
            return lhs.score > rhs.score;
        }

        if (lhs.fightPower != rhs.fightPower)
        {
            return lhs.fightPower > rhs.fightPower;
        }

        return lhs.timestamp < rhs.timestamp;
    }
};

TVectorLeaderboard<uint64_t, RankData, RankCompare> board(3);
board.UpdateEntry(1, {100, 5000, 1000});
board.UpdateEntry(2, {100, 5000, 2000});
board.UpdateEntry(3, {100, 6000, 3000});
board.UpdateEntry(4, {200, 3000, 4000});
```

业务比较规则只负责比较 `value`。当 `value` 完全相等时，`TVectorLeaderboard` 内部会自动使用 `key` 作为稳定决胜条件，保证全序关系成立。

## 常见接入方式

### 新玩家上榜

如果你能保证 `key` 不存在，可以直接使用 `InsertEntry`：

```cpp
uint32_t rank = board.InsertEntry(playerId, score);
```

这个接口跳过查重，适合初始化导榜或明确不会重复插入的场景。若 `key` 已存在，会产生重复条目，因此业务层必须保证唯一性。

### 玩家分数更新

常规更新直接使用：

```cpp
uint32_t rank = board.UpdateEntry(playerId, newScore);
```

如果业务侧同时持有旧值，优先使用：

```cpp
uint32_t rank = board.UpdateEntry(playerId, oldScore, newScore);
```

原因是带旧值版本可以优先按 `key + oldValue` 做二分定位；旧值命中时，定位更快，尤其适合“玩家状态变更时业务已知旧分数”的在线更新链路。

### 查询玩家名次

优先使用：

```cpp
uint32_t rank = board.GetRank(playerId, score);
```

这一路径是 `O(log N)`。

只有在确实拿不到当前 `value` 时，再退化为：

```cpp
uint32_t rank = board.GetRank(playerId);
```

这一路径需要线性扫描，是 `O(N)`。

### 拉取单个名次或分页榜单

按单个名次读取：

```cpp
board.ForeachEntryByRank(1, [](uint32_t rank, const auto& node)
{
    // rank == 1
});
```

按区间分页读取：

```cpp
board.ForeachEntries(101, 20, [](uint32_t rank, const auto& node)
{
    // 第 101 ~ 120 名
});
```

`ForeachEntries` 返回实际遍历数量，适合直接做分页返回。

### 删除玩家

优先使用：

```cpp
bool ok = board.RemoveEntry(playerId, score);
```

只有在没有当前 `value` 时，再使用：

```cpp
bool ok = board.RemoveEntry(playerId);
```

## 设计方案

### 核心结构

`TVectorLeaderboard` 只维护一个按排名有序的 `vector<ST_RANK_NODE>`，其中每个节点包含：

- `key`：业务唯一标识，例如玩家 `id`
- `value`：用于排序的数据，可以是基础类型、结构体或指针

这个设计的重点不是让所有操作都最优，而是优先优化排行榜最常见的访问模式：

- 按名次读取单个条目
- 连续遍历一段排名区间
- 取 TopN

这些操作在有序数组上非常直接，缓存友好，遍历性能稳定，也容易控制内存布局。

### 为什么选择有序 `vector`

相比平衡树、跳表或带索引哈希结构，这里的实现选择了更朴素的有序 `vector`，原因是排行榜的核心价值通常在“按排名读取”，而不是“按 `key` 高频随机访问”。

主要收益：

- `ForeachEntryByRank` 可以按下标直接访问，复杂度 `O(1)`
- `ForeachEntries` 只和实际返回数量相关，复杂度 `O(K)`
- 内存连续，CPU 缓存命中率高，做 Top 榜和分页拉榜时很直接
- 单头文件模板实现，接入成本低

代价也很明确：

- 插入、更新、删除都可能触发数组移动，最坏为 `O(N)`
- 仅按 `key` 查询时无法利用顺序结构，只能线性扫描

这意味着它更适合“读多写少”或“写入虽多，但业务更关注拉榜效率”的系统。

### 为什么比较规则是 `value` 优先、`key` 决胜

业务排序通常只关心 `value`，例如：

- 分数高者在前
- 分数相同则战力高者在前
- 再相同则时间早者在前

但容器内部需要一个稳定的全序关系，否则二分查找无法精确定位。因此 `TVectorLeaderboard` 的规则是：

1. 先按业务比较器 `TCompare(value)` 决定先后
2. 如果两个 `value` 互相都不更优，则认为业务排序相等
3. 此时自动用 `key` 作为最终决胜条件

这样设计有两个好处：

- 调用方不需要把 `key` 写进业务比较器
- 即使多个玩家的 `value` 完全相同，榜单内部仍然有稳定顺序，`GetRank(key, value)` 和二分定位才能正确工作

### 为什么提供“带旧值”和“不带旧值”两类接口

排行榜更新的真实业务场景并不统一：

- 有些链路只能拿到“玩家新分数”
- 有些链路能同时拿到“旧分数 -> 新分数”

因此 `TVectorLeaderboard` 同时提供两种接口：

- `UpdateEntry(key, value)`：易用，适合通用业务接入
- `UpdateEntry(key, oldValue, newValue)`：更适合性能敏感链路

查询和删除也采用同样思路：

- `GetRank(key, value)` / `RemoveEntry(key, value)` 优先
- `GetRank(key)` / `RemoveEntry(key)` 作为无法拿到 `value` 时的兜底版本

这不是接口重复，而是把“业务是否持有旧值/当前值”转化为可显式选择的性能路径。

## 复杂度

| 操作 | 复杂度 | 说明 |
| --- | --- | --- |
| `ForeachEntryByRank` | `O(1)` | 按排名直接访问单个节点 |
| `ForeachEntries` | `O(K)` | `K` 为实际遍历数量 |
| `GetRank(key, value)` | `O(log N)` | 基于全序比较做二分精确定位 |
| `GetRank(key)` | `O(N)` | 仅按 `key` 线性扫描 |
| `GetEntry(key, ...)` | `O(N)` | 仅按 `key` 线性扫描 |
| `InsertEntry` | `O(N)` | 二分定位后插入，数组移动占主成本 |
| `UpdateEntry` | `O(N)` | 可能发生单次块移动 |
| `RemoveEntry` | `O(N)` | 删除后数组移动 |

可以直接这样理解：

- 拉榜、取 TopN、按名次访问很强
- 按 `key` 查找不是它的强项
- 写入性能取决于数组搬移成本

## 关键接口说明

### `SetMaxSize`

```cpp
board.SetMaxSize(1000);
```

- `0` 表示不限长
- 调小上限时，会立即截断榜尾数据
- 当榜单已满时，新条目若无法进入前 `MaxSize`，接口返回 `0`

### `Reserve`

```cpp
board.Reserve(1000000);
```

用于预分配容量，适合预估榜单规模明确的场景，可以减少扩容次数。

### `GetEntry`

```cpp
TVectorLeaderboard<uint64_t, int64_t>::ST_RANK_NODE node;
uint32_t rank = 0;
bool found = board.GetEntry(playerId, rank, node);
```

这个接口会返回玩家当前节点和名次，但底层仍然是按 `key` 线性扫描，适合低频后台查询，不适合热点路径。

## 接入建议

- `key` 必须能比较大小，因为内部需要使用 `operator<`
- `InsertEntry` 不做去重，只在你能保证 `key` 不存在时使用
- 如果业务能拿到当前 `value`，优先使用 `GetRank(key, value)` 和 `RemoveEntry(key, value)`
- 如果业务能拿到旧值，优先使用 `UpdateEntry(key, oldValue, newValue)`
- 自定义比较器 `TCompare` 必须满足严格弱序语义，否则二分行为可能失真
- 当 `value` 相等时，最终顺序由 `key` 决定，因此相同分数玩家的先后顺序是确定的
- 当前实现未提供线程安全保证；并发访问需要业务层自行加锁

## 构建与运行

项目使用 `CMake` 构建。

### 构建示例程序

```bash
cmake -S . -B build
cmake --build build --config Release
```

生成目标：

- `ranklist_demo`：示例程序，入口见 [`src/main.cpp`](/D:/Code/RankList/src/main.cpp)
- `benchmark_leaderboard`：测试与基准程序

### 运行测试

测试目标在 [`test/CMakeLists.txt`](/D:/Code/RankList/test/CMakeLists.txt) 中注册为 `TestLeaderboard`。构建后可用：

```bash
ctest --test-dir build --output-on-failure
```

测试程序还会生成 `report.html` 报告文件，用于查看基准测试结果。

## 项目结构

```text
RankList/
├─ include/
│  └─ TVectorLeaderboard.h
├─ src/
│  └─ main.cpp
├─ test/
│  ├─ BenchmarkLeaderboard.cpp
│  └─ CMakeLists.txt
├─ CMakeLists.txt
└─ README.md
```

## 总结

如果你的业务重点是：

- 快速接入一个简单可靠的排行榜
- 高效支持 TopN、分页拉榜、按名次读取
- 接受更新和删除为 `O(N)` 的实现取舍

那么 `TVectorLeaderboard` 是一个足够直接、可维护、易扩展的选择。

## 多数据结构扩展方案评估

当需要引入不同底层数据结构（如跳表、红黑树）的排行榜实现时，有三种常见架构路线：

### 方案一：策略模式（Policy-based Design）

将存储后端作为模板参数注入：

```cpp
template <typename TKey, typename TValue,
          typename TCompare = std::greater<TValue>,
          typename TStorage = SortedVectorStorage<TKey, TValue, TCompare>>
class TLeaderboard { ... };
```

**优点：**

- 零开销抽象，全部编译期绑定，可内联，无虚函数
- 公共业务逻辑（MaxSize 管理、rank 约定、遍历接口）只维护一份
- 和现有 `TCompare` 策略设计一脉相承，扩展自然
- 向后兼容，`TStorage` 有默认值，现有代码无需修改

**缺点：**

- Storage 接口的提取需要对现有代码做一定重构
- 对底层不熟悉的使用者来说，模板参数层数增加，阅读成本略高
- 不同底层的最优 API 可能不同，若强行统一 Storage 接口会产生约束

---

### 方案二：各自独立实现（当前选型）

每种底层数据结构对应独立的模板类：

```cpp
template<...> class TVectorLeaderboard { ... };
template<...> class TSkipListLeaderboard { ... };
```

**优点：**

- 实现简单，每个类职责清晰，无额外抽象层
- 各底层可自由设计最优 API，不受其他实现约束
- 实现和测试相互独立，不存在接口耦合风险

**缺点：**

- 公共逻辑（MaxSize 截断、rank 返回约定、回调遍历）在多个类中重复
- API 可能随实现各自演进而产生漂移，维护成本随类数量上升

---

### 方案三：虚基类 + 运行时多态

定义抽象接口类，各实现继承并重写：

```cpp
class ILeaderboard {
    virtual uint32_t InsertEntry(...) = 0;
    virtual uint32_t GetRank(...) = 0;
};
class TVectorLeaderboard : public ILeaderboard { ... };
class TSkipListLeaderboard : public ILeaderboard { ... };
```

**优点：**

- 可运行时切换实现，适合需要依赖注入或 Mock 的场景
- 对使用方完全透明，只依赖接口

**缺点：**

- 模板类与虚函数天然冲突：`TKey` / `TValue` 是类型参数，要支持虚函数需要类型擦除，复杂度显著上升
- 虚函数调用开销对游戏服务器热路径（高频 Insert / GetRank）不必要
- 强制统一接口会牵制各底层的 API 设计

---

### 最终选型：方案二（各自独立实现）

**理由：**

- 当前项目只有一种底层实现，策略模式的抽象收益尚未体现，重构成本先于价值到来
- 游戏服务器场景排行榜数量有限（通常 3-10 种），独立实现的重复代码量可控
- 虚基类方案性能开销与模板类型系统的冲突均不值得引入
- 独立类名（`TVectorLeaderboard`、`TSkipListLeaderboard`…）直接体现底层数据结构，使用者选型一目了然

若未来独立实现数量超过 3 个，且公共逻辑重复已形成明显维护负担，可再评估是否迁移至方案一。
