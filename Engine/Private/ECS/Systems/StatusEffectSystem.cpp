#include "ECS/Systems/StatusEffectSystem.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"

std::unique_ptr<CStatusEffectSystem> CStatusEffectSystem::Create()
{
	auto pInstance = unique_ptr<CStatusEffectSystem>(
		new CStatusEffectSystem());

	if (!pInstance)
		return nullptr;

	return pInstance;
}

void CStatusEffectSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
	(void)world;
	(void)fTimeDelta;
	WINTERS_PROFILE_SCOPE("Status::Execute");
}
