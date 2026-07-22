#pragma once

#include "GameObject/ChampionVisualData.h"
#include "GameObject/SkillDef.h"
#include "GameObject/SkillVisualData.h"
#include "Shared/GameSim/Definitions/SkillDefGameDataAdapter.h"

namespace SkillDefAdapters
{
    inline u8_t ResolveVisualStageCount(const SkillDef& def)
    {
        const u8_t gameplayStageCount =
            SkillDefAdapters::ClampSkillStageCount(def.stageCount);
        return def.stage2AnimKey ? 2u : gameplayStageCount;
    }

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
        data.stageCount = ResolveVisualStageCount(def);
        data.vfxKey = def.vfxKey;
        data.sfxKey = def.sfxKey;
        data.endTransitionIdleAnim = def.endTransitionIdleAnim;
        data.endTransitionRunAnim = def.endTransitionRunAnim;
        data.endTransitionDuration = def.endTransitionDuration;

        SkillVisualStageData& stage1 = data.stages[0];
        stage1.stage = 1;
        stage1.animationKey = def.animKey;
        stage1.playbackSpeed = def.visualPlaySpeed;
        stage1.bLoop = false;
        AppendSkillVisualEvent(stage1, eVisualEventKind::KeySwap, 0.f, def.keySwapHookId);
        AppendSkillVisualEvent(stage1, eVisualEventKind::CastAccepted, 0.f, def.onCastAcceptedHookId);
        AppendSkillVisualEvent(stage1, eVisualEventKind::Cast, def.visualCastFrame, def.castHookId);
        AppendSkillVisualEvent(stage1, eVisualEventKind::Recovery, def.visualRecoveryFrame, def.recoveryHookId);

        SkillVisualStageData& stage2 = data.stages[1];
        stage2.stage = 2;
        stage2.animationKey = def.stage2AnimKey;
        stage2.playbackSpeed = def.stage2VisualPlaySpeed;
        stage2.bLoop = false;
        AppendSkillVisualEvent(stage2, eVisualEventKind::Cast, def.stage2VisualCastFrame, def.castHookId);
        AppendSkillVisualEvent(stage2, eVisualEventKind::Recovery, def.stage2VisualRecoveryFrame, def.recoveryHookId);

        return data;
    }

    inline ChampionActionVisualData BuildChampionActionVisualData(const SkillDef& def)
    {
        ChampionActionVisualData data{};
        data.bValid = true;
        data.actionId = def.slot;
        data.stageCount = ResolveVisualStageCount(def);

        ChampionActionVisualStageData& stage1 = data.stages[0];
        stage1.stage = 1;
        stage1.animationKey = def.animKey;
        stage1.playbackSpeed = def.visualPlaySpeed;
        stage1.bLoop = false;
        AppendVisualEvent(stage1, eVisualEventKind::KeySwap, 0.f, def.keySwapHookId);
        AppendVisualEvent(stage1, eVisualEventKind::CastAccepted, 0.f, def.onCastAcceptedHookId);
        AppendVisualEvent(stage1, eVisualEventKind::Cast, def.visualCastFrame, def.castHookId);
        AppendVisualEvent(stage1, eVisualEventKind::Recovery, def.visualRecoveryFrame, def.recoveryHookId);

        ChampionActionVisualStageData& stage2 = data.stages[1];
        stage2.stage = 2;
        stage2.animationKey = def.stage2AnimKey;
        stage2.playbackSpeed = def.stage2VisualPlaySpeed;
        stage2.bLoop = false;
        AppendVisualEvent(stage2, eVisualEventKind::Cast, def.stage2VisualCastFrame, def.castHookId);
        AppendVisualEvent(stage2, eVisualEventKind::Recovery, def.stage2VisualRecoveryFrame, def.recoveryHookId);

        return data;
    }
}
