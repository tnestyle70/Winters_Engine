#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/PoseStateComponent.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "WintersTypes.h"

inline bool_t IsReplicatedGameplayAction(eActionStateId actionId)
{
    switch (actionId)
    {
    case eActionStateId::BasicAttack:
    case eActionStateId::SkillQ:
    case eActionStateId::SkillW:
    case eActionStateId::SkillE:
    case eActionStateId::SkillR:
        return true;
    default:
        return false;
    }
}

inline u8_t SkillSlotFromActionId(eActionStateId actionId)
{
    switch (actionId)
    {
    case eActionStateId::SkillQ:
        return static_cast<u8_t>(eSkillSlot::Q);
    case eActionStateId::SkillW:
        return static_cast<u8_t>(eSkillSlot::W);
    case eActionStateId::SkillE:
        return static_cast<u8_t>(eSkillSlot::E);
    case eActionStateId::SkillR:
        return static_cast<u8_t>(eSkillSlot::R);
    case eActionStateId::BasicAttack:
    default:
        return static_cast<u8_t>(eSkillSlot::BasicAttack);
    }
}

inline eActionStateId ActionIdFromSkillSlot(u8_t slot)
{
    switch (slot)
    {
    case static_cast<u8_t>(eSkillSlot::Q):
        return eActionStateId::SkillQ;
    case static_cast<u8_t>(eSkillSlot::W):
        return eActionStateId::SkillW;
    case static_cast<u8_t>(eSkillSlot::E):
        return eActionStateId::SkillE;
    case static_cast<u8_t>(eSkillSlot::R):
        return eActionStateId::SkillR;
    case static_cast<u8_t>(eSkillSlot::BasicAttack):
    default:
        return eActionStateId::BasicAttack;
    }
}

template <typename TWorld>
PoseStateComponent& EnsurePoseState(TWorld& world, EntityID entity)
{
    return world.template HasComponent<PoseStateComponent>(entity)
        ? world.template GetComponent<PoseStateComponent>(entity)
        : world.template AddComponent<PoseStateComponent>(entity, PoseStateComponent{});
}

template <typename TWorld>
ActionStateComponent& EnsureActionState(TWorld& world, EntityID entity)
{
    return world.template HasComponent<ActionStateComponent>(entity)
        ? world.template GetComponent<ActionStateComponent>(entity)
        : world.template AddComponent<ActionStateComponent>(entity, ActionStateComponent{});
}

template <typename TWorld>
void SetPoseState(
    TWorld& world,
    EntityID entity,
    ePoseStateId poseId,
    u64_t startTick,
    bool_t bRefreshStartTick = false)
{
    PoseStateComponent& pose = EnsurePoseState(world, entity);
    const u16_t nextPoseId = static_cast<u16_t>(poseId);
    if (pose.poseId != nextPoseId || bRefreshStartTick)
    {
        pose.poseId = nextPoseId;
        pose.startTick = startTick;
    }
}

template <typename TWorld>
ActionStateComponent& StartActionState(
    TWorld& world,
    EntityID entity,
    eActionStateId actionId,
    u64_t startTick,
    u8_t stage = 1)
{
    ActionStateComponent& action = EnsureActionState(world, entity);
    ++action.sequence;
    action.actionId = static_cast<u16_t>(actionId);
    action.startTick = startTick;
    action.lockEndTick = startTick;
    action.commandSequence = 0u;
    action.sourceChampion = eChampion::NONE;
    action.sourceSlot = SkillSlotFromActionId(actionId);
    action.stage = stage == 0u ? 1u : stage;
    action.movePolicy = eSkillActionMovePolicy::Allow;
    action.bHasQueuedMove = false;
    action.queuedMoveSequence = 0u;
    action.queuedMoveTarget = {};
    action.queuedMoveDirection = {};
    return action;
}
