#pragma once

#include "GameObject/ChampionVisualData.h"
#include "GameObject/SkillDef.h"
#include "GameObject/SkillVisualData.h"
#include "Shared/GameSim/Definitions/SkillDefGameDataAdapter.h"

namespace SkillDefAdapters
{
    inline void AppendVisualEvent(
        ChampionActionVisualStageData& stage,
        eVisualEventKind kind,
        f32_t frame,
        u32_t hookId)
    {
        if (hookId == 0 || stage.eventCount >= kChampionVisualActionEventMax)
        {
            return;
        }

        VisualEventData& eventData = stage.events[stage.eventCount++];
        eventData.kind = static_cast<u8_t>(kind);
        eventData.frame = frame;
        eventData.hookId = hookId;
    }

    inline void AppendSkillVisualEvent(
        SkillVisualStageData& stage,
        eVisualEventKind kind,
        f32_t frame,
        u32_t hookId)
    {
        if (hookId == 0 || stage.eventCount >= kSkillVisualEventMax)
        {
            return;
        }

        VisualEventData& eventData = stage.events[stage.eventCount++];
        eventData.kind = static_cast<u8_t>(kind);
        eventData.frame = frame;
        eventData.hookId = hookId;
    }

    inline SkillVisualData BuildSkillVisualData(const SkillDef& def)
    {
        SkillVisualData data{};
        data.bValid = true;
        data.slot = def.slot;
        data.stageCount = SkillDefAdapters::ClampSkillStageCount(def.stageCount);
        data.vfxKey = def.vfxKey;
        data.sfxKey = def.sfxKey;
        data.endTransitionIdleAnim = def.endTransitionIdleAnim;
        data.endTransitionRunAnim = def.endTransitionRunAnim;
        data.endTransitionDuration = def.endTransitionDuration;

        SkillVisualStageData& stage1 = data.stages[0];
        stage1.stage = 1;
        stage1.animationKey = def.animKey;
        stage1.playbackSpeed = def.animPlaySpeed;
        stage1.bLoop = false;
        AppendSkillVisualEvent(stage1, eVisualEventKind::KeySwap, 0.f, def.keySwapHookId);
        AppendSkillVisualEvent(stage1, eVisualEventKind::CastAccepted, 0.f, def.onCastAcceptedHookId);
        AppendSkillVisualEvent(stage1, eVisualEventKind::Cast, def.castFrame, def.castFrameHookId);
        AppendSkillVisualEvent(stage1, eVisualEventKind::Recovery, def.recoveryFrame, def.recoveryHookId);

        SkillVisualStageData& stage2 = data.stages[1];
        stage2.stage = 2;
        stage2.animationKey = def.stage2AnimKey;
        stage2.playbackSpeed = def.stage2PlaySpeed;
        stage2.bLoop = false;
        AppendSkillVisualEvent(stage2, eVisualEventKind::Cast, def.stage2CastFrame, def.castFrameHookId);
        AppendSkillVisualEvent(stage2, eVisualEventKind::Recovery, def.stage2RecoveryFrame, def.recoveryHookId);

        return data;
    }

    inline ChampionActionVisualData BuildChampionActionVisualData(const SkillDef& def)
    {
        ChampionActionVisualData data{};
        data.bValid = true;
        data.actionId = def.slot;
        data.stageCount = SkillDefAdapters::ClampSkillStageCount(def.stageCount);

        ChampionActionVisualStageData& stage1 = data.stages[0];
        stage1.stage = 1;
        stage1.animationKey = def.animKey;
        stage1.playbackSpeed = def.animPlaySpeed;
        stage1.bLoop = false;
        AppendVisualEvent(stage1, eVisualEventKind::KeySwap, 0.f, def.keySwapHookId);
        AppendVisualEvent(stage1, eVisualEventKind::CastAccepted, 0.f, def.onCastAcceptedHookId);
        AppendVisualEvent(stage1, eVisualEventKind::Cast, def.castFrame, def.castFrameHookId);
        AppendVisualEvent(stage1, eVisualEventKind::Recovery, def.recoveryFrame, def.recoveryHookId);

        ChampionActionVisualStageData& stage2 = data.stages[1];
        stage2.stage = 2;
        stage2.animationKey = def.stage2AnimKey;
        stage2.playbackSpeed = def.stage2PlaySpeed;
        stage2.bLoop = false;
        AppendVisualEvent(stage2, eVisualEventKind::Cast, def.stage2CastFrame, def.castFrameHookId);
        AppendVisualEvent(stage2, eVisualEventKind::Recovery, def.stage2RecoveryFrame, def.recoveryHookId);

        return data;
    }
}
