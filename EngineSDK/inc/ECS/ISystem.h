#pragma once

#include <cstdint>

#include "ECS/SystemAccess.h"

class CWorld;

class ISystem
{
public:
	virtual ~ISystem() = default;
	virtual uint32_t GetPhase() const = 0;
	virtual void Execute(CWorld& world, float fTimeDelta) = 0;
	virtual const char* GetName() const = 0;

	virtual void DescribeAccess(CSystemAccessBuilder& builder) const
	{
		builder.UnknownWritesAll();
	}
};
