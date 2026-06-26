#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "ECS/ISystem.h"

#include <memory>

class CWorld;

NS_BEGIN(Engine)

class WINTERS_ENGINE CNavigationThrottleSystem final : public ISystem
{
public:
	~CNavigationThrottleSystem() = default;

	static std::unique_ptr<CNavigationThrottleSystem> Create()
	{
		return std::unique_ptr<CNavigationThrottleSystem>(new CNavigationThrottleSystem());
	}

	u32_t GetPhase() const override { return 2; }
	const char* GetName() const override { return "Navigation Throttle System"; }
	void DescribeAccess(CSystemAccessBuilder& builder) const override;
	void Execute(CWorld& world, f32_t fTimeDelta) override;

private:
	CNavigationThrottleSystem() = default;
};

NS_END
