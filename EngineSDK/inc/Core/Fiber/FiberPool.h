#pragma once

#include <cstdint>
#include <vector>

#include "Core/Fiber/Fiber.h"

// Worker 하나가 소유하는 native fiber handle 재사용 pool.
// CreateFiberEx/DeleteFiber 와 execution state machine 은 scheduler 관할이며,
// 이 pool 은 Free <-> Acquired 소유권만 관리한다. 한 worker root fiber 에서만 호출한다.
class CFiberPool
{
public:
	void Reserve(std::uint32_t iCapacity)
	{
		m_vecEntries.reserve(iCapacity);
	}

	bool Add(NativeFiberHandle hFiber)
	{
		if (!hFiber)
			return false;

		Entry entry{};
		entry.Fiber.SetNativeHandle(hFiber);
		m_vecEntries.push_back(entry);
		++m_iAvailableCount;
		return true;
	}

	NativeFiberHandle Acquire()
	{
		for (Entry& entry : m_vecEntries)
		{
			if (entry.bAcquired)
				continue;

			entry.bAcquired = true;
			--m_iAvailableCount;
			return entry.Fiber.GetNativeHandle();
		}
		return nullptr;
	}

	bool Release(NativeFiberHandle hFiber)
	{
		for (Entry& entry : m_vecEntries)
		{
			if (entry.Fiber.GetNativeHandle() != hFiber || !entry.bAcquired)
				continue;

			entry.bAcquired = false;
			++m_iAvailableCount;
			return true;
		}
		return false;
	}

	void Reset()
	{
		m_vecEntries.clear();
		m_iAvailableCount = 0;
	}

	std::uint32_t GetCount() const
	{
		return static_cast<std::uint32_t>(m_vecEntries.size());
	}

	std::uint32_t GetAvailableCount() const
	{
		return m_iAvailableCount;
	}

private:
	struct Entry
	{
		CFiber Fiber{};
		bool bAcquired = false;
	};

	std::vector<Entry> m_vecEntries;
	std::uint32_t m_iAvailableCount = 0;
};
