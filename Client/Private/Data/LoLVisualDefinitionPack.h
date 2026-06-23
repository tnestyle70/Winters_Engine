#pragma once

#include "GameContext.h"
#include "Shared/GameSim/Definitions/DefinitionIds.h"
#include "WintersTypes.h"

namespace ClientData
{
    inline constexpr u8_t kVisualSkillSlotCount = 5u;
    inline constexpr u8_t kVisualSkillStageCount = 2u;

    struct SkillVisualStageDef
    {
        f32_t animationPlaybackSpeed = 1.f;
        f32_t castFrame = 0.f;
        f32_t recoveryFrame = 0.f;
    };

    struct SkillVisualDefinition
    {
        u8_t stageCount = 1u;
        ReplicatedCueId replicatedCueId = 0u;
        SkillVisualStageDef stages[kVisualSkillStageCount] = {};
    };

    struct ChampionVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        eChampion legacyChampion = eChampion::END;
        f32_t modelYawOffsetRadians = 0.f;
        SkillVisualDefinition skills[kVisualSkillSlotCount] = {};
    };

    const ChampionVisualDefinition* FindChampionVisualDefinition(eChampion champion);
    f32_t ResolveChampionModelYawOffset(eChampion champion);
}
