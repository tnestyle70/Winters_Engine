#include "WintersPCH.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/World.h"
#include "ECS/SpatialIndex.h"
#include "ProfilerAPI.h"

NS_BEGIN(Engine)

void CSpatialHashSystem::Execute(CWorld& world, f32_t)
{
    WINTERS_PROFILE_SCOPE("SpatialHashSystem::Execute");

    CSpatialIndex* pIndex = world.Get_SpatialIndex();
    if (!pIndex)
        return;

    pIndex->Rebuild(world);
}

NS_END
