#include "GameObject/Champion/Yasuo/YasuoProjectileSystem.h"
#include "GameObject/FX/WindWallSystem.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"
#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{
    bool IsInsideWindWall(const YasuoProjectileComponent& p, const WindWallComponent& wall)
    {
        const f32_t halfW = wall.fWidth * 0.5f;
        const f32_t halfH = wall.fHeight * 0.5f;
        const Vec3& fwd = wall.vForward;
        const Vec3 right{ -fwd.z, 0.f, fwd.x };

        const f32_t dx = p.vWorldPos.x - wall.vWorldPos.x;
        const f32_t dz = p.vWorldPos.z - wall.vWorldPos.z;
        const f32_t along = dx * right.x + dz * right.z;
        const f32_t perp = dx * fwd.x + dz * fwd.z;
        return std::fabs(along) <= halfW && std::fabs(perp) <= halfH;
    }

    bool IsBlockedByWindWall(CWorld& world, const YasuoProjectileComponent& p)
    {
        bool_t bBlocked = false;
        world.ForEach<WindWallComponent>(
            std::function<void(EntityID, WindWallComponent&)>(
                [&](EntityID, WindWallComponent& wall)
                {
                    if (!bBlocked && IsInsideWindWall(p, wall))
                        bBlocked = true;
                }));
        return bBlocked;
    }
}

std::unique_ptr<CYasuoProjectileSystem> CYasuoProjectileSystem::Create()
{
    return std::unique_ptr<CYasuoProjectileSystem>(new CYasuoProjectileSystem());
}

EntityID CYasuoProjectileSystem::Spawn(CWorld& world,
    const Vec3& vOrigin, const Vec3& vDirection,
    f32_t fSpeed, f32_t fMaxDist, f32_t fRadius,
    f32_t fDamage, f32_t fStunSec,
    eYasuoProjectileKind kind,
    EntityID owner, eTeam ownerTeam)
{
    EntityID entity = world.CreateEntity();

    TransformComponent tf{};
    tf.m_LocalPosition = vOrigin;
    world.AddComponent<TransformComponent>(entity, tf);

    YasuoProjectileComponent projectile{};
    projectile.vWorldPos = vOrigin;
    projectile.vDirection = WintersMath::NormalizeXZ(vDirection);
    projectile.fSpeed = fSpeed;
    projectile.fMaxDist = fMaxDist;
    projectile.fRadius = fRadius;
    projectile.fDamage = fDamage;
    projectile.fStunSec = fStunSec;
    projectile.kind = kind;
    projectile.ownerEntity = owner;
    projectile.ownerTeam = ownerTeam;
    world.AddComponent<YasuoProjectileComponent>(entity, projectile);

    return entity;
}

void CYasuoProjectileSystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("YasuoProjectile::Execute");

    std::vector<EntityID> vecDelete;

    world.ForEach<YasuoProjectileComponent, TransformComponent>(
        std::function<void(EntityID, YasuoProjectileComponent&, TransformComponent&)>(
            [&](EntityID entity, YasuoProjectileComponent& projectile, TransformComponent& tf)
            {
                const f32_t step = projectile.fSpeed * dt;
                projectile.vWorldPos.x += projectile.vDirection.x * step;
                projectile.vWorldPos.z += projectile.vDirection.z * step;
                projectile.fTravelled += step;
                tf.m_LocalPosition = projectile.vWorldPos;

                if (projectile.kind != eYasuoProjectileKind::EQRing
                    && (projectile.fTravelled >= projectile.fMaxDist
                        || IsBlockedByWindWall(world, projectile)))
                {
                    vecDelete.push_back(entity);
                    return;
                }

                const f32_t radiusSq = projectile.fRadius * projectile.fRadius;
                world.ForEach<ChampionComponent, TransformComponent>(
                    std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                        [&](EntityID target, ChampionComponent& champion, TransformComponent& targetTf)
                        {
                            if (projectile.hitSet.count(target)) return;
                            if (target == projectile.ownerEntity) return;
                            if (champion.team == projectile.ownerTeam) return;

                            const f32_t dx = targetTf.m_LocalPosition.x - projectile.vWorldPos.x;
                            const f32_t dz = targetTf.m_LocalPosition.z - projectile.vWorldPos.z;
                            if (dx * dx + dz * dz > radiusSq)
                                return;

                            champion.hp -= projectile.fDamage;
                            if (champion.hp < 0.f)
                                champion.hp = 0.f;

                            if (projectile.kind == eYasuoProjectileKind::Tornado
                                && projectile.fStunSec > 0.f)
                            {
                                StunComponent stun{};
                                stun.fRemaining = projectile.fStunSec;
                                stun.sourceEntity = projectile.ownerEntity;

                                if (world.HasComponent<StunComponent>(target))
                                    world.GetComponent<StunComponent>(target) = stun;
                                else
                                    world.AddComponent<StunComponent>(target, stun);
                            }

                            projectile.hitSet.insert(target);

                            char dbg[160]{};
                            sprintf_s(dbg,
                                "[YasuoProj] hit kind=%u target=%u dmg=%.1f hp=%.1f stun=%.2f\n",
                                static_cast<u32_t>(projectile.kind),
                                static_cast<u32_t>(target),
                                projectile.fDamage,
                                champion.hp,
                                projectile.fStunSec);
                        }));

                if (projectile.kind == eYasuoProjectileKind::EQRing)
                    vecDelete.push_back(entity);
            }));

    for (EntityID entity : vecDelete)
        world.DestroyEntity(entity);
}
