#pragma once

#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Definitions/SkillTypes.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <cstddef>
#include <type_traits>

enum class eActionStateId : u16_t
{
    None = 0,
    BasicAttack = 10,
    SkillQ = 20,
    SkillW = 21,
    SkillE = 22,
    SkillR = 23,
    Recall = 30,
    DeathStart = 50,
    ViegoConsumeSoul = 60,
};

struct ActionStateComponent
{
    u16_t actionId = static_cast<u16_t>(eActionStateId::None);
    u8_t reservedActionAlignment[6]{};
    u64_t startTick = 0;
    u64_t lockEndTick = 0;
    u32_t sequence = 0;
    u32_t commandSequence = 0;
    eChampion sourceChampion = eChampion::NONE;
    u8_t sourceSlot = 0;
    u8_t stage = 1;
    eSkillActionMovePolicy movePolicy = eSkillActionMovePolicy::Allow;
    bool_t bHasQueuedMove = false;
    u8_t reservedQueuedMoveAlignment[3]{};
    u32_t queuedMoveSequence = 0;
    Vec3 queuedMoveTarget{};
    Vec3 queuedMoveDirection{};
    u32_t reservedTail = 0u;
};

static_assert(std::is_trivially_copyable_v<ActionStateComponent>,
    "ActionStateComponent must be trivially_copyable for sim determinism.");
static_assert(sizeof(ActionStateComponent) == 72u);
static_assert(offsetof(ActionStateComponent, startTick) == 8u);
static_assert(offsetof(ActionStateComponent, bHasQueuedMove) == 36u);
static_assert(offsetof(ActionStateComponent, queuedMoveSequence) == 40u);
static_assert(offsetof(ActionStateComponent, queuedMoveDirection) == 56u);
