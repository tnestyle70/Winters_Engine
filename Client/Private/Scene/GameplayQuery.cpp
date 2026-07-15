#include "Scene/GameplayQuery.h"
#include "Scene/RenderVisibilityFilter.h"

#include "ECS/World.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#include "Shared/GameSim/Components/ViegoSoulComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>

bool_t GameplayQuery::RayVsCylinder(const Vec3& vRayOrigin, const Vec3& vRayDir,
    const Vec3& vBase, f32_t fRadius, f32_t fHeight, f32_t& outT)
{
    const f32_t fDX = vRayDir.x;
    const f32_t fDZ = vRayDir.z;
    const f32_t fLenSq = fDX * fDX + fDZ * fDZ;
    const f32_t fRadiusSq = fRadius * fRadius;

    if (fLenSq < 1e-8f)
    {
        const f32_t fOx = vRayOrigin.x - vBase.x;
        const f32_t fOz = vRayOrigin.z - vBase.z;
        if (fOx * fOx + fOz * fOz > fRadiusSq)
            return false;

        if (std::fabs(vRayDir.y) < 1e-6f)
            return false;

        const f32_t fTopT = (vBase.y + fHeight - vRayOrigin.y) / vRayDir.y;
        const f32_t fBottomT = (vBase.y - vRayOrigin.y) / vRayDir.y;
        f32_t fT = (fTopT < fBottomT) ? fTopT : fBottomT;
        if (fT < 0.f)
            fT = (fTopT > fBottomT) ? fTopT : fBottomT;
        if (fT < 0.f)
            return false;

        outT = fT;
        return true;
    }

    const f32_t fOx = vRayOrigin.x - vBase.x;
    const f32_t fOz = vRayOrigin.z - vBase.z;
    const f32_t fB = fOx * fDX + fOz * fDZ;
    const f32_t fC = fOx * fOx + fOz * fOz - fRadiusSq;
    const f32_t fDisc = fB * fB - fLenSq * fC;
    if (fDisc < 0.f)
        return false;

    const f32_t fSqrt = static_cast<f32_t>(std::sqrt(fDisc));
    const f32_t fCandidates[2] = {
        (-fB - fSqrt) / fLenSq,
        (-fB + fSqrt) / fLenSq
    };

    for (f32_t fT : fCandidates)
    {
        if (fT < 0.f)
            continue;

        const f32_t fY = vRayOrigin.y + fT * vRayDir.y;
        if (fY >= vBase.y && fY <= vBase.y + fHeight)
        {
            outT = fT;
            return true;
        }
    }

    return false;
}

bool_t GameplayQuery::TryFindHoverTarget(CWorld& world, EntityID player,
    eTeam playerTeam, const Vec3& vRayOrigin,
    const Vec3& vRayDir, f32_t fChampionHitRadius, f32_t fChampionHitHeight,
    EntityID& outEntity, eTeam& outTeam)
{
    outEntity = NULL_ENTITY;
    outTeam = eTeam::TEAM_END;

    f32_t fBestT = (std::numeric_limits<f32_t>::max)();

    const auto TestCylinder = [&](EntityID entity, const Vec3& vPos, f32_t fRadius,
        f32_t fHeight, eTeam team)
        {
            if (entity == player)
                return;
            if (!IsAttackTargetLocallySelectable(world, player, entity))
                return;

            const Vec3 vBase = { vPos.x, vPos.y - fHeight * 0.5f, vPos.z };
            f32_t fT = 0.f;
            if (RayVsCylinder(vRayOrigin, vRayDir, vBase, fRadius, fHeight, fT) && fT < fBestT)
            {
                fBestT = fT;
                outEntity = entity;
                outTeam = team;
            }
        };

    world.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID entity, ChampionComponent& champion, TransformComponent& tf)
        {
            TestCylinder(entity, tf.GetPosition(), fChampionHitRadius, fChampionHitHeight, champion.team);
        }
    );

    world.ForEach<MinionStateComponent, TransformComponent>(
        [&](EntityID entity, MinionStateComponent& minion, TransformComponent& tf)
        {
            TestCylinder(entity, tf.GetPosition(), 0.6f, 2.f, minion.team);
        }
    );

    world.ForEach<StructureComponent, TransformComponent>(
        [&](EntityID entity, StructureComponent& structure, TransformComponent& tf)
        {
            TestCylinder(entity, tf.GetPosition(), 2.f, 6.f, static_cast<eTeam>(structure.team));
        }
    );

    world.ForEach<JungleComponent, TransformComponent>(
        [&](EntityID entity, JungleComponent&, TransformComponent& tf)
        {
            TestCylinder(entity, tf.GetPosition(), GameplayStateQuery::ResolveGameplayRadius(world, entity), 3.f, eTeam::Neutral);
        }
    );

    world.ForEach<VisionSensorComponent, TransformComponent>(
        [&](EntityID entity, VisionSensorComponent& ward, TransformComponent& tf)
        {
            TestCylinder(entity, tf.GetPosition(), 0.45f, 1.8f, static_cast<eTeam>(ward.ownerTeam));
        }
    );

    return outEntity != NULL_ENTITY;
}

bool_t GameplayQuery::IsAttackTargetLocallySelectable(
    CWorld& world,
    EntityID player,
    EntityID target)
{
    if (target == NULL_ENTITY ||
        target == player ||
        !world.IsAlive(target) ||
        !world.HasComponent<TransformComponent>(target) ||
        !world.HasComponent<TargetableTag>(target) ||
        UI::IsKalistaCarried(world, target))
    {
        return false;
    }

    if (world.HasComponent<HealthComponent>(target))
    {
        const auto& health = world.GetComponent<HealthComponent>(target);
        if (health.bIsDead || health.fCurrent <= 0.f)
            return false;
    }

    if (!world.HasComponent<ViegoSoulComponent>(target) &&
        !GameplayStateQuery::CanBeTargetedBy(world, player, target))
    {
        return false;
    }

    if (world.HasComponent<ViegoSoulComponent>(target))
    {
        if (!world.HasComponent<ChampionComponent>(player))
            return false;

        const auto& playerChampion = world.GetComponent<ChampionComponent>(player);
        const auto& soul = world.GetComponent<ViegoSoulComponent>(target);
        if (playerChampion.id != eChampion::VIEGO ||
            playerChampion.team != soul.eligibleTeam)
            return false;
    }

    return true;
}

bool_t GameplayQuery::IsValidAttackTarget(
    CWorld& world,
    EntityID player,
    EntityID target,
    eTeam playerTeam)
{
    if (!IsAttackTargetLocallySelectable(world, player, target))
        return false;

    const eTeam targetTeam = GameplayStateQuery::ResolveEntityTeam(world, target);
    return targetTeam != eTeam::TEAM_END && targetTeam != playerTeam;
}

f32_t GameplayQuery::ResolvePlayerAttackRange(
    CWorld& world,
    EntityID player,
    eChampion playerChampion,
    f32_t fFallbackRange)
{
    if (player != NULL_ENTITY && world.HasComponent<FormOverrideComponent>(player))
    {
        const auto& form = world.GetComponent<FormOverrideComponent>(player);
        const u8_t basicAttackSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        if (form.bActive &&
            form.skillChampion != eChampion::NONE &&
            form.skillChampion != eChampion::END &&
            (form.skillSlotMask & static_cast<u8_t>(1u << basicAttackSlot)) != 0u)
        {
            const StatComponent stolenStat =
                ChampionGameDataDB::BuildStat(form.skillChampion);
            if (stolenStat.attackRange > 0.f)
                return stolenStat.attackRange;
        }
    }

    if (player != NULL_ENTITY && world.HasComponent<StatComponent>(player))
    {
        const auto& stat = world.GetComponent<StatComponent>(player);
        if (stat.attackRange > 0.f)
            return stat.attackRange;

        const StatComponent statFallback = ChampionGameDataDB::BuildStat(stat.championId);
        if (statFallback.attackRange > 0.f)
            return statFallback.attackRange;
    }

    const StatComponent championFallback = ChampionGameDataDB::BuildStat(playerChampion);
    return championFallback.attackRange > 0.f ? championFallback.attackRange : fFallbackRange;
}

f32_t GameplayQuery::ResolveEffectiveAttackRange(
    CWorld& world,
    EntityID player,
    EntityID target,
    eChampion playerChampion,
    f32_t fFallbackRange)
{
    return ResolvePlayerAttackRange(world, player, playerChampion, fFallbackRange) +
        GameplayStateQuery::ResolveGameplayRadius(world, player) +
        GameplayStateQuery::ResolveGameplayRadius(world, target);
}

f32_t GameplayQuery::ResolveAttackRangePreviewRadius(
    CWorld& world,
    EntityID player,
    eChampion playerChampion,
    f32_t fFallbackRange,
    bool_t bNetworkAuthoritativeGameplay)
{
    if (!bNetworkAuthoritativeGameplay)
        return fFallbackRange;

    constexpr f32_t fPreviewTargetRadius = 1.2f;
    return ResolvePlayerAttackRange(world, player, playerChampion, fFallbackRange) +
        GameplayStateQuery::ResolveGameplayRadius(world, player) +
        fPreviewTargetRadius;
}

EntityID GameplayQuery::FindAttackTargetNearCursor(
    CWorld& world,
    EntityID player,
    EntityID hoveredEntity,
    eTeam playerTeam,
    const Vec3& vCursorGround,
    bool_t bAttackMoveClick,
    eChampion playerChampion,
    f32_t fFallbackRange)
{
    if (IsValidAttackTarget(world, player, hoveredEntity, playerTeam))
        return hoveredEntity;

    if (player == NULL_ENTITY || !world.HasComponent<TransformComponent>(player))
        return NULL_ENTITY;

    const Vec3 vPlayerPos = world.GetComponent<TransformComponent>(player).GetPosition();

    EntityID entityBestTarget = NULL_ENTITY;
    f32_t fBestScore = (std::numeric_limits<f32_t>::max)();

    const auto TryCandidate = [&](EntityID entity, TransformComponent& tf)
        {
            if (!IsValidAttackTarget(world, player, entity, playerTeam))
                return;

            const Vec3 vTargetPos = tf.GetPosition();
            const f32_t fTargetRadius = GameplayStateQuery::ResolveGameplayRadius(world, entity);
            const f32_t fPickRadius = fTargetRadius + 0.85f;
            const f32_t fCursorDistSq = WintersMath::DistanceSqXZ(vCursorGround, vTargetPos);
            const bool_t bCursorNearTarget = fCursorDistSq <= fPickRadius * fPickRadius;

            const f32_t fEffectiveRange =
                ResolveEffectiveAttackRange(world, player, entity, playerChampion, fFallbackRange);
            const bool_t bInsideAttackRange =
                WintersMath::DistanceSqXZ(vPlayerPos, vTargetPos) <= fEffectiveRange * fEffectiveRange;

            if (!bCursorNearTarget && !(bAttackMoveClick && bInsideAttackRange))
                return;

            if (fCursorDistSq < fBestScore)
            {
                fBestScore = fCursorDistSq;
                entityBestTarget = entity;
            }
        };

    world.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID entity, ChampionComponent&, TransformComponent& tf)
        {
            TryCandidate(entity, tf);
        });

    world.ForEach<MinionStateComponent, TransformComponent>(
        [&](EntityID entity, MinionStateComponent&, TransformComponent& tf)
        {
            TryCandidate(entity, tf);
        });

    world.ForEach<StructureComponent, TransformComponent>(
        [&](EntityID entity, StructureComponent&, TransformComponent& tf)
        {
            TryCandidate(entity, tf);
        });

    world.ForEach<JungleComponent, TransformComponent>(
        [&](EntityID entity, JungleComponent&, TransformComponent& tf)
        {
            TryCandidate(entity, tf);
        });

    return entityBestTarget;
}
