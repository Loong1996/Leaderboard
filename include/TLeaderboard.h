/**
 * @file TLeaderboard.h
 * @author Loong
 * @date 2026-04-08
 * @details
 *     百万级实时排行榜模板类。
 *     基于有序 vector 实现，支持自定义排序规则。
 *     查询操作 O(1)（ForeachEntryByRank），O(K)（ForeachEntries，K 为请求数量），
 *     更新操作 O(N)（UpdateEntry / RemoveEntry），
 *     GetRank/RemoveEntry: O(log N + K)（传入 value 时二分定位，K 为相等区间大小）
 *     TKey:     玩家唯一标识类型（如 uint64_t，须支持 operator< / operator==）
 *     TValue:   排序数据类型（POD 结构体、基础类型或指针均可）
 *     TCompare: 比较仿函数，默认 std::greater<TValue>（仅比较 value），
 *               value 相等时内部固定用 key 作为决胜条件
 */
#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <functional>

namespace detail
{
	template <typename T, typename = void>
	struct HasLess : std::false_type {};
	template <typename T>
	struct HasLess<T, decltype(void(std::declval<const T&>() < std::declval<const T&>()))> : std::true_type {};

	template <typename T, typename = void>
	struct HasEqual : std::false_type {};
	template <typename T>
	struct HasEqual<T, decltype(void(std::declval<const T&>() == std::declval<const T&>()))> : std::true_type {};
}

template <typename TKey, typename TValue, typename TCompare = std::greater<TValue>>
class TLeaderboard
{
	static_assert(detail::HasLess<TKey>::value,  "TKey 须实现 operator<");
	static_assert(detail::HasEqual<TKey>::value, "TKey 须实现 operator==");

public:
	struct ST_RANK_NODE
	{
		TKey key;
		TValue value;
	};

public:
	explicit TLeaderboard(uint32_t uiMaxSize = 0, TCompare fnCompare = TCompare())
		: m_fnCompare(fnCompare)
		, m_uiMaxSize(uiMaxSize)
	{
	}

	~TLeaderboard() = default;

	/**
	 * @brief 设置排行榜最大数量（0 表示不限制）
	 * @param [in] uiMaxSize 最大容量
	 */
	void SetMaxSize(uint32_t uiMaxSize)
	{
		m_uiMaxSize = uiMaxSize;

		// 截断超出新上限的末尾
		if ((m_uiMaxSize > 0) && (this->GetCount() > m_uiMaxSize))
		{
			m_vecRank.resize(m_uiMaxSize);
		}
	}

	/**
	 * @brief 获取排行榜最大数量
	 * @return 最大容量，0 表示不限制
	 */
	uint32_t GetMaxSize() const
	{
		return m_uiMaxSize;
	}

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
	 * @brief 插入新条目（调用方保证 key 不存在，跳过查重）
	 * @param [in] key 玩家唯一标识
	 * @param [in] value 排序数据
	 * @return 新排名（1-based），0 表示未上榜（被 MaxSize 截断）
	 * @note 若 key 已存在会导致重复条目，调用方必须自行保证唯一性
	 */
	uint32_t InsertEntry(const TKey& key, const TValue& value)
	{
		return this->InsertNode(ST_RANK_NODE{ key, value });
	}

	/**
	 * @brief 插入或更新排行榜条目（调用方持有旧值时使用，O(log N + K) 定位；oldValue 失配时退化为 O(N)）
	 * @param [in] key 玩家唯一标识
	 * @param [in] oldValue 旧排序数据（用于二分定位旧条目）
	 * @param [in] newValue 新排序数据
	 * @return 新排名（1-based），0 表示未上榜（被 MaxSize 截断）
	 */
	uint32_t UpdateEntry(const TKey& key, const TValue& oldValue, const TValue& newValue)
	{
		uint32_t uiIndex = this->FindByNode(ST_RANK_NODE{ key, oldValue });
		if (uiIndex >= this->GetCount())
		{
			// oldValue 失配时退化到按 key 查找，避免插入重复 key。
			uiIndex = this->FindByKey(key);
		}

		this->TryErase(uiIndex);
		return this->InsertNode(ST_RANK_NODE{ key, newValue });
	}

	/**
	 * @brief 插入或更新排行榜条目（无旧值时使用，O(N) 线性扫描）
	 * @param [in] key 玩家唯一标识
	 * @param [in] value 排序数据
	 * @return 新排名（1-based），0 表示未上榜（被 MaxSize 截断）
	 */
	uint32_t UpdateEntry(const TKey& key, const TValue& value)
	{
		this->TryErase(this->FindByKey(key));
		return this->InsertNode(ST_RANK_NODE{ key, value });
	}

	/**
	 * @brief 移除排行榜条目（O(N) 线性扫描）
	 * @param [in] key 玩家唯一标识
	 * @return 是否成功移除
	 * @note 性能较差，仅用于无法获取 value 的场景，建议尽可能使用 RemoveEntry(key, value)
	 */
	bool RemoveEntry(const TKey& key)
	{
		return this->TryErase(this->FindByKey(key));
	}

	/**
	 * @brief 移除排行榜条目（O(log N + K) 二分定位）
	 * @param [in] key 玩家唯一标识
	 * @param [in] value 排序数据（用于二分定位）
	 * @return 是否成功移除
	 */
	bool RemoveEntry(const TKey& key, const TValue& value)
	{
		return this->TryErase(this->FindByNode(ST_RANK_NODE{ key, value }));
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
		return this->IndexToRank(this->FindByNode(ST_RANK_NODE{ key, value }));
	}

	/**
	 * @brief 获取玩家排名（O(N) 线性扫描）
	 * @param [in] key 玩家唯一标识
	 * @return 排名（1-based），未找到返回 0
	 * @note 性能较差，仅用于无法获取 value 的场景，建议尽可能使用 GetRank(key, value)
	 */
	uint32_t GetRank(const TKey& key) const
	{
		return this->IndexToRank(this->FindByKey(key));
	}

	/**
	 * @brief 访问指定排名的节点
	 * @param [in] uiRank 排名（1-based）
	 * @param [in] fn 回调函数 void(uint32_t uiRank, const ST_RANK_NODE& stNode)
	 * @return 是否找到并调用了回调
	 */
	template <typename TFunc>
	bool ForeachEntryByRank(uint32_t uiRank, TFunc fn) const
	{
		if ((uiRank == 0) || (uiRank > this->GetCount()))
		{
			return false;
		}

		fn(uiRank, m_vecRank[uiRank - 1]);
		return true;
	}

	/**
	 * @brief 遍历指定排名区间的条目
	 * @param [in] uiStartRank 起始排名（1-based）
	 * @param [in] uiCount 请求数量
	 * @param [in] fn 回调函数 void(uint32_t uiRank, const ST_RANK_NODE& stNode)
	 * @return 实际遍历数量
	 */
	template <typename TFunc>
	uint32_t ForeachEntries(uint32_t uiStartRank, uint32_t uiCount, TFunc fn) const
	{
		if ((uiStartRank == 0) || (uiStartRank > this->GetCount()) || (uiCount == 0))
		{
			return 0;
		}

		uint32_t uiStart = uiStartRank - 1;
		uint32_t uiRemain = this->GetCount() - uiStart;
		uint32_t uiActualCount = std::min(uiCount, uiRemain);
		uint32_t uiEnd = uiStart + uiActualCount;
		for (uint32_t ui = uiStart; ui < uiEnd; ++ui)
		{
			fn(ui + 1, m_vecRank[ui]);
		}
		return uiActualCount;
	}

	/**
	 * @brief 按 key 获取节点及其排名（O(N) 线性扫描）
	 * @param [in] key 玩家唯一标识
	 * @param [out] uiOutRank 排名（1-based）
	 * @param [out] stOutNode 节点数据
	 * @return 是否找到
	 */
	bool GetEntry(const TKey& key, uint32_t& uiOutRank, ST_RANK_NODE& stOutNode) const
	{
		uint32_t uiIndex = this->FindByKey(key);
		if (uiIndex >= this->GetCount())
		{
			return false;
		}

		uiOutRank = uiIndex + 1;
		stOutNode = m_vecRank[uiIndex];
		return true;
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
	using VEC_RANK_NODE = std::vector<ST_RANK_NODE>;

	/**
	 * @brief 若下标有效则删除对应条目
	 * @param [in] uiIndex 下标
	 * @return 是否成功删除
	 */
	bool TryErase(uint32_t uiIndex)
	{
		if (uiIndex >= this->GetCount())
		{
			return false;
		}

		m_vecRank.erase(m_vecRank.begin() + uiIndex);
		return true;
	}

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
		if (m_fnCompare(lhs.value, rhs.value))
		{
			return true;
		}

		if (m_fnCompare(rhs.value, lhs.value))
		{
			return false;
		}

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

	/**
	 * @brief 将下标转换为排名（1-based）
	 * @param [in] uiIndex 下标
	 * @return 排名（1-based），未找到（uiIndex >= GetCount()）返回 0
	 */
	uint32_t IndexToRank(uint32_t uiIndex) const
	{
		if (uiIndex >= this->GetCount())
		{
			return 0;
		}

		return uiIndex + 1;
	}

	/**
	 * @brief 二分查找插入位置并插入节点，处理 MaxSize 截断
	 * @param [in] stNode 待插入的节点
	 * @return 新排名（1-based），0 表示未上榜（被 MaxSize 截断）
	 */
	uint32_t InsertNode(const ST_RANK_NODE& stNode)
	{
		uint32_t uiInsertPos = this->FindInsertPos(stNode);

		// MaxSize 截断检查
		if ((m_uiMaxSize > 0) && (uiInsertPos >= m_uiMaxSize))
		{
			return 0;
		}

		// 先截断末尾再插入，避免 size 短暂超过 MaxSize 触发扩容
		if ((m_uiMaxSize > 0) && (this->GetCount() >= m_uiMaxSize))
		{
			m_vecRank.pop_back();
		}

		m_vecRank.insert(m_vecRank.begin() + uiInsertPos, stNode);

		return uiInsertPos + 1;
	}

	VEC_RANK_NODE m_vecRank;     // 有序数组（唯一数据源）
	TCompare      m_fnCompare;   // 比较仿函数
	uint32_t      m_uiMaxSize;   // 最大容量，0 表示不限制
};
