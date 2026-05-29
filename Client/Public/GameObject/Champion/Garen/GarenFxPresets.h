#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

class CWorld;

namespace Engine
{
	class CFxStaticMeshRenderer;
}
//Garen Fx 종류에 따른 Funtion Helper!

namespace GarenFx
{
	//Q Trail
	void SpawnQTrail(CWorld& world, EntityID owner, f32_t fLifetime);
	//W Courage
	void SpawnWShield(CWorld& world, EntityID owner, f32_t fDuration);
	//E - Judgment 
	EntityID SpawnESpinBlade(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
		EntityID owner, f32_t fDuration);
	//R 
	void SpawnRSword(CWorld& world, EntityID target, f32_t fLifetime);
}
