#pragma once

#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
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

    inline bool_t ApplyAirborne(
        CWorld& world,
        EntityID target,
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec,
        f32_t arcHeight = 2.1f,
        const Vec3* landingPosition = nullptr)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            durationSec <= 0.f ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        Vec3 landing = world.GetComponent<TransformComponent>(target).GetPosition();
        if (landingPosition)
            landing = *landingPosition;
        else if (world.HasComponent<ForcedMotionComponent>(target))
            landing = world.GetComponent<ForcedMotionComponent>(target).end;

        if (!TryApplyStatusEffect(
            world,
            target,
            MakeAirborneDesc(source, champion, slot, durationSec)))
        {
            return false;
        }
        return StartAirborneMotion(
            world,
            target,
            source,
            landing,
            durationSec,
            arcHeight,
            landingPosition != nullptr);
    }

    inline bool_t ApplyAirborne(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec,
        f32_t arcHeight = 2.1f,
        const Vec3* landingPosition = nullptr)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            durationSec <= 0.f ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        Vec3 landing = world.GetComponent<TransformComponent>(target).GetPosition();
        if (landingPosition)
            landing = *landingPosition;
        else if (world.HasComponent<ForcedMotionComponent>(target))
            landing = world.GetComponent<ForcedMotionComponent>(target).end;

        if (!TryApplyStatusEffect(
            world,
            target,
            MakeAirborneDesc(source, champion, slot, durationSec),
            tc))
        {
            return false;
        }
        return StartAirborneMotion(
            world,
            target,
            source,
            landing,
            durationSec,
            arcHeight,
            landingPosition != nullptr);
    }
}
