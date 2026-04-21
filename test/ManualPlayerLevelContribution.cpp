#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "TVectorLeaderboard.h"

// 手动测试使用的排行榜 value：
// level 越高越靠前，若 level 相同则历史贡献越高越靠前。
struct ST_PLAYER_LEVEL_CONTRIB_DATA
{
	int32_t iLevel;
	uint32_t uiHistoryContribution;
};

struct ST_PLAYER_LEVEL_CONTRIB_COMPARE
{
	// 这里只比较 value 本身，不处理 id。
	// 当 level 和历史贡献都相等时，TVectorLeaderboard 会自动按 key 升序决胜。
	bool operator()(const ST_PLAYER_LEVEL_CONTRIB_DATA& lhs, const ST_PLAYER_LEVEL_CONTRIB_DATA& rhs) const
	{
		if (lhs.iLevel != rhs.iLevel)
		{
			return lhs.iLevel > rhs.iLevel;
		}

		if (lhs.uiHistoryContribution != rhs.uiHistoryContribution)
		{
			return lhs.uiHistoryContribution > rhs.uiHistoryContribution;
		}

		return false;
	}
};

using TBoard = TVectorLeaderboard<uint32_t, ST_PLAYER_LEVEL_CONTRIB_DATA, ST_PLAYER_LEVEL_CONTRIB_COMPARE>;
using TNode = TBoard::ST_RANK_NODE;

static bool g_bCurrentPassed = true;
static std::string g_strCurrentDetail;

// 简化测试数据构造，避免每个场景里反复写花括号。
static ST_PLAYER_LEVEL_CONTRIB_DATA MakeData(int32_t iLevel, uint32_t uiHistoryContribution)
{
	return ST_PLAYER_LEVEL_CONTRIB_DATA{ iLevel, uiHistoryContribution };
}

// 统一记录失败信息，便于一个场景内累计多个断言错误后一次性打印。
static void AppendFailure(const char* pszFormat, ...)
{
	g_bCurrentPassed = false;

	char szBuf[512];
	va_list args;
	va_start(args, pszFormat);
	vsnprintf(szBuf, sizeof(szBuf), pszFormat, args);
	va_end(args);

	if (!g_strCurrentDetail.empty())
	{
		g_strCurrentDetail += "; ";
	}
	g_strCurrentDetail += szBuf;
}

static void BeginScenario(const char* pszName)
{
	g_bCurrentPassed = true;
	g_strCurrentDetail.clear();
	printf("[RUN ] %s\n", pszName);
}

static bool EndScenario(const char* pszName)
{
	printf("[%s] %s\n", g_bCurrentPassed ? "PASS" : "FAIL", pszName);
	if (!g_bCurrentPassed)
	{
		printf("       %s\n", g_strCurrentDetail.c_str());
	}
	return g_bCurrentPassed;
}

// 失败时把当前榜单完整打印出来，方便手动比对名次是否符合预期。
static std::string BuildBoardSnapshot(const TBoard& objBoard)
{
	if (objBoard.GetCount() == 0)
	{
		return "  <empty>\n";
	}

	std::string strOut;
	objBoard.ForeachEntries(1, objBoard.GetCount(), [&](uint32_t uiRank, const TNode& stNode)
	{
		char szBuf[128];
		snprintf(
			szBuf, sizeof(szBuf),
			"  rank=%u id=%u level=%d contrib=%u\n",
			uiRank,
			stNode.key,
			stNode.value.iLevel,
			stNode.value.uiHistoryContribution);
		strOut += szBuf;
	});
	return strOut;
}

static void DumpBoard(const char* pszTitle, const TBoard& objBoard)
{
	printf("%s\n%s", pszTitle, BuildBoardSnapshot(objBoard).c_str());
}

static void Check(bool bExpr, const char* pszExpr, int iLine)
{
	if (!bExpr)
	{
		AppendFailure("line %d: %s", iLine, pszExpr);
	}
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

// 核对整张榜单的玩家顺序。
// 这里只比 id 顺序，因为每个场景里的输入 value 已经固定，id 顺序最直观。
static void ExpectBoardOrder(const TBoard& objBoard, const std::vector<uint32_t>& vecExpectedIds, const char* pszCaseName)
{
	if (objBoard.GetCount() != static_cast<uint32_t>(vecExpectedIds.size()))
	{
		AppendFailure(
			"%s count mismatch, expected=%u actual=%u",
			pszCaseName,
			static_cast<uint32_t>(vecExpectedIds.size()),
			objBoard.GetCount());
		DumpBoard("Actual board:", objBoard);
		return;
	}

	uint32_t uiActualIndex = 0;
	bool bMismatch = false;
	objBoard.ForeachEntries(1, objBoard.GetCount(), [&](uint32_t uiRank, const TNode& stNode)
	{
		if (uiActualIndex >= vecExpectedIds.size())
		{
			bMismatch = true;
			return;
		}

		if (stNode.key != vecExpectedIds[uiActualIndex])
		{
			AppendFailure(
				"%s rank %u mismatch, expected id=%u actual id=%u",
				pszCaseName,
				uiRank,
				vecExpectedIds[uiActualIndex],
				stNode.key);
			bMismatch = true;
		}
		++uiActualIndex;
	});

	if (bMismatch)
	{
		DumpBoard("Actual board:", objBoard);
	}
}

// 同时校验 GetRank(id, value) 和 GetRank(id) 两条查询路径，避免只测到其中一个接口。
static void ExpectRank(const TBoard& objBoard, uint32_t uiPlayerId, const ST_PLAYER_LEVEL_CONTRIB_DATA& stValue, uint32_t uiExpectedRank)
{
	uint32_t uiRankWithValue = objBoard.GetRank(uiPlayerId, stValue);
	uint32_t uiRankByKey = objBoard.GetRank(uiPlayerId);
	if (uiRankWithValue != uiExpectedRank)
	{
		AppendFailure(
			"GetRank(id, value) mismatch for id=%u, expected=%u actual=%u",
			uiPlayerId,
			uiExpectedRank,
			uiRankWithValue);
		DumpBoard("Actual board:", objBoard);
	}
	if (uiRankByKey != uiExpectedRank)
	{
		AppendFailure(
			"GetRank(id) mismatch for id=%u, expected=%u actual=%u",
			uiPlayerId,
			uiExpectedRank,
			uiRankByKey);
		DumpBoard("Actual board:", objBoard);
	}
}

// 边界：空榜上的各种查询、遍历、删除都应安全返回默认值，不应崩溃或误返回有效结果。
static bool Scenario_EmptyBoard()
{
	const char* pszName = "Empty board";
	BeginScenario(pszName);

	TBoard objBoard;
	CHECK(objBoard.GetCount() == 0);
	CHECK(objBoard.GetRank(1) == 0);
	CHECK(objBoard.GetRank(1, MakeData(1, 1)) == 0);
	CHECK(objBoard.RemoveEntry(1) == false);
	CHECK(objBoard.RemoveEntry(1, MakeData(1, 1)) == false);

	uint32_t uiOutRank = 0;
	TNode stOutNode = { 0, MakeData(0, 0) };
	CHECK(objBoard.GetEntry(1, uiOutRank, stOutNode) == false);
	CHECK(objBoard.ForeachEntryByRank(1, [](uint32_t, const TNode&) {}) == false);
	CHECK(objBoard.ForeachEntries(1, 10, [](uint32_t, const TNode&) {}) == 0);

	return EndScenario(pszName);
}

// 边界：单条目时插入、查询、GetEntry、按 key/value 删除都应工作正常。
static bool Scenario_SingleEntry()
{
	const char* pszName = "Single entry";
	BeginScenario(pszName);

	TBoard objBoard;
	ST_PLAYER_LEVEL_CONTRIB_DATA stValue = MakeData(10, 500);

	CHECK(objBoard.UpdateEntry(42, stValue) == 1);
	ExpectBoardOrder(objBoard, { 42 }, "single insert");
	ExpectRank(objBoard, 42, stValue, 1);

	uint32_t uiRank = 0;
	TNode stNode = { 0, MakeData(0, 0) };
	CHECK(objBoard.GetEntry(42, uiRank, stNode) == true);
	CHECK(uiRank == 1);
	CHECK(stNode.key == 42);
	CHECK(stNode.value.iLevel == 10);
	CHECK(stNode.value.uiHistoryContribution == 500);

	CHECK(objBoard.RemoveEntry(42, stValue) == true);
	CHECK(objBoard.GetCount() == 0);
	CHECK(objBoard.UpdateEntry(42, stValue) == 1);
	CHECK(objBoard.RemoveEntry(42) == true);
	CHECK(objBoard.GetCount() == 0);

	return EndScenario(pszName);
}

// 核心规则：
// 1. 等级高在前
// 2. 等级相同则历史贡献高在前
// 3. 两者都相同则玩家 id 小在前
static bool Scenario_BasicOrdering()
{
	const char* pszName = "Basic ordering";
	BeginScenario(pszName);

	TBoard objBoard;
	CHECK(objBoard.UpdateEntry(1001, MakeData(9, 500)) == 1);
	CHECK(objBoard.UpdateEntry(1002, MakeData(10, 100)) == 1);
	CHECK(objBoard.UpdateEntry(1003, MakeData(10, 300)) == 1);
	CHECK(objBoard.UpdateEntry(1004, MakeData(10, 300)) == 2);
	CHECK(objBoard.UpdateEntry(1005, MakeData(8, 999)) == 5);

	ExpectBoardOrder(objBoard, { 1003, 1004, 1002, 1001, 1005 }, "basic ordering");
	ExpectRank(objBoard, 1003, MakeData(10, 300), 1);
	ExpectRank(objBoard, 1004, MakeData(10, 300), 2);
	ExpectRank(objBoard, 1002, MakeData(10, 100), 3);
	ExpectRank(objBoard, 1001, MakeData(9, 500), 4);
	ExpectRank(objBoard, 1005, MakeData(8, 999), 5);

	return EndScenario(pszName);
}

// 边界：value 完全相同，只剩 key 决胜时，必须是更小 id 排名更高。
static bool Scenario_ExactTieByKey()
{
	const char* pszName = "Exact tie by key";
	BeginScenario(pszName);

	TBoard objBoard;
	objBoard.UpdateEntry(30, MakeData(7, 700));
	objBoard.UpdateEntry(10, MakeData(7, 700));
	objBoard.UpdateEntry(20, MakeData(7, 700));

	ExpectBoardOrder(objBoard, { 10, 20, 30 }, "exact tie by key");
	ExpectRank(objBoard, 10, MakeData(7, 700), 1);
	ExpectRank(objBoard, 20, MakeData(7, 700), 2);
	ExpectRank(objBoard, 30, MakeData(7, 700), 3);

	return EndScenario(pszName);
}

// 数值极值边界：
// - 等级覆盖 int32_t 最小值和最大值
// - 历史贡献覆盖 0 和 uint32_t 最大值
// - 玩家 id 覆盖 0 和 uint32_t 最大值
// 用来确认比较逻辑不会在极值下反转或溢出。
static bool Scenario_ExtremeValues()
{
	const char* pszName = "Extreme values";
	BeginScenario(pszName);

	const int32_t iMinLevel = std::numeric_limits<int32_t>::min();
	const int32_t iMaxLevel = std::numeric_limits<int32_t>::max();
	const uint32_t uiMaxContrib = std::numeric_limits<uint32_t>::max();
	const uint32_t uiMaxId = std::numeric_limits<uint32_t>::max();

	TBoard objBoard;
	objBoard.UpdateEntry(uiMaxId, MakeData(iMinLevel, uiMaxContrib));
	objBoard.UpdateEntry(0, MakeData(iMaxLevel, 0));
	objBoard.UpdateEntry(7, MakeData(iMaxLevel, uiMaxContrib));
	objBoard.UpdateEntry(8, MakeData(iMaxLevel, uiMaxContrib));

	ExpectBoardOrder(objBoard, { 7, 8, 0, uiMaxId }, "extreme values");
	ExpectRank(objBoard, 7, MakeData(iMaxLevel, uiMaxContrib), 1);
	ExpectRank(objBoard, 8, MakeData(iMaxLevel, uiMaxContrib), 2);
	ExpectRank(objBoard, 0, MakeData(iMaxLevel, 0), 3);
	ExpectRank(objBoard, uiMaxId, MakeData(iMinLevel, uiMaxContrib), 4);

	return EndScenario(pszName);
}

// 查询边界：
// - 已上榜玩家能查到正确名次
// - 不存在玩家、旧 value、错误 value 都应返回 0
static bool Scenario_GetRank()
{
	const char* pszName = "GetRank";
	BeginScenario(pszName);

	TBoard objBoard;
	objBoard.UpdateEntry(1, MakeData(10, 100));
	objBoard.UpdateEntry(2, MakeData(10, 90));
	objBoard.UpdateEntry(3, MakeData(9, 1000));

	ExpectRank(objBoard, 1, MakeData(10, 100), 1);
	ExpectRank(objBoard, 2, MakeData(10, 90), 2);
	ExpectRank(objBoard, 3, MakeData(9, 1000), 3);

	CHECK(objBoard.GetRank(4) == 0);
	CHECK(objBoard.GetRank(2, MakeData(10, 91)) == 0);
	CHECK(objBoard.GetRank(2, MakeData(9, 90)) == 0);

	return EndScenario(pszName);
}

// 更新边界：
// - 只提升贡献时应前移
// - value 不变时 rank 不变
// - 提升等级时应直接跃升
// - 降低等级后应回落到正确位置
static bool Scenario_UpdateReorder()
{
	const char* pszName = "Update reorder";
	BeginScenario(pszName);

	TBoard objBoard;
	objBoard.UpdateEntry(1, MakeData(10, 100));
	objBoard.UpdateEntry(2, MakeData(10, 90));
	objBoard.UpdateEntry(3, MakeData(9, 100));

	CHECK(objBoard.UpdateEntry(2, MakeData(10, 110)) == 1);
	ExpectBoardOrder(objBoard, { 2, 1, 3 }, "contribution increase");

	CHECK(objBoard.UpdateEntry(2, MakeData(10, 110)) == 1);
	ExpectBoardOrder(objBoard, { 2, 1, 3 }, "same value update");

	CHECK(objBoard.UpdateEntry(3, MakeData(11, 1)) == 1);
	ExpectBoardOrder(objBoard, { 3, 2, 1 }, "level increase jump");

	CHECK(objBoard.UpdateEntry(3, MakeData(9, 10)) == 3);
	ExpectBoardOrder(objBoard, { 2, 1, 3 }, "level drop fallback");

	return EndScenario(pszName);
}

// stale oldValue 边界：
// 调用方传错旧 value 时，UpdateEntry(key, oldValue, newValue)
// 仍应通过 key 命中已有玩家并更新，而不是插入重复条目。
static bool Scenario_StaleOldValue()
{
	const char* pszName = "Stale oldValue";
	BeginScenario(pszName);

	TBoard objBoard;
	objBoard.UpdateEntry(100, MakeData(10, 100));
	objBoard.UpdateEntry(200, MakeData(9, 100));

	CHECK(objBoard.UpdateEntry(200, MakeData(1, 1), MakeData(11, 1)) == 1);
	CHECK(objBoard.GetCount() == 2);
	ExpectBoardOrder(objBoard, { 200, 100 }, "stale oldValue update");
	CHECK(objBoard.GetRank(200, MakeData(9, 100)) == 0);
	ExpectRank(objBoard, 200, MakeData(11, 1), 1);

	return EndScenario(pszName);
}

// 删除边界：
// 错误等级、错误贡献都不能误删；
// 只有完全匹配的 key+value 才能删除成功。
static bool Scenario_Remove()
{
	const char* pszName = "Remove";
	BeginScenario(pszName);

	TBoard objBoard;
	objBoard.UpdateEntry(1, MakeData(10, 100));
	objBoard.UpdateEntry(2, MakeData(10, 90));
	objBoard.UpdateEntry(3, MakeData(9, 80));

	CHECK(objBoard.RemoveEntry(2, MakeData(10, 91)) == false);
	CHECK(objBoard.RemoveEntry(2, MakeData(9, 90)) == false);
	CHECK(objBoard.GetCount() == 3);

	CHECK(objBoard.RemoveEntry(2, MakeData(10, 90)) == true);
	CHECK(objBoard.GetCount() == 2);
	ExpectBoardOrder(objBoard, { 1, 3 }, "remove exact value");
	CHECK(objBoard.GetRank(2) == 0);
	CHECK(objBoard.RemoveEntry(2, MakeData(10, 90)) == false);

	return EndScenario(pszName);
}

// MaxSize 边界：
// - 更强玩家应能挤入榜单
// - 同 value 但更小 id 的玩家应替换边界玩家
// - 更弱玩家应被拒绝并返回 0
static bool Scenario_MaxSize()
{
	const char* pszName = "MaxSize";
	BeginScenario(pszName);

	TBoard objBoard(3);
	CHECK(objBoard.UpdateEntry(30, MakeData(10, 100)) == 1);
	CHECK(objBoard.UpdateEntry(40, MakeData(10, 90)) == 2);
	CHECK(objBoard.UpdateEntry(50, MakeData(10, 90)) == 3);
	ExpectBoardOrder(objBoard, { 30, 40, 50 }, "initial maxsize");

	CHECK(objBoard.UpdateEntry(20, MakeData(11, 1)) == 1);
	ExpectBoardOrder(objBoard, { 20, 30, 40 }, "higher level enters");

	CHECK(objBoard.UpdateEntry(35, MakeData(10, 90)) == 3);
	ExpectBoardOrder(objBoard, { 20, 30, 35 }, "same value smaller key replaces tail");

	CHECK(objBoard.UpdateEntry(45, MakeData(10, 95)) == 3);
	ExpectBoardOrder(objBoard, { 20, 30, 45 }, "same level higher contrib enters");

	CHECK(objBoard.UpdateEntry(60, MakeData(9, 999)) == 0);
	ExpectBoardOrder(objBoard, { 20, 30, 45 }, "lower level rejected");

	CHECK(objBoard.UpdateEntry(70, MakeData(10, 94)) == 0);
	ExpectBoardOrder(objBoard, { 20, 30, 45 }, "same level lower contrib rejected");

	return EndScenario(pszName);
}

static int RunAllScenarios()
{
	struct ST_SCENARIO_ENTRY
	{
		const char* pszName;
		bool (*fn)();
	};

	const ST_SCENARIO_ENTRY arrScenarios[] =
	{
		{ "Empty board",      Scenario_EmptyBoard },
		{ "Single entry",     Scenario_SingleEntry },
		{ "Basic ordering",   Scenario_BasicOrdering },
		{ "Exact tie by key", Scenario_ExactTieByKey },
		{ "Extreme values",   Scenario_ExtremeValues },
		{ "GetRank",          Scenario_GetRank },
		{ "Update reorder",   Scenario_UpdateReorder },
		{ "Stale oldValue",   Scenario_StaleOldValue },
		{ "Remove",           Scenario_Remove },
		{ "MaxSize",          Scenario_MaxSize },
	};

	uint32_t uiPassed = 0;
	const uint32_t uiTotal = static_cast<uint32_t>(sizeof(arrScenarios) / sizeof(arrScenarios[0]));

	printf("=== Manual Player Level Contribution Scenarios ===\n\n");
	for (const auto& stScenario : arrScenarios)
	{
		// 每个场景独立运行，便于手动定位是哪一类边界出了问题。
		if (stScenario.fn())
		{
			++uiPassed;
		}
		printf("\n");
	}

	printf("Summary: %u/%u passed\n", uiPassed, uiTotal);
	return (uiPassed == uiTotal) ? 0 : 1;
}

int main()
{
	return RunAllScenarios();
}
