#include "Shared/GameSim/Systems/JungleAI/JungleAISystem.h"

#include "Shared/GameSim/Components/AttackChaseComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/JungleAIComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

#include <algorithm>
#include <limits>

namespace
{
    bool_t IsAlive(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;

        if (!world.HasComponent<HealthComponent>(entity))
            return true;

        const auto& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    bool_t IsValidChampionTarget(CWorld& world, EntityID jungle, EntityID target)
    {
        return IsAlive(world, target) &&
            world.HasComponent<ChampionComponent>(target) &&
            world.HasComponent<TransformComponent>(target) &&
            GameplayStateQuery::CanBeTargetedBy(world, jungle, target);
    }

    bool_t HasActiveAttackWork(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<AttackChaseComponent>(entity) &&
            world.GetComponent<AttackChaseComponent>(entity).bActive)
        {
            return true;
        }

        return world.HasComponent<CombatActionComponent>(entity);
    }

    bool_t IsBasicAttackReady(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<SkillStateComponent>(entity))
            return true;

        const auto& slot = world.GetComponent<SkillStateComponent>(entity)
            .slots[static_cast<u8_t>(eSkillSlot::BasicAttack)];
        return slot.cooldownRemaining <= 0.f;
    }

    void ClearJungleAggro(CWorld& world, EntityID entity, JungleAIComponent& ai)
    {
        ai.target = NULL_ENTITY;
        ai.bAggro = false;

        if (world.HasComponent<AttackChaseComponent>(entity))
            world.RemoveComponent<AttackChaseComponent>(entity);

        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};
    }

    bool_t UpdateJungleRespawn(
        CWorld& world, EntityID entity, JungleComponent& jungle,
        JungleAIComponent& ai, f32_t deltaSeconds, u64_t tickIndex)
    {
        if (!world.HasComponent<HealthComponent>(entity))
            return true;
        HealthComponent& health = world.GetComponent<HealthComponent>(entity);
        const bool_t bDead = health.bIsDead || health.fCurrent <= 0.f;
        if (!bDead)
        {
            if (jungle.bRespawnPending)
            {
                jungle.bRespawnPending = false;
                jungle.fRespawnTimerSec = 0.f;
                if (!world.HasComponent<TargetableTag>(entity))
                    world.AddComponent<TargetableTag>(entity);
            }
            return true;
        }
        if (!jungle.bRespawnPending)
        {
            jungle.bRespawnPending = true;
            jungle.fRespawnTimerSec = jungle.fRespawnDelaySec;
            ClearJungleAggro(world, entity, ai);
            ai.bReturning = false;
            if (world.HasComponent<CombatActionComponent>(entity))
                world.RemoveComponent<CombatActionComponent>(entity);
            GameplayStatus::ClearStatusEffects(world, entity);
            if (world.HasComponent<TargetableTag>(entity))
                world.RemoveComponent<TargetableTag>(entity);
        }
        jungle.fRespawnTimerSec = std::max(
            0.f, jungle.fRespawnTimerSec - std::max(0.f, deltaSeconds));
        if (jungle.fRespawnTimerSec > 0.f)
            return false;

        jungle.bRespawnPending = false;
        health.fMaximum = jungle.maxHp;
        health.fCurrent = jungle.maxHp;
        health.bIsDead = false;
        jungle.hp = jungle.maxHp;
        if (world.HasComponent<TransformComponent>(entity))
            world.GetComponent<TransformComponent>(entity).SetPosition(
                jungle.vSpawnPosition);
        if (world.HasComponent<SkillStateComponent>(entity))
            world.GetComponent<SkillStateComponent>(entity) = SkillStateComponent{};
        ClearJungleAggro(world, entity, ai);
        ai.attackSequence = 0u;
        ai.bReturning = false;
        if (!world.HasComponent<TargetableTag>(entity))
            world.AddComponent<TargetableTag>(entity);
        SetPoseState(world, entity, ePoseStateId::Idle, tickIndex, true);
        return true;
    }

    constexpr f32_t kJungleReturnArriveRadius = 0.5f;

    f32_t DistanceSqXZ(const Vec3& pos, f32_t x, f32_t z)
    {
        const f32_t dx = pos.x - x;
        const f32_t dz = pos.z - z;
        return dx * dx + dz * dz;
    }

    void RequestMoveToAnchor(
        CWorld& world,
        const TickContext& tc,
        EntityID entity,
        const JungleAIComponent& ai,
        const Vec3& selfPos)
    {
        if (!world.HasComponent<MoveTargetComponent>(entity))
            world.AddComponent<MoveTargetComponent>(entity, MoveTargetComponent{});

        auto& moveTarget = world.GetComponent<MoveTargetComponent>(entity);
        if (moveTarget.bHasTarget &&
            DistanceSqXZ(moveTarget.target, ai.anchorX, ai.anchorZ) <=
                kJungleReturnArriveRadius * kJungleReturnArriveRadius)
        {
            return;
        }

        moveTarget = MoveTargetComponent{};
        const Vec3 home{ ai.anchorX, selfPos.y, ai.anchorZ };
        Vec3 resolvedTarget = home;
        if (tc.pWalkable)
        {
            Vec3 waypoints[kMovePathMaxWaypoints]{};
            u16_t waypointCount = 0;
            if (tc.pWalkable->TryBuildMovePath(
                selfPos,
                home,
                waypoints,
                kMovePathMaxWaypoints,
                waypointCount,
                resolvedTarget))
            {
                moveTarget.pathCount = waypointCount;
                for (u16_t i = 0; i < waypointCount; ++i)
                    moveTarget.pathWaypoints[i] = waypoints[i];
            }
            else
            {
                // 경로 실패 시 직선 복귀 (스폰 지점은 항상 걷기 가능 지형).
                resolvedTarget = home;
            }
        }
        moveTarget.target = resolvedTarget;
        moveTarget.bHasTarget = true;
    }

    void ResetJungleToCamp(
        CWorld& world,
        const TickContext& tc,
        EntityID entity,
        JungleAIComponent& ai,
        const Vec3& selfPos)
    {
        ClearJungleAggro(world, entity, ai);
        ai.bReturning = true;

        if (world.HasComponent<HealthComponent>(entity))
        {
            auto& health = world.GetComponent<HealthComponent>(entity);
            if (!health.bIsDead && health.fMaximum > 0.f)
                health.fCurrent = health.fMaximum;
        }

        auto& jungle = world.GetComponent<JungleComponent>(entity);
        if (jungle.maxHp > 0.f)
            jungle.hp = jungle.maxHp;

        RequestMoveToAnchor(world, tc, entity, ai, selfPos);
    }

    EntityID FindProximityChampionTarget(
        CWorld& world,
        EntityID jungle,
        const Vec3& selfPos,
        f32_t aggroRange)
    {
        const f32_t aggroRangeSq = aggroRange * aggroRange;
        EntityID best = NULL_ENTITY;
        f32_t bestDistSq = aggroRangeSq;

        const auto champions =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);
        for (EntityID champion : champions)
        {
            if (!IsValidChampionTarget(world, jungle, champion))
                continue;

            const Vec3 championPos =
                world.GetComponent<TransformComponent>(champion).GetLocalPosition();
            const f32_t distSq =
                DistanceSqXZ(championPos, selfPos.x, selfPos.z);
            if (distSq <= aggroRangeSq &&
                (best == NULL_ENTITY || distSq < bestDistSq))
            {
                best = champion;
                bestDistSq = distSq;
            }
        }
        return best;
    }

    EntityID FindChampionInEffectiveAttackRange(
        CWorld& world,
        EntityID jungle,
        const Vec3& selfPos,
        EntityID preferred)
    {
        f32_t baseRange = 5.5f;
        if (world.HasComponent<StatComponent>(jungle))
        {
            const f32_t configured =
                world.GetComponent<StatComponent>(jungle).attackRange;
            if (configured > 0.f)
                baseRange = configured;
        }
        const f32_t sourceRadius =
            GameplayStateQuery::ResolveGameplayRadius(world, jungle);
        const auto isInRange = [&](EntityID candidate, f32_t& outDistanceSq)
        {
            if (!IsValidChampionTarget(world, jungle, candidate))
                return false;
            const Vec3 targetPos =
                world.GetComponent<TransformComponent>(candidate).GetLocalPosition();
            outDistanceSq = DistanceSqXZ(targetPos, selfPos.x, selfPos.z);
            const f32_t effectiveRange = baseRange + sourceRadius +
                GameplayStateQuery::ResolveGameplayRadius(world, candidate);
            return outDistanceSq <= effectiveRange * effectiveRange;
        };

        f32_t preferredDistanceSq = 0.f;
        if (preferred != NULL_ENTITY &&
            isInRange(preferred, preferredDistanceSq))
        {
            return preferred;
        }

        EntityID best = NULL_ENTITY;
        f32_t bestDistanceSq = (std::numeric_limits<f32_t>::max)();
        const auto champions =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);
        for (EntityID candidate : champions)
        {
            f32_t distanceSq = 0.f;
            if (isInRange(candidate, distanceSq) && distanceSq < bestDistanceSq)
            {
                best = candidate;
                bestDistanceSq = distanceSq;
            }
        }
        return best;
    }

    GameCommand MakeJungleBasicAttackCommand(
        const TickContext& tc,
        EntityID issuer,
        EntityID target,
        u32_t sequence,
        const Vec3& issuerPos,
        const Vec3& targetPos)
    {
        GameCommand cmd{};
        cmd.kind = eCommandKind::BasicAttack;
        cmd.issuerEntity = issuer;
        cmd.issuedAtTick = tc.tickIndex;
        cmd.sequenceNum = sequence;
        cmd.slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        cmd.targetEntity = target;
        cmd.groundPos = targetPos;
        cmd.direction = WintersMath::DirectionXZ(issuerPos, targetPos);
        return cmd;
    }
}

void CJungleAISystem::Execute(CWorld& world, const TickContext& tc,
    std::vector<GameCommand>& outCommands)
{
    const auto entities =
        DeterministicEntityIterator<JungleAIComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        if (!world.HasComponent<JungleAIComponent>(entity) ||
            !world.HasComponent<JungleComponent>(entity))
        {
            continue;
        }

        auto& ai = world.GetComponent<JungleAIComponent>(entity);

        auto& jungle = world.GetComponent<JungleComponent>(entity);
        if (!UpdateJungleRespawn(
                world, entity, jungle, ai, tc.fDt, tc.tickIndex))
            continue;

        if (!world.HasComponent<TransformComponent>(entity))
            continue;

        const Vec3 selfPos =
            world.GetComponent<TransformComponent>(entity).GetLocalPosition();

        // 스폰 앵커 미기록 엔티티(레거시/하네스 스폰)는 첫 틱 위치를 캠프로 삼는다.
        if (!ai.bHasAnchor)
        {
            ai.anchorX = selfPos.x;
            ai.anchorZ = selfPos.z;
            ai.bHasAnchor = true;
        }

        if (ai.bStationary)
        {
            auto& transform = world.GetComponent<TransformComponent>(entity);
            Vec3 anchored = transform.GetLocalPosition();
            anchored.x = ai.anchorX;
            anchored.z = ai.anchorZ;
            transform.SetPosition(anchored);
            ai.bReturning = false;

            if (world.HasComponent<AttackChaseComponent>(entity))
                world.RemoveComponent<AttackChaseComponent>(entity);
            if (world.HasComponent<MoveTargetComponent>(entity))
                world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};

            if (!ai.bAggro)
            {
                ai.target = NULL_ENTITY;
                continue;
            }

            ai.target = FindChampionInEffectiveAttackRange(
                world, entity, anchored, ai.target);
            if (ai.target == NULL_ENTITY ||
                HasActiveAttackWork(world, entity) ||
                !IsBasicAttackReady(world, entity))
            {
                continue;
            }

            const Vec3 targetPos =
                world.GetComponent<TransformComponent>(ai.target).GetLocalPosition();
            ++ai.attackSequence;
            outCommands.push_back(MakeJungleBasicAttackCommand(
                tc,
                entity,
                ai.target,
                ai.attackSequence,
                anchored,
                targetPos));
            continue;
        }

        // 복귀 중에는 전투를 무시하고 캠프로 걷는다. 도착 시 해제.
        if (ai.bReturning)
        {
            ai.target = NULL_ENTITY;
            ai.bAggro = false;
            if (world.HasComponent<AttackChaseComponent>(entity))
                world.RemoveComponent<AttackChaseComponent>(entity);

            if (DistanceSqXZ(selfPos, ai.anchorX, ai.anchorZ) <=
                kJungleReturnArriveRadius * kJungleReturnArriveRadius)
            {
                ai.bReturning = false;
                if (world.HasComponent<MoveTargetComponent>(entity))
                    world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};
            }
            else
            {
                RequestMoveToAnchor(world, tc, entity, ai, selfPos);
            }
            continue;
        }

        // 리쉬 범위 이탈: 어그로 해제 + 풀피 리셋 + 복귀 시작.
        if (ai.bAggro &&
            ai.leashRange > 0.f &&
            DistanceSqXZ(selfPos, ai.anchorX, ai.anchorZ) >
                ai.leashRange * ai.leashRange)
        {
            ResetJungleToCamp(world, tc, entity, ai, selfPos);
            continue;
        }

        if (ai.bAggro &&
            (ai.target == NULL_ENTITY ||
                !IsValidChampionTarget(world, entity, ai.target)))
        {
            ClearJungleAggro(world, entity, ai);
        }

        // 근접 어그로 획득: 비어그로 상태에서 사거리 내 가장 가까운 챔피언.
        if (!ai.bAggro && ai.aggroRange > 0.f)
        {
            const EntityID proximityTarget =
                FindProximityChampionTarget(world, entity, selfPos, ai.aggroRange);
            if (proximityTarget != NULL_ENTITY)
            {
                ai.target = proximityTarget;
                ai.bAggro = true;
            }
        }

        if (!ai.bAggro || ai.target == NULL_ENTITY)
            continue;

        if (HasActiveAttackWork(world, entity) ||
            !IsBasicAttackReady(world, entity))
        {
            continue;
        }

        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(ai.target).GetLocalPosition();

        ++ai.attackSequence;
        outCommands.push_back(MakeJungleBasicAttackCommand(
            tc,
            entity,
            ai.target,
            ai.attackSequence,
            selfPos,
            targetPos));
    }
}
