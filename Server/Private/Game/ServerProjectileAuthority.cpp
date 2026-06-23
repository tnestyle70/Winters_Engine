#include "Game/ServerProjectileAuthority.h"

#include "GameRoomInternal.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/World.h"

#include <algorithm>
#include <functional>
#include <limits>

namespace
{
    f32_t ResolveCombatRadius(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<SpatialAgentComponent>(entity))
            return std::max(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);
        if (world.HasComponent<StructureComponent>(entity))
            return 1.5f;
        if (world.HasComponent<MinionComponent>(entity) ||
            world.HasComponent<MinionStateComponent>(entity))
            return 0.45f;
        return 0.65f;
    }
}

bool_t CServerProjectileAuthority::IsMinionRangedProjectileKind(eProjectileKind kind)
{
    return kind == eProjectileKind::MinionRangedBasicBlue ||
        kind == eProjectileKind::MinionRangedBasicRed;
}

EntityID CServerProjectileAuthority::FindSkillProjectileHitTarget(
    CWorld& world,
    const SkillProjectileComponent& projectile,
    const Vec3& start,
    const Vec3& end,
    Vec3& outHitPos)
{
    EntityID bestTarget = NULL_ENTITY;
    f32_t bestT = 1.f;

    world.ForEach<HealthComponent, TransformComponent>(
        std::function<void(EntityID, HealthComponent&, TransformComponent&)>(
            [&](EntityID entity, HealthComponent& health, TransformComponent& transform)
            {
                const bool_t bYasuoTornado =
                    projectile.kind == eProjectileKind::Tornado;
                if (entity == projectile.sourceEntity ||
                    !world.IsAlive(entity) ||
                    health.bIsDead ||
                    health.fCurrent <= 0.f)
                {
                    return;
                }

                eTeam targetTeam = eTeam::Neutral;
                if (!TryResolveCombatTeam(world, entity, targetTeam))
                    return;
                if (targetTeam == projectile.sourceTeam &&
                    targetTeam != eTeam::Neutral)
                {
                    return;
                }
                if (!GameplayStateQuery::CanReceiveProjectileHit(
                    world,
                    projectile.sourceEntity,
                    entity))
                {
                    return;
                }

                const Vec3 targetPos = transform.GetPosition();
                f32_t t = 0.f;
                const f32_t distSq = WintersMath::DistanceSqPointToSegmentXZ(
                    targetPos,
                    start,
                    end,
                    &t,
                    std::numeric_limits<f32_t>::epsilon());
                const f32_t projectileRadius = bYasuoTornado
                    ? std::max(projectile.hitRadius, 2.25f)
                    : projectile.hitRadius;
                const f32_t radius = projectileRadius + ResolveCombatRadius(world, entity);
                if (distSq <= radius * radius && t <= bestT)
                {
                    bestTarget = entity;
                    bestT = t;
                    outHitPos = Vec3{
                        start.x + (end.x - start.x) * t,
                        targetPos.y + 1.0f,
                        start.z + (end.z - start.z) * t
                    };
                }
            }));

    return bestTarget;
}

ReplicatedEventComponent CServerProjectileAuthority::BuildProjectileSpawnEvent(
    EntityID sourceEntity,
    EntityID targetEntity,
    EntityID projectileEntity,
    u16_t projectileKind,
    const Vec3& position,
    const Vec3& direction,
    f32_t speed,
    f32_t maxDistance,
    u64_t startTick)
{
    ReplicatedEventComponent event{};
    event.kind = eReplicatedEventKind::ProjectileSpawn;
    event.sourceEntity = sourceEntity;
    event.targetEntity = targetEntity;
    event.projectileEntity = projectileEntity;
    event.projectileKind = projectileKind;
    event.position = position;
    event.direction = direction;
    event.speed = speed;
    event.maxDistance = maxDistance;
    event.startTick = startTick;
    return event;
}

ReplicatedEventComponent CServerProjectileAuthority::BuildProjectileHitEvent(
    EntityID sourceEntity,
    EntityID targetEntity,
    EntityID projectileEntity,
    u16_t projectileKind,
    const Vec3& position,
    u64_t startTick)
{
    ReplicatedEventComponent event{};
    event.kind = eReplicatedEventKind::ProjectileHit;
    event.sourceEntity = sourceEntity;
    event.targetEntity = targetEntity;
    event.projectileEntity = projectileEntity;
    event.projectileKind = projectileKind;
    event.position = position;
    event.bDestroyed = true;
    event.startTick = startTick;
    return event;
}

DamageRequest CServerProjectileAuthority::BuildTurretDamageRequest(
    EntityID sourceEntity,
    EntityID targetEntity,
    eTeam sourceTeam,
    f32_t damage)
{
    DamageRequest request{};
    request.source = sourceEntity;
    request.target = targetEntity;
    request.sourceTeam = sourceTeam;
    request.type = eDamageType::Physical;
    request.flatAmount = damage;
    request.skillId = kTurretProjectileKind;
    return request;
}

DamageRequest CServerProjectileAuthority::BuildSkillProjectileDamageRequest(
    const SkillProjectileComponent& projectile,
    EntityID targetEntity,
    eDamageType damageType)
{
    DamageRequest request{};
    request.source = projectile.sourceEntity;
    request.target = targetEntity;
    request.sourceTeam = projectile.sourceTeam;
    request.type = damageType;
    request.flatAmount = projectile.damage;
    request.skillId = projectile.skillId;
    request.rank = projectile.rank;
    request.flags = DamageFlag_OnHit;
    return request;
}
