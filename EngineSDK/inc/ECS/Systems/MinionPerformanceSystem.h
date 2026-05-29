#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "ECS/ISystem.h"
#include <memory>

class CWorld;

NS_BEGIN(Engine)
//client에 공개해서 Performance Check
class WINTERS_ENGINE CMinionPerformanceSystem final : public ISystem
{
public:
	~CMinionPerformanceSystem() = default;

	static std::unique_ptr<CMinionPerformanceSystem> Create()
	{
		//왜지??
		return std::unique_ptr<CMinionPerformanceSystem>(new CMinionPerformanceSystem());
	}

	u32_t GetPhase() const override { return 2; }
	const char* GetName() const override { return "Minion Performance System"; }
	void DescribeAccess(CSystemAccessBuilder& builder) const override;
	void Execute(CWorld& world, f32_t fTimeDelta) override;

private:
	CMinionPerformanceSystem() = default;
};

NS_END