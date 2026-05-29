#include "GameObject/FX/WindWallSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxBeamComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxRibbonComponent.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"
#include <cmath>
#include <functional>
#include <vector>

std::unique_ptr<CWindWallSystem> CWindWallSystem::Create()
{
    return std::unique_ptr<CWindWallSystem>(new CWindWallSystem());
}

EntityID CWindWallSystem::Spawn(CWorld& world, const Vec3& vOrigin, const Vec3& vForward,
    f32_t fLifetime, f32_t fWidth, f32_t fHeight, EntityID owner, eTeam ownerTeam)
{
    EntityID entity = world.CreateEntity();

    WindWallComponent wall{};
    wall.vWorldPos = { vOrigin.x + vForward.x * 1.5f, vOrigin.y, vOrigin.z + vForward.z * 1.5f };
    wall.vForward = vForward;
    wall.fWidth = fWidth;
    wall.fHeight = fHeight;
    wall.fLifetime = fLifetime;
    wall.ownerEntity = owner;
    wall.ownerTeam = ownerTeam;
    world.AddComponent<WindWallComponent>(entity, wall);

    return entity;
}

void CWindWallSystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("WindWall::Execute");

    std::vector<EntityID> vecDelete;
    world.ForEach<WindWallComponent>(
        std::function<void(EntityID, WindWallComponent&)>(
            [&](EntityID entity, WindWallComponent& wall)
            {
                wall.fElapsed += dt;
                if (wall.fElapsed >= wall.fLifetime)
                {
                    vecDelete.push_back(entity);
                    return;
                }

                const f32_t halfW = wall.fWidth * 0.5f;
                const f32_t halfH = wall.fHeight * 0.5f;
                const Vec3& fwd = wall.vForward;
                const Vec3 right{ -fwd.z, 0.f, fwd.x };

                world.ForEach<FxBillboardComponent>(
                    std::function<void(EntityID, FxBillboardComponent&)>(
                        [&](EntityID, FxBillboardComponent& fx)
                        {
                            if (!fx.bBlockableByWindWall) return;
                            const f32_t vmag2 = fx.vVelocity.x * fx.vVelocity.x
                                + fx.vVelocity.z * fx.vVelocity.z;
                            if (vmag2 < 0.01f) return;

                            const f32_t dx = fx.vWorldPos.x - wall.vWorldPos.x;
                            const f32_t dz = fx.vWorldPos.z - wall.vWorldPos.z;
                            const f32_t along = dx * right.x + dz * right.z;
                            const f32_t perp = dx * fwd.x + dz * fwd.z;
                            if (std::fabs(along) <= halfW && std::fabs(perp) <= halfH)
                                fx.bPendingDelete = true;
                        }));

                world.ForEach<FxMeshComponent>(
                    std::function<void(EntityID, FxMeshComponent&)>(
                        [&](EntityID, FxMeshComponent& fx)
                        {
                            if (!fx.bBlockableByWindWall) return;
                            const f32_t vmag2 = fx.vVelocity.x * fx.vVelocity.x
                                + fx.vVelocity.z * fx.vVelocity.z;
                            if (vmag2 < 0.01f) return;

                            const f32_t dx = fx.vWorldPos.x - wall.vWorldPos.x;
                            const f32_t dz = fx.vWorldPos.z - wall.vWorldPos.z;
                            const f32_t along = dx * right.x + dz * right.z;
                            const f32_t perp = dx * fwd.x + dz * fwd.z;
                            if (std::fabs(along) <= halfW && std::fabs(perp) <= halfH)
                                fx.bPendingDelete = true;
                        }));

                world.ForEach<FxBeamComponent>(
                    std::function<void(EntityID, FxBeamComponent&)>(
                        [&](EntityID, FxBeamComponent& fx)
                        {
                            if (!fx.bBlockableByWindWall) return;
                            const f32_t vmag2 = fx.vVelocity.x * fx.vVelocity.x
                                + fx.vVelocity.z * fx.vVelocity.z;
                            if (vmag2 < 0.01f) return;

                            const f32_t mx = (fx.vStartWorldPos.x + fx.vEndWorldPos.x) * 0.5f;
                            const f32_t mz = (fx.vStartWorldPos.z + fx.vEndWorldPos.z) * 0.5f;
                            const f32_t dx = mx - wall.vWorldPos.x;
                            const f32_t dz = mz - wall.vWorldPos.z;
                            const f32_t along = dx * right.x + dz * right.z;
                            const f32_t perp = dx * fwd.x + dz * fwd.z;
                            if (std::fabs(along) <= halfW && std::fabs(perp) <= halfH)
                                fx.bPendingDelete = true;
                        }));

                world.ForEach<FxRibbonComponent>(
                    std::function<void(EntityID, FxRibbonComponent&)>(
                        [&](EntityID, FxRibbonComponent& fx)
                        {
                            if (!fx.bBlockableByWindWall || fx.iPointCount == 0) return;
                            const f32_t vmag2 = fx.vVelocity.x * fx.vVelocity.x
                                + fx.vVelocity.z * fx.vVelocity.z;
                            if (vmag2 < 0.01f) return;

                            const f32_t dx = fx.points[0].x - wall.vWorldPos.x;
                            const f32_t dz = fx.points[0].z - wall.vWorldPos.z;
                            const f32_t along = dx * right.x + dz * right.z;
                            const f32_t perp = dx * fwd.x + dz * fwd.z;
                            if (std::fabs(along) <= halfW && std::fabs(perp) <= halfH)
                                fx.bPendingDelete = true;
                        }));
            }));

    for (EntityID entity : vecDelete)
        world.DestroyEntity(entity);
}
