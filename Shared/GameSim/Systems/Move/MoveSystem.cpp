#include "Shared/GameSim/Systems/Move/MoveSystem.h"

#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/KalistaPassiveDashComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr f32_t kAvoidancePadding = 0.05f;
    constexpr f32_t kPathWaypointArriveRadius = 0.12f;
    constexpr f32_t kYawHalfTurnTolerance = 0.35f;
    constexpr u8_t kUnknownSpatialTeam = 0xffu;

    bool_t IsYawHalfTurn(f32_t yawDelta)
    {
        return std::fabs(std::fabs(NormalizeChampionVisualYaw(yawDelta)) - WintersMath::kPi) <=
            kYawHalfTurnTolerance;
    }

    f32_t ResolveEntityVisualYawOffset(
        CWorld& world,
        EntityID entity,
        const StatComponent& stat)
    {
        if (entity != NULL_ENTITY && world.HasComponent<JungleComponent>(entity))
            return WintersMath::kPi;

        return ChampionGameDataDB::ResolveVisualYawOffset(stat.championId);
    }

    f32_t ResolveVisualYawFromDirection(const Vec3& direction, f32_t visualYawOffset)
    {
        return WintersMath::NormalizeRadians(
            WintersMath::YawFromDirectionXZ(direction, visualYawOffset));
    }

    f32_t ResolveVisualYawNear(
        const Vec3& direction,
        f32_t referenceYaw,
        f32_t visualYawOffset)
    {
        return MakeChampionVisualYawNear(
            ResolveVisualYawFromDirection(direction, visualYawOffset),
            referenceYaw);
    }

    Vec3 GameplayForwardFromVisualYaw(f32_t yaw, f32_t visualYawOffset)
    {
        const f32_t gameplayYaw = yaw - visualYawOffset;
        return Vec3{ std::sinf(gameplayYaw), 0.f, std::cosf(gameplayYaw) };
    }

    void ClearMoveFacingOverride(MoveTargetComponent& moveTarget)
    {
        moveTarget.facingTarget = {};
        moveTarget.facingDirection = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = 0;
        moveTarget.bHasFacingTarget = false;
    }

    void ClearMoveRuntimeTarget(MoveTargetComponent& moveTarget)
    {
        moveTarget.bHasTarget = false;
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        ClearMoveFacingOverride(moveTarget);
    }

    bool_t IsMoveYawCandidateOpposedToFacingIntent(
        const Vec3& facingDirection,
        const Vec3& candidateDirection)
    {
        const f32_t intentLenSq =
            facingDirection.x * facingDirection.x +
            facingDirection.z * facingDirection.z;
        const f32_t candidateLenSq =
            candidateDirection.x * candidateDirection.x +
            candidateDirection.z * candidateDirection.z;
        if (intentLenSq <= 0.0001f || candidateLenSq <= 0.0001f)
            return false;

        const f32_t dot =
            facingDirection.x * candidateDirection.x +
            facingDirection.z * candidateDirection.z;
        const f32_t minDot =
            -0.10f * std::sqrt(intentLenSq * candidateLenSq);
        return dot < minDot;
    }

    Vec3 ResolveMoveYawDirection(
        MoveTargetComponent& moveTarget,
        const Vec3& origin,
        const Vec3& moveDirection,
        bool_t& outUsedFacingIntent,
        bool_t& outMoveYawOpposed,
        bool_t& outFacingLocked)
    {
        outUsedFacingIntent = false;
        outMoveYawOpposed = false;
        outFacingLocked = false;

        if (!moveTarget.bHasFacingTarget)
            return moveDirection;

        Vec3 facingDirection =
            WintersMath::NormalizeXZ(moveTarget.facingDirection, Vec3{}, 0.0001f);
        if (facingDirection.x == 0.f && facingDirection.z == 0.f)
        {
            ClearMoveFacingOverride(moveTarget);
            return moveDirection;
        }

        outMoveYawOpposed = IsMoveYawCandidateOpposedToFacingIntent(
            facingDirection,
            moveDirection);

        outFacingLocked = moveTarget.facingLockTicks > 0;
        if (outFacingLocked)
            --moveTarget.facingLockTicks;

        // Move path steers position only; the accepted input intent owns facing until the move ends.
        outUsedFacingIntent = true;
        return facingDirection;
    }

    struct MoveBlockerSnap
    {
        EntityID id = NULL_ENTITY;
        Vec3 pos{};
        f32_t radius = 0.5f;
        u8_t team = kUnknownSpatialTeam;
    };

    bool_t IsMoveBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::JungleMob;
    }

    f32_t ResolveAgentRadius(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<SpatialAgentComponent>(entity))
            return (std::max)(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);

        return 0.5f;
    }

    std::vector<MoveBlockerSnap> CollectMoveBlockers(CWorld& world)
    {
        std::vector<MoveBlockerSnap> blockers;
        const auto entities = DeterministicEntityIterator<SpatialAgentComponent>::CollectSorted(world);
        blockers.reserve(entities.size());

        for (EntityID entity : entities)
        {
            if (!world.HasComponent<TransformComponent>(entity))
                continue;

            const auto& agent = world.GetComponent<SpatialAgentComponent>(entity);
            if (!IsMoveBlockingKind(agent.kind))
                continue;

            if (world.HasComponent<HealthComponent>(entity))
            {
                const auto& health = world.GetComponent<HealthComponent>(entity);
                if (health.bIsDead || health.fCurrent <= 0.f)
                    continue;
            }

            MoveBlockerSnap snap{};
            snap.id = entity;
            snap.pos = world.GetComponent<TransformComponent>(entity).GetPosition();
            snap.radius = (std::max)(0.2f, agent.radius);
            snap.team = agent.team;
            blockers.push_back(snap);
        }

        return blockers;
    }

    bool_t IsSeparatingCandidate(
        const Vec3& vCurrent, const Vec3& vCandidate, const Vec3& vBlockerPos, f32_t minDistSq)
    {
        const f32_t currentDistSq = WintersMath::DistanceSqXZ(vCurrent, vBlockerPos);

        if (currentDistSq >= minDistSq)
            return false;

        const f32_t candidateDistSq = WintersMath::DistanceSqXZ(vCandidate, vBlockerPos);
        return candidateDistSq > currentDistSq + 0.0001f;
    }

    bool_t IsCandidateClear(
        const std::vector<MoveBlockerSnap>& blockers,
        EntityID self,
        u8_t selfTeam,
        const Vec3& current,
        const Vec3& candidate,
        f32_t radius)
    {
        for (const MoveBlockerSnap& blocker : blockers)
        {
            if (blocker.id == self)
                continue;

            const f32_t minDist = radius + blocker.radius + kAvoidancePadding;
            const f32_t minDistSq = minDist * minDist;
            const f32_t candidateDistSq = WintersMath::DistanceSqXZ(candidate, blocker.pos);
            if (candidateDistSq >= minDistSq)
                continue;

            const bool_t bSameTeam =
                selfTeam != kUnknownSpatialTeam &&
                blocker.team == selfTeam &&
                blocker.team != static_cast<u8_t>(eTeam::Neutral);
            if (bSameTeam)
            {
                const f32_t currentDistSq = WintersMath::DistanceSqXZ(current, blocker.pos);
                if (candidateDistSq + 0.0001f >= currentDistSq)
                    continue;
            }

            if (!IsSeparatingCandidate(current, candidate, blocker.pos, minDistSq))
                return false;
        }

        return true;
    }

    Vec3 ResolveAvoidedDirection(
        const std::vector<MoveBlockerSnap>& blockers,
        EntityID self,
        u8_t selfTeam,
        const Vec3& pos,
        const Vec3& desired,
        f32_t step,
        f32_t radius,
        const IWalkableQuery* pWalkable)
    {
        static constexpr f32_t kAngles[] = {
            0.f,
            0.610865f, -0.610865f,
            1.22173f, -1.22173f,
            1.570796f, -1.570796f
        };

        for (const f32_t angle : kAngles)
        {
            const Vec3 dir = WintersMath::RotateXZ(desired, angle);
            const Vec3 candidate{
                pos.x + dir.x * step,
                pos.y,
                pos.z + dir.z * step
            };

            if (!IsCandidateClear(blockers, self, selfTeam, pos, candidate, radius))
                continue;

            if (pWalkable && !pWalkable->SegmentWalkableXZ(pos, candidate, radius))
                continue;

            return dir;
        }

        return Vec3{};
    }

    bool_t IsActionStateLocked(
        CWorld& world,
        EntityID entity,
        const StatComponent& stat,
        const ActionStateComponent& action,
        const TickContext& tc)
    {
        const auto currentAction = static_cast<eActionStateId>(action.actionId);
        if (!IsReplicatedGameplayAction(currentAction))
            return false;

        if (tc.tickIndex < action.startTick)
            return false;

        const u8_t slot = SkillSlotFromActionId(currentAction);
        const u8_t stage = action.stage == 0u ? 1u : action.stage;
        if (stat.championId == eChampion::JAX &&
            currentAction == eActionStateId::SkillE &&
            slot == static_cast<u8_t>(eSkillSlot::E) &&
            stage == 1u)
        {
            return false;
        }

        const u64_t lockTicks = GameplayDefinitionQuery::ResolveSkillActionLockTicks(
            world,
            entity,
            tc,
            stat.championId,
            slot,
            stage);
        return (tc.tickIndex - action.startTick) < lockTicks;
    }

    bool_t IsActionStateLocked(
        CWorld& world,
        EntityID entity,
        const StatComponent& stat,
        const ActionStateComponent* pAction,
        const TickContext& tc)
    {
        if (!pAction)
            return false;

        const auto currentAction = static_cast<eActionStateId>(pAction->actionId);
        if (currentAction == eActionStateId::BasicAttack)
        {
            if (!world.HasComponent<CombatActionComponent>(entity))
                return false;

            const auto& action = world.GetComponent<CombatActionComponent>(entity);
            if (action.eKind != eCombatActionKind::BasicAttack)
                return false;
            if (action.bImpactIssued || tc.tickIndex >= action.uImpactTick)
                return false;

            return true;
        }

        return IsActionStateLocked(world, entity, stat, *pAction, tc);
    }

    const ActionStateComponent* FindActionState(CWorld& world, EntityID entity)
    {
        return world.HasComponent<ActionStateComponent>(entity)
            ? &world.GetComponent<ActionStateComponent>(entity)
            : nullptr;
    }

    bool_t IsMovePose(CWorld& world, EntityID entity, ePoseStateId poseId)
    {
        return world.HasComponent<PoseStateComponent>(entity) &&
            world.GetComponent<PoseStateComponent>(entity).poseId ==
                static_cast<u16_t>(poseId);
    }

    void SetMovePose(
        CWorld& world,
        EntityID entity,
        ePoseStateId nextPose,
        const TickContext& tc)
    {
        SetPoseState(world, entity, nextPose, tc.tickIndex);
    }
}

void CMoveSystem::Execute(CWorld& world, const TickContext& tc)
{
    const auto entities = DeterministicEntityIterator<MoveTargetComponent>::CollectSorted(world);
    const std::vector<MoveBlockerSnap> blockers = CollectMoveBlockers(world);

    for (EntityID entity : entities)
    {
        auto& moveTarget = world.GetComponent<MoveTargetComponent>(entity);
        if (!moveTarget.bHasTarget)
        {
            ClearMoveFacingOverride(moveTarget);
            if (world.HasComponent<StatComponent>(entity) &&
                world.HasComponent<PoseStateComponent>(entity))
            {
                const auto& stat = world.GetComponent<StatComponent>(entity);
                const ActionStateComponent* pAction = FindActionState(world, entity);
                if (IsMovePose(world, entity, ePoseStateId::Run) &&
                    !IsActionStateLocked(world, entity, stat, pAction, tc))
                {
                    SetMovePose(world, entity, ePoseStateId::Idle, tc);
                }
            }
            continue;
        }

        if (world.HasComponent<KalistaPassiveDashComponent>(entity))
        {
            const auto& dash = world.GetComponent<KalistaPassiveDashComponent>(entity);
            if (dash.bPending || dash.bActive)
            {
                ClearMoveRuntimeTarget(moveTarget);
                continue;
            }
        }

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& health = world.GetComponent<HealthComponent>(entity);
            if (health.bIsDead)
            {
                ClearMoveRuntimeTarget(moveTarget);
                continue;
            }
        }

        if (!world.HasComponent<TransformComponent>(entity) ||
            !world.HasComponent<StatComponent>(entity))
        {
            continue;
        }

        auto& transform = world.GetComponent<TransformComponent>(entity);
        const auto& stat = world.GetComponent<StatComponent>(entity);
        const ActionStateComponent* pAction = FindActionState(world, entity);
        EnsurePoseState(world, entity);

        const Vec3 pos = transform.GetLocalPosition();

        if (!GameplayStateQuery::CanMove(world, entity))
        {
            ClearMoveRuntimeTarget(moveTarget);
            if (!IsActionStateLocked(world, entity, stat, pAction, tc))
                SetMovePose(world, entity, ePoseStateId::Idle, tc);
            continue;
        }

        if (IsActionStateLocked(world, entity, stat, pAction, tc))
            continue;

        const f32_t step =
            stat.moveSpeed *
            GameplayStateQuery::GetMoveSpeedMultiplier(world, entity) *
            tc.fDt;
        const bool_t bCurrentWalkable = !tc.pWalkable || tc.pWalkable->IsWalkableXZ(pos);

        if (!bCurrentWalkable && moveTarget.pathCount == 0)
        {
            ClearMoveRuntimeTarget(moveTarget);
            if (!IsActionStateLocked(world, entity, stat, pAction, tc))
                SetMovePose(world, entity, ePoseStateId::Idle, tc);
            continue;
        }

        Vec3 activeTarget = moveTarget.target;
        f32_t activeArriveRadius = moveTarget.arriveRadius;
        if (moveTarget.pathCount > 0)
        {
            while (moveTarget.pathIndex < moveTarget.pathCount)
            {
                const Vec3 waypoint = moveTarget.pathWaypoints[moveTarget.pathIndex];
                const bool_t bFinalWaypoint = (moveTarget.pathIndex + 1u) >= moveTarget.pathCount;
                const f32_t waypointRadius = bFinalWaypoint
                    ? moveTarget.arriveRadius
                    : kPathWaypointArriveRadius;
                if (WintersMath::DistanceSqXZ(pos, waypoint) > waypointRadius * waypointRadius)
                {
                    activeTarget = waypoint;
                    activeArriveRadius = waypointRadius;
                    break;
                }

                ++moveTarget.pathIndex;
            }

            if (moveTarget.pathIndex >= moveTarget.pathCount)
            {
                ClearMoveRuntimeTarget(moveTarget);
                if (!IsActionStateLocked(world, entity, stat, pAction, tc))
                    SetMovePose(world, entity, ePoseStateId::Idle, tc);
                continue;
            }
        }

        const Vec3 to{
            activeTarget.x - pos.x,
            0.f,
            activeTarget.z - pos.z
        };

        const f32_t dist = std::sqrt((to.x * to.x) + (to.z * to.z));
        if (dist <= activeArriveRadius)
        {
            if (moveTarget.pathCount > 0 && moveTarget.pathIndex + 1u < moveTarget.pathCount)
            {
                ++moveTarget.pathIndex;
                continue;
            }
            else
            {
                ClearMoveRuntimeTarget(moveTarget);
            }

            if (!IsActionStateLocked(world, entity, stat, pAction, tc))
                SetMovePose(world, entity, ePoseStateId::Idle, tc);
            continue;
        }

        const f32_t advance = (std::min)(step, dist);
        const f32_t invDist = 1.f / dist;
        const f32_t radius = ResolveAgentRadius(world, entity);
        const u8_t selfTeam = world.HasComponent<SpatialAgentComponent>(entity)
            ? world.GetComponent<SpatialAgentComponent>(entity).team
            : kUnknownSpatialTeam;

        Vec3 dir{
            to.x * invDist,
            0.f,
            to.z * invDist
        };
        dir = ResolveAvoidedDirection(
            blockers,
            entity,
            selfTeam,
            pos,
            dir,
            advance,
            radius,
            bCurrentWalkable ? tc.pWalkable : nullptr);

        const f32_t dirLenSq = dir.x * dir.x + dir.z * dir.z;
        if (dirLenSq <= 0.0001f)
            continue;

        const Vec3 next{
            pos.x + dir.x * advance,
            pos.y,
            pos.z + dir.z * advance
        };

        Vec3 resolvedNext = next;
        bool_t bSegmentClamped = false;
        if (tc.pWalkable)
        {
            if (!tc.pWalkable->TryClampMoveSegmentXZ(pos, next, radius, resolvedNext))
            {
                ClearMoveRuntimeTarget(moveTarget);
                if (!IsActionStateLocked(world, entity, stat, pAction, tc))
                    SetMovePose(world, entity, ePoseStateId::Idle, tc);
                continue;
            }

            bSegmentClamped = WintersMath::DistanceSqXZ(resolvedNext, next) > 0.0001f;
        }

        if (tc.pWalkable)
        {
            f32_t surfaceY = 0.f;
            if (tc.pWalkable->TrySampleHeight(resolvedNext.x, resolvedNext.z, surfaceY))
                resolvedNext.y = surfaceY;
        }

        transform.SetPosition(resolvedNext);

        const bool_t bHadFacingIntent = moveTarget.bHasFacingTarget;
        const Vec3 traceFacingTarget = moveTarget.facingTarget;
        const Vec3 traceFacingDirection = moveTarget.facingDirection;
        const u32_t traceFacingSequenceNum = moveTarget.facingSequenceNum;
        const u16_t traceFacingLockTicks = moveTarget.facingLockTicks;
        bool_t bUsedFacingIntent = false;
        bool_t bMoveYawOpposed = false;
        bool_t bFacingLocked = false;
        const Vec3 yawDirection = ResolveMoveYawDirection(
            moveTarget,
            pos,
            dir,
            bUsedFacingIntent,
            bMoveYawOpposed,
            bFacingLocked);
        const Vec3 rot = transform.GetRotation();
        const f32_t visualYawOffset =
            ResolveEntityVisualYawOffset(world, entity, stat);
        const f32_t resolvedYaw =
            ResolveVisualYawNear(yawDirection, rot.y, visualYawOffset);
        transform.SetRotation({
            rot.x,
            resolvedYaw,
            rot.z });

        static u32_t s_moveSystemYawTraceCount = 0;
        const f32_t yawDelta = NormalizeChampionVisualYaw(resolvedYaw - rot.y);
        const f32_t moveYaw =
            ResolveVisualYawFromDirection(dir, visualYawOffset);
        const f32_t intentYaw =
            (traceFacingDirection.x != 0.f || traceFacingDirection.z != 0.f)
                ? ResolveVisualYawFromDirection(traceFacingDirection, visualYawOffset)
                : moveYaw;
        const Vec3 activeDirection =
            WintersMath::DirectionXZ(pos, activeTarget, Vec3{});
        const f32_t activeYaw =
            (activeDirection.x != 0.f || activeDirection.z != 0.f)
                ? ResolveVisualYawFromDirection(activeDirection, visualYawOffset)
                : moveYaw;
        const f32_t appliedVsMove =
            NormalizeChampionVisualYaw(resolvedYaw - moveYaw);
        const f32_t appliedVsIntent =
            NormalizeChampionVisualYaw(resolvedYaw - intentYaw);
        const f32_t appliedVsActive =
            NormalizeChampionVisualYaw(resolvedYaw - activeYaw);
        const f32_t intentVsMoveDot =
            traceFacingDirection.x * dir.x + traceFacingDirection.z * dir.z;
        const Vec3 previousForward = GameplayForwardFromVisualYaw(rot.y, visualYawOffset);
        const Vec3 appliedForward = GameplayForwardFromVisualYaw(resolvedYaw, visualYawOffset);
        const f32_t prevVsAppliedDot =
            previousForward.x * appliedForward.x + previousForward.z * appliedForward.z;
        const bool_t bHalfTurn = IsYawHalfTurn(yawDelta);
        if ((bHadFacingIntent || bUsedFacingIntent || bMoveYawOpposed ||
            std::fabs(yawDelta) > 1.0f) &&
            s_moveSystemYawTraceCount < 512u)
        {
            char msg[1536]{};
            sprintf_s(
                msg,
                "[YawTrace][MoveSystem] tick=%llu entity=%u champion=%u source=%s hadFacing=%u pathYawOpposed=%u lockTicks=%u facingDir=(%.3f,%.3f) moveDir=(%.3f,%.3f) activeDir=(%.3f,%.3f) pos=(%.3f,%.3f,%.3f) active=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f) facing=(%.3f,%.3f,%.3f) pathIndex=%u pathCount=%u seq=%u prevYaw=%.4f appliedYaw=%.4f moveYaw=%.4f intentYaw=%.4f activeYaw=%.4f yawDelta=%.4f halfTurn=%u prevF=(%.3f,%.3f) appliedF=(%.3f,%.3f) prevVsAppliedDot=%.4f appliedVsMove=%.4f appliedVsIntent=%.4f appliedVsActive=%.4f intentVsMoveDot=%.4f\n",
                static_cast<unsigned long long>(tc.tickIndex),
                static_cast<u32_t>(entity),
                static_cast<u32_t>(stat.championId),
                bUsedFacingIntent
                    ? (bFacingLocked ? "intent-lock" : "intent")
                    : "move",
                bHadFacingIntent ? 1u : 0u,
                bMoveYawOpposed ? 1u : 0u,
                static_cast<u32_t>(traceFacingLockTicks),
                traceFacingDirection.x,
                traceFacingDirection.z,
                dir.x,
                dir.z,
                activeDirection.x,
                activeDirection.z,
                pos.x,
                pos.y,
                pos.z,
                activeTarget.x,
                activeTarget.y,
                activeTarget.z,
                moveTarget.target.x,
                moveTarget.target.y,
                moveTarget.target.z,
                traceFacingTarget.x,
                traceFacingTarget.y,
                traceFacingTarget.z,
                static_cast<u32_t>(moveTarget.pathIndex),
                static_cast<u32_t>(moveTarget.pathCount),
                traceFacingSequenceNum,
                rot.y,
                resolvedYaw,
                moveYaw,
                intentYaw,
                activeYaw,
                yawDelta,
                bHalfTurn ? 1u : 0u,
                previousForward.x,
                previousForward.z,
                appliedForward.x,
                appliedForward.z,
                prevVsAppliedDot,
                appliedVsMove,
                appliedVsIntent,
                appliedVsActive,
                intentVsMoveDot);
            WintersOutputAIDebugStringA(msg);
            ++s_moveSystemYawTraceCount;
        }

        if (bSegmentClamped)
        {
            ClearMoveRuntimeTarget(moveTarget);
            if (!IsActionStateLocked(world, entity, stat, pAction, tc))
                SetMovePose(world, entity, ePoseStateId::Idle, tc);
            continue;
        }

        if (!IsActionStateLocked(world, entity, stat, pAction, tc))
            SetMovePose(world, entity, ePoseStateId::Run, tc);
    }
}
