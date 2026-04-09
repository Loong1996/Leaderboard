/**
 * @file TLeaderboard.h
 * @author Loong
 * @date 2026-04-08
 * @details
 *     百万级实时排行榜模板类。
 *     基于有序 vector 实现，支持自定义排序规则。
 *     查询操作 O(1)（TopN / AroundRank / GetEntryByRank），
 *     更新操作 O(N)（UpdateEntry / RemoveEntry / GetRank）。
 *     TKey:     玩家唯一标识类型（如 uint64_t）
 *     TValue:   排序数据类型（POD 结构体、基础类型或指针均可）
 *     TCompare: 比较仿函数，返回 true 表示 lhs 排名应高于 rhs，
 *               默认 std::greater<TValue>（值大者排名靠前）
 */
#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <functional>

template <typename TKey, typename TValue, typename TCompare = std::greater<TValue>>
class TLeaderboard
{
public:
	// 排行榜节点
	struct ST_RANK_NODE
	{
		TKey key;
		TValue value;
	};

	using VEC_RANK_NODE = std::vector<ST_RANK_NODE>;

public:
	explicit TLeaderboard(uint32_t uiMaxSize = 0, TCompare fnCompare = TCompare())
		: m_fnCompare(fnCompare)
		, m_uiMaxSize(uiMaxSize)
	{
	}

	~TLeaderboard() = default;

	// --- 更新操作 ---

	/**
	 * @brief 插入或更新排行榜条目
	 * @param [in] key 玩家唯一标识
	 * @param [in] value 排序数据
	 * @return 新排名（1-based），0 表示未上榜（被 MaxSize 截断）
	 */
	uint32_t UpdateEntry(const TKey& key, const TValue& value)
	{
		// 若已存在，先删除旧条目
		uint32_t uiOldIndex = FindByKey(key);
		if (uiOldIndex < GetCount())
		{
			m_vecRank.erase(m_vecRank.begin() + uiOldIndex);
		}

		// 二分查找插入位置
		uint32_t uiInsertPos = FindInsertPos(value);

		// MaxSize 截断检查
		if ((m_uiMaxSize > 0) && (uiInsertPos >= m_uiMaxSize))
		{
			return 0;
		}

		// 插入新条目
		ST_RANK_NODE stNode;
		stNode.key = key;
		stNode.value = value;
		m_vecRank.insert(m_vecRank.begin() + uiInsertPos, stNode);

		// 截断超出 MaxSize 的末尾
		if ((m_uiMaxSize > 0) && (GetCount() > m_uiMaxSize))
		{
			m_vecRank.pop_back();
		}

		return uiInsertPos + 1;
	}

	/**
	 * @brief 移除排行榜条目
	 * @param [in] key 玩家唯一标识
	 * @return 是否成功移除
	 */
	bool RemoveEntry(const TKey& key)
	{
		uint32_t uiIndex = FindByKey(key);
		if (uiIndex >= GetCount())
		{
			return false;
		}

		m_vecRank.erase(m_vecRank.begin() + uiIndex);
		return true;
	}

	/**
	 * @brief 清空排行榜
	 */
	void Clear()
	{
		m_vecRank.clear();
	}

	// --- 查询操作 ---

	/**
	 * @brief 获取玩家排名
	 * @param [in] key 玩家唯一标识
	 * @return 排名（1-based），未找到返回 0
	 */
	uint32_t GetRank(const TKey& key) const
	{
		uint32_t uiIndex = FindByKey(key);
		if (uiIndex >= GetCount())
		{
			return 0;
		}

		return uiIndex + 1;
	}

	/**
	 * @brief 获取指定排名的节点
	 * @param [in] uiRank 排名（1-based）
	 * @return 节点指针，越界返回 nullptr
	 */
	const ST_RANK_NODE* GetEntryByRank(uint32_t uiRank) const
	{
		if ((uiRank == 0) || (uiRank > GetCount()))
		{
			return nullptr;
		}

		return &m_vecRank[uiRank - 1];
	}

	/**
	 * @brief 获取 Top N
	 * @param [in] uiCount 请求数量
	 * @param [out] rpBegin 指向首元素的指针
	 * @return 实际返回数量
	 */
	uint32_t GetTopN(uint32_t uiCount, const ST_RANK_NODE*& rpBegin) const
	{
		rpBegin = nullptr;
		if ((uiCount == 0) || (GetCount() == 0))
		{
			return 0;
		}

		uint32_t uiActual = std::min(uiCount, GetCount());
		rpBegin = m_vecRank.data();
		return uiActual;
	}

	/**
	 * @brief 获取某排名附近的玩家
	 * @param [in] uiRank 中心排名（1-based）
	 * @param [in] uiCount 请求总数量（中心排名前后各取一半）
	 * @param [out] rpBegin 指向首元素的指针
	 * @return 实际返回数量
	 */
	uint32_t GetAroundRank(uint32_t uiRank, uint32_t uiCount,
	                       const ST_RANK_NODE*& rpBegin) const
	{
		rpBegin = nullptr;
		if ((uiRank == 0) || (uiRank > GetCount()) || (uiCount == 0))
		{
			return 0;
		}

		uint32_t uiHalf = uiCount / 2;
		uint32_t uiCenterIndex = uiRank - 1;

		// 计算起始下标（防止下溢）
		uint32_t uiStart = (uiCenterIndex > uiHalf) ? (uiCenterIndex - uiHalf) : 0;
		// 计算结束下标（不超过总数）
		uint32_t uiEnd = std::min(uiStart + uiCount, GetCount());
		// 若尾部不足，向前扩展起始位置
		if ((uiEnd - uiStart) < uiCount)
		{
			uiStart = (uiEnd > uiCount) ? (uiEnd - uiCount) : 0;
		}

		rpBegin = m_vecRank.data() + uiStart;
		return uiEnd - uiStart;
	}

	/**
	 * @brief 按 key 查找节点
	 * @param [in] key 玩家唯一标识
	 * @return 节点指针，未找到返回 nullptr
	 */
	const ST_RANK_NODE* GetEntry(const TKey& key) const
	{
		uint32_t uiIndex = FindByKey(key);
		if (uiIndex >= GetCount())
		{
			return nullptr;
		}

		return &m_vecRank[uiIndex];
	}

	/**
	 * @brief 获取排行榜人数
	 * @return 当前排行榜中的条目数量
	 */
	uint32_t GetCount() const
	{
		return static_cast<uint32_t>(m_vecRank.size());
	}

private:
	/**
	 * @brief 按 key 线性查找
	 * @param [in] key 玩家唯一标识
	 * @return 下标，未找到返回 GetCount()
	 */
	uint32_t FindByKey(const TKey& key) const
	{
		for (uint32_t ui = 0; ui < GetCount(); ++ui)
		{
			if (m_vecRank[ui].key == key)
			{
				return ui;
			}
		}

		return GetCount();
	}

	/**
	 * @brief 二分查找插入位置
	 * @param [in] value 待插入的排序数据
	 * @return 插入位置下标
	 */
	uint32_t FindInsertPos(const TValue& value) const
	{
		// upper_bound 找到第一个"排名低于 value"的位置
		auto it = std::upper_bound(
			m_vecRank.begin(), m_vecRank.end(), value,
			[this](const TValue& lhs, const ST_RANK_NODE& rhs)
			{
				return m_fnCompare(lhs, rhs.value);
			});

		return static_cast<uint32_t>(it - m_vecRank.begin());
	}

	VEC_RANK_NODE m_vecRank;     // 有序数组（唯一数据源）
	TCompare      m_fnCompare;   // 比较仿函数
	uint32_t      m_uiMaxSize;   // 最大容量，0 表示不限制
};
