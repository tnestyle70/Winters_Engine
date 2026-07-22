#include "Game/GameRoom.h"

#include "GameRoomInternal.h"

#include "Game/ServerMinionFlowField.h"
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/Move/MinionSoftSeparationPolicy.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"

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
        return kind == eSpatialKind::NeutralUnit ||
            kind == eSpatialKind::Structure ||
            kind == eSpatialKind::Objective ||
            kind == eSpatialKind::Core;
    }

    bool_t IsStaticMoveBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::Structure ||
            kind == eSpatialKind::Objective ||
            kind == eSpatialKind::Core;
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

    void RecordServerMinionBlockedMove(MinionStateComponent& state)
    {
        if (state.BlockedMoveFrames < (std::numeric_limits<u8_t>::max)())
            ++state.BlockedMoveFrames;
    }

    void UpdateServerMinionMoveProgress(
        MinionStateComponent& state,
        const Vec3& vBefore,
        const Vec3& vAfter,
        const Vec3& vGoal)
    {
        const f32_t fSlackSq =
            ServerData::GetActiveLoLSpawnObjectDefinitionPack()
                .minionBehavior.flowFieldProgressSlackSq;
        const bool_t bProgressed =
            WintersMath::DistanceSqXZ(vAfter, vGoal) + fSlackSq <
            WintersMath::DistanceSqXZ(vBefore, vGoal);
        if (bProgressed)
            state.BlockedMoveFrames = 0u;
        else
            RecordServerMinionBlockedMove(state);
    }

    void ClearServerMinionPathRuntime(MinionStateComponent& state)
    {
        state.PathTarget = {};
        state.PathResolvedTarget = {};
        state.PathCount = 0u;
        state.PathIndex = 0u;
        state.PathRebuildCooldown = 0.f;
        state.BlockedMoveFrames = 0u;
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
            if (agent.kind == eSpatialKind::Unit)
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
                SpatialMask(eSpatialKind::NeutralUnit) |
                SpatialMask(eSpatialKind::Structure) |
                SpatialMask(eSpatialKind::Objective) |
                SpatialMask(eSpatialKind::Core);

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
        if (world.HasComponent<ViegoSoulComponent>(candidate))
            return false;
        if (candidate == self || !IsAliveHealth(world, candidate))
            return false;
        if (!world.HasComponent<TransformComponent>(candidate))
            return false;
        // lane is a preference, not a permanent exclusion boundary. Permanent
        // lane rejection makes nearby waves ignore one another at intersections.
        const bool_t bAnyLane = myLane == 0xffu;
        if (world.HasComponent<StructureComponent>(candidate) &&
            !world.HasComponent<TargetableTag>(candidate))
        {
            return false;
        }
        if (world.HasComponent<StructureComponent>(candidate))
        {
            const StructureComponent& structure = world.GetComponent<StructureComponent>(candidate);
            if (!bAnyLane && structure.lane != myLane && structure.lane != kLaneBase)
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
                ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.structureAcquireRangePadding;
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
        return state.type == ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.rangedRoleType;
    }

    eProjectileKind ResolveServerMinionRangedProjectileKind(eTeam team)
    {
        return team == eTeam::Red
            ? eProjectileKind::MinionRangedBasicRed
            : eProjectileKind::MinionRangedBasicBlue;
    }

    void ApplyServerMinionAttackImpact(
        CWorld& world,
        EntityID entity,
        MinionStateComponent& state,
        const MinionComponent& minion,
        const TransformComponent& transform,
        EntityID target,
        f32_t effectiveAttackRange)
    {
        const Vec3 pos = transform.GetPosition();
        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetPosition();
        eTeam sourceTeam = minion.team;
        (void)TryResolveCombatTeam(world, entity, sourceTeam);

        if (IsServerRangedMinion(state))
        {
            const MinionWaveRangedProjectileDef& rangedDef =
                ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionWave.rangedProjectile;
            const Vec3 projectileDir = NormalizeXZOrForward(
                Vec3{ targetPos.x - pos.x, 0.f, targetPos.z - pos.z },
                sourceTeam);
            const Vec3 projectileStart{
                pos.x + projectileDir.x * rangedDef.forwardOffset,
                pos.y + rangedDef.spawnHeight,
                pos.z + projectileDir.z * rangedDef.forwardOffset
            };

            SkillProjectileComponent projectile{};
            projectile.sourceEntity = entity;
            projectile.targetEntity = target;
            projectile.sourceHandle = world.GetEntityHandle(entity);
            projectile.targetHandle = world.GetEntityHandle(target);
            projectile.sourceTeam = sourceTeam;
            projectile.kind = ResolveServerMinionRangedProjectileKind(sourceTeam);
            projectile.skillId = static_cast<u16_t>(projectile.kind);
            projectile.currentPos = projectileStart;
            projectile.direction = projectileDir;
            projectile.speed = rangedDef.speed;
            projectile.maxDistance =
                effectiveAttackRange + rangedDef.maxDistancePadding;
            projectile.hitRadius = rangedDef.hitRadius;
            projectile.damage = state.attackDamage;

            const EntityID projectileEntity = world.CreateEntity();
            world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

            TransformComponent projectileTransform{};
            projectileTransform.SetPosition(projectileStart);
            world.AddComponent<TransformComponent>(projectileEntity, projectileTransform);
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
            EnqueueDamageRequest(world, request);
        }
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

            i32_t resolvedPriority = priority;
            if (myLane != 0xffu &&
                world.HasComponent<MinionStateComponent>(entity) &&
                world.GetComponent<MinionStateComponent>(entity).lane != myLane)
            {
                ++resolvedPriority;
            }

            if (resolvedPriority < bestPriority ||
                (resolvedPriority == bestPriority && distSq < bestDistSq) ||
                (resolvedPriority == bestPriority &&
                    std::fabs(distSq - bestDistSq) <= 0.0001f &&
                    (best == NULL_ENTITY || entity < best)))
            {
                bestPriority = resolvedPriority;
                bestDistSq = distSq;
                best = entity;
            }
        };

        if (CSpatialIndex* pSpatial = world.Get_SpatialIndex())
        {
            const u32_t targetMask =
                SpatialMask(eSpatialKind::Unit) |
                SpatialMask(eSpatialKind::Structure) |
                SpatialMask(eSpatialKind::Objective) |
                SpatialMask(eSpatialKind::Core) |
                SpatialMask(eSpatialKind::Character);
            std::vector<EntityID> candidates;
            const f32_t queryRange =
                maxRange + ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.structureAcquireRangePadding;
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
    if (!IsInGamePhase() || !m_bGameplayObjectsSpawned)
        return;

    RefreshServerStructureNavigationIfNeeded();

    m_serverMinionWaves.TickWave(
        tc.tickIndex,
        [this](const CServerMinionWaveRuntime::SpawnRequest& request)
        {
            SpawnServerMinion(request.team, request.roleType, request.lane, request.pos);
        });
}

void CGameRoom::Phase_ServerUnitAI(TickContext& tc)
{
    if (!IsInGamePhase())
        return;

    m_serverMinionTickStartPositions.clear();
    const auto minions =
        DeterministicEntityIterator<MinionStateComponent>::CollectSorted(m_world);

    u32_t PathBuildBudget = ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.pathBuildBudgetPerTick;

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
        m_serverMinionTickStartPositions[entity] = transform.GetPosition();

        if (m_world.HasComponent<HealthComponent>(entity))
        {
            auto& hp = m_world.GetComponent<HealthComponent>(entity);
            minion.hp = hp.fCurrent;
            minion.maxHp = hp.fMaximum;
            if (hp.bIsDead || hp.fCurrent <= 0.f)
            {
                if (state.current != MinionStateComponent::Dead)
                {
                    GameplayStatus::ClearStatusEffects(m_world, entity);
                    state.current = MinionStateComponent::Dead;
                    // 시체 타이머 단일화: 1.2f -> minionWave.corpseDeathTimerSec(1.5, 공용 경로와 동일).
                    state.deathTimer = ServerData::GetActiveLoLSpawnObjectDefinitionPack()
                        .minionWave.corpseDeathTimerSec;
                    StartReplicatedAction(m_world, entity, eActionStateId::DeathStart, tc);
                    SetReplicatedPose(m_world, entity, ePoseStateId::Dead, tc);
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

        const bool_t bCanMove = GameplayStateQuery::CanMove(m_world, entity);
        const bool_t bCanAttack = GameplayStateQuery::CanAttack(m_world, entity);
        const bool_t bForcedMotion =
            m_world.HasComponent<ForcedMotionComponent>(entity);

        if (!bCanAttack && state.attackTimer > 0.f)
        {
            state.attackTimer = 0.f;
            state.bHitFired = true;
            state.current = MinionStateComponent::Idle;
        }

        if (bForcedMotion || (!bCanMove && !bCanAttack))
        {
            state.current = MinionStateComponent::Idle;
            SetReplicatedPose(m_world, entity, ePoseStateId::Idle, tc);
            continue;
        }

        const Vec3 pos = transform.GetPosition();
        AnnieTibbersComponent* pTibbersCommand =
            m_world.HasComponent<AnnieTibbersComponent>(entity)
            ? &m_world.GetComponent<AnnieTibbersComponent>(entity)
            : nullptr;
        if (pTibbersCommand && pTibbersCommand->commandTarget != NULL_ENTITY)
        {
            if (IsAliveHealth(m_world, pTibbersCommand->commandTarget) &&
                GameplayStateQuery::CanBeTargetedBy(
                    m_world,
                    entity,
                    pTibbersCommand->commandTarget))
            {
                state.attackTargetId = pTibbersCommand->commandTarget;
            }
            else
            {
                pTibbersCommand->commandTarget = NULL_ENTITY;
            }
        }
        const bool_t bExplicitTibbersMove =
            pTibbersCommand &&
            pTibbersCommand->commandTarget == NULL_ENTITY &&
            pTibbersCommand->bHasCommandPosition;
        if (bExplicitTibbersMove)
            state.attackTargetId = NULL_ENTITY;

        const bool_t bAttackInProgress = state.attackTimer > 0.f;
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
            else if (!bAttackInProgress)
            {
                state.attackTargetId = NULL_ENTITY;
                state.targetScanCooldown = 0.f;
            }
        }

        if (pTibbersCommand &&
            pTibbersCommand->commandTarget != NULL_ENTITY &&
            target == NULL_ENTITY &&
            !bAttackInProgress)
        {
            pTibbersCommand->commandTarget = NULL_ENTITY;
        }

        if (!bAttackInProgress &&
            target == NULL_ENTITY &&
            !bExplicitTibbersMove &&
            state.targetScanCooldown <= 0.f)
        {
            target = FindClosestEnemyCombatTarget(
                m_world, entity, minion.team, state.lane, pos, state.sightRange);

            const f32_t scanInterval = state.targetScanInterval > 0.f
                ? state.targetScanInterval
                : ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.targetScanIntervalSec;
            state.targetScanCooldown = scanInterval;
        }

        if (state.attackTimer > 0.f)
        {
            state.current = MinionStateComponent::Attack;
            const f32_t previousAttackTimer = state.attackTimer;
            state.attackTimer = (std::max)(0.f, state.attackTimer - tc.fDt);

            if (target != NULL_ENTITY &&
                m_world.HasComponent<TransformComponent>(target))
            {
                const Vec3 targetPos =
                    m_world.GetComponent<TransformComponent>(target).GetPosition();
                FaceServerMinionTowardTarget(transform, pos, targetPos);
            }

            const f32_t impactThreshold = state.attackRecovery;
            if (!state.bHitFired &&
                previousAttackTimer > impactThreshold &&
                state.attackTimer <= impactThreshold)
            {
                bool_t bImpactValid =
                    target != NULL_ENTITY &&
                    IsAliveHealth(m_world, target) &&
                    m_world.HasComponent<TransformComponent>(target);
                if (bImpactValid)
                {
                    const Vec3 targetPos =
                        m_world.GetComponent<TransformComponent>(target).GetPosition();
                    const f32_t effectiveAttackRange =
                        ResolveServerMinionAttackRange(m_world, entity, target, state);
                    const f32_t impactRange =
                        effectiveAttackRange + ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.attackExitRangePadding;
                    bImpactValid =
                        WintersMath::DistanceSqXZ(pos, targetPos) <= impactRange * impactRange &&
                        (GameplayStateQuery::IsAttackSegmentGateExemptTarget(m_world, target) ||
                            SegmentWalkableXZ(
                                pos,
                                targetPos,
                                ResolveAgentRadius(m_world, entity)));

                    if (bImpactValid)
                    {
                        ApplyServerMinionAttackImpact(
                            m_world,
                            entity,
                            state,
                            minion,
                            transform,
                            target,
                            effectiveAttackRange);
                    }
                }

                state.bHitFired = true;
                if (!bImpactValid)
                {
                    state.attackTargetId = NULL_ENTITY;
                    state.targetScanCooldown = 0.f;
                }
            }

            SetReplicatedPose(m_world, entity, ePoseStateId::Idle, tc);
            continue;
        }

        bool_t bMoved = false;
        if (target != NULL_ENTITY && m_world.HasComponent<TransformComponent>(target))
        {
            const Vec3 targetPos = m_world.GetComponent<TransformComponent>(target).GetPosition();
            const f32_t distSq = WintersMath::DistanceSqXZ(pos, targetPos);
            const f32_t effectiveAttackRange =
                ResolveServerMinionAttackRange(m_world, entity, target, state);
            const f32_t rangeSq = effectiveAttackRange * effectiveAttackRange;
            const f32_t exitAttackRange =
                effectiveAttackRange + ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.attackExitRangePadding;
            const bool_t bInAttackRange = distSq <= rangeSq;
            const bool_t bInExitRange = distSq <= exitAttackRange * exitAttackRange;
            const bool_t bHoldAttackRange =
                state.current == MinionStateComponent::Attack &&
                state.attackCooldown > 0.f &&
                bInExitRange;
            const bool_t bHasClearAttackSegment =
                !bInExitRange ||
                GameplayStateQuery::IsAttackSegmentGateExemptTarget(m_world, target) ||
                SegmentWalkableXZ(
                    pos,
                    targetPos,
                    ResolveAgentRadius(m_world, entity));
            state.attackTargetId = target;

            if (bCanAttack &&
                (bInAttackRange || bHoldAttackRange) &&
                bHasClearAttackSegment)
            {
                state.current = MinionStateComponent::Attack;
                FaceServerMinionTowardTarget(transform, pos, targetPos);
                if (bCanAttack && state.attackCooldown <= 0.f)
                {
                    state.attackTimer = state.attackWindup + state.attackRecovery;
                    state.attackCooldown = state.attackCooldownMax;
                    state.bHitFired = false;
                    StartReplicatedAction(m_world, entity, eActionStateId::BasicAttack, tc);

                    static u32_t s_minionAttackLogCount = 0;
                    if (s_minionAttackLogCount < 128u)
                    {
                        const char* pTargetKind = m_world.HasComponent<StructureComponent>(target)
                            ? "structure"
                            : (m_world.HasComponent<MinionComponent>(target) ? "minion" : "champion");
                        char msg[288]{};
                        sprintf_s(msg,
                            "[UnitAI] attack tick=%llu entity=%u team=%u lane=%u target=%u targetKind=%s pos=(%.2f,%.2f,%.2f) targetPos=(%.2f,%.2f,%.2f)\n",
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
                SetReplicatedPose(m_world, entity, ePoseStateId::Idle, tc);
            }
            else if (!bCanAttack && bInExitRange)
            {
                state.current = MinionStateComponent::Idle;
            }
            else
            {
                const f32_t chaseStopRange = bInAttackRange
                    ? 0.f
                    : effectiveAttackRange;
                (void)TryMoveServerMinionToward(
                    entity,
                    state,
                    transform,
                    targetPos,
                    chaseStopRange,
                    tc,
                    PathBuildBudget,
                    bMoved,
                    MinionStateComponent::Chase);
            }
        }
        else if (pTibbersCommand)
        {
            state.attackTargetId = NULL_ENTITY;
            auto& tibbers = *pTibbersCommand;
            if (bExplicitTibbersMove)
            {
                constexpr f32_t kTibbersCommandArriveDistance = 0.65f;
                if (WintersMath::DistanceSqXZ(pos, tibbers.commandPosition) >
                    kTibbersCommandArriveDistance * kTibbersCommandArriveDistance)
                {
                    const bool_t bCommandProgressed = TryMoveServerMinionToward(
                        entity,
                        state,
                        transform,
                        tibbers.commandPosition,
                        kTibbersCommandArriveDistance,
                        tc,
                        PathBuildBudget,
                        bMoved,
                        MinionStateComponent::Chase);
                    if (!bCommandProgressed &&
                        bCanMove &&
                        !bForcedMotion &&
                        state.BlockedMoveFrames >=
                            ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.blockedFramesBeforeRepath * 2u)
                    {
                        tibbers.bHasCommandPosition = false;
                        state.current = MinionStateComponent::Idle;
                    }
                }
                else
                {
                    tibbers.bHasCommandPosition = false;
                    state.current = MinionStateComponent::Idle;
                }
            }
            else
            {
                const bool_t bOwnerValid =
                    tibbers.owner != NULL_ENTITY &&
                    m_world.IsAlive(tibbers.owner) &&
                    m_world.HasComponent<TransformComponent>(tibbers.owner);
                if (bOwnerValid)
                {
                    const Vec3 ownerPos =
                        m_world.GetComponent<TransformComponent>(tibbers.owner).GetPosition();
                    constexpr f32_t kTibbersFollowDistance = 2.5f;
                    if (WintersMath::DistanceSqXZ(pos, ownerPos) >
                        kTibbersFollowDistance * kTibbersFollowDistance)
                    {
                        (void)TryMoveServerMinionToward(
                            entity,
                            state,
                            transform,
                            ownerPos,
                            kTibbersFollowDistance,
                            tc,
                            PathBuildBudget,
                            bMoved,
                            MinionStateComponent::Chase);
                    }
                    else
                    {
                        state.current = MinionStateComponent::Idle;
                    }
                }
                else
                {
                    state.current = MinionStateComponent::Idle;
                }
            }
        }
        else
        {
            Vec3 laneTarget{};
            bool_t bHasLaneTarget = false;
            const bool_t bResumeLaneFromCombat =
                state.current == MinionStateComponent::Chase ||
                state.current == MinionStateComponent::Attack ||
                state.attackTargetId != NULL_ENTITY;
            state.attackTargetId = NULL_ENTITY;
            if (bResumeLaneFromCombat)
                ClearServerMinionPathRuntime(state);

            const u8_t waypointLane = ResolveServerWaypointLane(minion.team, state.lane);
            const u32_t waypointCount = GetServerMinionWaypointCount(minion.team, waypointLane);
            AdvanceServerMinionWaypointPastPosition(
                state, pos, waypointLane, waypointCount);
            if (waypointCount > 0u && state.currentWaypoint < waypointCount)
            {
                laneTarget = GetServerMinionWaypoint(minion.team, waypointLane, state.currentWaypoint);
                bHasLaneTarget = true;
            }

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

        if (bMoved)
        {
            SetReplicatedPose(m_world, entity, ePoseStateId::Run, tc);
        }
        else if (state.current != MinionStateComponent::Attack)
        {
            SetReplicatedPose(m_world, entity, ePoseStateId::Idle, tc);
        }
    }
}

void CGameRoom::Phase_ServerMinionDepenetration(TickContext& tc)
{
    if (!IsInGamePhase())
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
        if (!GameplayStateQuery::CanMove(m_world, entity) ||
            m_world.HasComponent<ForcedMotionComponent>(entity))
        {
            continue;
        }
        if (state.current != MinionStateComponent::LaneMove &&
            state.current != MinionStateComponent::Chase)
            continue;

        TransformComponent& transform = m_world.GetComponent<TransformComponent>(entity);
        const Vec3 vPos = transform.GetPosition();
        const f32_t fStep = (std::max)(
            0.08f,
            state.moveSpeed *
                GameplayStateQuery::GetMoveSpeedMultiplier(m_world, entity) *
                tc.fDt);

        Vec3 vPreferredTarget{};
        bool_t bHasPreferredTarget = false;
        if (state.PathCount > 0u && state.PathIndex < state.PathCount)
        {
            vPreferredTarget = state.PathWaypoints[state.PathIndex];
            bHasPreferredTarget = true;
        }
        else if (state.current == MinionStateComponent::Chase &&
            IsAliveHealth(m_world, state.attackTargetId) &&
            m_world.HasComponent<TransformComponent>(state.attackTargetId))
        {
            vPreferredTarget = m_world.GetComponent<TransformComponent>(
                state.attackTargetId).GetPosition();
            bHasPreferredTarget = true;
        }
        else
        {
            const u8_t waypointLane =
                ResolveServerWaypointLane(state.team, state.lane);
            const u32_t waypointCount =
                GetServerMinionWaypointCount(state.team, waypointLane);
            if (state.currentWaypoint < waypointCount)
            {
                vPreferredTarget = GetServerMinionWaypoint(
                    state.team,
                    waypointLane,
                    state.currentWaypoint);
                bHasPreferredTarget = true;
            }
        }
        const Vec3 vPreferredForward = bHasPreferredTarget
            ? Vec3{
                vPreferredTarget.x - vPos.x,
                0.f,
                vPreferredTarget.z - vPos.z }
            : NormalizeXZOrForward(Vec3{}, state.team);

        Vec3 vResolved{};
        if (!TryResolveMinionDepenetrationStep(
                entity,
                vPos,
                fStep,
                vPreferredForward,
                tc,
                vResolved))
            continue;

        Vec3 vFacingOrigin = vPos;
        const auto startIt = m_serverMinionTickStartPositions.find(entity);
        if (startIt != m_serverMinionTickStartPositions.end())
            vFacingOrigin = startIt->second;
        const Vec3 vActualMove{
            vResolved.x - vFacingOrigin.x,
            0.f,
            vResolved.z - vFacingOrigin.z };
        transform.SetPosition(vResolved);
        FaceServerMinionTowardDirection(transform, vActualMove);
    }
}

void CGameRoom::AdvanceServerMinionWaypointPastPosition(
    MinionStateComponent& state,
    const Vec3& position,
    u8_t waypointLane,
    u32_t waypointCount) const
{
    if (state.currentWaypoint >= waypointCount)
        return;

    constexpr f32_t kArriveRadius = 0.8f;
    constexpr f32_t kLaneRebaseCorridor = 5.f;
    const u32_t index = state.currentWaypoint;
    const Vec3 waypoint = GetServerMinionWaypoint(
        state.team, waypointLane, index);
    if (WintersMath::DistanceSqXZ(position, waypoint) <=
        kArriveRadius * kArriveRadius)
    {
        ++state.currentWaypoint;
        return;
    }

    if (index == 0u || waypointCount < 2u)
        return;

    const Vec3 previous = GetServerMinionWaypoint(
        state.team, waypointLane, index - 1u);
    const Vec3 segment{
        waypoint.x - previous.x,
        0.f,
        waypoint.z - previous.z };
    const f32_t segmentLengthSq =
        segment.x * segment.x + segment.z * segment.z;
    if (segmentLengthSq <= 0.0001f)
        return;

    const Vec3 fromPrevious{
        position.x - previous.x,
        0.f,
        position.z - previous.z };
    const f32_t projection =
        (fromPrevious.x * segment.x + fromPrevious.z * segment.z) /
        segmentLengthSq;
    const f32_t clampedProjection = std::clamp(projection, 0.f, 1.f);
    const Vec3 closest{
        previous.x + segment.x * clampedProjection,
        position.y,
        previous.z + segment.z * clampedProjection };
    const bool_t bInsideLaneCorridor =
        WintersMath::DistanceSqXZ(position, closest) <=
        kLaneRebaseCorridor * kLaneRebaseCorridor;
    if (bInsideLaneCorridor && projection >= 1.f)
        ++state.currentWaypoint;
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
    const Vec3& vPreferredForward,
    const TickContext& tc,
    Vec3& vOutNext)
{
    if (!m_world.HasComponent<SpatialAgentComponent>(entity))
        return false;

    const SpatialAgentComponent& self = m_world.GetComponent<SpatialAgentComponent>(entity);
    const f32_t selfRadius = (std::max)(0.2f, self.radius);

    const u32_t blockerMask =
        SpatialMask(eSpatialKind::Unit) |
        SpatialMask(eSpatialKind::NeutralUnit) |
        SpatialMask(eSpatialKind::Structure) |
        SpatialMask(eSpatialKind::Objective) |
        SpatialMask(eSpatialKind::Core);

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

    Vec3 vSoftMinionPush{};
    Vec3 vOtherPush{};
    Vec3 vStaticPush{};
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
        const bool_t bSoftMinion = agent.kind == eSpatialKind::Unit;
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
            ? ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.softSeparationRadiusScale
            : 1.f;
        const f32_t minDist = (selfRadius + otherRadius) * radiusScale + padding;
        if (distSq >= minDist * minDist)
            continue;

        const f32_t dist = std::sqrt(distSq);
        const f32_t penetration = minDist - dist;
        const f32_t weight = bStatic
            ? 1.0f
            : (bSoftMinion ? ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.softSeparationWeight : ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.defaultSeparationWeight);

        const Vec3 vDelta{
            (vAway.x / dist) * penetration * weight,
            0.f,
            (vAway.z / dist) * penetration * weight };
        if (bSoftMinion)
        {
            vSoftMinionPush.x += vDelta.x;
            vSoftMinionPush.z += vDelta.z;
        }
        else
        {
            vOtherPush.x += vDelta.x;
            vOtherPush.z += vDelta.z;
            if (bStatic)
            {
                vStaticPush.x += vDelta.x;
                vStaticPush.z += vDelta.z;
            }
        }

        ++blockerCount;
        if (bStatic)
            ++staticCount;
        else if (bSoftMinion)
            ++softMinionCount;
        else
            ++dynamicCount;
    }

    if (blockerCount == 0u)
        return false;
    const Vec3 vRawPush{
        vSoftMinionPush.x + vOtherPush.x,
        0.f,
        vSoftMinionPush.z + vOtherPush.z };
    const Vec3 primaryDirection =
        MinionSoftSeparationPolicy::ResolveCompositeDepenetrationDirection(
            vSoftMinionPush, vOtherPush, vPreferredForward,
            static_cast<u32_t>(entity));
    const bool_t bSoftMinionOnly =
        softMinionCount > 0u && staticCount == 0u && dynamicCount == 0u;
    const f32_t maxPushStep = bSoftMinionOnly
        ? ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.softSeparationMaxStep
        : 0.35f;
    const f32_t pushStep = (std::min)((std::max)(0.08f, fStep), maxPushStep);
    const f32_t clearanceRadius =
        ServerData::GetActiveLoLSpawnObjectDefinitionPack()
            .minionBehavior.laneClearanceRadius;
    MinionSoftSeparationPolicy::DepenetrationCandidateSelection selection{};
    const auto clamp = [&](const Vec3& from, const Vec3& desired,
                           f32_t radius, Vec3& out)
    {
        return TryClampMoveSegmentXZ(from, desired, radius, out);
    };
    if (!MinionSoftSeparationPolicy::TrySelectDepenetrationCandidate(
            vPos,
            clearanceRadius,
            pushStep,
            primaryDirection,
            vStaticPush,
            vPreferredForward,
            static_cast<u32_t>(entity),
            staticCount > 0u,
            clamp,
            selection))
    {
        return false;
    }
    Vec3 vGuarded = selection.vPosition;
    const char* pCandidateKind =
        selection.bUsedStaticTangent ? "static-tangent" : "primary";

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
            "push=(%.3f,%.3f) resolved=(%.3f,%.3f) blockers=%u static=%u dynamic=%u softMinion=%u candidate=%s from=(%.2f,%.2f) to=(%.2f,%.2f)\n",
            static_cast<unsigned long long>(tc.tickIndex),
            static_cast<u32_t>(entity),
            posCell.x,
            posCell.y,
            nextCell.x,
            nextCell.y,
            vRawPush.x,
            vRawPush.z,
            primaryDirection.x,
            primaryDirection.z,
            blockerCount,
            staticCount,
            dynamicCount,
            softMinionCount,
            pCandidateKind,
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
    if (!GameplayStateQuery::CanMove(m_world, entity) ||
        m_world.HasComponent<ForcedMotionComponent>(entity))
    {
        return false;
    }

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
    if (!SegmentWalkableXZ(vPos, vTarget, ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.laneClearanceRadius))
    {
        const bool_t bTargetMoved =
            WintersMath::DistanceSqXZ(state.PathTarget, vTarget) >
            ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.pathTargetRefreshDistanceSq;

        const bool_t bNeedPath =
            state.PathCount == 0u ||
            state.PathIndex >= state.PathCount ||
            bTargetMoved ||
            state.BlockedMoveFrames >= ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.blockedFramesBeforeRepath;

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
                ? ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.chasePathRebuildIntervalSec
                : ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.lanePathRebuildIntervalSec;
        }

        if (state.PathCount == 0u || state.PathIndex >= state.PathCount)
        {
            vMoveGoal = vTarget;
        }
        else
        {
            while (state.PathIndex + 1u < state.PathCount &&
                WintersMath::DistanceSqXZ(vPos, state.PathWaypoints[state.PathIndex]) <=
                ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.pathWaypointArriveRadius *
                ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.pathWaypointArriveRadius)
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
    const f32_t fStep =
        state.moveSpeed *
        GameplayStateQuery::GetMoveSpeedMultiplier(m_world, entity) *
        tc.fDt;

    Vec3 vNext{};
    if (!TryResolveMinionMoveStep(entity, vPos, vDir, fStep, tc, vNext))
    {
        Vec3 vDepenetrated{};
        if (TryResolveMinionDepenetrationStep(
                entity,
                vPos,
                fStep,
                vDir,
                tc,
                vDepenetrated))
        {
            const Vec3 vActualMove{ vDepenetrated.x - vPos.x, 0.f, vDepenetrated.z - vPos.z };
            transform.SetPosition(vDepenetrated);
            FaceServerMinionTowardDirection(transform, vActualMove);
            state.current = moveState;
            UpdateServerMinionMoveProgress(state, vPos, vDepenetrated, vMoveGoal);
            outMoved = true;
            return true;
        }

        RecordServerMinionBlockedMove(state);

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
    UpdateServerMinionMoveProgress(state, vPos, vNext, vMoveGoal);
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
    if (!GameplayStateQuery::CanMove(m_world, entity) ||
        m_world.HasComponent<ForcedMotionComponent>(entity))
    {
        return false;
    }

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

    const Vec3 vLaneTargetDelta{
        vLaneTarget.x - vPos.x,
        0.f,
        vLaneTarget.z - vPos.z };
    const Vec3 vLaneTargetDirection = WintersMath::NormalizeXZOrZero(
        vLaneTargetDelta,
        std::numeric_limits<f32_t>::epsilon());
    const f32_t fFlowLaneAlignment =
        vDir.x * vLaneTargetDirection.x + vDir.z * vLaneTargetDirection.z;
    if (fFlowLaneAlignment < -0.05f)
    {
        static u32_t s_flowFieldOpposedLogCount = 0u;
        if (s_flowFieldOpposedLogCount < 64u)
        {
            char msg[320]{};
            sprintf_s(
                msg,
                "[UnitAI] flow fallback reason=opposes-waypoint entity=%u team=%u lane=%u "
                "blocked=%u alignment=%.3f pos=(%.2f,%.2f) target=(%.2f,%.2f)\n",
                static_cast<u32_t>(entity),
                static_cast<u32_t>(state.team),
                static_cast<u32_t>(state.lane),
                static_cast<u32_t>(state.BlockedMoveFrames),
                fFlowLaneAlignment,
                vPos.x,
                vPos.z,
                vLaneTarget.x,
                vLaneTarget.z);
            OutputServerAITrace(msg);
            ++s_flowFieldOpposedLogCount;
        }
        return false;
    }

    const f32_t fStep =
        state.moveSpeed *
        GameplayStateQuery::GetMoveSpeedMultiplier(m_world, entity) *
        tc.fDt;
    Vec3 vNext{};
    if (!TryResolveMinionMoveStep(entity, vPos, vDir, fStep, tc, vNext))
    {
        Vec3 vDepenetrated{};
        if (TryResolveMinionDepenetrationStep(
                entity,
                vPos,
                fStep,
                vDir,
                tc,
                vDepenetrated))
        {
            UpdateServerMinionMoveProgress(state, vPos, vDepenetrated, vLaneTarget);
            if (state.BlockedMoveFrames >=
                ServerData::GetActiveLoLSpawnObjectDefinitionPack()
                    .minionBehavior.flowFieldStallFramesBeforePathFallback)
            {
                return false;
            }

            const Vec3 vActualMove{ vDepenetrated.x - vPos.x, 0.f, vDepenetrated.z - vPos.z };
            transform.SetPosition(vDepenetrated);
            FaceServerMinionTowardDirection(transform, vActualMove);
            state.current = MinionStateComponent::LaneMove;
            outMoved = true;
            return true;
        }

        RecordServerMinionBlockedMove(state);
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
        fNextLaneDistSq + ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.flowFieldProgressSlackSq < fPrevLaneDistSq;

    if (!bProgressed)
    {
        RecordServerMinionBlockedMove(state);
        if (state.BlockedMoveFrames >= ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.flowFieldStallFramesBeforePathFallback)
        {
            static u32_t s_flowFieldFallbackLogCount = 0;
            if (s_flowFieldFallbackLogCount < 64u)
            {
                char msg[256]{};
                sprintf_s(msg,
                    "[UnitAI] flow fallback reason=stall entity=%u team=%u lane=%u blocked=%u pos=(%.2f,%.2f) target=(%.2f,%.2f)\n",
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
            ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior.laneClearanceRadius,
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
