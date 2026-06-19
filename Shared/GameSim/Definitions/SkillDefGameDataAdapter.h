#pragma once

#include "ChampionGameData.h"
#include "SkillAtomData.h"
#include "SkillDef.h"

namespace SkillDefAdapters
{
    inline u8_t ClampSkillStageCount(u8_t stageCount)
    {
        if (stageCount == 0)
        {
            return 1;
        }

        return stageCount > kSkillAtomStageMax
            ? kSkillAtomStageMax
            : stageCount;
    }

    inline eTargetShape ToTargetShape(eTargetMode mode)
    {
        switch (mode)
        {
        case eTargetMode::UnitTarget:
            return eTargetShape::Unit;
        case eTargetMode::GroundTarget:
            return eTargetShape::Ground;
        case eTargetMode::Direction:
        case eTargetMode::Conditional:
            return eTargetShape::Direction;
        case eTargetMode::Self:
        default:
            return eTargetShape::Self;
        }
    }

    inline eTargetResolvePolicy ToTargetResolvePolicy(eTargetMode mode)
    {
        return mode == eTargetMode::Conditional
            ? eTargetResolvePolicy::ChampionStateDependent
            : eTargetResolvePolicy::Direct;
    }

    inline eSkillFacingMode ToSkillFacingMode(eRotateMode mode)
    {
        switch (mode)
        {
        case eRotateMode::TowardsTarget:
            return eSkillFacingMode::TowardsTarget;
        case eRotateMode::TowardsCursor:
            return eSkillFacingMode::TowardsCommandDirection;
        case eRotateMode::None:
        default:
            return eSkillFacingMode::None;
        }
    }

    inline SkillGameAtomBundle BuildSkillGameAtomBundle(const SkillDef& def)
    {
        SkillGameAtomBundle data{};
        data.bValid = true;
        data.slot.bValid = true;
        data.slot.champion = def.champ;
        data.slot.slot = def.slot;
        data.slot.skillId = def.skillId;
        data.target.bValid = true;
        data.target.shape[0] = ToTargetShape(def.targetMode);
        data.target.shape[1] = ToTargetShape(def.stage2TargetMode);
        data.target.resolvePolicy = ToTargetResolvePolicy(def.targetMode);
        data.cost.manaCost = def.manaCost;
        data.cooldown.cooldownSec = def.cooldownSec;
        data.range.rangeMax = def.rangeMax;
        data.stage.stageCount = ClampSkillStageCount(def.stageCount);
        data.stage.stageWindowSec = def.stageWindowSec;
        data.stage.lockDurationSec[0] = def.lockDurationSec;
        data.stage.lockDurationSec[1] = def.stage2LockSec;
        data.facing.mode[0] = ToSkillFacingMode(def.rotate);
        data.facing.mode[1] = ToSkillFacingMode(def.stage2Rotate);
        data.effect.scalingTableId = def.scalingTableId;
        return data;
    }

    inline ChampionGameDataSkill BuildChampionGameDataSkill(const SkillDef& def)
    {
        ChampionGameDataSkill data{};
        data.bValid = true;
        data.slot = def.slot;
        data.targetMode = def.targetMode;
        data.stageCount = ClampSkillStageCount(def.stageCount);
        data.stageWindowSec = def.stageWindowSec;
        data.cooldownSec = def.cooldownSec;
        data.rangeMax = def.rangeMax;
        data.manaCost = def.manaCost;
        data.skillId = def.skillId;
        data.scalingTableId = def.scalingTableId;

        ChampionGameDataSkillStage& stage1 = data.stages[0];
        stage1.lockDurationSec = def.lockDurationSec;
        stage1.animPlaySpeed = def.animPlaySpeed;
        stage1.castFrame = def.castFrame;
        stage1.recoveryFrame = def.recoveryFrame;

        ChampionGameDataSkillStage& stage2 = data.stages[1];
        stage2.lockDurationSec = def.stage2LockSec;
        stage2.animPlaySpeed = def.stage2PlaySpeed;
        stage2.castFrame = def.stage2CastFrame;
        stage2.recoveryFrame = def.stage2RecoveryFrame;

        return data;
    }
}
