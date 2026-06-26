#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"

#include <type_traits>

enum class eReplicatedEventKind : u8_t
{
    None = 0,
    Damage,
    SkillCast,
    EffectTrigger,
    ProjectileSpawn,
    ProjectileHit,
    KillFeed,
};

enum class eKillFeedObjectKind : u8_t
{
    None = 0,
    Champion,
    Turret,
    Inhibitor,
    Dragon,
    Baron,
};

inline constexpr u32_t kGlobalEffectFlashBlink = 0xF1A50001u;

struct ReplicatedEventComponent
{
    eReplicatedEventKind kind = eReplicatedEventKind::None;

    EntityID sourceEntity = NULL_ENTITY;
    EntityID targetEntity = NULL_ENTITY;
    EntityID projectileEntity = NULL_ENTITY;

    u32_t effectId = 0;
    u16_t skillId = 0;
    u16_t projectileKind = 0;
    u16_t attachBone = 0;
    u16_t durationMs = 0;
    u16_t flags = 0;

    u8_t slot = 0;
    u8_t rank = 0;
    eDamageType damageType = eDamageType::Physical;
    bool_t bWasCrit = false;
    bool_t bKilled = false;
    bool_t bDestroyed = false;

    eKillFeedObjectKind killFeedObjectKind = eKillFeedObjectKind::None;
    eChampion sourceChampion = eChampion::NONE;
    eChampion targetChampion = eChampion::NONE;
    u8_t sourceTeam = static_cast<u8_t>(eTeam::Neutral);
    u8_t targetTeam = static_cast<u8_t>(eTeam::Neutral);

    f32_t amount = 0.f;
    f32_t speed = 0.f;
    f32_t maxDistance = 0.f;
    u64_t startTick = 0;

    Vec3 position{};
    Vec3 direction{};
};

static_assert(std::is_trivially_copyable_v<ReplicatedEventComponent>,
    "ReplicatedEventComponent must remain POD for deterministic event ordering.");
