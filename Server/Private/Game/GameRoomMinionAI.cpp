#include "Game/GameRoom.h"

#include "GameRoomInternal.h"

#include "Game/ServerMinionFlowField.h"
#include "Game/ServerMinionTuning.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/SpatialIndex.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "Manager/Navigation/Pathfinder.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace
{
    bool_t IsMoveBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::JungleMob ||
            kind == eSpatialKind::Turret ||
            kind == eSpatialKind::Inhibitor ||
            kind == eSpatialKind::Nexus;
    }

    bool_t IsStaticMoveBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::Turret ||
            kind == eSpatialKind::Inhibitor ||
            kind == eSpatialKind::Nexus;
    }

    f32_t ResolveAgentRadius(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<SpatialAgentComponent>(entity))
            return (std::max)(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);

        return 0.5f;
    }

    f32_t ResolveServerMinionAttackRange(
        CWorld& world,
        EntityID self,
        EntityID target,
        const MinionStateComponent& state)
    {
        return state.attackRange +
            ResolveAgentRadius(world, self) +
            ResolveAgentRadius(world, target);
    }

    void FaceServerMinionTowardDirection(TransformComponent& transform, const Vec3& vDirection)
    {
        const f32_t fLenSq =
            vDirection.x * vDirection.x +
            vDirection.z * vDirection.z;
        if (fLenSq <= 0.0001f)
            return;

        Vec3 vRotation = transform.GetRotation();
        vRotation.y = static_cast<f32_t>(std::atan2(-vDirection.x, -vDirection.z));
        transform.SetRotation(vRotation);
    }

    void FaceServerMinionTowardTarget(
        TransformComponent& transform,
        const Vec3& vSource,
        const Vec3& vTarget)
    {
        FaceServerMinionTowardDirection(
            transform,
            Vec3{ vTarget.x - vSource.x, 0.f, vTarget.z - vSource.z });
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

    bool_t IsAvoidanceBlockedByEntity(
        CWorld& world,
        EntityID self,
        u8_t selfTeam,
        const Vec3& current,
        const Vec3& candidate,
        f32_t radius,
        EntityID other)
    {
        constexpr u8_t kUnknownSpatialTeam = 0xffu;
        constexpr f32_t kAvoidancePadding = 0.05f;

        if (other == self ||
            !world.HasComponent<TransformComponent>(other) ||
            !world.HasComponent<SpatialAgentComponent>(other))
        {
            return false;
        }

        const auto& agent = world.GetComponent<SpatialAgentComponent>(other);
        if (!IsMoveBlockingKind(agent.kind))
            return false;

        if (world.HasComponent<HealthComponent>(other))
        {
            const auto& health = world.GetComponent<HealthComponent>(other);
            if (health.bIsDead || health.fCurrent <= 0.f)
                return false;
        }

        const Vec3 otherPos = world.GetComponent<TransformComponent>(other).GetPosition();
        const f32_t minDist = radius + (std::max)(0.2f, agent.radius) + kAvoidancePadding;
        const f32_t minDistSq = minDist * minDist;
        const f32_t candidateDistSq = WintersMath::DistanceSqXZ(candidate, otherPos);
        if (candidateDistSq >= minDistSq)
            return false;

        const bool_t bSameTeam =
            selfTeam != kUnknownSpatialTeam &&
            agent.team == selfTeam &&
            agent.team != static_cast<u8_t>(eTeam::Neutral);
        if (bSameTeam)
        {
            if (agent.kind == eSpatialKind::Minion)
                return false;

            const f32_t currentDistSq = WintersMath::DistanceSqXZ(current, otherPos);
            if (candidateDistSq + 0.0001f >= currentDistSq)
                return false;
        }

        return !IsSeparatingCandidate(current, candidate, otherPos, minDistSq);
    }

    bool_t IsAvoidanceCandidateClear(
        CWorld& world,
        EntityID self,
        const Vec3& current,
        const Vec3& candidate,
        f32_t radius)
    {
        constexpr u8_t kUnknownSpatialTeam = 0xffu;
        const u8_t selfTeam = world.HasComponent<SpatialAgentComponent>(self)
            ? world.GetComponent<SpatialAgentComponent>(self).team
            : kUnknownSpatialTeam;

        if (CSpatialIndex* pSpatial = world.Get_SpatialIndex())
        {
            constexpr f32_t kAvoidancePadding = 0.05f;
            constexpr f32_t kStaleIndexMargin = 0.25f;
            const f32_t candidateStep =
                std::sqrt(WintersMath::DistanceSqXZ(current, candidate));
            const f32_t queryRadius =
                radius + kAvoidancePadding + candidateStep + kStaleIndexMargin;
            const u32_t moveBlockerMask =
                SpatialMask(eSpatialKind::JungleMob) |
                SpatialMask(eSpatialKind::Turret) |
                SpatialMask(eSpatialKind::Inhibitor) |
                SpatialMask(eSpatialKind::Nexus);

            std::vector<EntityID> candidates;
            candidates.reserve(16);
            pSpatial->QueryRadius(candidate, queryRadius, moveBlockerMask, 0u, candidates);
            for (EntityID other : candidates)
            {
                if (IsAvoidanceBlockedByEntity(
                    world, self, selfTeam, current, candidate, radius, other))
                {
                    return false;
                }
            }

            return true;
        }

        const auto entities = DeterministicEntityIterator<SpatialAgentComponent>::CollectSorted(world);
        for (EntityID other : entities)
        {
            if (IsAvoidanceBlockedByEntity(
                world, self, selfTeam, current, candidate, radius, other))
            {
                return false;
            }
        }

        return true;
    }

    constexpr u8_t kServerMinionRoleRanged = 1u;
    constexpr f32_t kServerMinionRangedProjectileSpeed = 14.f;
    constexpr f32_t kServerMinionRangedProjectileHitRadius = 0.45f;
    constexpr f32_t kServerMinionRangedProjectileStartForwardOffset = 0.45f;
    constexpr f32_t kServerMinionRangedProjectileStartHeight = 0.85f;
    constexpr f32_t kServerMinionRangedProjectileMaxDistancePadding = 2.f;

    const char* ServerMinionDebugStateName(MinionStateComponent::State state)
    {
        switch (state)
        {
        case MinionStateComponent::Idle: return "Idle";
        case MinionStateComponent::LaneMove: return "LaneMove";
        case MinionStateComponent::Chase: return "Chase";
        case MinionStateComponent::Attack: return "Attack";
        case MinionStateComponent::Dead: return "Dead";
        default: return "Unknown";
        }
    }

    Engine::CNavGrid::Cell ResolveDebugCell(const Engine::CNavGrid* pGrid, const Vec3& pos)
    {
        if (!pGrid)
            return Engine::CNavGrid::Cell{ -1, -1 };

        return pGrid->WorldToCell(pos);
    }

    void OutputServerMinionPathDebug(
        const Engine::CNavGrid* pMoveGrid,
        const Engine::CNavGrid* pPathGrid,
        u64_t tickIndex,
        EntityID entity,
        const MinionStateComponent& state,
        const Vec3& vPos,
        const Vec3& vTarget,
        const Vec3& vResolvedTarget,
        u16_t pathCount,
        u32_t pathBuildBudget,
        bool_t bBuilt)
    {
        static u32_t s_minionPathLogCount = 0u;
        if (s_minionPathLogCount >= 256u)
            return;

        const Engine::CNavGrid::Cell posMoveCell = ResolveDebugCell(pMoveGrid, vPos);
        const Engine::CNavGrid::Cell posPathCell = ResolveDebugCell(pPathGrid, vPos);
        const Engine::CNavGrid::Cell targetMoveCell = ResolveDebugCell(pMoveGrid, vTarget);
        const Engine::CNavGrid::Cell targetPathCell = ResolveDebugCell(pPathGrid, vTarget);
        const Engine::CNavGrid::Cell resolvedMoveCell = ResolveDebugCell(pMoveGrid, vResolvedTarget);
        const Engine::CNavGrid::Cell resolvedPathCell = ResolveDebugCell(pPathGrid, vResolvedTarget);

        char msg[640]{};
        sprintf_s(
            msg,
            "[MinionMove][Path] tick=%llu entity=%u team=%u lane=%u result=%s "
            "pos=(%.2f,%.2f) moveCell=(%d,%d) pathCell=(%d,%d) "
            "target=(%.2f,%.2f) targetMoveCell=(%d,%d) targetPathCell=(%d,%d) "
            "resolved=(%.2f,%.2f) resolvedMoveCell=(%d,%d) resolvedPathCell=(%d,%d) pathCount=%u budget=%u\n",
            static_cast<unsigned long long>(tickIndex),
            static_cast<u32_t>(entity),
            static_cast<u32_t>(state.team),
            static_cast<u32_t>(state.lane),
            bBuilt ? "built" : "failed",
            vPos.x,
            vPos.z,
            posMoveCell.x,
            posMoveCell.y,
            posPathCell.x,
            posPathCell.y,
            vTarget.x,
            vTarget.z,
            targetMoveCell.x,
            targetMoveCell.y,
            targetPathCell.x,
            targetPathCell.y,
            vResolvedTarget.x,
            vResolvedTarget.z,
            resolvedMoveCell.x,
            resolvedMoveCell.y,
            resolvedPathCell.x,
            resolvedPathCell.y,
            static_cast<u32_t>(pathCount),
            pathBuildBudget);
        OutputServerAITrace(msg);
        ++s_minionPathLogCount;
    }

    void OutputServerMinionStuckDebug(
        const char* pReason,
        const Engine::CNavGrid* pMoveGrid,
        u64_t tickIndex,
        EntityID entity,
        const MinionStateComponent& state,
        const Vec3& vPos,
        const Vec3& vGoal,
        const Vec3* pWaypoint)
    {
        static u32_t s_minionStuckLogCount = 0u;
        if (s_minionStuckLogCount >= 256u)
            return;

        const Engine::CNavGrid::Cell posCell = ResolveDebugCell(pMoveGrid, vPos);
        const Engine::CNavGrid::Cell goalCell = ResolveDebugCell(pMoveGrid, vGoal);
        const Engine::CNavGrid::Cell waypointCell = pWaypoint
            ? ResolveDebugCell(pMoveGrid, *pWaypoint)
            : Engine::CNavGrid::Cell{ -1, -1 };

        char msg[512]{};
        sprintf_s(
            msg,
            "[MinionMove][Stuck] tick=%llu entity=%u team=%u lane=%u state=%s reason=%s "
            "blocked=%u pos=(%.2f,%.2f) posCell=(%d,%d) "
            "goal=(%.2f,%.2f) goalCell=(%d,%d) "
            "path=%u/%u waypointCell=(%d,%d)\n",
            static_cast<unsigned long long>(tickIndex),
            static_cast<u32_t>(entity),
            static_cast<u32_t>(state.team),
            static_cast<u32_t>(state.lane),
            ServerMinionDebugStateName(state.current),
            pReason ? pReason : "unknown",
            static_cast<u32_t>(state.BlockedMoveFrames),
            vPos.x,
            vPos.z,
            posCell.x,
            posCell.y,
            vGoal.x,
            vGoal.z,
            goalCell.x,
            goalCell.y,
            static_cast<u32_t>(state.PathIndex),
            static_cast<u32_t>(state.PathCount),
            waypointCell.x,
            waypointCell.y);
        OutputServerAITrace(msg);
        ++s_minionStuckLogCount;
    }

    bool_t TryResolveServerMinionTargetCandidate(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        u8_t myLane,
        const Vec3& myPos,
        f32_t maxRange,
        EntityID candidate,
        Vec3& outPos,
        f32_t& outDistSq)
    {
        if (world.HasComponent<PracticeDummyTag>(candidate))
            return false;
        if (candidate == self || !IsAliveHealth(world, candidate))
            return false;
        if (!world.HasComponent<TransformComponent>(candidate))
            return false;
        if (world.HasComponent<MinionComponent>(candidate) &&
            world.GetComponent<MinionComponent>(candidate).laneType != myLane)
        {
            return false;
        }
        if (world.HasComponent<MinionStateComponent>(candidate) &&
            world.GetComponent<MinionStateComponent>(candidate).lane != myLane)
        {
            return false;
        }
        if (world.HasComponent<StructureComponent>(candidate) &&
            !world.HasComponent<TargetableTag>(candidate))
        {
            return false;
        }
        if (world.HasComponent<StructureComponent>(candidate))
        {
            const StructureComponent& structure = world.GetComponent<StructureComponent>(candidate);
            if (structure.lane != myLane && structure.lane != kLaneBase)
                return false;
        }

        eTeam targetTeam = eTeam::Neutral;
        if (!TryResolveCombatTeam(world, candidate, targetTeam))
            return false;
        if (targetTeam == myTeam || targetTeam == eTeam::Neutral)
            return false;
        if (!GameplayStateQuery::CanBeTargetedBy(world, self, candidate))
            return false;

        outPos = world.GetComponent<TransformComponent>(candidate).GetPosition();
        outDistSq = WintersMath::DistanceSqXZ(myPos, outPos);

        f32_t resolvedMaxRange = maxRange;
        if (world.HasComponent<StructureComponent>(candidate))
        {
            resolvedMaxRange += ResolveAgentRadius(world, candidate) +
                ServerMinionTuning::kStructureAcquireRangePadding;
        }

        const f32_t maxRangeSq = resolvedMaxRange * resolvedMaxRange;
        return outDistSq <= maxRangeSq;
    }

    bool_t TryResolveServerMinionTargetPriority(
        CWorld& world,
        EntityID entity,
        i32_t& outPriority)
    {
        if (world.HasComponent<MinionComponent>(entity))
        {
            outPriority = 0;
            return true;
        }
        if (world.HasComponent<StructureComponent>(entity))
        {
            outPriority = 1;
            return true;
        }
        if (world.HasComponent<ChampionComponent>(entity))
        {
            outPriority = 2;
            return true;
        }
        return false;
    }

    bool_t IsServerRangedMinion(const MinionStateComponent& state)
    {
        return state.type == kServerMinionRoleRanged;
    }

    eProjectileKind ResolveServerMinionRangedProjectileKind(eTeam team)
    {
        return team == eTeam::Red
            ? eProjectileKind::MinionRangedBasicRed
            : eProjectileKind::MinionRangedBasicBlue;
    }

    EntityID FindClosestEnemyCombatTarget(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        u8_t myLane,
        const Vec3& myPos,
        f32_t maxRange)
    {
        EntityID best = NULL_ENTITY;
        i32_t bestPriority = std::numeric_limits<i32_t>::max();
        const f32_t maxRangeSq = maxRange * maxRange;
        f32_t bestDistSq = maxRangeSq;

        auto tryTarget = [&](EntityID entity, i32_t priority)
        {
            Vec3 pos{};
            f32_t distSq = 0.f;
            if (!TryResolveServerMinionTargetCandidate(
                world,
                self,
                myTeam,
                myLane,
                myPos,
                maxRange,
                entity,
                pos,
                distSq))
            {
                return;
            }

            if (priority < bestPriority || (priority == bestPriority && distSq < bestDistSq))
            {
                bestPriority = priority;
                bestDistSq = distSq;
                best = entity;
            }
        };

        if (CSpatialIndex* pSpatial = world.Get_SpatialIndex())
        {
            const u32_t targetMask =
                SpatialMask(eSpatialKind::Minion) |
                SpatialMask(eSpatialKind::Turret) |
                SpatialMask(eSpatialKind::Inhibitor) |
                SpatialMask(eSpatialKind::Nexus) |
                SpatialMask(eSpatialKind::Champion);
            std::vector<EntityID> candidates;
            const f32_t queryRange =
                maxRange + ServerMinionTuning::kStructureAcquireRangePadding;
            candidates.reserve(64);
            pSpatial->QueryRadius(
                myPos,
                queryRange,
                targetMask,
                1u << TeamByte(myTeam),
                candidates);

            std::sort(candidates.begin(), candidates.end());
            candidates.erase(
                std::unique(candidates.begin(), candidates.end()),
                candidates.end());

            for (EntityID candidate : candidates)
            {
                i32_t priority = 0;
                if (TryResolveServerMinionTargetPriority(world, candidate, priority))
                    tryTarget(candidate, priority);
            }

            return best;
        }

        world.ForEach<MinionComponent>(
            std::function<void(EntityID, MinionComponent&)>(
                [&](EntityID entity, MinionComponent&) { tryTarget(entity, 0); }));
        world.ForEach<StructureComponent>(
            std::function<void(EntityID, StructureComponent&)>(
                [&](EntityID entity, StructureComponent&) { tryTarget(entity, 1); }));
        world.ForEach<ChampionComponent>(
            std::function<void(EntityID, ChampionComponent&)>(
                [&](EntityID entity, ChampionComponent&) { tryTarget(entity, 2); }));

        return best;
    }
}

void CGameRoom::Phase_ServerMinionWave(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame || !m_bGameplayObjectsSpawned)
        return;

    m_serverMinionWaves.TickWave(
        tc.tickIndex,
        [this](const CServerMinionWaveRuntime::SpawnRequest& request)
        {
            SpawnServerMinion(request.team, request.roleType, request.lane, request.pos);
        });
}

void CGameRoom::Phase_ServerMinionAI(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    const auto minions =
        DeterministicEntityIterator<MinionStateComponent>::CollectSorted(m_world);

    u32_t PathBuildBudget = ServerMinionTuning::kPathBuildBudgetPerTick;

    for (EntityID entity : minions)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<MinionStateComponent>(entity) ||
            !m_world.HasComponent<MinionComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& state = m_world.GetComponent<MinionStateComponent>(entity);
        auto& minion = m_world.GetComponent<MinionComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);

        if (m_world.HasComponent<HealthComponent>(entity))
        {
            auto& hp = m_world.GetComponent<HealthComponent>(entity);
            minion.hp = hp.fCurrent;
            minion.maxHp = hp.fMaximum;
            if (hp.bIsDead || hp.fCurrent <= 0.f)
            {
                if (state.current != MinionStateComponent::Dead)
                {
                    state.current = MinionStateComponent::Dead;
                    state.deathTimer = 1.2f;
                    StartReplicatedAnimation(m_world, entity, eNetAnimId::Death, tc);
                }
                else if (state.deathTimer > 0.f)
                {
                    state.deathTimer -= tc.fDt;
                }

                if (state.deathTimer <= 0.f)
                {
                    const NetEntityId netId = m_entityMap.ToNet(entity);
                    if (netId != NULL_NET_ENTITY)
                        m_entityMap.Unbind(netId);
                    m_world.DestroyEntity(entity);
                }
                continue;
            }
        }

        if (state.attackCooldown > 0.f)
        {
            state.attackCooldown -= tc.fDt;
            if (state.attackCooldown < 0.f)
                state.attackCooldown = 0.f;
        }
        if (state.targetScanCooldown > 0.f)
        {
            state.targetScanCooldown -= tc.fDt;
            if (state.targetScanCooldown < 0.f)
                state.targetScanCooldown = 0.f;
        }

        const Vec3 pos = transform.GetPosition();
        EntityID target = NULL_ENTITY;
        if (state.attackTargetId != NULL_ENTITY)
        {
            Vec3 targetPos{};
            f32_t targetDistSq = 0.f;
            if (TryResolveServerMinionTargetCandidate(
                m_world,
                entity,
                minion.team,
                state.lane,
                pos,
                state.sightRange,
                state.attackTargetId,
                targetPos,
                targetDistSq))
            {
                target = state.attackTargetId;
            }
            else
            {
                state.attackTargetId = NULL_ENTITY;
            }
        }

        if (target == NULL_ENTITY && state.targetScanCooldown <= 0.f)
        {
            target = FindClosestEnemyCombatTarget(
                m_world, entity, minion.team, state.lane, pos, state.sightRange);

            const f32_t scanInterval = state.targetScanInterval > 0.f
                ? state.targetScanInterval
                : ServerMinionTuning::kTargetScanIntervalSec;
            state.targetScanCooldown = scanInterval;
        }

        bool_t bMoved = false;
        if (target != NULL_ENTITY && m_world.HasComponent<TransformComponent>(target))
        {
            const Vec3 targetPos = m_world.GetComponent<TransformComponent>(target).GetPosition();
            const f32_t distSq = WintersMath::DistanceSqXZ(pos, targetPos);
            const f32_t effectiveAttackRange =
                ResolveServerMinionAttackRange(m_world, entity, target, state);
            const f32_t rangeSq = effectiveAttackRange * effectiveAttackRange;
            state.attackTargetId = target;

            if (distSq <= rangeSq)
            {
                state.current = MinionStateComponent::Attack;
                FaceServerMinionTowardTarget(transform, pos, targetPos);
                if (state.attackCooldown <= 0.f)
                {
                    eTeam sourceTeam = minion.team;
                    (void)TryResolveCombatTeam(m_world, entity, sourceTeam);

                    if (IsServerRangedMinion(state))
                    {
                        const Vec3 projectileDir = NormalizeXZOrForward(
                            Vec3{ targetPos.x - pos.x, 0.f, targetPos.z - pos.z },
                            sourceTeam);
                        const Vec3 projectileStart{
                            pos.x + projectileDir.x * kServerMinionRangedProjectileStartForwardOffset,
                            pos.y + kServerMinionRangedProjectileStartHeight,
                            pos.z + projectileDir.z * kServerMinionRangedProjectileStartForwardOffset
                        };

                        SkillProjectileComponent projectile{};
                        projectile.sourceEntity = entity;
                        projectile.targetEntity = target;
                        projectile.sourceTeam = sourceTeam;
                        projectile.kind = ResolveServerMinionRangedProjectileKind(sourceTeam);
                        projectile.skillId = static_cast<u16_t>(projectile.kind);
                        projectile.currentPos = projectileStart;
                        projectile.direction = projectileDir;
                        projectile.speed = kServerMinionRangedProjectileSpeed;
                        projectile.maxDistance =
                            effectiveAttackRange + kServerMinionRangedProjectileMaxDistancePadding;
                        projectile.hitRadius = kServerMinionRangedProjectileHitRadius;
                        projectile.damage = state.attackDamage;

                        const EntityID projectileEntity = m_world.CreateEntity();
                        m_world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

                        TransformComponent projectileTransform{};
                        projectileTransform.SetPosition(projectileStart);
                        m_world.AddComponent<TransformComponent>(
                            projectileEntity,
                            projectileTransform);
                    }
                    else
                    {
                        DamageRequest request{};
                        request.source = entity;
                        request.target = target;
                        request.sourceTeam = sourceTeam;
                        request.type = eDamageType::Physical;
                        request.flatAmount = state.attackDamage;
                        request.flags = DamageFlag_OnHit;
                        EnqueueDamageRequest(m_world, request);
                    }

                    state.attackCooldown = state.attackCooldownMax;
                    state.bHitFired = true;
                    StartReplicatedAnimation(m_world, entity, eNetAnimId::BasicAttack, tc);

                    static u32_t s_minionAttackLogCount = 0;
                    if (s_minionAttackLogCount < 128u)
                    {
                        const char* pTargetKind = m_world.HasComponent<StructureComponent>(target)
                            ? "structure"
                            : (m_world.HasComponent<MinionComponent>(target) ? "minion" : "champion");
                        char msg[288]{};
                        sprintf_s(msg,
                            "[MinionAI] attack tick=%llu entity=%u team=%u lane=%u target=%u targetKind=%s pos=(%.2f,%.2f,%.2f) targetPos=(%.2f,%.2f,%.2f)\n",
                            static_cast<unsigned long long>(tc.tickIndex),
                            static_cast<u32_t>(entity),
                            static_cast<u32_t>(minion.team),
                            static_cast<u32_t>(state.lane),
                            static_cast<u32_t>(target),
                            pTargetKind,
                            pos.x,
                            pos.y,
                            pos.z,
                            targetPos.x,
                            targetPos.y,
                            targetPos.z);
                        OutputServerAITrace(msg);
                        ++s_minionAttackLogCount;
                    }
                }
            }
            else
            {
                (void)TryMoveServerMinionToward(
                    entity,
                    state,
                    transform,
                    targetPos,
                    effectiveAttackRange,
                    tc,
                    PathBuildBudget,
                    bMoved,
                    MinionStateComponent::Chase);
            }
        }
        else
        {
            Vec3 laneTarget{};
            bool_t bHasLaneTarget = false;
            const u8_t waypointLane = ResolveServerWaypointLane(minion.team, state.lane);
            const u32_t waypointCount = GetServerMinionWaypointCount(minion.team, waypointLane);
            if (waypointCount > 0u && state.currentWaypoint < waypointCount)
            {
                laneTarget = GetServerMinionWaypoint(minion.team, waypointLane, state.currentWaypoint);
                const f32_t arriveSq = 0.8f * 0.8f;
                if (WintersMath::DistanceSqXZ(pos, laneTarget) <= arriveSq)
                {
                    ++state.currentWaypoint;
                    if (state.currentWaypoint < waypointCount)
                    {
                        laneTarget = GetServerMinionWaypoint(minion.team, waypointLane, state.currentWaypoint);
                        bHasLaneTarget = true;
                    }
                }
                else
                {
                    bHasLaneTarget = true;
                }
            }

            state.attackTargetId = NULL_ENTITY;
            if (bHasLaneTarget)
            {
                if (!TryMoveServerMinionByFlowFields(entity, state, transform, laneTarget, tc, bMoved))
                {
                    (void)TryMoveServerMinionToward(
                        entity,
                        state,
                        transform,
                        laneTarget,
                        0.8f,
                        tc,
                        PathBuildBudget,
                        bMoved,
                        MinionStateComponent::LaneMove);
                }
            }
            else
            {
                state.current = MinionStateComponent::Idle;
            }
        }

        auto& anim = m_world.HasComponent<NetAnimationComponent>(entity)
            ? m_world.GetComponent<NetAnimationComponent>(entity)
            : m_world.AddComponent<NetAnimationComponent>(entity, NetAnimationComponent{});
        if (bMoved)
        {
            anim.animId = static_cast<u16_t>(eNetAnimId::Run);
            anim.animPhaseFrame = static_cast<u16_t>(tc.tickIndex & 0xffffu);
        }
        else if (state.current != MinionStateComponent::Attack)
        {
            anim.animId = static_cast<u16_t>(eNetAnimId::Idle);
        }
    }
}

void CGameRoom::Phase_ServerMinionDepenetration(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    const auto minions =
        DeterministicEntityIterator<MinionStateComponent>::CollectSorted(m_world);

    for (EntityID entity : minions)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<MinionStateComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        MinionStateComponent& state = m_world.GetComponent<MinionStateComponent>(entity);
        if (state.current == MinionStateComponent::Dead)
            continue;

        TransformComponent& transform = m_world.GetComponent<TransformComponent>(entity);
        const Vec3 vPos = transform.GetPosition();
        const f32_t fStep = (std::max)(0.08f, state.moveSpeed * tc.fDt);

        Vec3 vResolved{};
        if (!TryResolveMinionDepenetrationStep(entity, vPos, fStep, tc, vResolved))
            continue;

        const Vec3 vActualMove{ vResolved.x - vPos.x, 0.f, vResolved.z - vPos.z };
        transform.SetPosition(vResolved);
        FaceServerMinionTowardDirection(transform, vActualMove);

        if (state.BlockedMoveFrames > 0u)
            state.BlockedMoveFrames = 0u;
    }
}

u32_t CGameRoom::ResolveServerMinionStartWaypoint(eTeam team, u8_t lane, const Vec3& vSpawnPos) const
{
    const u8_t waypointLane = ResolveServerWaypointLane(team, lane);
    const u32_t waypointCount = GetServerMinionWaypointCount(team, waypointLane);
    if (waypointCount <= 1u)
        return 0u;

    for (u32_t index = 1u; index < waypointCount; ++index)
    {
        Vec3 resolvedTarget{};
        if (TryResolveMoveTarget(
            vSpawnPos,
            GetServerMinionWaypoint(team, waypointLane, index),
            resolvedTarget))
        {
            return index;
        }
    }

    return 1u;
}

bool_t CGameRoom::TryResolveMinionDepenetrationStep(
    EntityID entity,
    const Vec3& vPos,
    f32_t fStep,
    const TickContext& tc,
    Vec3& vOutNext)
{
    if (!m_world.HasComponent<SpatialAgentComponent>(entity))
        return false;

    const SpatialAgentComponent& self = m_world.GetComponent<SpatialAgentComponent>(entity);
    const f32_t selfRadius = (std::max)(0.2f, self.radius);

    const u32_t blockerMask =
        SpatialMask(eSpatialKind::Minion) |
        SpatialMask(eSpatialKind::JungleMob) |
        SpatialMask(eSpatialKind::Turret) |
        SpatialMask(eSpatialKind::Inhibitor) |
        SpatialMask(eSpatialKind::Nexus);

    std::vector<EntityID> blockers;
    blockers.reserve(32);

    if (CSpatialIndex* pSpatial = m_world.Get_SpatialIndex())
    {
        pSpatial->QueryRadius(vPos, selfRadius + 3.0f, blockerMask, 0u, blockers);
    }
    else
    {
        blockers = DeterministicEntityIterator<SpatialAgentComponent>::CollectSorted(m_world);
    }

    Vec3 vPush{};
    u32_t blockerCount = 0u;
    u32_t staticCount = 0u;
    u32_t dynamicCount = 0u;
    u32_t softMinionCount = 0u;

    for (EntityID other : blockers)
    {
        if (other == entity ||
            !m_world.HasComponent<SpatialAgentComponent>(other) ||
            !m_world.HasComponent<TransformComponent>(other))
        {
            continue;
        }

        const SpatialAgentComponent& agent = m_world.GetComponent<SpatialAgentComponent>(other);
        const bool_t bSoftMinion = agent.kind == eSpatialKind::Minion;
        if (!bSoftMinion && !IsMoveBlockingKind(agent.kind))
            continue;

        if (m_world.HasComponent<HealthComponent>(other))
        {
            const HealthComponent& health = m_world.GetComponent<HealthComponent>(other);
            if (health.bIsDead || health.fCurrent <= 0.f)
                continue;
        }

        const Vec3 otherPos = m_world.GetComponent<TransformComponent>(other).GetPosition();
        Vec3 vAway{ vPos.x - otherPos.x, 0.f, vPos.z - otherPos.z };
        f32_t distSq = vAway.x * vAway.x + vAway.z * vAway.z;

        if (distSq <= 0.0001f)
        {
            const u32_t hash =
                static_cast<u32_t>(entity) * 73856093u ^
                static_cast<u32_t>(other) * 19349663u;
            vAway = Vec3{
                (hash & 1u) ? 1.f : -1.f,
                0.f,
                (hash & 2u) ? 1.f : -1.f };
            distSq = vAway.x * vAway.x + vAway.z * vAway.z;
        }

        const bool_t bStatic = IsStaticMoveBlockingKind(agent.kind);
        const f32_t otherRadius = (std::max)(0.2f, agent.radius);
        const f32_t padding = bStatic ? 0.20f : (bSoftMinion ? 0.f : 0.04f);
        const f32_t radiusScale = bSoftMinion
            ? ServerMinionTuning::kMinionSoftSeparationRadiusScale
            : 1.f;
        const f32_t minDist = (selfRadius + otherRadius) * radiusScale + padding;
        if (distSq >= minDist * minDist)
            continue;

        const f32_t dist = std::sqrt(distSq);
        const f32_t penetration = minDist - dist;
        const f32_t weight = bStatic
            ? 1.0f
            : (bSoftMinion ? ServerMinionTuning::kMinionSoftSeparationWeight : 0.55f);

        vPush.x += (vAway.x / dist) * penetration * weight;
        vPush.z += (vAway.z / dist) * penetration * weight;

        ++blockerCount;
        if (bStatic)
            ++staticCount;
        else if (bSoftMinion)
            ++softMinionCount;
        else
            ++dynamicCount;
    }

    const f32_t pushLenSq = vPush.x * vPush.x + vPush.z * vPush.z;
    if (pushLenSq <= 0.0001f)
        return false;

    const f32_t pushLen = std::sqrt(pushLenSq);
    const f32_t maxPushStep = softMinionCount > 0u && staticCount == 0u && dynamicCount == 0u
        ? ServerMinionTuning::kMinionSoftSeparationMaxStep
        : 0.35f;
    const f32_t pushStep = (std::min)((std::max)(0.08f, fStep), maxPushStep);
    const Vec3 vCandidate{
        vPos.x + (vPush.x / pushLen) * pushStep,
        vPos.y,
        vPos.z + (vPush.z / pushLen) * pushStep
    };

    Vec3 vGuarded = vCandidate;
    if (!TryClampMoveSegmentXZ(
        vPos,
        vCandidate,
        ServerMinionTuning::kMinionLaneClearanceRadius,
        vGuarded))
    {
        return false;
    }

    if (!TrySampleHeight(vGuarded.x, vGuarded.z, vGuarded.y))
        vGuarded.y = vPos.y;

    static u32_t s_minionDepenetrationLogCount = 0u;
    if (s_minionDepenetrationLogCount < 256u)
    {
        const Engine::CNavGrid::Cell posCell = ResolveDebugCell(m_pNavGrid.get(), vPos);
        const Engine::CNavGrid::Cell nextCell = ResolveDebugCell(m_pNavGrid.get(), vGuarded);

        char msg[512]{};
        sprintf_s(
            msg,
            "[MinionMove][Depenetrate] tick=%llu entity=%u posCell=(%d,%d) nextCell=(%d,%d) "
            "push=(%.3f,%.3f) blockers=%u static=%u dynamic=%u softMinion=%u from=(%.2f,%.2f) to=(%.2f,%.2f)\n",
            static_cast<unsigned long long>(tc.tickIndex),
            static_cast<u32_t>(entity),
            posCell.x,
            posCell.y,
            nextCell.x,
            nextCell.y,
            vPush.x,
            vPush.z,
            blockerCount,
            staticCount,
            dynamicCount,
            softMinionCount,
            vPos.x,
            vPos.z,
            vGuarded.x,
            vGuarded.z);
        OutputServerAITrace(msg);
        ++s_minionDepenetrationLogCount;
    }

    vOutNext = vGuarded;
    return WintersMath::DistanceSqXZ(vPos, vOutNext) > 0.0001f;
}

bool_t CGameRoom::TryMoveServerMinionToward(
    EntityID entity,
    MinionStateComponent& state,
    TransformComponent& transform,
    const Vec3& vTarget,
    f32_t fArriveRadius,
    TickContext& tc,
    u32_t& PathBuildBudget,
    bool_t& outMoved,
    MinionStateComponent::State moveState)
{
    state.PathRebuildCooldown = (std::max)(0.f, state.PathRebuildCooldown - tc.fDt);

    const Vec3 vPos = transform.GetPosition();
    const f32_t fResolvedArriveRadius = (std::max)(0.1f, fArriveRadius);
    if (WintersMath::DistanceSqXZ(vPos, vTarget) <= fResolvedArriveRadius * fResolvedArriveRadius)
    {
        state.current = moveState;
        state.PathCount = 0u;
        state.PathIndex = 0u;
        state.BlockedMoveFrames = 0u;
        return true;
    }

    Vec3 vMoveGoal = vTarget;
    if (!SegmentWalkableXZ(vPos, vTarget, ServerMinionTuning::kMinionLaneClearanceRadius))
    {
        const bool_t bTargetMoved =
            WintersMath::DistanceSqXZ(state.PathTarget, vTarget) >
            ServerMinionTuning::kPathTargetRefreshDistanceSq;

        const bool_t bNeedPath =
            state.PathCount == 0u ||
            state.PathIndex >= state.PathCount ||
            bTargetMoved ||
            state.BlockedMoveFrames >= ServerMinionTuning::kBlockedFramesBeforeRepath;

        if (bNeedPath &&
            state.PathRebuildCooldown <= 0.f &&
            PathBuildBudget > 0u)
        {
            Vec3 vResolvedTarget = vTarget;
            u16_t PathCount = 0u;
            const bool_t bPathBuilt = TryBuildServerMinionMovePath(
                vPos,
                vTarget,
                state.PathWaypoints,
                MinionStateComponent::PathMaxWaypoints,
                PathCount,
                vResolvedTarget);
            if (bPathBuilt)
            {
                state.PathTarget = vTarget;
                state.PathResolvedTarget = vResolvedTarget;
                state.PathCount = PathCount;
                state.PathIndex = 0u;
                state.BlockedMoveFrames = 0u;
            }

            const Engine::CNavGrid* pPathDebugGrid = m_pMinionLaneNavGrid ? m_pMinionLaneNavGrid.get() : m_pPathNavGrid.get();
            if (!pPathDebugGrid)
                pPathDebugGrid = m_pNavGrid.get();

            OutputServerMinionPathDebug(
                m_pNavGrid.get(),
                pPathDebugGrid,
                tc.tickIndex,
                entity,
                state,
                vPos,
                vTarget,
                vResolvedTarget,
                PathCount,
                PathBuildBudget,
                bPathBuilt);

            --PathBuildBudget;
            state.PathRebuildCooldown =
                moveState == MinionStateComponent::Chase
                ? ServerMinionTuning::kChasePathRebuildIntervalSec
                : ServerMinionTuning::kLanePathRebuildIntervalSec;
        }

        if (state.PathCount == 0u || state.PathIndex >= state.PathCount)
        {
            vMoveGoal = vTarget;
        }
        else
        {
            while (state.PathIndex + 1u < state.PathCount &&
                WintersMath::DistanceSqXZ(vPos, state.PathWaypoints[state.PathIndex]) <=
                ServerMinionTuning::kPathWaypointArriveRadius *
                ServerMinionTuning::kPathWaypointArriveRadius)
            {
                ++state.PathIndex;
            }

            vMoveGoal = state.PathWaypoints[state.PathIndex];
        }
    }
    else
    {
        state.PathCount = 0u;
        state.PathIndex = 0u;
    }

    const Vec3 vToGoal{ vMoveGoal.x - vPos.x, 0.f, vMoveGoal.z - vPos.z };
    if ((vToGoal.x * vToGoal.x + vToGoal.z * vToGoal.z) <= 0.0001f)
        return false;

    const Vec3 vDir = NormalizeXZOrForward(vToGoal, state.team);
    const f32_t fStep = state.moveSpeed * tc.fDt;

    Vec3 vNext{};
    if (!TryResolveMinionMoveStep(entity, vPos, vDir, fStep, tc, vNext))
    {
        Vec3 vDepenetrated{};
        if (TryResolveMinionDepenetrationStep(entity, vPos, fStep, tc, vDepenetrated))
        {
            const Vec3 vActualMove{ vDepenetrated.x - vPos.x, 0.f, vDepenetrated.z - vPos.z };
            transform.SetPosition(vDepenetrated);
            FaceServerMinionTowardDirection(transform, vActualMove);
            state.current = moveState;
            state.BlockedMoveFrames = 0u;
            outMoved = true;
            return true;
        }

        ++state.BlockedMoveFrames;

        const Vec3* pWaypoint =
            state.PathCount > 0u && state.PathIndex < state.PathCount
            ? &state.PathWaypoints[state.PathIndex]
            : nullptr;
        OutputServerMinionStuckDebug(
            "toward-resolve-failed",
            m_pNavGrid.get(),
            tc.tickIndex,
            entity,
            state,
            vPos,
            vMoveGoal,
            pWaypoint);
        return false;
    }

    const Vec3 vActualMove{ vNext.x - vPos.x, 0.f, vNext.z - vPos.z };
    transform.SetPosition(vNext);
    FaceServerMinionTowardDirection(transform, vActualMove);
    state.current = moveState;
    state.BlockedMoveFrames = 0u;
    outMoved = true;
    return true;
}

bool_t CGameRoom::TryMoveServerMinionByFlowFields(
    EntityID entity,
    MinionStateComponent& state,
    TransformComponent& transform,
    const Vec3& vLaneTarget,
    TickContext& tc,
    bool_t& outMoved)
{
    state.PathRebuildCooldown = (std::max)(0.f, state.PathRebuildCooldown - tc.fDt);

    if (state.PathCount > 0u && state.PathIndex < state.PathCount)
        return false;

    const Vec3 vPos = transform.GetPosition();
    const f32_t fPrevLaneDistSq = WintersMath::DistanceSqXZ(vPos, vLaneTarget);

    Vec3 vDir{};
    if (!m_serverMinionWaves.TryResolveFlowDirection(
        state.team,
        state.lane,
        vPos,
        vDir))
    {
        return false;
    }

    const f32_t fLenSq = vDir.x * vDir.x + vDir.z * vDir.z;
    if (fLenSq <= 0.0001f)
        return false;

    const f32_t fStep = state.moveSpeed * tc.fDt;
    Vec3 vNext{};
    if (!TryResolveMinionMoveStep(entity, vPos, vDir, fStep, tc, vNext))
    {
        Vec3 vDepenetrated{};
        if (TryResolveMinionDepenetrationStep(entity, vPos, fStep, tc, vDepenetrated))
        {
            const Vec3 vActualMove{ vDepenetrated.x - vPos.x, 0.f, vDepenetrated.z - vPos.z };
            transform.SetPosition(vDepenetrated);
            FaceServerMinionTowardDirection(transform, vActualMove);
            state.current = MinionStateComponent::LaneMove;
            state.BlockedMoveFrames = 0u;
            outMoved = true;
            return true;
        }

        ++state.BlockedMoveFrames;
        OutputServerMinionStuckDebug(
            "flow-resolve-failed",
            m_pNavGrid.get(),
            tc.tickIndex,
            entity,
            state,
            vPos,
            vLaneTarget,
            nullptr);
        return false;
    }

    const f32_t fNextLaneDistSq = WintersMath::DistanceSqXZ(vNext, vLaneTarget);
    const bool_t bProgressed =
        fNextLaneDistSq + ServerMinionTuning::kFlowFieldProgressSlackSq < fPrevLaneDistSq;

    if (!bProgressed)
    {
        ++state.BlockedMoveFrames;
        if (state.BlockedMoveFrames >= ServerMinionTuning::kFlowFieldStallFramesBeforePathFallback)
        {
            static u32_t s_flowFieldFallbackLogCount = 0;
            if (s_flowFieldFallbackLogCount < 64u)
            {
                char msg[256]{};
                sprintf_s(msg,
                    "[MinionAI] flow fallback reason=stall entity=%u team=%u lane=%u blocked=%u pos=(%.2f,%.2f) target=(%.2f,%.2f)\n",
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(state.team),
                    static_cast<u32_t>(state.lane),
                    static_cast<u32_t>(state.BlockedMoveFrames),
                    vPos.x,
                    vPos.z,
                    vLaneTarget.x,
                    vLaneTarget.z);
                OutputServerAITrace(msg);
                ++s_flowFieldFallbackLogCount;
            }

            return false;
        }
    }
    else
    {
        state.BlockedMoveFrames = 0u;
    }

    const Vec3 vActualMove{ vNext.x - vPos.x, 0.f, vNext.z - vPos.z };
    transform.SetPosition(vNext);
    FaceServerMinionTowardDirection(transform, vActualMove);
    state.current = MinionStateComponent::LaneMove;
    state.PathCount = 0u;
    state.PathIndex = 0u;
    outMoved = true;
    return true;
}

bool_t CGameRoom::TryResolveMinionMoveStep(
    EntityID entity,
    const Vec3& vPos,
    const Vec3& vDesiredDir,
    f32_t fStep,
    const TickContext& tc,
    Vec3& vOutNext)
{
    static constexpr f32_t kAngles[] =
    {
        0.f,
        0.610865f, -0.610865f,
        1.22173f, -1.22173f,
        1.570796f, -1.570796f
    };

    const f32_t fRadius = ResolveAgentRadius(m_world, entity);
    u32_t actorBlocked = 0u;
    u32_t navBlocked = 0u;

    for (const f32_t fAngle : kAngles)
    {
        const Vec3 vDir = WintersMath::RotateXZ(vDesiredDir, fAngle);
        const Vec3 vCandidate{
            vPos.x + vDir.x * fStep,
            vPos.y,
            vPos.z + vDir.z * fStep
        };

        if (!IsAvoidanceCandidateClear(m_world, entity, vPos, vCandidate, fRadius))
        {
            ++actorBlocked;
            continue;
        }

        Vec3 vGuarded = vCandidate;
        if (!TryClampMoveSegmentXZ(
            vPos,
            vCandidate,
            ServerMinionTuning::kMinionLaneClearanceRadius,
            vGuarded))
        {
            ++navBlocked;
            continue;
        }

        f32_t fSurfaceY = 0.f;
        if (TrySampleHeight(vGuarded.x, vGuarded.z, fSurfaceY))
            vGuarded.y = fSurfaceY;

        const bool_t bAngleAdjusted = std::fabs(fAngle) > 0.001f;
        const bool_t bClamped =
            WintersMath::DistanceSqXZ(vCandidate, vGuarded) > 0.0001f;
        if (bAngleAdjusted || bClamped || actorBlocked > 0u || navBlocked > 0u)
        {
            static u32_t s_minionResolveLogCount = 0u;
            if (s_minionResolveLogCount < 512u)
            {
                const Engine::CNavGrid::Cell posCell = ResolveDebugCell(m_pNavGrid.get(), vPos);
                const Engine::CNavGrid::Cell candidateCell = ResolveDebugCell(m_pNavGrid.get(), vCandidate);
                const Engine::CNavGrid::Cell guardedCell = ResolveDebugCell(m_pNavGrid.get(), vGuarded);

                char msg[640]{};
                sprintf_s(
                    msg,
                    "[MinionMove][Resolve] tick=%llu entity=%u posCell=(%d,%d) "
                    "desiredDir=(%.3f,%.3f) angle=%.3f selectedDir=(%.3f,%.3f) "
                    "candidate=(%.2f,%.2f) candidateCell=(%d,%d) "
                    "guarded=(%.2f,%.2f) guardedCell=(%d,%d) "
                    "actorBlocked=%u navBlocked=%u clamped=%u\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(entity),
                    posCell.x,
                    posCell.y,
                    vDesiredDir.x,
                    vDesiredDir.z,
                    fAngle,
                    vDir.x,
                    vDir.z,
                    vCandidate.x,
                    vCandidate.z,
                    candidateCell.x,
                    candidateCell.y,
                    vGuarded.x,
                    vGuarded.z,
                    guardedCell.x,
                    guardedCell.y,
                    actorBlocked,
                    navBlocked,
                    bClamped ? 1u : 0u);
                OutputServerAITrace(msg);
                ++s_minionResolveLogCount;
            }
        }

        vOutNext = vGuarded;
        return true;
    }

    static u32_t s_minionResolveFailLogCount = 0u;
    if (s_minionResolveFailLogCount < 256u)
    {
        const Engine::CNavGrid::Cell posCell = ResolveDebugCell(m_pNavGrid.get(), vPos);

        char msg[384]{};
        sprintf_s(
            msg,
            "[MinionMove][ResolveFail] tick=%llu entity=%u pos=(%.2f,%.2f) posCell=(%d,%d) "
            "desiredDir=(%.3f,%.3f) step=%.3f radius=%.2f actorBlocked=%u navBlocked=%u candidates=%u\n",
            static_cast<unsigned long long>(tc.tickIndex),
            static_cast<u32_t>(entity),
            vPos.x,
            vPos.z,
            posCell.x,
            posCell.y,
            vDesiredDir.x,
            vDesiredDir.z,
            fStep,
            fRadius,
            actorBlocked,
            navBlocked,
            static_cast<u32_t>(sizeof(kAngles) / sizeof(kAngles[0])));
        OutputServerAITrace(msg);
        ++s_minionResolveFailLogCount;
    }

    return false;
}

bool_t CGameRoom::TryBuildServerMinionMovePath(
    const Vec3& vStart,
    const Vec3& vGoal,
    Vec3* pOutWaypoints,
    u16_t maxWayPoints,
    u16_t& outCount,
    Vec3& outResolvedGoal) const
{
    outCount = 0u;
    outResolvedGoal = vGoal;
    if (!pOutWaypoints || maxWayPoints == 0u)
        return false;

    auto ApplyPathHeight = [&](Vec3& ioPos, f32_t fallbackY)
        {
            f32_t sampledY = fallbackY;
            if (!TrySampleHeight(ioPos.x, ioPos.z, sampledY))
            {
                ioPos.y = fallbackY;
                return;
            }

            if (std::fabs(sampledY - vStart.y) <= kMoveTargetMaxSurfaceDeltaY)
                ioPos.y = sampledY;
            else
                ioPos.y = fallbackY;
        };

    const Engine::CNavGrid* pGrid = m_pMinionLaneNavGrid ? m_pMinionLaneNavGrid.get() : m_pPathNavGrid.get();
    if (!pGrid)
        pGrid = m_pNavGrid.get();

    if (!pGrid)
    {
        ApplyPathHeight(outResolvedGoal, vStart.y);
        pOutWaypoints[outCount++] = outResolvedGoal;
        return true;
    }

    Engine::CNavGrid::Cell start = pGrid->WorldToCell(vStart);
    if (!pGrid->IsWalkable(start.x, start.y))
    {
        Engine::CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 16, nearestStart))
            return false;
        start = nearestStart;
    }

    const Engine::CNavGrid::Cell rawGoal = pGrid->WorldToCell(vGoal);
    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
        return false;

    if (pGrid->IsWalkable(rawGoal.x, rawGoal.y) &&
        pGrid->SegmentWalkable(vStart, vGoal, 0.f))
    {
        outResolvedGoal = vGoal;
        ApplyPathHeight(outResolvedGoal, vStart.y);
        pOutWaypoints[outCount++] = outResolvedGoal;
        return true;
    }

    Engine::CNavGrid::Cell resolvedGoal{};
    std::vector<Engine::CNavGrid::Cell> path{};
    if (!Engine::CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolvedGoal,
        &path))
    {
        return false;
    }

    outResolvedGoal = pGrid->CellToWorld(resolvedGoal.x, resolvedGoal.y);
    ApplyPathHeight(outResolvedGoal, vStart.y);

    Engine::CNavGrid::Cell lastAppended{ -1, -1 };
    auto AppendCell = [&](Engine::CNavGrid::Cell cell) -> bool_t
        {
            if (cell.x == lastAppended.x && cell.y == lastAppended.y)
                return true;
            if (outCount >= maxWayPoints)
                return false;

            Vec3 waypoint = pGrid->CellToWorld(cell.x, cell.y);
            ApplyPathHeight(waypoint, outResolvedGoal.y);

            pOutWaypoints[outCount++] = waypoint;
            lastAppended = cell;
            return true;
        };

    const std::vector<Engine::CNavGrid::Cell> smoothedPath = SmoothServerPathCells(*pGrid, path);
    if (smoothedPath.size() <= 1u)
        return AppendCell(resolvedGoal);

    for (size_t i = 1u; i < smoothedPath.size(); ++i)
    {
        if (!AppendCell(smoothedPath[i]))
            return false;
    }

    return outCount > 0u;
}

u32_t CGameRoom::GetServerMinionWaypointCount(eTeam team, u8_t lane) const
{
    return m_serverMinionWaves.GetWaypointCount(team, lane);
}

Vec3 CGameRoom::GetServerMinionWaypoint(eTeam team, u8_t lane, u32_t index) const
{
    return m_serverMinionWaves.GetWaypoint(team, lane, index);
}
