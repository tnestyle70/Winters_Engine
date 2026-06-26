#include "Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.h"

#include "Shared/GameSim/Components/KalistaPassiveDashComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Systems/Move/DashArrival.h"
#include "WintersMath.h"

namespace
{
    void ClearMoveTarget(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};
    }

    bool_t AssignQueuedMovePath(
        MoveTargetComponent& moveTarget,
        const TickContext& tc,
        const Vec3& from,
        const Vec3& rawTarget)
    {
        moveTarget = MoveTargetComponent{};
        Vec3 resolvedTarget = rawTarget;
        resolvedTarget.y = from.y;

        if (tc.pWalkable)
        {
            Vec3 waypoints[kMovePathMaxWaypoints]{};
            u16_t waypointCount = 0;
            if (!tc.pWalkable->TryBuildMovePath(
                from,
                rawTarget,
                waypoints,
                kMovePathMaxWaypoints,
                waypointCount,
                resolvedTarget))
            {
                return false;
            }

            moveTarget.pathCount = waypointCount;
            moveTarget.pathIndex = 0;
            for (u16_t i = 0; i < waypointCount; ++i)
                moveTarget.pathWaypoints[i] = waypoints[i];
        }

        moveTarget.target = resolvedTarget;
        moveTarget.bHasTarget = true;
        return true;
    }

    void UpdateKalistaPassiveDash(CWorld& world, const TickContext& tc)
    {
        const auto entities = DeterministicEntityIterator<KalistaPassiveDashComponent>::CollectSorted(world);
        for (EntityID entity : entities)
        {
            if (!world.IsAlive(entity) || !world.HasComponent<KalistaPassiveDashComponent>(entity))
                continue;

            auto& dash = world.GetComponent<KalistaPassiveDashComponent>(entity);
            if (!world.HasComponent<TransformComponent>(entity))
            {
                world.RemoveComponent<KalistaPassiveDashComponent>(entity);
                continue;
            }

            auto& transform = world.GetComponent<TransformComponent>(entity);

            if (dash.bPending && !dash.bActive && tc.tickIndex >= dash.triggerTick)
            {
                if (!world.HasComponent<ActionStateComponent>(entity) ||
                    world.GetComponent<ActionStateComponent>(entity).sequence != dash.sourceActionSeq)
                {
                    world.RemoveComponent<KalistaPassiveDashComponent>(entity);
                    continue;
                }

                dash.bPending = false;
                dash.bActive = true;
                dash.elapsedSec = 0.f;
                dash.startPos = transform.GetLocalPosition();
                dash.endPos = Vec3{
                    dash.startPos.x + dash.direction.x * dash.distance,
                    dash.startPos.y,
                    dash.startPos.z + dash.direction.z * dash.distance
                };
            }

            if (!dash.bActive)
                continue;

            ClearMoveTarget(world, entity);

            dash.elapsedSec += tc.fDt;
            f32_t t = (dash.durationSec > 0.01f)
                ? dash.elapsedSec / dash.durationSec
                : 1.f;
            t = WintersMath::Clamp01(t);

            Vec3 desiredPos = WintersMath::LerpXZ(dash.startPos, dash.endPos, t);
            Vec3 guardedPos = desiredPos;
            bool_t bDashBlocked = false;
            if (tc.pWalkable)
            {
                const Vec3 currentPos = transform.GetLocalPosition();
                if (!tc.pWalkable->TryClampMoveSegmentXZ(currentPos, desiredPos, 0.5f, guardedPos))
                {
                    guardedPos = currentPos;
                    bDashBlocked = true;
                }
                else if (WintersMath::DistanceSqXZ(guardedPos, desiredPos) > 0.0001f)
                {
                    bDashBlocked = true;
                }
            }

            transform.SetPosition(guardedPos);

            if (t >= 1.f || bDashBlocked)
            {
                SnapDashArrivalToWalkable(world, tc, entity, dash.startPos);
                dash.endPos = transform.GetLocalPosition();
                if (dash.bHasQueuedMove)
                {
                    auto& moveTarget = world.HasComponent<MoveTargetComponent>(entity)
                        ? world.GetComponent<MoveTargetComponent>(entity)
                        : world.AddComponent<MoveTargetComponent>(entity, MoveTargetComponent{});
                    (void)AssignQueuedMovePath(
                        moveTarget,
                        tc,
                        dash.endPos,
                        dash.queuedMoveTarget);
                }
                world.RemoveComponent<KalistaPassiveDashComponent>(entity);
            }
        }
    }
}

void CSkillCooldownSystem::Execute(CWorld& world, const TickContext& tc)
{
    UpdateKalistaPassiveDash(world, tc);

    const auto spellEntities =
        DeterministicEntityIterator<SummonerSpellStateComponent>::CollectSorted(world);
    for (EntityID entity : spellEntities)
    {
        auto& spells = world.GetComponent<SummonerSpellStateComponent>(entity);
        for (u8_t i = 0; i < SummonerSpellStateComponent::kSlotCount; ++i)
        {
            if (spells.cooldownRemaining[i] > 0.f)
            {
                spells.cooldownRemaining[i] -= tc.fDt;
                if (spells.cooldownRemaining[i] <= 0.f)
                {
                    spells.cooldownRemaining[i] = 0.f;
                    spells.cooldownDuration[i] = 0.f;
                }
            }
            else
            {
                spells.cooldownDuration[i] = 0.f;
            }
        }
    }

    const auto entities = DeterministicEntityIterator<SkillStateComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        auto& skillState = world.GetComponent<SkillStateComponent>(entity);

        for (u8_t i = 0; i < 5; ++i)
        {
            auto& slot = skillState.slots[i];
            if (slot.cooldownRemaining > 0.f)
            {
                slot.cooldownRemaining -= tc.fDt;
                if (slot.cooldownRemaining <= 0.f)
                {
                    slot.cooldownRemaining = 0.f;
                    slot.cooldownDuration = 0.f;
                }
            }
            else
            {
                slot.cooldownDuration = 0.f;
            }

            if (slot.currentStage == 1 && slot.stageWindow > 0.f)
            {
                slot.stageWindow -= tc.fDt;
                if (slot.stageWindow <= 0.f)
                {
                    slot.currentStage = 0;
                    slot.stageWindow = 0.f;
                }
            }
        }
    }
}
