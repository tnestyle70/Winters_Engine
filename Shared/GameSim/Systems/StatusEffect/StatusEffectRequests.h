#pragma once

#include "GameContext.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"

namespace GameplayStatus
{
    inline constexpr u16_t MakeStatusStackGroup(eChampion champion, eSkillSlot slot)
    {
        return static_cast<u16_t>(
            (static_cast<u16_t>(static_cast<u8_t>(champion)) << 8) |
            static_cast<u16_t>(static_cast<u8_t>(slot)));
    }

    inline StatusEffectApplyDesc MakeStunDesc(
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec,
        eStatusEffectId effectId = eStatusEffectId::GenericStun)
    {
        StatusEffectApplyDesc desc{};
        desc.effectId = effectId;
        desc.stackPolicy = eStatusStackPolicy::RefreshDuration;
        desc.sourceEntity = source;
        desc.stackGroup = MakeStatusStackGroup(champion, slot);
        desc.stateFlags =
            kGameplayStateStunnedFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag;
        desc.fDurationSec = durationSec;
        desc.fMoveSpeedMul = 1.f;
        return desc;
    }

    inline StatusEffectApplyDesc MakeSlowDesc(
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec,
        f32_t moveSpeedMul,
        eStatusEffectId effectId = eStatusEffectId::GenericSlow)
    {
        StatusEffectApplyDesc desc{};
        desc.effectId = effectId;
        desc.stackPolicy = eStatusStackPolicy::RefreshDuration;
        desc.sourceEntity = source;
        desc.stackGroup = MakeStatusStackGroup(champion, slot);
        desc.stateFlags = kGameplayStateSlowedFlag;
        desc.fDurationSec = durationSec;
        desc.fMoveSpeedMul = moveSpeedMul;
        return desc;
    }

    inline StatusEffectApplyDesc MakeAirborneDesc(
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec)
    {
        StatusEffectApplyDesc desc{};
        desc.effectId = eStatusEffectId::GenericAirborne;
        desc.stackPolicy = eStatusStackPolicy::RefreshDuration;
        desc.sourceEntity = source;
        desc.stackGroup = MakeStatusStackGroup(champion, slot);
        desc.stateFlags =
            kGameplayStateAirborneFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag;
        desc.fDurationSec = durationSec;
        desc.fMoveSpeedMul = 1.f;
        return desc;
    }

    inline void ApplyStun(
        CWorld& world,
        EntityID target,
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec,
        eStatusEffectId effectId = eStatusEffectId::GenericStun)
    {
        ApplyStatusEffect(
            world,
            target,
            MakeStunDesc(source, champion, slot, durationSec, effectId));
    }

    inline void ApplyStun(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec,
        eStatusEffectId effectId = eStatusEffectId::GenericStun)
    {
        ApplyStatusEffect(
            world,
            target,
            MakeStunDesc(source, champion, slot, durationSec, effectId),
            tc);
    }

    inline void ApplySlow(
        CWorld& world,
        EntityID target,
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec,
        f32_t moveSpeedMul,
        eStatusEffectId effectId = eStatusEffectId::GenericSlow)
    {
        ApplyStatusEffect(
            world,
            target,
            MakeSlowDesc(source, champion, slot, durationSec, moveSpeedMul, effectId));
    }

    inline void ApplySlow(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec,
        f32_t moveSpeedMul,
        eStatusEffectId effectId = eStatusEffectId::GenericSlow)
    {
        ApplyStatusEffect(
            world,
            target,
            MakeSlowDesc(source, champion, slot, durationSec, moveSpeedMul, effectId),
            tc);
    }

    inline void ApplyAirborne(
        CWorld& world,
        EntityID target,
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec)
    {
        ApplyStatusEffect(
            world,
            target,
            MakeAirborneDesc(source, champion, slot, durationSec));
    }

    inline void ApplyAirborne(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec)
    {
        ApplyStatusEffect(
            world,
            target,
            MakeAirborneDesc(source, champion, slot, durationSec),
            tc);
    }
}
