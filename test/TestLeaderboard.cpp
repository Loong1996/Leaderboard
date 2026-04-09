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
	assert(objBoard.GetRank(1002) == 1);
	assert(objBoard.GetRank(1001) == 2);
	assert(objBoard.GetRank(1003) == 3);
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
	assert(objBoard.GetRank(1003) == 1);
	assert(objBoard.GetRank(1002) == 2);
	assert(objBoard.GetRank(1001) == 3);
	assert(objBoard.GetCount() == 3);
}

TEST_CASE(TestRemoveEntry)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);

	assert(objBoard.RemoveEntry(1002) == true);
	assert(objBoard.GetCount() == 2);
	assert(objBoard.GetRank(1002) == 0);
	assert(objBoard.GetRank(1001) == 1);
	assert(objBoard.GetRank(1003) == 2);

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
	assert(objBoard.GetRank(1001) == 0);
}

TEST_CASE(TestGetEntryByRank)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);

	const auto* pEntry = objBoard.GetEntryByRank(1);
	assert(pEntry != nullptr);
	assert(pEntry->key == 1002);
	assert(pEntry->value == 800);

	const auto* pEntry2 = objBoard.GetEntryByRank(2);
	assert(pEntry2 != nullptr);
	assert(pEntry2->key == 1001);

	// 越界
	assert(objBoard.GetEntryByRank(0) == nullptr);
	assert(objBoard.GetEntryByRank(3) == nullptr);
}

TEST_CASE(TestGetTopN)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	objBoard.UpdateEntry(1004, 700);

	const TLeaderboard<uint64_t, int64_t>::ST_RANK_NODE* pBegin = nullptr;
	uint32_t uiCount = objBoard.GetTopN(2, pBegin);

	assert(uiCount == 2);
	assert(pBegin != nullptr);
	assert(pBegin[0].key == 1002);  // 800
	assert(pBegin[1].key == 1004);  // 700

	// 请求超过总数
	uiCount = objBoard.GetTopN(100, pBegin);
	assert(uiCount == 4);
}

TEST_CASE(TestGetAroundRank)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	// 插入 10 个玩家，分数 100~1000
	for (uint64_t ui = 1; ui <= 10; ++ui)
	{
		objBoard.UpdateEntry(ui, static_cast<int64_t>(ui * 100));
	}

	// 排名: 10(1000), 9(900), 8(800), 7(700), 6(600), 5(500), 4(400), 3(300), 2(200), 1(100)

	const TLeaderboard<uint64_t, int64_t>::ST_RANK_NODE* pBegin = nullptr;

	// 第5名附近取5个：排名3~7
	uint32_t uiCount = objBoard.GetAroundRank(5, 5, pBegin);
	assert(uiCount == 5);
	assert(pBegin[0].key == 8);  // 排名3: 分数800
	assert(pBegin[4].key == 4);  // 排名7: 分数400

	// 第1名附近取5个：排名1~5
	uiCount = objBoard.GetAroundRank(1, 5, pBegin);
	assert(uiCount == 5);
	assert(pBegin[0].key == 10);  // 排名1

	// 第10名附近取5个：排名6~10
	uiCount = objBoard.GetAroundRank(10, 5, pBegin);
	assert(uiCount == 5);
	assert(pBegin[4].key == 1);  // 排名10
}

TEST_CASE(TestGetEntry)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);

	const auto* pEntry = objBoard.GetEntry(1001);
	assert(pEntry != nullptr);
	assert(pEntry->value == 500);

	assert(objBoard.GetEntry(9999) == nullptr);
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
	assert(objBoard.GetRank(1003) == 0);  // 300 被淘汰
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
	assert(objBoard.GetRank(1003) == 1);
	assert(objBoard.GetRank(1001) == 2);
	assert(objBoard.GetRank(1002) == 3);
}

// ============================================================
//  边界情况测试
// ============================================================

TEST_CASE(TestEmptyBoard)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	assert(objBoard.GetCount() == 0);
	assert(objBoard.GetRank(1001) == 0);
	assert(objBoard.GetEntryByRank(1) == nullptr);
	assert(objBoard.GetEntry(1001) == nullptr);

	const TLeaderboard<uint64_t, int64_t>::ST_RANK_NODE* pBegin = nullptr;
	assert(objBoard.GetTopN(10, pBegin) == 0);
	assert(objBoard.GetAroundRank(1, 5, pBegin) == 0);
}

TEST_CASE(TestSingleEntry)
{
	TLeaderboard<uint64_t, int64_t> objBoard;

	objBoard.UpdateEntry(1001, 500);

	assert(objBoard.GetCount() == 1);
	assert(objBoard.GetRank(1001) == 1);

	const TLeaderboard<uint64_t, int64_t>::ST_RANK_NODE* pBegin = nullptr;
	uint32_t uiCount = objBoard.GetAroundRank(1, 5, pBegin);
	assert(uiCount == 1);
	assert(pBegin[0].key == 1001);
}

TEST_CASE(TestStringKey)
{
	TLeaderboard<std::string, int64_t> objBoard;

	objBoard.UpdateEntry("Alice", 500);
	objBoard.UpdateEntry("Bob", 800);

	assert(objBoard.GetRank("Bob") == 1);
	assert(objBoard.GetRank("Alice") == 2);
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
