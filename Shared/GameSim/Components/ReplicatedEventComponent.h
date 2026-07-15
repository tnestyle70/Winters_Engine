#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
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

enum class ProjectileContactReason : u8_t
{
    None = 0,
    UnitHit,
    Barrier,
    Terrain,
    RangeExpired,
    SourceInvalid,
    TargetInvalid,
    InvalidTrajectory,
    HitLimit,
};

inline constexpr u32_t kGlobalEffectFlashBlink = 0xF1A50001u;
// S030: 넥서스 파괴 게임 종료 알림 — EffectTrigger 글로벌 이벤트로 복제, flags = 승리 팀(0/1).
inline constexpr u32_t kGlobalGameEndEffect = 0xF1A50002u;
inline constexpr u32_t kEzrealEffectArcaneShiftBlink = 0x455A4501u;
inline constexpr u32_t kEzrealEffectEssenceFluxMark = 0x455A5701u;
inline constexpr u32_t kEzrealEffectEssenceFluxDetonate = 0x455A5702u;
inline constexpr u32_t kEzrealEffectEssenceFluxClear = 0x455A5703u;

struct ReplicatedEventComponent
{
    eReplicatedEventKind kind = eReplicatedEventKind::None;

    EntityID sourceEntity = NULL_ENTITY;
    EntityID targetEntity = NULL_ENTITY;
    EntityID projectileEntity = NULL_ENTITY;
    u32_t sourceNetOverride = 0u;
    u32_t targetNetOverride = 0u;
    u32_t projectileNetOverride = 0u;

    u32_t effectId = 0;
    u16_t skillId = 0;
    u16_t projectileKind = 0;
    u16_t attachBone = 0;
    u16_t durationMs = 0;
    u16_t flags = 0;
    u16_t uContactOrdinal = 0u;

    u8_t slot = 0;
    u8_t rank = 0;
    eDamageType damageType = eDamageType::Physical;
    ProjectileContactReason eContactReason = ProjectileContactReason::None;
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
