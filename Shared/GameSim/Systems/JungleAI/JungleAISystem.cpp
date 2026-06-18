#include "Shared/GameSim/Systems/JungleAI/JungleAISystem.h"

#include "Shared/GameSim/Components/AttackChaseComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/JungleAIComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

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

        if (!IsAlive(world, entity))
        {
            ClearJungleAggro(world, entity, ai);
            continue;
        }

        if (!ai.bAggro || ai.target == NULL_ENTITY)
            continue;

        if (!world.HasComponent<TransformComponent>(entity) ||
            !IsValidChampionTarget(world, entity, ai.target))
        {
            ClearJungleAggro(world, entity, ai);
            continue;
        }

        if (HasActiveAttackWork(world, entity) ||
            !IsBasicAttackReady(world, entity))
        {
            continue;
        }

        const Vec3 selfPos =
            world.GetComponent<TransformComponent>(entity).GetLocalPosition();
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
