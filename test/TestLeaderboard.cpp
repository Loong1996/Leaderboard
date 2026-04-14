/**
 * @file TestLeaderboard.cpp
 * @author Loong
 * @date 2026-04-08
 * @details
 *     TLeaderboard 单元测试。
 *     使用 assert 断言，测试失败时直接终止并输出错误信息。
 */
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>

#include "TLeaderboard.h"

#define TEST_CASE(name) \
	static void name(); \
	struct ST_REG_##name { ST_REG_##name() { printf("  [RUN]  %s\n", #name); name(); printf("  [PASS] %s\n", #name); } } s_reg_##name; \
	static void name()

// ============================================================
//  基本类型测试（int64_t 分数，默认 std::greater）
// ============================================================

TEST_CASE(TestInsertAndGetRank)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);

	// 800 > 500 > 300
	assert(objBoard.GetRank(1002, 800) == 1);
	assert(objBoard.GetRank(1001, 500) == 2);
	assert(objBoard.GetRank(1003, 300) == 3);
	assert(objBoard.GetCount() == 3);
}

TEST_CASE(TestUpdateExisting)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);

	// 更新 1003 分数为最高
	uint32_t uiRank = objBoard.UpdateEntry(1003, 900);
	assert(uiRank == 1);
	assert(objBoard.GetRank(1003, 900) == 1);
	assert(objBoard.GetRank(1002, 800) == 2);
	assert(objBoard.GetRank(1001, 500) == 3);
	assert(objBoard.GetCount() == 3);
}

TEST_CASE(TestRemoveEntryWithValue)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);

	assert(objBoard.RemoveEntry(1002, 800) == true);
	assert(objBoard.GetCount() == 2);
	assert(objBoard.GetRank(1002, 800) == 0);
	assert(objBoard.GetRank(1001, 500) == 1);
	assert(objBoard.GetRank(1003, 300) == 2);

	// 删除不存在的 key
	assert(objBoard.RemoveEntry(9999, 0) == false);
}

TEST_CASE(TestRemoveEntryByKey)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);

	assert(objBoard.RemoveEntry(1002) == true);
	assert(objBoard.GetCount() == 2);
	assert(objBoard.GetRank(1001, 500) == 1);
	assert(objBoard.GetRank(1003, 300) == 2);

	// 删除不存在的 key
	assert(objBoard.RemoveEntry(9999) == false);
}

TEST_CASE(TestClear)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.Clear();

	assert(objBoard.GetCount() == 0);
	assert(objBoard.GetRank(1001, 500) == 0);
}

TEST_CASE(TestForeachEntryByRank)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);

	bool bFound = objBoard.ForeachEntryByRank(1, [](uint32_t uiRank, const auto& stNode)
	{
		assert(uiRank == 1);
		assert(stNode.key == 1002);
		assert(stNode.value == 800);
	});
	assert(bFound == true);

	bFound = objBoard.ForeachEntryByRank(2, [](uint32_t uiRank, const auto& stNode)
	{
		assert(uiRank == 2);
		assert(stNode.key == 1001);
	});
	assert(bFound == true);

	// 越界
	assert(objBoard.ForeachEntryByRank(0, [](uint32_t, const auto&) {}) == false);
	assert(objBoard.ForeachEntryByRank(3, [](uint32_t, const auto&) {}) == false);
}

TEST_CASE(TestForeachEntries)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	objBoard.UpdateEntry(1004, 700);

	// 排名: 1002(800), 1004(700), 1001(500), 1003(300)

	// Top 2
	std::vector<uint64_t> vecKeys;
	uint32_t uiCount = objBoard.ForeachEntries(1, 2, [&vecKeys](uint32_t, const auto& stNode)
	{
		vecKeys.push_back(stNode.key);
	});
	assert(uiCount == 2);
	assert(vecKeys[0] == 1002);  // 800
	assert(vecKeys[1] == 1004);  // 700

	// 请求超过总数
	uiCount = objBoard.ForeachEntries(1, 100, [](uint32_t, const auto&) {});
	assert(uiCount == 4);

	// 中间区间
	vecKeys.clear();
	uiCount = objBoard.ForeachEntries(2, 2, [&vecKeys](uint32_t, const auto& stNode)
	{
		vecKeys.push_back(stNode.key);
	});
	assert(uiCount == 2);
	assert(vecKeys[0] == 1004);  // 排名2
	assert(vecKeys[1] == 1001);  // 排名3

	// 起始排名超出
	assert(objBoard.ForeachEntries(5, 1, [](uint32_t, const auto&) {}) == 0);

	// 请求 0 个
	assert(objBoard.ForeachEntries(1, 0, [](uint32_t, const auto&) {}) == 0);

	// 起始排名 0
	assert(objBoard.ForeachEntries(0, 1, [](uint32_t, const auto&) {}) == 0);
}

TEST_CASE(TestGetEntry)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);

	uint32_t uiRank = 0;
	TLeaderboard<uint64_t, int64_t>::ST_RANK_NODE stNode{};

	assert(objBoard.GetEntry(1001, uiRank, stNode) == true);
	assert(uiRank == 1);
	assert(stNode.value == 500);

	assert(objBoard.GetEntry(9999, uiRank, stNode) == false);
}

TEST_CASE(TestGetRankByKey)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);

	assert(objBoard.GetRank(1002) == 1);
	assert(objBoard.GetRank(1001) == 2);
	assert(objBoard.GetRank(1003) == 3);
	assert(objBoard.GetRank(9999) == 0);
}

// ============================================================
//  MaxSize 截断测试
// ============================================================

TEST_CASE(TestMaxSize)
{
	TLeaderboard<uint64_t, int64_t> objBoard(3);

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	assert(objBoard.GetCount() == 3);

	// 插入比最低分更低的，应被截断
	uint32_t uiRank = objBoard.UpdateEntry(1004, 100);
	assert(uiRank == 0);
	assert(objBoard.GetCount() == 3);

	// 插入比最低分更高的，末尾被淘汰
	uiRank = objBoard.UpdateEntry(1005, 600);
	assert(uiRank == 2);
	assert(objBoard.GetCount() == 3);
	assert(objBoard.GetRank(1003, 300) == 0);  // 300 被淘汰
}

TEST_CASE(TestMaxSizeUpdateExisting)
{
	TLeaderboard<uint64_t, int64_t> objBoard(3);

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);

	// 更新已有玩家分数，不应被截断
	uint32_t uiRank = objBoard.UpdateEntry(1003, 900);
	assert(uiRank == 1);
	assert(objBoard.GetCount() == 3);
}

TEST_CASE(TestSetMaxSize)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	objBoard.UpdateEntry(1004, 700);
	objBoard.UpdateEntry(1005, 600);
	assert(objBoard.GetCount() == 5);
	assert(objBoard.GetMaxSize() == 0);

	// 设置 MaxSize 为 3，截断末尾
	objBoard.SetMaxSize(3);
	assert(objBoard.GetMaxSize() == 3);
	assert(objBoard.GetCount() == 3);
	assert(objBoard.GetRank(1002, 800) == 1);
	assert(objBoard.GetRank(1004, 700) == 2);
	assert(objBoard.GetRank(1005, 600) == 3);
	assert(objBoard.GetRank(1001, 500) == 0);  // 被截断
	assert(objBoard.GetRank(1003, 300) == 0);  // 被截断

	// 放开限制
	objBoard.SetMaxSize(0);
	assert(objBoard.GetMaxSize() == 0);
	objBoard.UpdateEntry(1006, 100);
	assert(objBoard.GetCount() == 4);
}

TEST_CASE(TestReserve)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	// Reserve 不改变 size，只影响 capacity
	objBoard.Reserve(1000);
	assert(objBoard.GetCount() == 0);

	objBoard.UpdateEntry(1001, 500);
	assert(objBoard.GetCount() == 1);
}

// ============================================================
//  同分排序测试（key 决胜）
// ============================================================

TEST_CASE(TestSameValueTieBreaking)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	// 同分时 key 小的排前面
	objBoard.UpdateEntry(1003, 500);
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 500);

	assert(objBoard.GetRank(1001, 500) == 1);
	assert(objBoard.GetRank(1002, 500) == 2);
	assert(objBoard.GetRank(1003, 500) == 3);
	assert(objBoard.GetCount() == 3);
}

TEST_CASE(TestSameValueDifferentKeys)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	// 混合不同分数和同分
	objBoard.UpdateEntry(10, 800);
	objBoard.UpdateEntry(20, 500);
	objBoard.UpdateEntry(30, 500);
	objBoard.UpdateEntry(40, 500);
	objBoard.UpdateEntry(50, 300);

	assert(objBoard.GetRank(10, 800) == 1);
	assert(objBoard.GetRank(20, 500) == 2);
	assert(objBoard.GetRank(30, 500) == 3);
	assert(objBoard.GetRank(40, 500) == 4);
	assert(objBoard.GetRank(50, 300) == 5);
}

// ============================================================
//  UpdateEntry 返回值测试
// ============================================================

TEST_CASE(TestUpdateEntryReturnValue)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	assert(objBoard.UpdateEntry(1001, 500) == 1);  // 第一个插入排第1
	assert(objBoard.UpdateEntry(1002, 800) == 1);  // 更高分排第1
	assert(objBoard.UpdateEntry(1003, 300) == 3);  // 最低分排第3
	assert(objBoard.UpdateEntry(1004, 600) == 2);  // 中间分排第2

	// 更新现有条目
	assert(objBoard.UpdateEntry(1003, 900) == 1);  // 提升到第1
	assert(objBoard.UpdateEntry(1003, 100) == 4);  // 降低到最后
}

// ============================================================
//  自定义比较仿函数测试
// ============================================================

struct ST_RANK_DATA
{
	int64_t iScore;
	int64_t iTimestamp;
};

struct ST_RANK_DATA_COMPARE
{
	bool operator()(const ST_RANK_DATA& lhs, const ST_RANK_DATA& rhs) const
	{
		if (lhs.iScore != rhs.iScore)
		{
			return lhs.iScore > rhs.iScore;
		}

		return lhs.iTimestamp < rhs.iTimestamp;  // 先达到者靠前
	}
};

TEST_CASE(TestCustomCompare)
{
	TLeaderboard<uint64_t, ST_RANK_DATA, ST_RANK_DATA_COMPARE> objBoard;

	objBoard.UpdateEntry(1001, {500, 100});
	objBoard.UpdateEntry(1002, {500, 200});
	objBoard.UpdateEntry(1003, {800, 300});

	// 800 先，然后同分 500 按时间：100 < 200
	assert(objBoard.GetRank(1003, {800, 300}) == 1);
	assert(objBoard.GetRank(1001, {500, 100}) == 2);
	assert(objBoard.GetRank(1002, {500, 200}) == 3);
}

TEST_CASE(TestCustomCompareRemoveAndUpdate)
{
	TLeaderboard<uint64_t, ST_RANK_DATA, ST_RANK_DATA_COMPARE> objBoard;

	objBoard.UpdateEntry(1, {100, 10});
	objBoard.UpdateEntry(2, {100, 20});
	objBoard.UpdateEntry(3, {200, 30});

	// 移除并验证
	assert(objBoard.RemoveEntry(3, {200, 30}) == true);
	assert(objBoard.GetCount() == 2);
	assert(objBoard.GetRank(1, {100, 10}) == 1);

	// 更新
	objBoard.UpdateEntry(2, {300, 40});
	assert(objBoard.GetRank(2, {300, 40}) == 1);
}

// ============================================================
//  边界情况测试
// ============================================================

TEST_CASE(TestEmptyBoard)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	assert(objBoard.GetCount() == 0);
	assert(objBoard.GetRank(1001, 0) == 0);
	assert(objBoard.GetRank(1001) == 0);
	assert(objBoard.ForeachEntryByRank(1, [](uint32_t, const auto&) {}) == false);
	{
		uint32_t uiRank = 0;
		TLeaderboard<uint64_t, int64_t>::ST_RANK_NODE stNode{};
		assert(objBoard.GetEntry(1001, uiRank, stNode) == false);
	}
	assert(objBoard.RemoveEntry(1001) == false);
	assert(objBoard.RemoveEntry(1001, 0) == false);

	assert(objBoard.ForeachEntries(1, 10, [](uint32_t, const auto&) {}) == 0);
	assert(objBoard.ForeachEntries(0, 5, [](uint32_t, const auto&) {}) == 0);
}

TEST_CASE(TestSingleEntry)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);

	assert(objBoard.GetCount() == 1);
	assert(objBoard.GetRank(1001, 500) == 1);
	assert(objBoard.GetRank(1001) == 1);

	uint64_t ulFoundKey = 0;
	uint32_t uiCount = objBoard.ForeachEntries(1, 5, [&ulFoundKey](uint32_t, const auto& stNode)
	{
		ulFoundKey = stNode.key;
	});
	assert(uiCount == 1);
	assert(ulFoundKey == 1001);
}

TEST_CASE(TestStringKey)
{
	TLeaderboard<std::string, int64_t> objBoard;

	objBoard.UpdateEntry("Alice", 500);
	objBoard.UpdateEntry("Bob", 800);

	assert(objBoard.GetRank("Bob", 800) == 1);
	assert(objBoard.GetRank("Alice", 500) == 2);
	assert(objBoard.GetRank("Bob") == 1);
	assert(objBoard.GetRank("Alice") == 2);
}

TEST_CASE(TestMaxSizeOne)
{
	TLeaderboard<uint64_t, int64_t> objBoard(1);

	assert(objBoard.UpdateEntry(1, 100) == 1);
	assert(objBoard.GetCount() == 1);

	// 更高分替换
	assert(objBoard.UpdateEntry(2, 200) == 1);
	assert(objBoard.GetCount() == 1);
	assert(objBoard.GetRank(2, 200) == 1);
	assert(objBoard.GetRank(1, 100) == 0);

	// 更低分被截断
	assert(objBoard.UpdateEntry(3, 50) == 0);
	assert(objBoard.GetCount() == 1);
}

TEST_CASE(TestInsertRemoveReinsert)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1, 100);
	objBoard.UpdateEntry(2, 200);
	assert(objBoard.GetCount() == 2);

	objBoard.RemoveEntry(2, 200);
	assert(objBoard.GetCount() == 1);

	// 重新插入
	objBoard.UpdateEntry(2, 300);
	assert(objBoard.GetCount() == 2);
	assert(objBoard.GetRank(2, 300) == 1);
	assert(objBoard.GetRank(1, 100) == 2);
}

TEST_CASE(TestLargeScaleCorrectness)
{
	const uint32_t COUNT = 10000;
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(COUNT);

	// 插入 10000 个不同分数
	for (uint64_t ui = 0; ui < COUNT; ++ui)
	{
		objBoard.UpdateEntry(ui, static_cast<int64_t>(ui));
	}

	assert(objBoard.GetCount() == COUNT);

	// 最高分排第1
	assert(objBoard.GetRank(COUNT - 1, static_cast<int64_t>(COUNT - 1)) == 1);
	// 最低分排最后
	assert(objBoard.GetRank(0, 0) == COUNT);

	// 验证 Top 5
	std::vector<uint64_t> vecKeys;
	uint32_t uiCount = objBoard.ForeachEntries(1, 5, [&vecKeys](uint32_t, const auto& stNode)
	{
		vecKeys.push_back(stNode.key);
	});
	assert(uiCount == 5);
	assert(vecKeys[0] == COUNT - 1);
	assert(vecKeys[4] == COUNT - 5);
}

// ============================================================
//  升序排行榜测试（std::less）
// ============================================================

TEST_CASE(TestAscendingOrder)
{
	TLeaderboard<uint64_t, int64_t, std::less<int64_t>> objBoard;

	objBoard.UpdateEntry(1, 500);
	objBoard.UpdateEntry(2, 100);
	objBoard.UpdateEntry(3, 800);

	// std::less: 值小的排前面
	assert(objBoard.GetRank(2, 100) == 1);
	assert(objBoard.GetRank(1, 500) == 2);
	assert(objBoard.GetRank(3, 800) == 3);
}

// ============================================================
//  主函数
// ============================================================

int main()
{
	printf("=== TLeaderboard Unit Tests ===\n");
	printf("All tests passed.\n");
	return 0;
}
