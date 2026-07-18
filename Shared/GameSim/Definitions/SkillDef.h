#pragma once

#include "SkillCommand.h"
#include "SkillAtomData.h"
#include "SkillTypes.h"
#include "WintersTypes.h"
#include "LoLMatchContext.h"

#include <cstdint>

struct SkillDef
{
    eChampion   champ = eChampion::END;
    uint8_t     slot = 0;
    eSkillInputActivation inputActivation = eSkillInputActivation::Press;
    eTargetMode targetMode = eTargetMode::Self;

    f32_t       cooldownSec = 0.f;
    f32_t       rangeMax = 0.f;
    f32_t       manaCost = 0.f;

    const char* animKey = nullptr;
    const char* vfxKey = nullptr;
    const char* sfxKey = nullptr;

    f32_t       lockDurationSec = 0.4f;
    bool        bOneShot = true;
    eRotateMode rotate = eRotateMode::None;

    uint8_t     stageCount = 1;
    eTargetMode stage2TargetMode = eTargetMode::Self;
    const char* stage2AnimKey = nullptr;
    f32_t       stage2LockSec = 0.f;
    eRotateMode stage2Rotate = eRotateMode::None;
    f32_t       stageWindowSec = 0.f;

    f32_t       commandLockSec = 0.f;
    f32_t       stage2CommandLockSec = 0.f;
    eSkillActionMovePolicy movePolicy = eSkillActionMovePolicy::Allow;
    eSkillActionMovePolicy stage2MovePolicy = eSkillActionMovePolicy::Allow;
    bool_t      bCreatesActionState = true;
    bool_t      bStage2CreatesActionState = true;
    bool_t      bPresentationLoopWhileActive = false;
    bool_t      bStage2PresentationLoopWhileActive = false;
    SkillChargeSpec charge{};

    f32_t visualCastFrame = 0.f;
    f32_t visualRecoveryFrame = 0.f;
    f32_t stage2VisualCastFrame = 0.f;
    f32_t stage2VisualRecoveryFrame = 0.f;

    f32_t visualPlaySpeed = 1.f;
    f32_t stage2VisualPlaySpeed = 1.f;

    const char* endTransitionIdleAnim = nullptr;
    const char* endTransitionRunAnim = nullptr;
    f32_t       endTransitionDuration = 0.1f;

    uint32_t keySwapHookId = 0;
    uint32_t onCastAcceptedHookId = 0;
    uint32_t castHookId = 0;
    uint32_t recoveryHookId = 0;

    uint16_t skillId = 0;
    uint16_t scalingTableId = 0;
};

const SkillDef* FindSkillDef(eChampion champ, uint8_t slot);
