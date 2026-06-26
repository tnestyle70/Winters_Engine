#pragma once

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "WintersTypes.h"

namespace GameplayFeedback
{
    enum class WorldTextFeedbackKind : u16_t
    {
        None = 0,
        Dodge = 1,
        Slow = 2,
        Stun = 3,
        Airborne = 4,
        Disarm = 5,
        Untargetable = 6,
        Invisible = 7,
        Shield = 8,
        Heal = 9,
        Crit = 10,
        Gold = 11,
    };

    inline constexpr u32_t kWorldTextEffectBase = 0x7f000000u;
    inline constexpr u32_t kWorldTextEffectMask = 0xffff0000u;
    inline constexpr u32_t kMaxWorldTextGoldAmount = 0xffffu;

    inline constexpr u32_t BuildWorldTextEffectId(WorldTextFeedbackKind kind)
    {
        return kWorldTextEffectBase | static_cast<u32_t>(kind);
    }

    inline constexpr u16_t PackWorldTextGoldAmount(u32_t amount)
    {
        return static_cast<u16_t>(
            amount > kMaxWorldTextGoldAmount ? kMaxWorldTextGoldAmount : amount);
    }

    inline constexpr u32_t UnpackWorldTextGoldAmount(u16_t flags)
    {
        return static_cast<u32_t>(flags);
    }

    inline bool_t TryResolveWorldTextEffectId(
        u32_t effectId,
        WorldTextFeedbackKind& outKind)
    {
        if ((effectId & kWorldTextEffectMask) != kWorldTextEffectBase)
        {
            outKind = WorldTextFeedbackKind::None;
            return false;
        }

        const WorldTextFeedbackKind kind =
            static_cast<WorldTextFeedbackKind>(effectId & 0xffffu);
        switch (kind)
        {
        case WorldTextFeedbackKind::Dodge:
        case WorldTextFeedbackKind::Slow:
        case WorldTextFeedbackKind::Stun:
        case WorldTextFeedbackKind::Airborne:
        case WorldTextFeedbackKind::Disarm:
        case WorldTextFeedbackKind::Untargetable:
        case WorldTextFeedbackKind::Invisible:
        case WorldTextFeedbackKind::Shield:
        case WorldTextFeedbackKind::Heal:
        case WorldTextFeedbackKind::Crit:
        case WorldTextFeedbackKind::Gold:
            outKind = kind;
            return true;
        default:
            outKind = WorldTextFeedbackKind::None;
            return false;
        }
    }

    inline WorldTextFeedbackKind ResolveStatusFeedbackKind(
        eStatusEffectId effectId,
        u32_t stateFlags)
    {
        if ((stateFlags & kGameplayStateAirborneFlag) != 0u)
            return WorldTextFeedbackKind::Airborne;
        if ((stateFlags & kGameplayStateStunnedFlag) != 0u)
            return WorldTextFeedbackKind::Stun;
        if ((stateFlags & kGameplayStateSlowedFlag) != 0u)
            return WorldTextFeedbackKind::Slow;
        if ((stateFlags & kGameplayStateDisarmedFlag) != 0u)
            return WorldTextFeedbackKind::Disarm;
        if ((stateFlags & kGameplayStateUntargetableFlag) != 0u)
            return WorldTextFeedbackKind::Untargetable;
        if ((stateFlags & kGameplayStateInvisibleFlag) != 0u)
            return WorldTextFeedbackKind::Invisible;

        switch (effectId)
        {
        case eStatusEffectId::GenericAirborne:
            return WorldTextFeedbackKind::Airborne;
        case eStatusEffectId::GenericStun:
        case eStatusEffectId::AsheCrystalArrowStun:
            return WorldTextFeedbackKind::Stun;
        case eStatusEffectId::GenericSlow:
        case eStatusEffectId::AsheVolleySlow:
            return WorldTextFeedbackKind::Slow;
        case eStatusEffectId::GenericDisarm:
            return WorldTextFeedbackKind::Disarm;
        case eStatusEffectId::ViegoMist:
            return WorldTextFeedbackKind::Invisible;
        case eStatusEffectId::KalistaFateCallUntargetable:
            return WorldTextFeedbackKind::Untargetable;
        default:
            return WorldTextFeedbackKind::None;
        }
    }
}
