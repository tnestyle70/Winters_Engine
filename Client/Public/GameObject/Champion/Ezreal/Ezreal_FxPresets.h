#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
	class CFxStaticMeshRenderer;
}

namespace Ezreal::Fx
{
	void SpawnBAProjectile(CWorld& world, EntityID owner, const Vec3& origin,
		const Vec3& dir, f32_t fLifetime);
	void SpawnQProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed);
	void SpawnWProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed);
	void SpawnEFlash(CWorld& world, const Vec3& origin, const Vec3& dest,
		f32_t fLifetime);
	void SpawnRBow(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, f32_t fLifetime);
	void SpawnRProjectile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, const Vec3& origin, const Vec3& dir,
		f32_t fLifetime, f32_t fSpeed);
}
