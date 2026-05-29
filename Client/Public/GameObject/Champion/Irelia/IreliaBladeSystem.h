#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include <memory>
#include <unordered_set>

class CWorld;

namespace Engine
{
	class CFxStaticMeshRenderer;
}

struct IreliaBladeComponent
{
	Vec3 vWorldPos = { 0.f, 0.f, 0.f };
	EntityID ownerEntity = NULL_ENTITY;
	EntityID fxMeshID = NULL_ENTITY;
	EntityID orbitFxID1 = NULL_ENTITY;
	EntityID orbitFxID2 = NULL_ENTITY;

	EntityID groundGlowFxID = NULL_ENTITY;
	EntityID groundCoreFxID = NULL_ENTITY;
	EntityID placeCueFxID = NULL_ENTITY;
	EntityID placeCueFxIDs[12] = {};
	u32_t placeCueFxCount = 0;

	f32_t fLifetime = 3.f;
	f32_t fElapsed = 0.f;
	f32_t fOrbitAngle = 0.f;
	Vec3 vOrbitBaseRotation = { 0.f, 0.f, 0.f };
};

class CIreliaBladeSystem final
{
public:
	~CIreliaBladeSystem() = default;

	static std::unique_ptr<CIreliaBladeSystem> Create();

	void Execute(CWorld& world, f32_t fTimeDelta);

	static EntityID SpawnPlaced(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		const Vec3& vGround, EntityID owner, f32_t fScale, const Vec3& vRotation,
		f32_t fWorldYawSpinSpeed = 0.f);

	static bool TriggerReturn(CWorld& world, EntityID bladeEntity);

	static void ExpireAfter(CWorld& world, EntityID bladeEntity, f32_t fDelay);

private:
	CIreliaBladeSystem() = default;
};
