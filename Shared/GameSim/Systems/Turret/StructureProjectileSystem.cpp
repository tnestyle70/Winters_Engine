#include "Shared/GameSim/Systems/Turret/StructureProjectileSystem.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

#include <cmath>
#include <functional>
#include <vector>

namespace GameplayTurret
{
    void CStructureProjectileSystem::Execute(CWorld& world, f32_t fTimeDelta)
    {
        WINTERS_PROFILE_SCOPE("StructureProjectile::Execute");

        std::vector<EntityID> vecDestroy;
        struct HitEvent
        {
            EntityID source = NULL_ENTITY;
            EntityID target = NULL_ENTITY;
            f32_t damage = 0.f;
        };
        std::vector<HitEvent> vecHits;

        world.ForEach<StructureProjectileComponent, TransformComponent>(
            std::function<void(EntityID, StructureProjectileComponent&, TransformComponent&)>(
                [&](EntityID id, StructureProjectileComponent& pc, TransformComponent& xf)
                {
                    if (!world.IsAlive(pc.targetEntity) ||
                        !world.HasComponent<TransformComponent>(pc.targetEntity))
                    {
                        vecDestroy.push_back(id);
                        return;
                    }

                    const Vec3 targetPos = world.GetComponent<TransformComponent>(pc.targetEntity).GetPosition();
                    const Vec3 targetAim{ targetPos.x, targetPos.y + 1.2f, targetPos.z };
                    Vec3 pos = pc.currentPos;
                    const Vec3 delta{ targetAim.x - pos.x, targetAim.y - pos.y, targetAim.z - pos.z };
                    const f32_t distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
                    const f32_t hitRadiusSq = pc.hitRadius * pc.hitRadius;

                    if (distSq <= hitRadiusSq)
                    {
                        vecHits.push_back({ pc.sourceEntity, pc.targetEntity, pc.damage });
                        vecDestroy.push_back(id);
                        return;
                    }

                    const f32_t dist = std::sqrt(distSq);
                    if (dist <= 0.0001f)
                    {
                        vecDestroy.push_back(id);
                        return;
                    }

                    const f32_t step = pc.speed * fTimeDelta;
                    const f32_t t = (step >= dist) ? 1.f : (step / dist);
                    pos.x += delta.x * t;
                    pos.y += delta.y * t;
                    pos.z += delta.z * t;

                    pc.currentPos = pos;
                    xf.SetPosition(pos);
                }));

        for (const HitEvent& hit : vecHits)
            ApplyDamage(world, hit.source, hit.target, hit.damage);

        for (EntityID id : vecDestroy)
        {
            if (world.IsAlive(id))
                world.DestroyEntity(id);
        }

        WINTERS_PROFILE_COUNT("StructureProjectile::Hits", static_cast<i32_t>(vecHits.size()));
    }

    void CStructureProjectileSystem::ApplyDamage(CWorld& world, EntityID, EntityID targetEntity,
        f32_t damage) const
    {
        if (targetEntity == NULL_ENTITY || !world.IsAlive(targetEntity))
            return;
        if (!world.HasComponent<HealthComponent>(targetEntity))
            return;

        HealthComponent& hp = world.GetComponent<HealthComponent>(targetEntity);
        if (hp.bIsDead)
            return;

        hp.fCurrent = (hp.fCurrent > damage) ? (hp.fCurrent - damage) : 0.f;
        hp.bIsDead = (hp.fCurrent <= 0.f);

        if (world.HasComponent<ChampionComponent>(targetEntity))
            world.GetComponent<ChampionComponent>(targetEntity).hp = hp.fCurrent;

        if (world.HasComponent<MinionComponent>(targetEntity))
            world.GetComponent<MinionComponent>(targetEntity).hp = hp.fCurrent;

        if (world.HasComponent<StructureComponent>(targetEntity))
            world.GetComponent<StructureComponent>(targetEntity).hp = hp.fCurrent;

        if (hp.bIsDead && world.HasComponent<MinionStateComponent>(targetEntity))
        {
            MinionStateComponent& ms = world.GetComponent<MinionStateComponent>(targetEntity);
            ms.current = MinionStateComponent::Dead;
            ms.visualState = MinionStateComponent::Dead;
            ms.deathTimer = 1.5f;
            ms.attackTargetId = NULL_ENTITY;
            ms.attackTimer = 0.f;
            ms.bHitFired = false;
            ms.bAttackAnimRequested = false;

            if (world.HasComponent<VelocityComponent>(targetEntity))
            {
                VelocityComponent& vel = world.GetComponent<VelocityComponent>(targetEntity);
                vel.vDirection = { 0.f, 0.f, 0.f };
                vel.fSpeed = 0.f;
            }

            if (world.HasComponent<NavAgentComponent>(targetEntity))
                world.GetComponent<NavAgentComponent>(targetEntity).bHasGoal = false;
        }
    }
}
