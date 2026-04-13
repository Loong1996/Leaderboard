/**
 * @file BenchmarkLeaderboard.cpp
 * @author Loong
 * @date 2026-04-13
 * @details
 *     TLeaderboard 单元测试 + 性能基准测试。
 *     运行全部测试用例和基准测试，生成 HTML 可视化报告。
 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <random>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#include "TLeaderboard.h"

// ============================================================
//  计时辅助
// ============================================================
class CStopWatch
{
public:
	CStopWatch() : m_tpStart(std::chrono::high_resolution_clock::now()) {}

	double ElapsedMs() const
	{
		auto tpNow = std::chrono::high_resolution_clock::now();
		return std::chrono::duration<double, std::milli>(tpNow - m_tpStart).count();
	}

	void Reset() { m_tpStart = std::chrono::high_resolution_clock::now(); }

private:
	std::chrono::high_resolution_clock::time_point m_tpStart;
};

// ============================================================
//  单元测试框架
// ============================================================
struct ST_TEST_RESULT
{
	std::string strName;
	std::string strCategory;
	bool bPassed;
	std::string strDetail;
};

static std::vector<ST_TEST_RESULT> g_vecTestResults;
static bool g_bCurrentPassed = true;
static std::string g_strCurrentDetail;

#define CHECK(expr) \
	do { \
		if (!(expr)) { \
			g_bCurrentPassed = false; \
			char szBuf[256]; \
			snprintf(szBuf, sizeof(szBuf), "第 %d 行: %s", __LINE__, #expr); \
			if (!g_strCurrentDetail.empty()) g_strCurrentDetail += "; "; \
			g_strCurrentDetail += szBuf; \
		} \
	} while (0)

static void BeginTest(const char* pszCategory, const char* pszName)
{
	g_bCurrentPassed = true;
	g_strCurrentDetail.clear();
	printf("  [RUN]  %s\n", pszName);
}

static void EndTest(const char* pszCategory, const char* pszName)
{
	ST_TEST_RESULT stResult;
	stResult.strName = pszName;
	stResult.strCategory = pszCategory;
	stResult.bPassed = g_bCurrentPassed;
	stResult.strDetail = g_strCurrentDetail;
	g_vecTestResults.push_back(stResult);
	printf("  [%s] %s\n", g_bCurrentPassed ? "PASS" : "FAIL", pszName);
}

#define RUN_TEST(category, name, fn) \
	do { \
		BeginTest(category, name); \
		fn(); \
		EndTest(category, name); \
	} while (0)

// ============================================================
//  自定义比较仿函数（用于测试）
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
			return lhs.iScore > rhs.iScore;
		return lhs.iTimestamp < rhs.iTimestamp;
	}
};

// ============================================================
//  单元测试函数
// ============================================================

static void Test_InsertAndGetRank()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	CHECK(objBoard.GetRank(1002, 800) == 1);
	CHECK(objBoard.GetRank(1001, 500) == 2);
	CHECK(objBoard.GetRank(1003, 300) == 3);
	CHECK(objBoard.GetCount() == 3);
}

static void Test_UpdateExisting()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	uint32_t uiRank = objBoard.UpdateEntry(1003, 900);
	CHECK(uiRank == 1);
	CHECK(objBoard.GetRank(1003, 900) == 1);
	CHECK(objBoard.GetRank(1002, 800) == 2);
	CHECK(objBoard.GetRank(1001, 500) == 3);
	CHECK(objBoard.GetCount() == 3);
}

static void Test_ReturnValue()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	CHECK(objBoard.UpdateEntry(1001, 500) == 1);
	CHECK(objBoard.UpdateEntry(1002, 800) == 1);
	CHECK(objBoard.UpdateEntry(1003, 300) == 3);
	CHECK(objBoard.UpdateEntry(1004, 600) == 2);
	CHECK(objBoard.UpdateEntry(1003, 900) == 1);
	CHECK(objBoard.UpdateEntry(1003, 100) == 4);
}

static void Test_RemoveWithValue()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	CHECK(objBoard.RemoveEntry(1002, 800) == true);
	CHECK(objBoard.GetCount() == 2);
	CHECK(objBoard.GetRank(1002, 800) == 0);
	CHECK(objBoard.GetRank(1001, 500) == 1);
	CHECK(objBoard.RemoveEntry(9999, 0) == false);
}

static void Test_RemoveByKey()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	CHECK(objBoard.RemoveEntry(1002) == true);
	CHECK(objBoard.GetCount() == 2);
	CHECK(objBoard.GetRank(1001, 500) == 1);
	CHECK(objBoard.RemoveEntry(9999) == false);
}

static void Test_Clear()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.Clear();
	CHECK(objBoard.GetCount() == 0);
	CHECK(objBoard.GetRank(1001, 500) == 0);
}

static void Test_GetRankByKey()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	CHECK(objBoard.GetRank(1002) == 1);
	CHECK(objBoard.GetRank(1001) == 2);
	CHECK(objBoard.GetRank(1003) == 3);
	CHECK(objBoard.GetRank(9999) == 0);
}

static void Test_RemoveReinsert()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1, 100);
	objBoard.UpdateEntry(2, 200);
	objBoard.RemoveEntry(2, 200);
	CHECK(objBoard.GetCount() == 1);
	objBoard.UpdateEntry(2, 300);
	CHECK(objBoard.GetCount() == 2);
	CHECK(objBoard.GetRank(2, 300) == 1);
	CHECK(objBoard.GetRank(1, 100) == 2);
}

static void Test_GetEntryByRank()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);

	bool bFound = objBoard.ForeachEntryByRank(1, [](uint32_t uiRank, const auto& stNode)
	{
		(void)uiRank;
		CHECK(stNode.key == 1002);
		CHECK(stNode.value == 800);
	});
	CHECK(bFound == true);

	bFound = objBoard.ForeachEntryByRank(2, [](uint32_t, const auto& stNode)
	{
		CHECK(stNode.key == 1001);
	});
	CHECK(bFound == true);
	CHECK(objBoard.ForeachEntryByRank(0, [](uint32_t, const auto&) {}) == false);
	CHECK(objBoard.ForeachEntryByRank(3, [](uint32_t, const auto&) {}) == false);
}

static void Test_GetTopN()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	objBoard.UpdateEntry(1004, 700);

	std::vector<uint64_t> vecKeys;
	uint32_t uiCount = objBoard.ForeachTopN(2, [&vecKeys](uint32_t, const auto& stNode)
	{
		vecKeys.push_back(stNode.key);
	});
	CHECK(uiCount == 2);
	CHECK(vecKeys.size() == 2);
	CHECK(vecKeys[0] == 1002);
	CHECK(vecKeys[1] == 1004);
	uiCount = objBoard.ForeachTopN(100, [](uint32_t, const auto&) {});
	CHECK(uiCount == 4);
	uiCount = objBoard.ForeachTopN(0, [](uint32_t, const auto&) {});
	CHECK(uiCount == 0);
}

static void Test_GetAroundRank()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	for (uint64_t ui = 1; ui <= 10; ++ui)
		objBoard.UpdateEntry(ui, static_cast<int64_t>(ui * 100));

	std::vector<uint64_t> vecKeys;
	uint32_t uiCount = objBoard.ForeachAroundRank(5, 5, [&vecKeys](uint32_t, const auto& stNode)
	{
		vecKeys.push_back(stNode.key);
	});
	CHECK(uiCount == 5);
	CHECK(vecKeys[0] == 8);
	CHECK(vecKeys[4] == 4);

	vecKeys.clear();
	uiCount = objBoard.ForeachAroundRank(1, 5, [&vecKeys](uint32_t, const auto& stNode)
	{
		vecKeys.push_back(stNode.key);
	});
	CHECK(uiCount == 5);
	CHECK(vecKeys[0] == 10);

	vecKeys.clear();
	uiCount = objBoard.ForeachAroundRank(10, 5, [&vecKeys](uint32_t, const auto& stNode)
	{
		vecKeys.push_back(stNode.key);
	});
	CHECK(uiCount == 5);
	CHECK(vecKeys[4] == 1);

	CHECK(objBoard.ForeachAroundRank(0, 5, [](uint32_t, const auto&) {}) == 0);
	CHECK(objBoard.ForeachAroundRank(11, 5, [](uint32_t, const auto&) {}) == 0);
}

static void Test_GetEntry()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	bool bFound = objBoard.ForeachEntry(1001, [](uint32_t uiRank, const auto& stNode)
	{
		CHECK(uiRank == 1);
		CHECK(stNode.value == 500);
	});
	CHECK(bFound == true);
	CHECK(objBoard.ForeachEntry(9999, [](uint32_t, const auto&) {}) == false);
}

static void Test_MaxSize()
{
	TLeaderboard<uint64_t, int64_t> objBoard(3);
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	CHECK(objBoard.GetCount() == 3);
	uint32_t r = objBoard.UpdateEntry(1004, 100);
	CHECK(r == 0);
	CHECK(objBoard.GetCount() == 3);
	r = objBoard.UpdateEntry(1005, 600);
	CHECK(r == 2);
	CHECK(objBoard.GetCount() == 3);
	CHECK(objBoard.GetRank(1003, 300) == 0);
}

static void Test_MaxSizeUpdate()
{
	TLeaderboard<uint64_t, int64_t> objBoard(3);
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	uint32_t r = objBoard.UpdateEntry(1003, 900);
	CHECK(r == 1);
	CHECK(objBoard.GetCount() == 3);
}

static void Test_SetMaxSize()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 800);
	objBoard.UpdateEntry(1003, 300);
	objBoard.UpdateEntry(1004, 700);
	objBoard.UpdateEntry(1005, 600);
	CHECK(objBoard.GetCount() == 5);
	CHECK(objBoard.GetMaxSize() == 0);
	objBoard.SetMaxSize(3);
	CHECK(objBoard.GetMaxSize() == 3);
	CHECK(objBoard.GetCount() == 3);
	CHECK(objBoard.GetRank(1002, 800) == 1);
	CHECK(objBoard.GetRank(1004, 700) == 2);
	CHECK(objBoard.GetRank(1005, 600) == 3);
	CHECK(objBoard.GetRank(1001, 500) == 0);
	CHECK(objBoard.GetRank(1003, 300) == 0);
	objBoard.SetMaxSize(0);
	CHECK(objBoard.GetMaxSize() == 0);
	objBoard.UpdateEntry(1006, 100);
	CHECK(objBoard.GetCount() == 4);
}

static void Test_MaxSizeOne()
{
	TLeaderboard<uint64_t, int64_t> objBoard(1);
	CHECK(objBoard.UpdateEntry(1, 100) == 1);
	CHECK(objBoard.GetCount() == 1);
	CHECK(objBoard.UpdateEntry(2, 200) == 1);
	CHECK(objBoard.GetCount() == 1);
	CHECK(objBoard.GetRank(2, 200) == 1);
	CHECK(objBoard.GetRank(1, 100) == 0);
	CHECK(objBoard.UpdateEntry(3, 50) == 0);
	CHECK(objBoard.GetCount() == 1);
}

static void Test_Reserve()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(1000);
	CHECK(objBoard.GetCount() == 0);
	objBoard.UpdateEntry(1001, 500);
	CHECK(objBoard.GetCount() == 1);
}

static void Test_SameValueTieBreak()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1003, 500);
	objBoard.UpdateEntry(1001, 500);
	objBoard.UpdateEntry(1002, 500);
	CHECK(objBoard.GetRank(1001, 500) == 1);
	CHECK(objBoard.GetRank(1002, 500) == 2);
	CHECK(objBoard.GetRank(1003, 500) == 3);
}

static void Test_MixedSameValue()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(10, 800);
	objBoard.UpdateEntry(20, 500);
	objBoard.UpdateEntry(30, 500);
	objBoard.UpdateEntry(40, 500);
	objBoard.UpdateEntry(50, 300);
	CHECK(objBoard.GetRank(10, 800) == 1);
	CHECK(objBoard.GetRank(20, 500) == 2);
	CHECK(objBoard.GetRank(30, 500) == 3);
	CHECK(objBoard.GetRank(40, 500) == 4);
	CHECK(objBoard.GetRank(50, 300) == 5);
}

static void Test_CustomCompare()
{
	TLeaderboard<uint64_t, ST_RANK_DATA, ST_RANK_DATA_COMPARE> objBoard;
	objBoard.UpdateEntry(1001, {500, 100});
	objBoard.UpdateEntry(1002, {500, 200});
	objBoard.UpdateEntry(1003, {800, 300});
	CHECK(objBoard.GetRank(1003, {800, 300}) == 1);
	CHECK(objBoard.GetRank(1001, {500, 100}) == 2);
	CHECK(objBoard.GetRank(1002, {500, 200}) == 3);
}

static void Test_CustomCompareRemove()
{
	TLeaderboard<uint64_t, ST_RANK_DATA, ST_RANK_DATA_COMPARE> objBoard;
	objBoard.UpdateEntry(1, {100, 10});
	objBoard.UpdateEntry(2, {100, 20});
	objBoard.UpdateEntry(3, {200, 30});
	CHECK(objBoard.RemoveEntry(3, {200, 30}) == true);
	CHECK(objBoard.GetCount() == 2);
	CHECK(objBoard.GetRank(1, {100, 10}) == 1);
	objBoard.UpdateEntry(2, {300, 40});
	CHECK(objBoard.GetRank(2, {300, 40}) == 1);
}

static void Test_EmptyBoard()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	CHECK(objBoard.GetCount() == 0);
	CHECK(objBoard.GetRank(1001, 0) == 0);
	CHECK(objBoard.GetRank(1001) == 0);
	CHECK(objBoard.ForeachEntryByRank(1, [](uint32_t, const auto&) {}) == false);
	CHECK(objBoard.ForeachEntry(1001, [](uint32_t, const auto&) {}) == false);
	CHECK(objBoard.RemoveEntry(1001) == false);
	CHECK(objBoard.RemoveEntry(1001, 0) == false);
	CHECK(objBoard.ForeachTopN(10, [](uint32_t, const auto&) {}) == 0);
	CHECK(objBoard.ForeachAroundRank(1, 5, [](uint32_t, const auto&) {}) == 0);
}

static void Test_SingleEntry()
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.UpdateEntry(1001, 500);
	CHECK(objBoard.GetCount() == 1);
	CHECK(objBoard.GetRank(1001, 500) == 1);
	CHECK(objBoard.GetRank(1001) == 1);
	uint64_t ulFoundKey = 0;
	uint32_t uiCount = objBoard.ForeachAroundRank(1, 5, [&ulFoundKey](uint32_t, const auto& stNode)
	{
		ulFoundKey = stNode.key;
	});
	CHECK(uiCount == 1);
	CHECK(ulFoundKey == 1001);
}

static void Test_StringKey()
{
	TLeaderboard<std::string, int64_t> objBoard;
	objBoard.UpdateEntry("Alice", 500);
	objBoard.UpdateEntry("Bob", 800);
	CHECK(objBoard.GetRank("Bob", 800) == 1);
	CHECK(objBoard.GetRank("Alice", 500) == 2);
	CHECK(objBoard.GetRank("Bob") == 1);
}

static void Test_AscendingOrder()
{
	TLeaderboard<uint64_t, int64_t, std::less<int64_t>> objBoard;
	objBoard.UpdateEntry(1, 500);
	objBoard.UpdateEntry(2, 100);
	objBoard.UpdateEntry(3, 800);
	CHECK(objBoard.GetRank(2, 100) == 1);
	CHECK(objBoard.GetRank(1, 500) == 2);
	CHECK(objBoard.GetRank(3, 800) == 3);
}

static void Test_LargeScale()
{
	const uint32_t COUNT = 10000;
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(COUNT);
	for (uint64_t ui = 0; ui < COUNT; ++ui)
		objBoard.UpdateEntry(ui, static_cast<int64_t>(ui));
	CHECK(objBoard.GetCount() == COUNT);
	CHECK(objBoard.GetRank(COUNT - 1, static_cast<int64_t>(COUNT - 1)) == 1);
	CHECK(objBoard.GetRank(0, 0) == COUNT);
	std::vector<uint64_t> vecKeys;
	uint32_t uiCount = objBoard.ForeachTopN(5, [&vecKeys](uint32_t, const auto& stNode)
	{
		vecKeys.push_back(stNode.key);
	});
	CHECK(uiCount == 5);
	CHECK(vecKeys[0] == COUNT - 1);
	CHECK(vecKeys[4] == COUNT - 5);
}

// ============================================================
//  运行全部单元测试
// ============================================================
static void RunUnitTests()
{
	printf("=== 单元测试 ===\n\n");

	printf("[基本操作]\n");
	RUN_TEST("基本操作", "插入与查询排名",       Test_InsertAndGetRank);
	RUN_TEST("基本操作", "更新已有条目",          Test_UpdateExisting);
	RUN_TEST("基本操作", "UpdateEntry 返回值",    Test_ReturnValue);
	RUN_TEST("基本操作", "按 key+value 删除",     Test_RemoveWithValue);
	RUN_TEST("基本操作", "按 key 删除",           Test_RemoveByKey);
	RUN_TEST("基本操作", "清空排行榜",            Test_Clear);
	RUN_TEST("基本操作", "按 key 查询排名",       Test_GetRankByKey);
	RUN_TEST("基本操作", "删除后重新插入",        Test_RemoveReinsert);

	printf("\n[查询操作]\n");
	RUN_TEST("查询操作", "按排名获取节点",        Test_GetEntryByRank);
	RUN_TEST("查询操作", "获取 TopN",             Test_GetTopN);
	RUN_TEST("查询操作", "获取排名附近玩家",      Test_GetAroundRank);
	RUN_TEST("查询操作", "按 key 查找节点",       Test_GetEntry);

	printf("\n[MaxSize 截断]\n");
	RUN_TEST("MaxSize 截断", "基本截断",           Test_MaxSize);
	RUN_TEST("MaxSize 截断", "更新已有条目不截断", Test_MaxSizeUpdate);
	RUN_TEST("MaxSize 截断", "动态 SetMaxSize",    Test_SetMaxSize);
	RUN_TEST("MaxSize 截断", "MaxSize=1 极限场景", Test_MaxSizeOne);
	RUN_TEST("MaxSize 截断", "Reserve 预分配",     Test_Reserve);

	printf("\n[同分排序]\n");
	RUN_TEST("同分排序", "同分按 key 决胜",       Test_SameValueTieBreak);
	RUN_TEST("同分排序", "混合同分不同分",        Test_MixedSameValue);

	printf("\n[自定义比较器]\n");
	RUN_TEST("自定义比较器", "多维排序",           Test_CustomCompare);
	RUN_TEST("自定义比较器", "多维排序增删",       Test_CustomCompareRemove);

	printf("\n[边界情况]\n");
	RUN_TEST("边界情况", "空排行榜",              Test_EmptyBoard);
	RUN_TEST("边界情况", "单条目",                Test_SingleEntry);
	RUN_TEST("边界情况", "字符串 key",            Test_StringKey);
	RUN_TEST("边界情况", "升序排行榜 (std::less)", Test_AscendingOrder);
	RUN_TEST("边界情况", "万级数据正确性",        Test_LargeScale);

	uint32_t uiPassed = 0;
	uint32_t uiTotal = static_cast<uint32_t>(g_vecTestResults.size());
	for (const auto& r : g_vecTestResults)
	{
		if (r.bPassed) ++uiPassed;
	}
	printf("\n单元测试: %u/%u 通过\n", uiPassed, uiTotal);
}

// ============================================================
//  基准测试结果
// ============================================================
struct ST_BENCH_RESULT
{
	std::string strName;
	uint32_t uiScale;
	uint32_t uiOps;
	double dbTotalMs;
	double dbAvgUs;
};

static std::vector<ST_BENCH_RESULT> g_vecBenchResults;

static void AddResult(const char* pszName, uint32_t uiScale, uint32_t uiOps,
                       double dbTotalMs)
{
	ST_BENCH_RESULT stResult;
	stResult.strName = pszName;
	stResult.uiScale = uiScale;
	stResult.uiOps = uiOps;
	stResult.dbTotalMs = dbTotalMs;
	stResult.dbAvgUs = dbTotalMs * 1000.0 / uiOps;
	g_vecBenchResults.push_back(stResult);

	printf("  %-35s  scale=%-7u  ops=%-7u  total=%.2f ms  avg=%.3f us/op\n",
	       pszName, uiScale, uiOps, dbTotalMs, stResult.dbAvgUs);
}

// ============================================================
//  基准测试
// ============================================================
static void BenchInsert(uint32_t uiScale)
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(uiScale);

	std::mt19937 rng(42);
	std::uniform_int_distribution<int64_t> dist(0, 10000000);

	std::vector<int64_t> vecScores(uiScale);
	for (auto& s : vecScores) s = dist(rng);

	// UpdateEntry：每次 FindByKey O(N) 查重
	{
		TLeaderboard<uint64_t, int64_t> objBoardCopy;
		objBoardCopy.Reserve(uiScale);

		CStopWatch sw;
		for (uint32_t ui = 0; ui < uiScale; ++ui)
			objBoardCopy.UpdateEntry(ui + 1, vecScores[ui]);
		AddResult("UpdateEntry (插入新条目)", uiScale, uiScale, sw.ElapsedMs());
	}

	// InsertEntry：跳过查重，直接二分插入
	{
		TLeaderboard<uint64_t, int64_t> objBoardCopy;
		objBoardCopy.Reserve(uiScale);

		CStopWatch sw;
		for (uint32_t ui = 0; ui < uiScale; ++ui)
			objBoardCopy.InsertEntry(ui + 1, vecScores[ui]);
		AddResult("InsertEntry (插入新条目)", uiScale, uiScale, sw.ElapsedMs());
	}
}

static void BenchUpdate(uint32_t uiScale)
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(uiScale);

	std::mt19937 rng(42);
	std::uniform_int_distribution<int64_t> distScore(0, 10000000);

	// 记录每个 key 的当前 value，用于三参数重载
	std::vector<int64_t> vecValues(uiScale + 1, 0);
	for (uint32_t ui = 0; ui < uiScale; ++ui)
	{
		int64_t iScore = distScore(rng);
		vecValues[ui + 1] = iScore;
		objBoard.UpdateEntry(ui + 1, iScore);
	}

	const uint32_t OPS = std::min(uiScale, (uint32_t)1000);
	std::uniform_int_distribution<uint64_t> distKey(1, uiScale);

	// 预生成测试数据
	std::vector<uint64_t> vecKeys(OPS);
	std::vector<int64_t> vecNewValues(OPS);
	for (uint32_t ui = 0; ui < OPS; ++ui)
	{
		vecKeys[ui] = distKey(rng);
		vecNewValues[ui] = distScore(rng);
	}

	// 旧版：无旧值，O(N) 线性扫描
	{
		auto objBoardCopy = objBoard;
		auto vecValuesCopy = vecValues;
		CStopWatch sw;
		for (uint32_t ui = 0; ui < OPS; ++ui)
		{
			objBoardCopy.UpdateEntry(vecKeys[ui], vecNewValues[ui]);
			vecValuesCopy[vecKeys[ui]] = vecNewValues[ui];
		}
		AddResult("UpdateEntry (更新已有, 无旧值)", uiScale, OPS, sw.ElapsedMs());
	}

	// 新版：有旧值，O(log N) 二分定位
	{
		auto objBoardCopy = objBoard;
		auto vecValuesCopy = vecValues;
		CStopWatch sw;
		for (uint32_t ui = 0; ui < OPS; ++ui)
		{
			objBoardCopy.UpdateEntry(vecKeys[ui], vecValuesCopy[vecKeys[ui]], vecNewValues[ui]);
			vecValuesCopy[vecKeys[ui]] = vecNewValues[ui];
		}
		AddResult("UpdateEntry (更新已有, 有旧值)", uiScale, OPS, sw.ElapsedMs());
	}
}

static void BenchGetRankWithValue(uint32_t uiScale)
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(uiScale);

	std::mt19937 rng(42);
	std::uniform_int_distribution<int64_t> distScore(0, 10000000);

	std::vector<int64_t> vecScores(uiScale);
	for (uint32_t ui = 0; ui < uiScale; ++ui)
	{
		vecScores[ui] = distScore(rng);
		objBoard.UpdateEntry(ui + 1, vecScores[ui]);
	}

	const uint32_t OPS = std::min(uiScale, (uint32_t)10000);
	std::uniform_int_distribution<uint64_t> distKey(1, uiScale);

	CStopWatch sw;
	for (uint32_t ui = 0; ui < OPS; ++ui)
	{
		uint64_t ulKey = distKey(rng);
		volatile uint32_t r = objBoard.GetRank(ulKey, vecScores[ulKey - 1]);
		(void)r;
	}
	AddResult("GetRank (key+value, O(logN))", uiScale, OPS, sw.ElapsedMs());
}

static void BenchGetRankByKey(uint32_t uiScale)
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(uiScale);

	std::mt19937 rng(42);
	std::uniform_int_distribution<int64_t> distScore(0, 10000000);

	for (uint32_t ui = 0; ui < uiScale; ++ui)
		objBoard.UpdateEntry(ui + 1, distScore(rng));

	const uint32_t OPS = std::min(uiScale, (uint32_t)1000);
	std::uniform_int_distribution<uint64_t> distKey(1, uiScale);

	CStopWatch sw;
	for (uint32_t ui = 0; ui < OPS; ++ui)
	{
		volatile uint32_t r = objBoard.GetRank(distKey(rng));
		(void)r;
	}
	AddResult("GetRank (仅 key, O(N))", uiScale, OPS, sw.ElapsedMs());
}

static void BenchRemoveWithValue(uint32_t uiScale)
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(uiScale);

	std::mt19937 rng(42);
	std::uniform_int_distribution<int64_t> distScore(0, 10000000);

	std::vector<int64_t> vecScores(uiScale);
	for (uint32_t ui = 0; ui < uiScale; ++ui)
	{
		vecScores[ui] = distScore(rng);
		objBoard.UpdateEntry(ui + 1, vecScores[ui]);
	}

	const uint32_t OPS = std::min(uiScale, (uint32_t)1000);

	CStopWatch sw;
	for (uint32_t ui = 0; ui < OPS; ++ui)
		objBoard.RemoveEntry(static_cast<uint64_t>(ui + 1), vecScores[ui]);
	AddResult("RemoveEntry (key+value, O(logN))", uiScale, OPS, sw.ElapsedMs());
}

static void BenchRemoveByKey(uint32_t uiScale)
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(uiScale);

	std::mt19937 rng(42);
	std::uniform_int_distribution<int64_t> distScore(0, 10000000);

	for (uint32_t ui = 0; ui < uiScale; ++ui)
		objBoard.UpdateEntry(ui + 1, distScore(rng));

	const uint32_t OPS = std::min(uiScale, (uint32_t)1000);

	CStopWatch sw;
	for (uint32_t ui = 0; ui < OPS; ++ui)
		objBoard.RemoveEntry(static_cast<uint64_t>(ui + 1));
	AddResult("RemoveEntry (仅 key, O(N))", uiScale, OPS, sw.ElapsedMs());
}

static void BenchForeachTopN(uint32_t uiScale)
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(uiScale);

	std::mt19937 rng(42);
	std::uniform_int_distribution<int64_t> distScore(0, 10000000);

	for (uint32_t ui = 0; ui < uiScale; ++ui)
		objBoard.UpdateEntry(ui + 1, distScore(rng));

	const uint32_t OPS = 100000;

	CStopWatch sw;
	for (uint32_t ui = 0; ui < OPS; ++ui)
	{
		volatile uint32_t r = objBoard.ForeachTopN(100, [](uint32_t, const auto&) {});
		(void)r;
	}
	AddResult("ForeachTopN(100)", uiScale, OPS, sw.ElapsedMs());
}

static void BenchForeachAroundRank(uint32_t uiScale)
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(uiScale);

	std::mt19937 rng(42);
	std::uniform_int_distribution<int64_t> distScore(0, 10000000);

	for (uint32_t ui = 0; ui < uiScale; ++ui)
		objBoard.UpdateEntry(ui + 1, distScore(rng));

	const uint32_t OPS = 100000;
	uint32_t uiCenter = uiScale / 2;

	CStopWatch sw;
	for (uint32_t ui = 0; ui < OPS; ++ui)
	{
		volatile uint32_t r = objBoard.ForeachAroundRank(uiCenter, 20, [](uint32_t, const auto&) {});
		(void)r;
	}
	AddResult("ForeachAroundRank(center, 20)", uiScale, OPS, sw.ElapsedMs());
}

static void BenchForeachEntryByRank(uint32_t uiScale)
{
	TLeaderboard<uint64_t, int64_t> objBoard;
	objBoard.Reserve(uiScale);

	std::mt19937 rng(42);
	std::uniform_int_distribution<int64_t> distScore(0, 10000000);

	for (uint32_t ui = 0; ui < uiScale; ++ui)
		objBoard.UpdateEntry(ui + 1, distScore(rng));

	const uint32_t OPS = 100000;
	std::uniform_int_distribution<uint32_t> distRank(1, uiScale);

	CStopWatch sw;
	for (uint32_t ui = 0; ui < OPS; ++ui)
	{
		volatile bool b = objBoard.ForeachEntryByRank(distRank(rng), [](uint32_t, const auto&) {});
		(void)b;
	}
	AddResult("ForeachEntryByRank", uiScale, OPS, sw.ElapsedMs());
}

static void RunBenchmarks()
{
	printf("\n=== 性能基准测试 ===\n\n");

	const uint32_t SCALES[] = { 1000, 10000, 100000 };
	const uint32_t NUM_SCALES = sizeof(SCALES) / sizeof(SCALES[0]);

	for (uint32_t si = 0; si < NUM_SCALES; ++si)
	{
		uint32_t uiScale = SCALES[si];
		printf("[数据规模: %u]\n", uiScale);

		BenchInsert(uiScale);
		BenchUpdate(uiScale);
		BenchGetRankWithValue(uiScale);
		BenchGetRankByKey(uiScale);
		BenchRemoveWithValue(uiScale);
		BenchRemoveByKey(uiScale);
		BenchForeachTopN(uiScale);
		BenchForeachAroundRank(uiScale);
		BenchForeachEntryByRank(uiScale);

		printf("\n");
	}
}

// ============================================================
//  JSON 转义
// ============================================================
static std::string JsonEscape(const std::string& str)
{
	std::string result;
	for (char c : str)
	{
		if (c == '"') result += "\\\"";
		else if (c == '\\') result += "\\\\";
		else result += c;
	}
	return result;
}

// ============================================================
//  生成 HTML 报告
// ============================================================
static void GenerateHtmlReport(const char* pszPath)
{
	std::ofstream ofs(pszPath);
	if (!ofs.is_open())
	{
		printf("ERROR: cannot open %s for writing\n", pszPath);
		return;
	}

	// 构造测试结果 JSON
	std::ostringstream ossTests;
	ossTests << "[\n";
	for (size_t i = 0; i < g_vecTestResults.size(); ++i)
	{
		const auto& r = g_vecTestResults[i];
		ossTests << "  {\"name\":\"" << JsonEscape(r.strName)
		         << "\",\"category\":\"" << JsonEscape(r.strCategory)
		         << "\",\"passed\":" << (r.bPassed ? "true" : "false")
		         << ",\"detail\":\"" << JsonEscape(r.strDetail)
		         << "\"}";
		if (i + 1 < g_vecTestResults.size()) ossTests << ",";
		ossTests << "\n";
	}
	ossTests << "]";

	// 构造基准结果 JSON
	std::ostringstream ossBench;
	ossBench << "[\n";
	for (size_t i = 0; i < g_vecBenchResults.size(); ++i)
	{
		const auto& r = g_vecBenchResults[i];
		ossBench << "  {\"name\":\"" << JsonEscape(r.strName)
		         << "\",\"scale\":" << r.uiScale
		         << ",\"ops\":" << r.uiOps
		         << ",\"totalMs\":" << r.dbTotalMs
		         << ",\"avgUs\":" << r.dbAvgUs
		         << "}";
		if (i + 1 < g_vecBenchResults.size()) ossBench << ",";
		ossBench << "\n";
	}
	ossBench << "]";

	ofs << R"(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>TLeaderboard 测试报告</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  font-family: -apple-system, "Microsoft YaHei", "Segoe UI", sans-serif;
  background: #f0f2f5; color: #333; padding: 20px;
}
.container { max-width: 1200px; margin: 0 auto; }
h1 { text-align: center; color: #1a1a2e; margin-bottom: 8px; font-size: 28px; }
.subtitle { text-align: center; color: #666; margin-bottom: 30px; font-size: 14px; }
.card {
  background: #fff; border-radius: 12px; padding: 24px;
  margin-bottom: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.08);
}
.card h2 {
  font-size: 18px; color: #1a1a2e; margin-bottom: 16px;
  padding-bottom: 8px; border-bottom: 2px solid #e8e8e8;
}
table { width: 100%; border-collapse: collapse; font-size: 14px; }
th {
  background: #f7f8fa; color: #555; font-weight: 600;
  padding: 10px 12px; text-align: left; border-bottom: 2px solid #e8e8e8;
}
td { padding: 10px 12px; border-bottom: 1px solid #f0f0f0; }
tr:hover td { background: #f7f8fa; }
.num { text-align: right; font-family: "Cascadia Code", "Consolas", monospace; }

/* 汇总卡片 */
.summary-grid {
  display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
  gap: 16px; margin-bottom: 20px;
}
.summary-item {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border-radius: 10px; padding: 16px; color: #fff;
}
.summary-item.green  { background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%); }
.summary-item.orange { background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); }
.summary-item.blue   { background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%); }
.summary-item.teal   { background: linear-gradient(135deg, #43e97b 0%, #38f9d7 100%); }
.summary-label { font-size: 12px; opacity: 0.9; }
.summary-value { font-size: 24px; font-weight: 700; margin-top: 4px; }

/* 测试结果 */
.test-badge {
  display: inline-block; padding: 2px 10px; border-radius: 10px;
  font-size: 12px; font-weight: 600;
}
.test-badge.pass { background: #d4edda; color: #155724; }
.test-badge.fail { background: #f8d7da; color: #721c24; }
.test-category {
  background: #e8e8e8; color: #555; padding: 4px 10px; border-radius: 6px;
  font-size: 12px; font-weight: 600; display: inline-block; margin-bottom: 8px;
}
.test-detail { color: #dc3545; font-size: 12px; margin-top: 4px; }

/* 性能条形图 */
.bar-cell { width: 30%; }
.bar-container {
  background: #f0f0f0; border-radius: 4px; height: 22px;
  position: relative; overflow: hidden;
}
.bar { height: 100%; border-radius: 4px; min-width: 2px; transition: width 0.6s ease; }
.bar-label {
  position: absolute; right: 6px; top: 2px;
  font-size: 11px; color: #666; font-family: monospace;
}

/* Tab */
.tab-bar { display: flex; gap: 4px; margin-bottom: 16px; flex-wrap: wrap; }
.tab-btn {
  padding: 8px 16px; border: none; background: #e8e8e8;
  border-radius: 6px 6px 0 0; cursor: pointer; font-size: 14px;
  color: #666; transition: all 0.2s;
}
.tab-btn.active { background: #fff; color: #1a1a2e; font-weight: 600; }
.tab-panel { display: none; }
.tab-panel.active { display: block; }

/* 图例 */
.legend { display: flex; gap: 20px; margin-bottom: 16px; flex-wrap: wrap; }
.legend-item { display: flex; align-items: center; gap: 6px; font-size: 13px; }
.legend-dot { width: 12px; height: 12px; border-radius: 3px; }

/* 复杂度 */
.complexity-tag {
  display: inline-block; padding: 2px 8px; border-radius: 4px;
  font-size: 12px; font-weight: 600;
}
.complexity-tag.logn { background: #d4edda; color: #155724; }
.complexity-tag.n    { background: #fff3cd; color: #856404; }
.complexity-tag.o1   { background: #cce5ff; color: #004085; }
</style>
</head>
<body>
<div class="container">
  <h1>TLeaderboard 测试报告</h1>
  <p class="subtitle">基于有序 vector 实现的百万级实时排行榜 &mdash; 生成时间：)" << __DATE__ << " " << __TIME__ << R"(</p>

  <div class="summary-grid" id="summary"></div>

  <!-- 单元测试 -->
  <div class="card">
    <h2>单元测试结果</h2>
    <div id="testResults"></div>
  </div>

  <!-- 性能基准 -->
  <div class="card">
    <h2>性能基准测试 - 按数据规模</h2>
    <div class="tab-bar" id="tabBar"></div>
    <div id="tabPanels"></div>
  </div>

  <div class="card">
    <h2>操作耗时对比（按操作类型分组）</h2>
    <div id="byOperation"></div>
  </div>

  <div class="card">
    <h2>复杂度说明</h2>
    <table>
      <thead><tr><th>操作</th><th>时间复杂度</th><th>说明</th></tr></thead>
      <tbody>
        <tr><td>InsertEntry (新插入)</td><td><span class="complexity-tag n">O(log N + N)</span></td><td>二分定位插入位置 + 数组移动，跳过查重</td></tr>
        <tr><td>UpdateEntry (无旧值)</td><td><span class="complexity-tag n">O(N)</span></td><td>线性查找旧条目 + 二分定位插入位置 + 数组移动</td></tr>
        <tr><td>UpdateEntry (有旧值)</td><td><span class="complexity-tag n">O(log N + N)</span></td><td>二分定位旧条目 + 二分定位插入位置 + 数组移动</td></tr>
        <tr><td>GetRank (key+value)</td><td><span class="complexity-tag logn">O(log N)</span></td><td>全序比较器二分精确定位</td></tr>
        <tr><td>GetRank (仅 key)</td><td><span class="complexity-tag n">O(N)</span></td><td>线性扫描匹配 key</td></tr>
        <tr><td>RemoveEntry (key+value)</td><td><span class="complexity-tag n">O(log N + N)</span></td><td>二分定位 O(log N) + 数组移动 O(N)</td></tr>
        <tr><td>RemoveEntry (仅 key)</td><td><span class="complexity-tag n">O(N)</span></td><td>线性扫描 + 数组移动</td></tr>
        <tr><td>GetTopN / GetAroundRank / GetEntryByRank</td><td><span class="complexity-tag o1">O(1)</span></td><td>直接指针偏移，零拷贝</td></tr>
      </tbody>
    </table>
  </div>
</div>

<script>
const TESTS = )" << ossTests.str() << R"(;
const BENCH = )" << ossBench.str() << R"(;

const COLORS = ['#667eea','#11998e','#f5576c','#4facfe','#fa709a','#fee140','#a18cd1','#fbc2eb'];

// 汇总
function renderSummary() {
  const totalTests = TESTS.length;
  const passedTests = TESTS.filter(t => t.passed).length;
  const failedTests = totalTests - passedTests;
  const scales = [...new Set(BENCH.map(d => d.scale))].sort((a,b) => a-b);
  const maxScale = scales[scales.length - 1];
  const maxData = BENCH.filter(d => d.scale === maxScale);
  const insertData = maxData.find(d => d.name.includes('插入'));
  const getRankLogN = maxData.find(d => d.name.includes('O(logN)') && d.name.includes('GetRank'));

  document.getElementById('summary').innerHTML = `
    <div class="summary-item ${failedTests === 0 ? 'green' : 'orange'}">
      <div class="summary-label">单元测试</div>
      <div class="summary-value">${passedTests}/${totalTests} 通过</div>
    </div>
    <div class="summary-item teal">
      <div class="summary-label">最大测试规模</div>
      <div class="summary-value">${maxScale >= 10000 ? (maxScale/10000) + ' 万' : maxScale}</div>
    </div>
    <div class="summary-item">
      <div class="summary-label">插入 ${maxScale >= 10000 ? (maxScale/10000)+'万' : maxScale} 条耗时</div>
      <div class="summary-value">${insertData ? insertData.totalMs.toFixed(0) : '-'} ms</div>
    </div>
    <div class="summary-item blue">
      <div class="summary-label">GetRank(O(logN)) 平均耗时</div>
      <div class="summary-value">${getRankLogN ? getRankLogN.avgUs.toFixed(3) : '-'} us</div>
    </div>`;
}

// 单元测试
function renderTests() {
  const container = document.getElementById('testResults');
  const categories = [...new Set(TESTS.map(t => t.category))];

  let html = '';
  categories.forEach(cat => {
    const items = TESTS.filter(t => t.category === cat);
    const allPassed = items.every(t => t.passed);
    html += `<div style="margin-bottom:16px">
      <span class="test-category">${cat}</span>
      <span class="test-badge ${allPassed ? 'pass' : 'fail'}">${items.filter(t=>t.passed).length}/${items.length}</span>
      <table style="margin-top:8px">
        <thead><tr><th style="width:50%">测试用例</th><th style="width:15%">结果</th><th>详情</th></tr></thead>
        <tbody>`;
    items.forEach(t => {
      html += `<tr>
        <td>${t.name}</td>
        <td><span class="test-badge ${t.passed ? 'pass' : 'fail'}">${t.passed ? '通过' : '失败'}</span></td>
        <td>${t.detail ? '<span class="test-detail">' + t.detail + '</span>' : '<span style="color:#999">-</span>'}</td>
      </tr>`;
    });
    html += '</tbody></table></div>';
  });
  container.innerHTML = html;
}

// 性能 tabs
function renderTabs() {
  const scales = [...new Set(BENCH.map(d => d.scale))].sort((a,b) => a-b);
  const tabBar = document.getElementById('tabBar');
  const panels = document.getElementById('tabPanels');

  scales.forEach((scale, idx) => {
    const btn = document.createElement('button');
    btn.className = 'tab-btn' + (idx === 0 ? ' active' : '');
    btn.textContent = scale >= 10000 ? (scale/10000) + ' 万' : scale.toLocaleString();
    btn.onclick = () => {
      document.querySelectorAll('.tab-btn').forEach((b,i) => b.className = 'tab-btn' + (i===idx?' active':''));
      document.querySelectorAll('.tab-panel').forEach((p,i) => p.className = 'tab-panel' + (i===idx?' active':''));
    };
    tabBar.appendChild(btn);

    const panel = document.createElement('div');
    panel.className = 'tab-panel' + (idx === 0 ? ' active' : '');

    const items = BENCH.filter(d => d.scale === scale);
    const maxAvg = Math.max(...items.map(d => d.avgUs));

    let rows = items.map((d, i) => {
      const pct = (d.avgUs / maxAvg * 100).toFixed(1);
      const color = COLORS[i % COLORS.length];
      return `<tr>
        <td>${d.name}</td>
        <td class="num">${d.ops.toLocaleString()}</td>
        <td class="num">${d.totalMs.toFixed(2)}</td>
        <td class="num">${d.avgUs.toFixed(3)}</td>
        <td class="bar-cell">
          <div class="bar-container">
            <div class="bar" style="width:${pct}%;background:${color}"></div>
            <div class="bar-label">${d.avgUs.toFixed(1)} us</div>
          </div>
        </td>
      </tr>`;
    }).join('');

    panel.innerHTML = `<table>
      <thead><tr>
        <th>操作名称</th><th class="num">执行次数</th>
        <th class="num">总耗时 (ms)</th><th class="num">平均耗时 (us)</th>
        <th>耗时分布</th>
      </tr></thead>
      <tbody>${rows}</tbody>
    </table>`;
    panels.appendChild(panel);
  });
}

// 按操作分组
function renderByOperation() {
  const container = document.getElementById('byOperation');
  const opNames = [...new Set(BENCH.map(d => d.name))];
  const scales = [...new Set(BENCH.map(d => d.scale))].sort((a,b) => a-b);

  let html = '<div class="legend">';
  scales.forEach((s, i) => {
    const label = s >= 10000 ? (s/10000) + ' 万' : s.toLocaleString();
    html += `<div class="legend-item"><div class="legend-dot" style="background:${COLORS[i]}"></div>${label}</div>`;
  });
  html += '</div>';

  const globalMax = Math.max(...BENCH.map(d => d.avgUs));

  opNames.forEach(name => {
    const items = BENCH.filter(d => d.name === name).sort((a,b) => a.scale - b.scale);
    html += `<div style="margin-bottom:16px"><strong>${name}</strong><div style="margin-top:6px">`;
    items.forEach(d => {
      const scaleIdx = scales.indexOf(d.scale);
      const color = COLORS[scaleIdx % COLORS.length];
      const pct = (d.avgUs / globalMax * 100).toFixed(1);
      const label = d.scale >= 10000 ? (d.scale/10000) + '万' : d.scale.toLocaleString();
      html += `<div style="display:flex;align-items:center;gap:8px;margin-bottom:3px">
        <span style="width:50px;font-size:12px;color:#888;text-align:right">${label}</span>
        <div style="flex:1;height:18px;background:#f0f0f0;border-radius:3px;position:relative;overflow:hidden">
          <div style="height:100%;width:${pct}%;background:${color};border-radius:3px;min-width:2px"></div>
        </div>
        <span style="width:80px;font-size:12px;font-family:monospace;color:#555">${d.avgUs.toFixed(3)} us</span>
      </div>`;
    });
    html += '</div></div>';
  });
  container.innerHTML = html;
}

renderSummary();
renderTests();
renderTabs();
renderByOperation();
</script>
</body>
</html>)";

	ofs.close();
	printf("\nHTML 报告已生成: %s\n", pszPath);
}

// ============================================================
//  主函数
// ============================================================
int main(int argc, char* argv[])
{
	RunUnitTests();
	RunBenchmarks();

	std::string strOutputPath = "report.html";
	if (argc > 1)
		strOutputPath = argv[1];

	GenerateHtmlReport(strOutputPath.c_str());

	// 检查是否有失败的测试
	for (const auto& r : g_vecTestResults)
	{
		if (!r.bPassed) return 1;
	}
	return 0;
}
