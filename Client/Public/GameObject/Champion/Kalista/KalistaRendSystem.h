#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include <memory>

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

struct KalistaRendComponent
{
    EntityID spearVisualEntity = NULL_ENTITY;
    EntityID hostTarget = NULL_ENTITY;
    EntityID caster = NULL_ENTITY;
    eTeam casterTeam = eTeam::Neutral;
    i32_t iStackCount = 1;
};

class CKalistaRendSystem final
{
public:
    ~CKalistaRendSystem() = default;

    static std::unique_ptr<CKalistaRendSystem> Create();

    static void AddStack(CWorld& world, EntityID target,
        EntityID caster, eTeam casterTeam,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr,
        f32_t fSpearScale = 0.005f);

    static void TriggerExplode(CWorld& world, EntityID caster,
        f32_t fBaseDamage, f32_t fStackDmg);

    void Execute(CWorld& world, f32_t dt);

private:
    CKalistaRendSystem() = default;
};
