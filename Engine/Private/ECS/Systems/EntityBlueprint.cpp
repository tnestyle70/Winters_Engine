#include "ECS/Systems/EntityBlueprint.h"
#include "ECS/World.h"

// Spawn(CWorld&) 는 헤더에서 인라인으로 Spawn(world, nullptr) 포워딩.
// 여기는 실제 구현 1개만.
EntityID CEntityBlueprint::Spawn(CWorld& world, const void* pArg) const
{
	const EntityID entity = world.CreateEntity();

	//1단계: 불변 원본 리소스 주입 (Initialize_Prototype 등가)
	for (const auto& fn : m_vecInstallers)
		fn(world, entity);

	//2단계: per-instance 인자 주입 (Initialize(void*) 등가 — 수업 관례)
	for (const auto& fn : m_vecArgsInstallers)
		fn(world, entity, pArg);

	return entity;
}
