/**
 * @file main.cpp
 * @author Loong
 * @date 2026-04-08
 * @details
 *     TLeaderboard 使用示例与性能基准测试。
 */
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <chrono>
#include <random>
#include <vector>

#include "TLeaderboard.h"

// ============================================================
//  自定义排序示例：分数 > 战力 > 时间
// ============================================================
struct ST_RANK_DATA
{
	int64_t iScore;
	int64_t iFightPower;
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

		if (lhs.iFightPower != rhs.iFightPower)
		{
			return lhs.iFightPower > rhs.iFightPower;
		}

		return lhs.iTimestamp < rhs.iTimestamp;
	}
};

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
//  使用示例
// ============================================================
static void ShowUsageExample()
{
	printf("=== Usage Example ===\n");

	// 简单分数排行榜
	TLeaderboard<uint64_t, int64_t> objSimpleBoard;
	objSimpleBoard.UpdateEntry(1001, 500);
	objSimpleBoard.UpdateEntry(1002, 800);
	objSimpleBoard.UpdateEntry(1003, 300);
	objSimpleBoard.UpdateEntry(1004, 700);

	printf("Simple board (4 players):\n");
	const TLeaderboard<uint64_t, int64_t>::ST_RANK_NODE* pBegin = nullptr;
	uint32_t uiCount = objSimpleBoard.GetTopN(10, pBegin);
	for (uint32_t ui = 0; ui < uiCount; ++ui)
	{
		printf("  Rank %u: key=%llu score=%lld\n",
		       ui + 1,
		       static_cast<unsigned long long>(pBegin[ui].key),
		       static_cast<long long>(pBegin[ui].value));
	}

	// 多维排序排行榜（MaxSize = 3）
	TLeaderboard<uint64_t, ST_RANK_DATA, ST_RANK_DATA_COMPARE> objMultiBoard(3);
	objMultiBoard.UpdateEntry(1, {100, 5000, 1000});
	objMultiBoard.UpdateEntry(2, {100, 5000, 2000});
	objMultiBoard.UpdateEntry(3, {100, 6000, 3000});
	objMultiBoard.UpdateEntry(4, {200, 3000, 4000});

	printf("\nMulti-sort board (MaxSize=3):\n");
	const TLeaderboard<uint64_t, ST_RANK_DATA, ST_RANK_DATA_COMPARE>::ST_RANK_NODE* pMultiBegin = nullptr;
	uiCount = objMultiBoard.GetTopN(10, pMultiBegin);
	for (uint32_t ui = 0; ui < uiCount; ++ui)
	{
		printf("  Rank %u: key=%llu score=%lld power=%lld time=%lld\n",
		       ui + 1,
		       static_cast<unsigned long long>(pMultiBegin[ui].key),
		       static_cast<long long>(pMultiBegin[ui].value.iScore),
		       static_cast<long long>(pMultiBegin[ui].value.iFightPower),
		       static_cast<long long>(pMultiBegin[ui].value.iTimestamp));
	}

	printf("\n");
}

// ============================================================
//  性能基准测试
// ============================================================
static void RunBenchmark()
{
	const uint32_t PLAYER_COUNT = 100000;
	printf("=== Benchmark (%u players) ===\n", PLAYER_COUNT);

	TLeaderboard<uint64_t, int64_t> objBoard;

	std::mt19937 rng(42);
	std::uniform_int_distribution<int64_t> distScore(0, 10000000);

	// 预生成随机分数
	std::vector<int64_t> vecScores(PLAYER_COUNT);
	for (auto& iScore : vecScores)
	{
		iScore = distScore(rng);
	}

	// 测试插入
	{
		CStopWatch sw;
		for (uint32_t ui = 0; ui < PLAYER_COUNT; ++ui)
		{
			objBoard.UpdateEntry(ui + 1, vecScores[ui]);
		}

		double dbElapsed = sw.ElapsedMs();
		printf("Insert %u entries: %.1f ms (avg %.3f us/op)\n",
		       PLAYER_COUNT, dbElapsed, dbElapsed * 1000.0 / PLAYER_COUNT);
	}

	assert(objBoard.GetCount() == PLAYER_COUNT);

	// 测试 GetRank（随机查询 1000 次）
	{
		const uint32_t QUERY_COUNT = 1000;
		std::uniform_int_distribution<uint64_t> distKey(1, PLAYER_COUNT);

		CStopWatch sw;
		for (uint32_t ui = 0; ui < QUERY_COUNT; ++ui)
		{
			uint64_t ulKey = distKey(rng);
			volatile uint32_t uiRank = objBoard.GetRank(ulKey);
			(void)uiRank;
		}

		double dbElapsed = sw.ElapsedMs();
		printf("GetRank x%u: %.1f ms (avg %.3f ms/op)\n",
		       QUERY_COUNT, dbElapsed, dbElapsed / QUERY_COUNT);
	}

	// 测试 GetTopN
	{
		const TLeaderboard<uint64_t, int64_t>::ST_RANK_NODE* pBegin = nullptr;

		CStopWatch sw;
		for (uint32_t ui = 0; ui < 10000; ++ui)
		{
			volatile uint32_t uiCount = objBoard.GetTopN(100, pBegin);
			(void)uiCount;
		}

		double dbElapsed = sw.ElapsedMs();
		printf("GetTopN(100) x10000: %.1f ms (avg %.3f us/op)\n",
		       dbElapsed, dbElapsed * 1000.0 / 10000);
	}

	// 测试 UpdateEntry（随机更新 1000 次）
	{
		const uint32_t UPDATE_COUNT = 1000;
		std::uniform_int_distribution<uint64_t> distKey(1, PLAYER_COUNT);

		CStopWatch sw;
		for (uint32_t ui = 0; ui < UPDATE_COUNT; ++ui)
		{
			uint64_t ulKey = distKey(rng);
			int64_t iNewScore = distScore(rng);
			objBoard.UpdateEntry(ulKey, iNewScore);
		}

		double dbElapsed = sw.ElapsedMs();
		printf("UpdateEntry x%u: %.1f ms (avg %.3f ms/op)\n",
		       UPDATE_COUNT, dbElapsed, dbElapsed / UPDATE_COUNT);
	}

	// 测试 GetAroundRank
	{
		const TLeaderboard<uint64_t, int64_t>::ST_RANK_NODE* pBegin = nullptr;

		CStopWatch sw;
		for (uint32_t ui = 0; ui < 10000; ++ui)
		{
			volatile uint32_t uiCount = objBoard.GetAroundRank(500000, 20, pBegin);
			(void)uiCount;
		}

		double dbElapsed = sw.ElapsedMs();
		printf("GetAroundRank(500000, 20) x10000: %.1f ms (avg %.3f us/op)\n",
		       dbElapsed, dbElapsed * 1000.0 / 10000);
	}
}

int main()
{
	ShowUsageExample();
	RunBenchmark();
	return 0;
}
