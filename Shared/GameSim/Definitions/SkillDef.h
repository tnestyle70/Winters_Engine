#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "GameContext.h"

#include <cstdint>

enum class eTargetMode : uint8_t
{
    Self,
    UnitTarget,
    GroundTarget,
    Direction,
    Conditional,
};

enum class eSkillSlot : uint8_t
{
    BasicAttack = 0,
    Q = 1,
    W = 2,
    E = 3,
    R = 4,
    SLOT_END = 5,
};

enum class eRotateMode : uint8_t
{
    None,
    TowardsTarget,
    TowardsCursor,
};

struct SkillDef
{
    eChampion   champ = eChampion::END;
    uint8_t     slot = 0;
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

    f32_t castFrame = 0.f;
    f32_t recoveryFrame = 0.f;
    f32_t stage2CastFrame = 0.f;
    f32_t stage2RecoveryFrame = 0.f;

    f32_t animPlaySpeed = 1.f;
    f32_t stage2PlaySpeed = 1.f;

    const char* endTransitionIdleAnim = nullptr;
    const char* endTransitionRunAnim = nullptr;
    f32_t       endTransitionDuration = 0.1f;

    uint32_t keySwapHookId = 0;
    uint32_t onCastAcceptedHookId = 0;
    uint32_t castFrameHookId = 0;
    uint32_t recoveryHookId = 0;

    uint16_t skillId = 0;
    uint16_t scalingTableId = 0;
};

struct CastSkillCommand
{
    uint8_t   slot = 0;
    uint8_t   resolvedTargetMode = 0;
    uint16_t  _pad = 0;

    EntityID  targetEntityId = NULL_ENTITY;
    Vec3      groundPos{ 0.f, 0.f, 0.f };
    Vec3      direction{ 0.f, 0.f, 0.f };
};

extern const SkillDef* const g_SkillTable;
extern const uint32_t        g_SkillCount;

const SkillDef* FindSkillDef(eChampion champ, uint8_t slot);
