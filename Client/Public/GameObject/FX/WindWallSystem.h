#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include <memory>

class CWorld;

struct WindWallComponent
{
    Vec3 vWorldPos{};
    Vec3 vForward{};
    f32_t fWidth = 6.f;
    f32_t fHeight = 0.5f;
    f32_t fLifetime = 5.f;
    f32_t fElapsed = 0.f;
    EntityID ownerEntity = NULL_ENTITY;
    eTeam ownerTeam = eTeam::Neutral;
};

class CWindWallSystem final
{
public:
    ~CWindWallSystem() = default;

    static std::unique_ptr<CWindWallSystem> Create();

    void Execute(CWorld& world, f32_t dt);

    static EntityID Spawn(CWorld& world,
        const Vec3& vOrigin, const Vec3& vForward,
        f32_t fLifetime, f32_t fWidth, f32_t fHeight,
        EntityID owner, eTeam ownerTeam = eTeam::Neutral);

private:
    CWindWallSystem() = default;
};
