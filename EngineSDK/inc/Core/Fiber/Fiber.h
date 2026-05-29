#pragma once

#include "Core/Fiber/FiberTypes.h"

class CFiber
{
public:
	NativeFiberHandle GetNativeHandle() const
	{
		return m_hFiber;
	}

	void SetNativeHandle(NativeFiberHandle hFiber)
	{
		m_hFiber = hFiber;
	}

private:
	NativeFiberHandle m_hFiber = nullptr;
};
