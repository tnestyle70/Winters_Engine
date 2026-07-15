#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "../Definitions/LoLMatchContext.h"

#include <cstddef>

struct ViegoSimComponent
{
    bool_t bMistActive = false;
    u8_t reservedMistAlignment[3]{};
    f32_t mistTimerSec = 0.f;
    f32_t mistDurationSec = 4.f;
    bool_t bMistHadTargetable = false;

    bool_t bPossessionActive = false;
    bool_t bPossessionPending = false;
    eChampion pendingPossessionChampion = eChampion::END;
    EntityID pendingPossessedTarget = NULL_ENTITY;
    u8_t pendingSkillRanks[SkillRankComponent::kSlotCount] = {};
    bool_t bPendingHasSkillRanks = false;
    u8_t reservedPendingAlignment[2]{};
    f32_t possessionApplyTimerSec = 0.f;
    f32_t possessionApplyDelaySec = 0.72f;
    EntityID possessedTarget = NULL_ENTITY;
    eChampion possessionChampion = eChampion::END;

    SkillRankComponent originalSkillRanks{};
    u8_t reservedOriginalStateAlignment = 0u;
    SkillStateComponent originalSkillState{};
    bool_t bHasOriginalSkillRanks = false;
    bool_t bHasOriginalSkillState = false;
    u8_t reservedTail[2]{};
};

static_assert(sizeof(ViegoSimComponent) == 132u);
static_assert(offsetof(ViegoSimComponent, mistTimerSec) == 4u);
static_assert(offsetof(ViegoSimComponent, possessionApplyTimerSec) == 28u);
static_assert(offsetof(ViegoSimComponent, originalSkillState) == 48u);
static_assert(offsetof(ViegoSimComponent, bHasOriginalSkillRanks) == 128u);
