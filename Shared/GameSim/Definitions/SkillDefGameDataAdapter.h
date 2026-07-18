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
            return eTargetShape::Direction;
        case eTargetMode::Self:
        default:
            return eTargetShape::Self;
        }
    }

    inline eTargetResolvePolicy ToTargetResolvePolicy(eTargetMode mode)
    {
        (void)mode;
        return eTargetResolvePolicy::Direct;
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
        data.input.activation = def.inputActivation;
        data.target.bValid = true;
        data.target.shape[0] = ToTargetShape(def.targetMode);
        data.target.shape[1] = ToTargetShape(def.stage2TargetMode);
        data.target.resolvePolicy = ToTargetResolvePolicy(def.targetMode);
        data.cost.rankCount = 1u;
        data.cost.manaCostByRank[0] = def.manaCost;
        data.cost.manaCost = def.manaCost;
        data.cooldown.rankCount = 1u;
        data.cooldown.cooldownSecByRank[0] = def.cooldownSec;
        data.cooldown.cooldownSec = def.cooldownSec;
        data.range.rangeMax = def.rangeMax;
        data.stage.stageCount = ClampSkillStageCount(def.stageCount);
        data.stage.stageWindowSec = def.stageWindowSec;
        data.stage.lockDurationSec[0] = def.lockDurationSec;
        data.stage.lockDurationSec[1] = def.stage2LockSec;
        data.stage.commandLockSec[0] = def.commandLockSec;
        data.stage.commandLockSec[1] = def.stage2CommandLockSec;
        data.stage.movePolicy[0] = def.movePolicy;
        data.stage.movePolicy[1] = def.stage2MovePolicy;
        data.stage.bCreatesActionState[0] = def.bCreatesActionState;
        data.stage.bCreatesActionState[1] = def.bStage2CreatesActionState;
        data.stage.bPresentationLoopWhileActive[0] = def.bPresentationLoopWhileActive;
        data.stage.bPresentationLoopWhileActive[1] = def.bStage2PresentationLoopWhileActive;
        data.facing.mode[0] = ToSkillFacingMode(def.rotate);
        data.facing.mode[1] = ToSkillFacingMode(def.stage2Rotate);
        data.effect.scalingTableId = def.scalingTableId;
        data.charge = def.charge;
        return data;
    }

    inline ChampionGameDataSkill BuildChampionGameDataSkill(const SkillDef& def)
    {
        ChampionGameDataSkill data{};
        data.bValid = true;
        data.slot = def.slot;
        data.inputActivation = def.inputActivation;
        data.targetMode = def.targetMode;
        data.stageCount = ClampSkillStageCount(def.stageCount);
        data.stageWindowSec = def.stageWindowSec;
        data.rankCount = 1u;
        data.cooldownSecByRank[0] = def.cooldownSec;
        data.manaCostByRank[0] = def.manaCost;
        data.cooldownSec = def.cooldownSec;
        data.rangeMax = def.rangeMax;
        data.manaCost = def.manaCost;
        data.skillId = def.skillId;
        data.scalingTableId = def.scalingTableId;

        ChampionGameDataSkillStage& stage1 = data.stages[0];
        stage1.lockDurationSec = def.lockDurationSec;
        stage1.targetMode = def.targetMode;
        stage1.commandLockSec = def.commandLockSec;
        stage1.movePolicy = def.movePolicy;
        stage1.bCreatesActionState = def.bCreatesActionState;
        stage1.bPresentationLoopWhileActive = def.bPresentationLoopWhileActive;

        ChampionGameDataSkillStage& stage2 = data.stages[1];
        stage2.lockDurationSec = def.stage2LockSec;
        stage2.targetMode = def.stage2TargetMode;
        stage2.commandLockSec = def.stage2CommandLockSec;
        stage2.movePolicy = def.stage2MovePolicy;
        stage2.bCreatesActionState = def.bStage2CreatesActionState;
        stage2.bPresentationLoopWhileActive = def.bStage2PresentationLoopWhileActive;

        data.charge = def.charge;

        return data;
    }
}
