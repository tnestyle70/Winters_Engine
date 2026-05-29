#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/ISystem.h"
#include <memory>

class CWorld;

//이거 NS_BEGIN(Engine) END 안 해도 상관 없음?

class WINTERS_ENGINE CStatusEffectSystem final : public ISystem
{
public:
	~CStatusEffectSystem() override = default;
	
	static std::unique_ptr<CStatusEffectSystem> Create();
	uint32_t GetPhase() const override { return 4; }
	const char* GetName() const override { return "StatusEffectSystem"; }
	void Execute(CWorld& world, f32_t fTimeDelta) override;
private:
	CStatusEffectSystem() = default;
};
