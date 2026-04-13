/**
 * @file TLeaderboard.h
 * @author Loong
 * @date 2026-04-08
 * @details
 *     百万级实时排行榜模板类。
 *     基于有序 vector 实现，支持自定义排序规则。
 *     查询操作 O(1)（TopN / AroundRank / GetEntryByRank），
 *     更新操作 O(N)（UpdateEntry / RemoveEntry），
 *     GetRank/RemoveEntry: O(log N + K)（传入 value 时二分定位，K 为相等区间大小）
 *     TKey:     玩家唯一标识类型（如 uint64_t）
 *     TValue:   排序数据类型（POD 结构体、基础类型或指针均可）
 *     TCompare: 比较仿函数，默认 std::greater<TValue>（仅比较 value），
 *               若需含 key 的全序比较，自定义 TKeyCompare
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

	/**
	 * @brief 预分配内存
	 * @param [in] uiCapacity 预分配容量
	 */
	void Reserve(uint32_t uiCapacity)
	{
		m_vecRank.reserve(uiCapacity);
	}

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
		uint32_t uiOldIndex = this->FindByKey(key);
		if (uiOldIndex < this->GetCount())
		{
			m_vecRank.erase(m_vecRank.begin() + uiOldIndex);
		}

		// 构造节点用于二分查找
		ST_RANK_NODE stNode{ key, value };

		// 二分查找插入位置
		uint32_t uiInsertPos = this->FindInsertPos(stNode);

		// MaxSize 截断检查
		if ((m_uiMaxSize > 0) && (uiInsertPos >= m_uiMaxSize))
		{
			return 0;
		}

		// 插入新条目
		m_vecRank.insert(m_vecRank.begin() + uiInsertPos, stNode);

		// 截断超出 MaxSize 的末尾
		if ((m_uiMaxSize > 0) && (this->GetCount() > m_uiMaxSize))
		{
			m_vecRank.pop_back();
		}

		return uiInsertPos + 1;
	}

	/**
	 * @brief 移除排行榜条目（O(N) 线性扫描）
	 * @param [in] key 玩家唯一标识
	 * @return 是否成功移除
	 * @note 性能较差，仅用于无法获取 value 的场景，建议尽可能使用 RemoveEntry(key, value)
	 */
	bool RemoveEntry(const TKey& key)
	{
		uint32_t uiIndex = this->FindByKey(key);
		if (uiIndex >= this->GetCount())
		{
			return false;
		}

		m_vecRank.erase(m_vecRank.begin() + uiIndex);
		return true;
	}

	/**
	 * @brief 移除排行榜条目（O(log N + K) 二分定位）
	 * @param [in] key 玩家唯一标识
	 * @param [in] value 排序数据（用于二分定位）
	 * @return 是否成功移除
	 */
	bool RemoveEntry(const TKey& key, const TValue& value)
	{
		ST_RANK_NODE stNode{ key, value };
		uint32_t uiIndex = this->FindByNode(stNode);
		if (uiIndex >= this->GetCount())
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
	 * @brief 获取玩家排名（O(log N + K) 二分定位）
	 * @param [in] key 玩家唯一标识
	 * @param [in] value 排序数据（用于二分定位）
	 * @return 排名（1-based），未找到返回 0
	 */
	uint32_t GetRank(const TKey& key, const TValue& value) const
	{
		ST_RANK_NODE stNode{ key, value };
		uint32_t uiIndex = this->FindByNode(stNode);
		if (uiIndex >= this->GetCount())
		{
			return 0;
		}

		return uiIndex + 1;
	}

	/**
	 * @brief 获取玩家排名（O(N) 线性扫描）
	 * @param [in] key 玩家唯一标识
	 * @return 排名（1-based），未找到返回 0
	 * @note 性能较差，仅用于无法获取 value 的场景，建议尽可能使用 GetRank(key, value)
	 */
	uint32_t GetRank(const TKey& key) const
	{
		uint32_t uiIndex = this->FindByKey(key);
		if (uiIndex >= this->GetCount())
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
		if ((uiRank == 0) || (uiRank > this->GetCount()))
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
		if ((uiCount == 0) || (this->GetCount() == 0))
		{
			return 0;
		}

		uint32_t uiActual = std::min(uiCount, this->GetCount());
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
		if ((uiRank == 0) || (uiRank > this->GetCount()) || (uiCount == 0))
		{
			return 0;
		}

		uint32_t uiHalf = uiCount / 2;
		uint32_t uiCenterIndex = uiRank - 1;

		// 计算起始下标（防止下溢）
		uint32_t uiStart = (uiCenterIndex > uiHalf) ? (uiCenterIndex - uiHalf) : 0;
		// 计算结束下标（不超过总数）
		uint32_t uiEnd = std::min(uiStart + uiCount, this->GetCount());
		// 若尾部不足，向前扩展起始位置
		if ((uiEnd - uiStart) < uiCount)
		{
			uiStart = (uiEnd > uiCount) ? (uiEnd - uiCount) : 0;
		}

		rpBegin = m_vecRank.data() + uiStart;
		return uiEnd - uiStart;
	}

	/**
	 * @brief 按 key 查找节点（O(N) 线性扫描）
	 * @param [in] key 玩家唯一标识
	 * @return 节点指针，未找到返回 nullptr
	 */
	const ST_RANK_NODE* GetEntry(const TKey& key) const
	{
		uint32_t uiIndex = this->FindByKey(key);
		if (uiIndex >= this->GetCount())
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
		for (uint32_t ui = 0; ui < this->GetCount(); ++ui)
		{
			if (m_vecRank[ui].key == key)
			{
				return ui;
			}
		}

		return this->GetCount();
	}

	/**
	 * @brief 节点全序比较（value 优先，key 决胜）
	 */
	bool CompareNodes(const ST_RANK_NODE& lhs, const ST_RANK_NODE& rhs) const
	{
		if (this->m_fnCompare(lhs.value, rhs.value))
			return true;
		if (this->m_fnCompare(rhs.value, lhs.value))
			return false;
		return lhs.key < rhs.key;
	}

	/**
	 * @brief 二分查找插入位置
	 * @param [in] stNode 待插入的节点
	 * @return 插入位置下标
	 */
	uint32_t FindInsertPos(const ST_RANK_NODE& stNode) const
	{
		auto it = std::upper_bound(
			m_vecRank.begin(), m_vecRank.end(), stNode,
			[this](const ST_RANK_NODE& lhs, const ST_RANK_NODE& rhs)
			{
				return this->CompareNodes(lhs, rhs);
			});

		return static_cast<uint32_t>(it - m_vecRank.begin());
	}

	/**
	 * @brief 二分定位 key+value 对应的下标（全序比较，O(log N) 精确定位）
	 * @param [in] stNode 包含 key 和 value 的节点
	 * @return 下标，未找到返回 GetCount()
	 */
	uint32_t FindByNode(const ST_RANK_NODE& stNode) const
	{
		auto it = std::lower_bound(
			m_vecRank.begin(), m_vecRank.end(), stNode,
			[this](const ST_RANK_NODE& lhs, const ST_RANK_NODE& rhs)
			{
				return this->CompareNodes(lhs, rhs);
			});

		if (it != m_vecRank.end() && !this->CompareNodes(*it, stNode) && !this->CompareNodes(stNode, *it))
		{
			return static_cast<uint32_t>(it - m_vecRank.begin());
		}

		return this->GetCount();
	}

	VEC_RANK_NODE m_vecRank;     // 有序数组（唯一数据源）
	TCompare      m_fnCompare;   // 比较仿函数
	uint32_t      m_uiMaxSize;   // 最大容量，0 表示不限制
};
