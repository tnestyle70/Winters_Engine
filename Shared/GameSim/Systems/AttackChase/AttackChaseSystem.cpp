#include "Shared/GameSim/Systems/AttackChase/AttackChaseSystem.h"

#include "Shared/GameSim/Core/Debug/SimDebugOutput.h"
#include "Shared/GameSim/Components/AttackChaseComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr f32_t kAttackChaseRepathIntervalSec = 0.10f;
    constexpr f32_t kAttackChaseArriveSlack = 0.05f;

    bool_t IsAliveForAttackChase(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;

        if (!world.HasComponent<HealthComponent>(entity))
            return true;

        const auto& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    f32_t ResolveAttackRange(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<StatComponent>(entity))
        {
            const f32_t range = world.GetComponent<StatComponent>(entity).attackRange;
            if (range > 0.f)
                return range;
        }

        return 5.5f;
    }

    eChampion ResolveChampion(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<StatComponent>(entity))
            return world.GetComponent<StatComponent>(entity).championId;
        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).id;
        return eChampion::NONE;
    }

    f32_t ResolveYawOrZero(eChampion champion, const Vec3& direction)
    {
        if (direction.x == 0.f && direction.z == 0.f)
            return 0.f;
        return ResolveChampionVisualYawFromDirection(champion, direction);
    }

    bool_t IsBasicAttackReady(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<SkillStateComponent>(entity))
            return true;

        return world.GetComponent<SkillStateComponent>(entity)
            .slots[static_cast<u8_t>(eSkillSlot::BasicAttack)]
            .cooldownRemaining <= 0.f;
    }

    bool_t CanChaseTarget(CWorld& world, EntityID entity,
        const AttackChaseComponent& chase)
    {
        if (GameplayStateQuery::CanBeTargetedBy(world, entity, chase.target))
            return true;

        if (chase.commandKind != static_cast<u8_t>(eCommandKind::BasicAttack) ||
            !world.HasComponent<ViegoSoulComponent>(chase.target))
        {
            return false;
        }

        return world.GetComponent<ViegoSoulComponent>(chase.target).eligibleViego == entity;
    }

    void ClearMoveTarget(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};
    }

    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        moveTarget.blockedMoveTicks = 0;
        moveTarget.bestMoveDistance = -1.f;
    }

    bool_t HasActiveMoveTarget(CWorld& world, EntityID entity)
    {
        return world.HasComponent<MoveTargetComponent>(entity) &&
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget;
    }

    bool_t SetChaseMoveTarget(CWorld& world, const TickContext& tc, EntityID entity,
        const Vec3& selfPos, const Vec3& target, f32_t effectiveRange)
    {
        auto& moveTarget = world.HasComponent<MoveTargetComponent>(entity)
            ? world.GetComponent<MoveTargetComponent>(entity)
            : world.AddComponent<MoveTargetComponent>(entity, MoveTargetComponent{});

        Vec3 resolvedTarget = target;
        ClearMovePath(moveTarget);
        if (tc.pWalkable)
        {
            Vec3 waypoints[kMovePathMaxWaypoints]{};
            u16_t waypointCount = 0;
            if (!tc.pWalkable->TryBuildMovePath(
                selfPos,
                target,
                waypoints,
                kMovePathMaxWaypoints,
                waypointCount,
                resolvedTarget))
            {
                moveTarget.bHasTarget = false;
                return false;
            }

            moveTarget.pathCount = waypointCount;
            for (u16_t i = 0; i < waypointCount; ++i)
                moveTarget.pathWaypoints[i] = waypoints[i];
        }

        moveTarget.target = resolvedTarget;
        moveTarget.arriveRadius =
            std::max(MoveTargetComponent{}.arriveRadius,
                effectiveRange - kAttackChaseArriveSlack);
        moveTarget.pathIndex = 0;
        moveTarget.bHasTarget = true;

        static u32_t s_chaseMoveTargetTraceCount = 0;
        if (false && s_chaseMoveTargetTraceCount < 1024u)
        {
            const eChampion champion = ResolveChampion(world, entity);
            const Vec3 targetDir = WintersMath::DirectionXZ(selfPos, target, Vec3{});
            const Vec3 resolvedDir = WintersMath::DirectionXZ(selfPos, resolvedTarget, Vec3{});
            const Vec3 path0 = moveTarget.pathCount > 0
                ? moveTarget.pathWaypoints[0]
                : Vec3{};
            const Vec3 path0Dir = moveTarget.pathCount > 0
                ? WintersMath::DirectionXZ(selfPos, path0, Vec3{})
                : Vec3{};
            const Vec3 facingDir =
                WintersMath::NormalizeXZ(moveTarget.facingDirection, Vec3{}, 0.0001f);
            const f32_t path0VsTargetDot =
                path0Dir.x * targetDir.x + path0Dir.z * targetDir.z;
            const f32_t facingVsTargetDot =
                facingDir.x * targetDir.x + facingDir.z * targetDir.z;
            char msg[1024]{};
            sprintf_s(
                msg,
                "[YawTrace][AttackChaseMoveTarget] tick=%llu entity=%u champion=%u self=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) path0=(%.3f,%.3f,%.3f) effectiveRange=%.3f arriveRadius=%.3f pathIndex=%u pathCount=%u hasFacing=%u lockTicks=%u facingSeq=%u targetDir=(%.3f,%.3f) resolvedDir=(%.3f,%.3f) path0Dir=(%.3f,%.3f) facingDir=(%.3f,%.3f) targetYaw=%.4f resolvedYaw=%.4f path0Yaw=%.4f facingYaw=%.4f path0VsTargetDot=%.4f facingVsTargetDot=%.4f\n",
                static_cast<unsigned long long>(tc.tickIndex),
                static_cast<u32_t>(entity),
                static_cast<u32_t>(champion),
                selfPos.x,
                selfPos.y,
                selfPos.z,
                target.x,
                target.y,
                target.z,
                resolvedTarget.x,
                resolvedTarget.y,
                resolvedTarget.z,
                path0.x,
                path0.y,
                path0.z,
                effectiveRange,
                moveTarget.arriveRadius,
                static_cast<u32_t>(moveTarget.pathIndex),
                static_cast<u32_t>(moveTarget.pathCount),
                moveTarget.bHasFacingTarget ? 1u : 0u,
                static_cast<u32_t>(moveTarget.facingLockTicks),
                moveTarget.facingSequenceNum,
                targetDir.x,
                targetDir.z,
                resolvedDir.x,
                resolvedDir.z,
                path0Dir.x,
                path0Dir.z,
                facingDir.x,
                facingDir.z,
                ResolveYawOrZero(champion, targetDir),
                ResolveYawOrZero(champion, resolvedDir),
                ResolveYawOrZero(champion, path0Dir),
                ResolveYawOrZero(champion, facingDir),
                path0VsTargetDot,
                facingVsTargetDot);
            WintersOutputAIDebugStringA(msg);
            ++s_chaseMoveTargetTraceCount;
        }
        return true;
    }

    GameCommand MakeBasicAttackCommand(const TickContext& tc,
        EntityID issuer, EntityID target, u32_t sequenceNum,
        const Vec3& issuerPos, const Vec3& targetPos)
    {
        GameCommand cmd{};
        cmd.kind = eCommandKind::BasicAttack;
        cmd.issuerEntity = issuer;
        cmd.issuedAtTick = tc.tickIndex;
        cmd.sequenceNum = sequenceNum;
        cmd.slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        cmd.targetEntity = target;
        cmd.groundPos = targetPos;
        cmd.direction = WintersMath::DirectionXZ(issuerPos, targetPos);
        return cmd;
    }

    GameCommand MakeChasedCastCommand(const TickContext& tc,
        EntityID issuer, const AttackChaseComponent& chase,
        const Vec3& issuerPos, const Vec3& targetPos)
    {
        GameCommand cmd{};
        cmd.kind = eCommandKind::CastSkill;
        cmd.issuerEntity = issuer;
        cmd.issuedAtTick = tc.tickIndex;
        cmd.sequenceNum = chase.sequenceNum;
        cmd.slot = chase.slot;
        cmd.targetEntity = chase.target;
        cmd.groundPos = chase.groundPos;
        cmd.direction = chase.direction;
        cmd.itemId = chase.itemId;

        const f32_t dirLenSq =
            cmd.direction.x * cmd.direction.x +
            cmd.direction.z * cmd.direction.z;
        if (dirLenSq <= 0.0001f)
            cmd.direction = WintersMath::DirectionXZ(issuerPos, targetPos);

        return cmd;
    }
}

void CAttackChaseSystem::Execute(CWorld& world, const TickContext& tc,
    std::vector<GameCommand>& outCommands)
{
    const auto entities =
        DeterministicEntityIterator<AttackChaseComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        if (!world.HasComponent<AttackChaseComponent>(entity))
            continue;

        auto& chase = world.GetComponent<AttackChaseComponent>(entity);
        const bool_t bSkillChase =
            chase.commandKind == static_cast<u8_t>(eCommandKind::CastSkill);
        const bool_t bCanExecute = bSkillChase
            ? GameplayStateQuery::CanCast(world, entity)
            : GameplayStateQuery::CanAttack(world, entity);
        const bool_t bCanTarget = CanChaseTarget(world, entity, chase);

        if (!chase.bActive ||
            !IsAliveForAttackChase(world, entity) ||
            !IsAliveForAttackChase(world, chase.target) ||
            !bCanExecute ||
            !bCanTarget ||
            !world.HasComponent<TransformComponent>(entity) ||
            !world.HasComponent<TransformComponent>(chase.target))
        {
            static u32_t s_chaseClearTraceCount = 0;
            if (false && s_chaseClearTraceCount < 256u)
            {
                char msg[384]{};
                sprintf_s(
                    msg,
                    "[YawTrace][AttackChaseClear] tick=%llu entity=%u target=%u seq=%u active=%u canAttack=%u canTarget=%u hasSelfTf=%u hasTargetTf=%u\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(chase.target),
                    chase.sequenceNum,
                    chase.bActive ? 1u : 0u,
                    bCanExecute ? 1u : 0u,
                    bCanTarget ? 1u : 0u,
                    world.HasComponent<TransformComponent>(entity) ? 1u : 0u,
                    world.HasComponent<TransformComponent>(chase.target) ? 1u : 0u);
                WintersOutputAIDebugStringA(msg);
                ++s_chaseClearTraceCount;
            }
            ClearMoveTarget(world, entity);
            world.RemoveComponent<AttackChaseComponent>(entity);
            continue;
        }

        const Vec3 selfPos =
            world.GetComponent<TransformComponent>(entity).GetLocalPosition();
        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(chase.target).GetLocalPosition();

        const f32_t effectiveRange = (chase.effectiveRange > 0.f)
            ? chase.effectiveRange
            : ResolveAttackRange(world, entity) +
                GameplayStateQuery::ResolveGameplayRadius(world, entity) +
                GameplayStateQuery::ResolveGameplayRadius(world, chase.target);
        const bool_t bInRange =
            WintersMath::DistanceSqXZ(selfPos, targetPos) <= effectiveRange * effectiveRange;
        const bool_t bHasClearSegment =
            !tc.pWalkable ||
            GameplayStateQuery::IsAttackSegmentGateExemptTarget(world, chase.target) ||
            tc.pWalkable->SegmentWalkableXZ(
                selfPos,
                targetPos,
                GameplayStateQuery::ResolveGameplayRadius(world, entity));

        if (bInRange && bHasClearSegment)
        {
            ClearMoveTarget(world, entity);
            if (bSkillChase)
            {
                outCommands.push_back(MakeChasedCastCommand(
                    tc, entity, chase, selfPos, targetPos));
                world.RemoveComponent<AttackChaseComponent>(entity);
            }
            else if (IsBasicAttackReady(world, entity))
            {
                static u32_t s_chaseAttackTraceCount = 0;
                if (false && s_chaseAttackTraceCount < 256u)
                {
                    const eChampion champion = ResolveChampion(world, entity);
                    const Vec3 targetDir = WintersMath::DirectionXZ(selfPos, targetPos, Vec3{});
                    char msg[512]{};
                    sprintf_s(
                        msg,
                        "[YawTrace][AttackChaseInRange] tick=%llu entity=%u champion=%u target=%u seq=%u self=(%.3f,%.3f,%.3f) targetPos=(%.3f,%.3f,%.3f) effectiveRange=%.3f targetDir=(%.3f,%.3f) targetYaw=%.4f\n",
                        static_cast<unsigned long long>(tc.tickIndex),
                        static_cast<u32_t>(entity),
                        static_cast<u32_t>(champion),
                        static_cast<u32_t>(chase.target),
                        chase.sequenceNum,
                        selfPos.x,
                        selfPos.y,
                        selfPos.z,
                        targetPos.x,
                        targetPos.y,
                        targetPos.z,
                        effectiveRange,
                        targetDir.x,
                        targetDir.z,
                        ResolveYawOrZero(champion, targetDir));
                    WintersOutputAIDebugStringA(msg);
                    ++s_chaseAttackTraceCount;
                }
                outCommands.push_back(MakeBasicAttackCommand(
                    tc, entity, chase.target, chase.sequenceNum, selfPos, targetPos));
                if (!chase.bSustain)
                    world.RemoveComponent<AttackChaseComponent>(entity);
            }
            continue;
        }

        chase.repathTimer -= tc.fDt;
        if (chase.repathTimer <= 0.f || !HasActiveMoveTarget(world, entity))
        {
            const f32_t moveStopRange = bInRange && !bHasClearSegment
                ? 0.f
                : effectiveRange;
            SetChaseMoveTarget(world, tc, entity, selfPos, targetPos, moveStopRange);
            chase.repathTimer = kAttackChaseRepathIntervalSec;
        }
    }
}
