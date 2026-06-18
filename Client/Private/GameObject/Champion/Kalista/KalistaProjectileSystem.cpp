#include "GameObject/Champion/Kalista/KalistaProjectileSystem.h"
#include "GameObject/Champion/Kalista/KalistaRendSystem.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"
#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{}

std::unique_ptr<CKalistaProjectileSystem> CKalistaProjectileSystem::Create()
{
    return std::unique_ptr<CKalistaProjectileSystem>(new CKalistaProjectileSystem());
}

void CKalistaProjectileSystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("KalistaProj::Execute");

    std::vector<EntityID> vecDelete;

    world.ForEach<KalistaProjectileComponent, TransformComponent>(
        std::function<void(EntityID, KalistaProjectileComponent&, TransformComponent&)>(
            [&](EntityID entity, KalistaProjectileComponent& projectile, TransformComponent& tf)
            {
                if (projectile.bHasHit)
                {
                    vecDelete.push_back(entity);
                    return;
                }

                const f32_t step = projectile.fSpeed * dt;
                projectile.vWorldPos.x += projectile.vDirection.x * step;
                projectile.vWorldPos.z += projectile.vDirection.z * step;
                projectile.fTravelled += step;
                tf.m_LocalPosition = projectile.vWorldPos;

                if (projectile.fTravelled >= projectile.fMaxDist)
                {
                    if (projectile.visualEntity != NULL_ENTITY
                        && world.HasComponent<FxMeshComponent>(projectile.visualEntity))
                    {
                        world.GetComponent<FxMeshComponent>(projectile.visualEntity).bPendingDelete = true;
                    }

                    vecDelete.push_back(entity);
                    return;
                }

                const f32_t radiusSq = projectile.fRadius * projectile.fRadius;
                world.ForEach<ChampionComponent, TransformComponent>(
                    std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                        [&](EntityID target, ChampionComponent& champion, TransformComponent& targetTf)
                        {
                            if (projectile.bHasHit) return;
                            if (target == projectile.ownerEntity) return;
                            if (champion.team == projectile.ownerTeam) return;

                            const f32_t dx = targetTf.m_LocalPosition.x - projectile.vWorldPos.x;
                            const f32_t dz = targetTf.m_LocalPosition.z - projectile.vWorldPos.z;
                            if (dx * dx + dz * dz > radiusSq)
                                return;

                            champion.hp -= projectile.fDamage;
                            if (champion.hp < 0.f)
                                champion.hp = 0.f;

                            if (projectile.bApplyRendStack)
                            {
                                CKalistaRendSystem::AddStack(world, target,
                                    projectile.ownerEntity, projectile.ownerTeam,
                                    projectile.pRenderer, projectile.fSpearScale);
                            }

                            if (projectile.visualEntity != NULL_ENTITY
                                && world.HasComponent<FxMeshComponent>(projectile.visualEntity))
                            {
                                world.GetComponent<FxMeshComponent>(projectile.visualEntity).bPendingDelete = true;
                            }

                            char dbg[160]{};
                            sprintf_s(dbg,
                                "[KalistaProj] hit target=%u dmg=%.1f hp=%.1f rend++\n",
                                static_cast<u32_t>(target),
                                projectile.fDamage,
                                champion.hp);

                            projectile.bHasHit = true;
                        }));
            }));

    for (EntityID entity : vecDelete)
        world.DestroyEntity(entity);
}

EntityID CKalistaProjectileSystem::Spawn(CWorld& world, const Vec3& vOrigin, const Vec3& vDirection, f32_t fSpeed, f32_t fMaxDist, f32_t fRadius, f32_t fDamage, EntityID owner, eTeam ownerTeam, Engine::CFxStaticMeshRenderer* pRenderer, f32_t fSpearScale, bool_t bApplyRendStack, EntityID visualEntity)
{
    EntityID entity = world.CreateEntity();

    TransformComponent tf{};
    tf.m_LocalPosition = vOrigin;
    world.AddComponent<TransformComponent>(entity, tf);

    KalistaProjectileComponent projectile{};
    projectile.vWorldPos = vOrigin;
    projectile.vDirection = WintersMath::NormalizeXZ(vDirection);
    projectile.fSpeed = fSpeed;
    projectile.fMaxDist = fMaxDist;
    projectile.fRadius = fRadius;
    projectile.fDamage = fDamage;
    projectile.ownerEntity = owner;
    projectile.ownerTeam = ownerTeam;
    projectile.pRenderer = pRenderer;
    projectile.fSpearScale = fSpearScale;
    projectile.bApplyRendStack = bApplyRendStack;
    projectile.visualEntity = visualEntity;
    world.AddComponent<KalistaProjectileComponent>(entity, projectile);

    return entity;
}
