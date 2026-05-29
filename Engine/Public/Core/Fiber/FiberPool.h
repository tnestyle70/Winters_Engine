#pragma once

#include <vector>

#include "Core/Fiber/Fiber.h"

class CFiberPool
{
public:
	void Reset()
	{
		m_vecFibers.clear();
	}

	uint32_t GetCount() const
	{
		return static_cast<uint32_t>(m_vecFibers.size());
	}

private:
	std::vector<CFiber> m_vecFibers;
};
