#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"

#include <type_traits>

struct FioraSimComponent
{
    bool_t bBladeworkActive = false;
    f32_t bladeworkTimerSec = 0.f;
    u8_t bladeworkHitsRemaining = 0;
    u8_t bladeworkPendingHitOrdinal = 0;
    f32_t bladeworkDamageBonus = 30.f;

    bool_t bRiposteActive = false;
    bool_t bRiposteCaughtHardCC = false;
    bool_t bRiposteReleasePending = false;
    u8_t riposteSkillRank = 1;
    f32_t riposteTimerSec = 0.f;
    f32_t riposteWindowSec = 0.75f;
    Vec3 riposteDirection{ 0.f, 0.f, 1.f };
    EntityID riposteReleaseTarget = NULL_ENTITY;
    f32_t riposteRange = 6.f;
    f32_t riposteRadius = 0.8f;
    f32_t riposteDamage = 80.f;
    f32_t riposteSlowDurationSec = 1.5f;
    f32_t riposteSlowMoveSpeedMul = 0.5f;
    f32_t riposteStunDurationSec = 1.5f;

    bool_t bPassiveVitalActive = false;
    u8_t passiveVitalDirection = 0;
    f32_t passiveVitalTimerSec = 0.f;
    f32_t passiveRespawnTimerSec = 0.f;
    EntityID passiveVitalTarget = NULL_ENTITY;

    bool_t bGrandChallengeActive = false;
    u8_t grandChallengeActiveMask = 0;
    u8_t grandChallengeRank = 1;
    f32_t grandChallengeTimerSec = 0.f;
    EntityID grandChallengeTarget = NULL_ENTITY;

    bool_t bGrandChallengeHealActive = false;
    f32_t grandChallengeHealTimerSec = 0.f;
    f32_t grandChallengeHealTickTimerSec = 0.f;
    Vec3 grandChallengeHealCenter{};
};

static_assert(std::is_trivially_copyable_v<FioraSimComponent>);
