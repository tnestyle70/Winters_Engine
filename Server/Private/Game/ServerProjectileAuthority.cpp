#include "Game/ServerProjectileAuthority.h"

#include "GameRoomInternal.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/ProjectileBarrierComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/World.h"

#include <algorithm>
#include <cmath>
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

    bool_t HasAlreadyHit(
        CWorld& world,
        const SkillProjectileComponent& projectile,
        EntityID target)
    {
        const EntityHandle hTarget = world.GetEntityHandle(target);
        if (!hTarget.IsValid())
            return false;

        const u16_t count = (std::min)(
            projectile.hitEntityCount,
            kMaxPiercingProjectileHits);
        for (u16_t i = 0u; i < count; ++i)
        {
            if (projectile.hitEntities[i] == hTarget)
                return true;
        }
        return false;
    }

    u8_t ResolveProjectileTargetMask(
        GameplayStateQuery::eGameplayTargetKind kind)
    {
        switch (kind)
        {
        case GameplayStateQuery::eGameplayTargetKind::Champion:
            return ProjectileTarget_Champion;
        case GameplayStateQuery::eGameplayTargetKind::MinionOrSummon:
            return ProjectileTarget_MinionOrSummon;
        case GameplayStateQuery::eGameplayTargetKind::JungleMonster:
            return ProjectileTarget_JungleMonster;
        case GameplayStateQuery::eGameplayTargetKind::Structure:
            return ProjectileTarget_Structure;
        default:
            return ProjectileTarget_None;
        }
    }

    bool_t IsEpicJungleMonster(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<JungleComponent>(entity))
            return false;

        const u32_t subKind = world.GetComponent<JungleComponent>(entity).subKind;
        return subKind == 0u || subKind == 1u;
    }

    bool_t IntersectBarrierXZ(
        const ProjectileBarrierComponent& barrier,
        const Vec3& start,
        const Vec3& end,
        f32_t projectileRadius,
        f32_t& outT)
    {
        const Vec3 forward = WintersMath::NormalizeXZ(
            barrier.direction,
            Vec3{ 0.f, 0.f, 1.f },
            0.0001f);
        const Vec3 right{ forward.z, 0.f, -forward.x };
        const Vec3 startOffset{
            start.x - barrier.previousCenter.x,
            0.f,
            start.z - barrier.previousCenter.z };
        const Vec3 endOffset{
            end.x - barrier.center.x,
            0.f,
            end.z - barrier.center.z };

        const f32_t localStart[2]{
            startOffset.x * right.x + startOffset.z * right.z,
            startOffset.x * forward.x + startOffset.z * forward.z };
        const f32_t localDelta[2]{
            endOffset.x * right.x + endOffset.z * right.z - localStart[0],
            endOffset.x * forward.x + endOffset.z * forward.z - localStart[1] };
        const f32_t extents[2]{
            (std::max)(0.f, barrier.halfLength) + projectileRadius,
            (std::max)(0.f, barrier.halfThickness) + projectileRadius };

        f32_t tMin = 0.f;
        f32_t tMax = 1.f;
        for (u8_t axis = 0u; axis < 2u; ++axis)
        {
            if (std::abs(localDelta[axis]) <= 0.000001f)
            {
                if (localStart[axis] < -extents[axis] ||
                    localStart[axis] > extents[axis])
                {
                    return false;
                }
                continue;
            }

            f32_t t0 = (-extents[axis] - localStart[axis]) / localDelta[axis];
            f32_t t1 = (extents[axis] - localStart[axis]) / localDelta[axis];
            if (t0 > t1)
                std::swap(t0, t1);
            tMin = (std::max)(tMin, t0);
            tMax = (std::min)(tMax, t1);
            if (tMin > tMax)
                return false;
        }

        outT = tMin;
        return tMin >= 0.f && tMin <= 1.f;
    }

    bool_t FindSweptCircleEntryXZ(
        const Vec3& start,
        const Vec3& end,
        const Vec3& center,
        f32_t radius,
        f32_t& outT)
    {
        const f32_t dx = end.x - start.x;
        const f32_t dz = end.z - start.z;
        const f32_t fx = start.x - center.x;
        const f32_t fz = start.z - center.z;
        const f32_t sanitizedRadius = (std::max)(0.f, radius);
        const f32_t c = fx * fx + fz * fz -
            sanitizedRadius * sanitizedRadius;
        if (c <= 0.f)
        {
            outT = 0.f;
            return true;
        }

        const f32_t a = dx * dx + dz * dz;
        if (a <= std::numeric_limits<f32_t>::epsilon())
            return false;

        const f32_t b = 2.f * (fx * dx + fz * dz);
        const f32_t discriminant = b * b - 4.f * a * c;
        if (discriminant < 0.f)
            return false;

        const f32_t sqrtDiscriminant = std::sqrt(discriminant);
        const f32_t inverseDenominator = 0.5f / a;
        const f32_t entryT = (-b - sqrtDiscriminant) * inverseDenominator;
        const f32_t exitT = (-b + sqrtDiscriminant) * inverseDenominator;
        if (exitT < 0.f || entryT > 1.f)
            return false;

        outT = (std::max)(0.f, entryT);
        return outT <= 1.f;
    }

    Vec3 ResolvePreviousTargetPosition(
        CWorld& world,
        EntityID target,
        const ILagCompensationQuery* pLagCompensation,
        u64_t uCurrentTick,
        const Vec3& currentPosition)
    {
        if (!pLagCompensation || uCurrentTick == 0u)
            return currentPosition;

        if (world.HasComponent<PositionDiscontinuityComponent>(target) &&
            world.GetComponent<PositionDiscontinuityComponent>(target).uTick ==
                uCurrentTick)
        {
            return currentPosition;
        }

        const EntityHandle hTarget = world.GetEntityHandle(target);
        LagCompensatedEntityState previous{};
        if (!hTarget.IsValid() ||
            !pLagCompensation->TryGetHistoricalStateAtTick(
                hTarget,
                uCurrentTick - 1u,
                previous))
        {
            return currentPosition;
        }
        if (!std::isfinite(previous.vPosition.x) ||
            !std::isfinite(previous.vPosition.y) ||
            !std::isfinite(previous.vPosition.z))
        {
            return currentPosition;
        }
        return previous.vPosition;
    }
}

bool_t CServerProjectileAuthority::IsMinionRangedProjectileKind(eProjectileKind kind)
{
    return kind == eProjectileKind::MinionRangedBasicBlue ||
        kind == eProjectileKind::MinionRangedBasicRed;
}

u32_t CServerProjectileAuthority::QuantizeContactT(f32_t fContactT)
{
    constexpr f32_t kContactTScale = 1048576.f;
    if (!std::isfinite(fContactT))
        return static_cast<u32_t>(kContactTScale);
    const f32_t fClamped = (std::clamp)(fContactT, 0.f, 1.f);
    return static_cast<u32_t>(std::floor(fClamped * kContactTScale + 0.5f));
}

f32_t CServerProjectileAuthority::DequantizeContactT(u32_t uContactT)
{
    constexpr f32_t kContactTScale = 1048576.f;
    const u32_t uClamped = (std::min)(
        uContactT,
        static_cast<u32_t>(kContactTScale));
    return static_cast<f32_t>(uClamped) / kContactTScale;
}

EntityID CServerProjectileAuthority::FindSkillProjectileHitTarget(
    CWorld& world,
    const SkillProjectileComponent& projectile,
    const ILagCompensationQuery* pLagCompensation,
    u64_t uCurrentTick,
    const Vec3& start,
    const Vec3& end,
    Vec3& outHitPos,
    f32_t& outHitT)
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

                const GameplayStateQuery::eGameplayTargetKind targetKind =
                    GameplayStateQuery::ResolveTargetKind(world, entity);
                const u8_t targetMask = ResolveProjectileTargetMask(targetKind);
                if (targetMask == ProjectileTarget_None ||
                    (projectile.targetKindMask & targetMask) == 0u)
                {
                    return;
                }
                if (projectile.bEpicMonstersOnly &&
                    targetKind == GameplayStateQuery::eGameplayTargetKind::JungleMonster &&
                    !IsEpicJungleMonster(world, entity))
                {
                    return;
                }
                if (projectile.kind == eProjectileKind::AsheCrystalArrow &&
                    targetKind != GameplayStateQuery::eGameplayTargetKind::Champion)
                {
                    return;
                }
                if ((projectile.unitHitPolicy == eProjectileUnitHitPolicy::Pierce ||
                        projectile.kind == eProjectileKind::Tornado) &&
                    HasAlreadyHit(world, projectile, entity))
                    return;

                const eTeam targetTeam =
                    GameplayStateQuery::ResolveEntityTeam(world, entity);
                if (targetTeam == eTeam::TEAM_END)
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
                if (!std::isfinite(targetPos.x) ||
                    !std::isfinite(targetPos.y) ||
                    !std::isfinite(targetPos.z))
                {
                    return;
                }
                const Vec3 previousTargetPos = ResolvePreviousTargetPosition(
                    world,
                    entity,
                    pLagCompensation,
                    uCurrentTick,
                    targetPos);
                const f32_t projectileRadius = bYasuoTornado
                    ? std::max(projectile.hitRadius, 2.25f)
                    : projectile.hitRadius;
                const f32_t radius = projectileRadius + ResolveCombatRadius(world, entity);
                const Vec3 relativeStart{
                    start.x - previousTargetPos.x,
                    start.y - previousTargetPos.y,
                    start.z - previousTargetPos.z };
                const Vec3 relativeEnd{
                    end.x - targetPos.x,
                    end.y - targetPos.y,
                    end.z - targetPos.z };
                f32_t t = 0.f;
                if (!FindSweptCircleEntryXZ(
                    relativeStart,
                    relativeEnd,
                    Vec3{},
                    radius,
                    t))
                    return;

                const u32_t uCandidateT = QuantizeContactT(t);
                const u32_t uBestT = QuantizeContactT(bestT);
                const EntityHandle hCandidate = world.GetEntityHandle(entity);
                const EntityHandle hBest = world.GetEntityHandle(bestTarget);
                const bool_t bCloser =
                    bestTarget == NULL_ENTITY ||
                    uCandidateT < uBestT ||
                    (uCandidateT == uBestT && hCandidate.ToU64() < hBest.ToU64());
                if (bCloser)
                {
                    bestTarget = entity;
                    bestT = DequantizeContactT(uCandidateT);
                    outHitPos = Vec3{
                        start.x + (end.x - start.x) * bestT,
                        targetPos.y + 1.0f,
                        start.z + (end.z - start.z) * bestT
                    };
                }
            }));

    if (bestTarget != NULL_ENTITY)
        outHitT = bestT;
    return bestTarget;
}

bool_t CServerProjectileAuthority::FindProjectileBarrierHit(
    CWorld& world,
    const SkillProjectileComponent& projectile,
    const Vec3& start,
    const Vec3& end,
    Vec3& outHitPos,
    f32_t& outHitT)
{
    if (!projectile.bBlockedByProjectileBarriers)
        return false;

    bool_t bHit = false;
    f32_t bestT = 1.f;
    EntityID bestBarrier = NULL_ENTITY;
    world.ForEach<ProjectileBarrierComponent>(
        std::function<void(EntityID, ProjectileBarrierComponent&)>(
            [&](EntityID entity, ProjectileBarrierComponent& barrier)
            {
                if (barrier.sourceTeam == projectile.sourceTeam &&
                    barrier.sourceTeam != eTeam::Neutral)
                {
                    return;
                }

                f32_t t = 1.f;
                if (!IntersectBarrierXZ(
                    barrier,
                    start,
                    end,
                    (std::max)(0.f, projectile.hitRadius),
                    t))
                {
                    return;
                }

                const u32_t uCandidateT = QuantizeContactT(t);
                const u32_t uBestT = QuantizeContactT(bestT);
                const EntityHandle hCandidate = world.GetEntityHandle(entity);
                const EntityHandle hBest = world.GetEntityHandle(bestBarrier);
                if (!bHit ||
                    uCandidateT < uBestT ||
                    (uCandidateT == uBestT && hCandidate.ToU64() < hBest.ToU64()))
                {
                    bHit = true;
                    bestT = DequantizeContactT(uCandidateT);
                    bestBarrier = entity;
                }
            }));

    if (bHit)
    {
        outHitT = bestT;
        outHitPos = Vec3{
            start.x + (end.x - start.x) * bestT,
            start.y + (end.y - start.y) * bestT,
            start.z + (end.z - start.z) * bestT };
    }
    return bHit;
}

bool_t CServerProjectileAuthority::FindTargetedProjectileHit(
    CWorld& world,
    const SkillProjectileComponent& projectile,
    const ILagCompensationQuery* pLagCompensation,
    u64_t uCurrentTick,
    EntityID targetEntity,
    const Vec3& start,
    const Vec3& end,
    Vec3& outHitPos,
    f32_t& outHitT)
{
    if (targetEntity == NULL_ENTITY ||
        !world.IsAlive(targetEntity) ||
        !world.HasComponent<TransformComponent>(targetEntity))
    {
        return false;
    }

    const Vec3 targetPos =
        world.GetComponent<TransformComponent>(targetEntity).GetPosition();
    if (!std::isfinite(targetPos.x) ||
        !std::isfinite(targetPos.y) ||
        !std::isfinite(targetPos.z))
    {
        return false;
    }
    const Vec3 previousTargetPos = ResolvePreviousTargetPosition(
        world,
        targetEntity,
        pLagCompensation,
        uCurrentTick,
        targetPos);
    const f32_t radius = (std::max)(0.f, projectile.hitRadius) +
        ResolveCombatRadius(world, targetEntity);
    const Vec3 relativeStart{
        start.x - previousTargetPos.x,
        start.y - previousTargetPos.y,
        start.z - previousTargetPos.z };
    const Vec3 relativeEnd{
        end.x - targetPos.x,
        end.y - targetPos.y,
        end.z - targetPos.z };
    f32_t t = 0.f;
    if (!FindSweptCircleEntryXZ(relativeStart, relativeEnd, Vec3{}, radius, t))
        return false;

    outHitT = DequantizeContactT(QuantizeContactT(t));
    outHitPos = Vec3{
        start.x + (end.x - start.x) * outHitT,
        start.y + (end.y - start.y) * outHitT,
        start.z + (end.z - start.z) * outHitT };
    return true;
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
    u64_t startTick,
    ProjectileContactReason eContactReason,
    u16_t uContactOrdinal,
    bool_t bDestroyed)
{
    ReplicatedEventComponent event{};
    event.kind = eReplicatedEventKind::ProjectileHit;
    event.sourceEntity = sourceEntity;
    event.targetEntity = targetEntity;
    event.projectileEntity = projectileEntity;
    event.projectileKind = projectileKind;
    event.position = position;
    event.eContactReason = eContactReason;
    event.uContactOrdinal = uContactOrdinal;
    event.bDestroyed = bDestroyed;
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
    request.skillId = kStructureProjectileKind;
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
    request.iSourceSlot = projectile.sourceSlot;
    request.eSourceKind = projectile.damageSourceKind;
    request.adRatioOverride = projectile.totalAdRatio;
    request.bonusAdRatioOverride = projectile.bonusAdRatio;
    request.apRatioOverride = projectile.apRatio;
    request.flags = projectile.damageFlags;
    return request;
}
