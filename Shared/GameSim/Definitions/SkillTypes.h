#pragma once

#include <cstdint>

enum class eTargetShape : uint8_t
{
    Self,
    Unit,
    Ground,
    Direction,
};

enum class eTargetResolvePolicy : uint8_t
{
    Direct,
    Contextual,
    StageDependent,
    ChampionStateDependent,
};

enum class eSkillFacingMode : uint8_t
{
    None,
    TowardsTarget,
    TowardsCommandDirection,
};

enum class eSkillInputActivation : uint8_t
{
    Press = 0,
    PressRecast = 1,
    PressRelease = 2,
};

enum class eSkillActionMovePolicy : uint8_t
{
    Allow = 0,
    QueueUntilUnlock = 1,
    StationaryChannel = 2,
    ForcedMotion = 3,
};

// Legacy compatibility. Delete after SkillDef/SkillTable readers are gone.
enum class eTargetMode : uint8_t
{
    Self,
    UnitTarget,
    GroundTarget,
    Direction,
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

// Legacy compatibility. Delete after Scene_InGame stops reading SkillDef.
enum class eRotateMode : uint8_t
{
    None,
    TowardsTarget,
    TowardsCursor,
};
