#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/ProjectileKindComponent.h"

#include <array>

inline constexpr u16_t kMaxPiercingProjectileHits = 512u;

enum class eProjectileUnitHitPolicy : u8_t
{
    Destroy = 0,
    Pierce = 1,
};

enum eProjectileTargetKindMask : u8_t
{
    ProjectileTarget_None = 0,
    ProjectileTarget_Champion = 1u << 0,
    ProjectileTarget_MinionOrSummon = 1u << 1,
    ProjectileTarget_JungleMonster = 1u << 2,
    ProjectileTarget_Structure = 1u << 3,
};

inline constexpr u8_t kProjectileTargetMobileUnits =
    ProjectileTarget_Champion |
    ProjectileTarget_MinionOrSummon |
    ProjectileTarget_JungleMonster;

struct SkillProjectileComponent
{
    EntityID sourceEntity = NULL_ENTITY;
    EntityID targetEntity = NULL_ENTITY;
    EntityHandle sourceHandle = NULL_ENTITY_HANDLE;
    EntityHandle targetHandle = NULL_ENTITY_HANDLE;
    u32_t uProjectileNetAtSpawn = 0u;
    u32_t uSourceNetAtSpawn = 0u;
    u32_t uTargetNetAtSpawn = 0u;
    eTeam sourceTeam = eTeam::Neutral;
    eProjectileKind kind = eProjectileKind::Generic;
    eProjectileUnitHitPolicy unitHitPolicy = eProjectileUnitHitPolicy::Destroy;
    u8_t targetKindMask = kProjectileTargetMobileUnits;
    u16_t maxUniqueHits = 1u;
    bool_t bEpicMonstersOnly = false;
    bool_t bCollidesWithTerrain = true;
    bool_t bBlockedByProjectileBarriers = true;
    bool_t bPersistAfterSourceDeath = false;
    bool_t bApplyDamageOnHit = true;
    u16_t skillId = 0;
    u8_t rank = 1;
    bool_t bSpawned = false;

    Vec3 currentPos{};
    Vec3 direction{ 0.f, 0.f, 1.f };
    f32_t speed = 0.f;
    f32_t maxDistance = 0.f;
    f32_t traveledDistance = 0.f;
    f32_t hitRadius = 0.5f;
    f32_t damage = 0.f;
    f32_t paidManaCost = -1.f;
    f32_t totalAdRatio = 0.f;
    f32_t bonusAdRatio = 0.f;
    f32_t apRatio = 0.f;
    eDamageType damageType = eDamageType::Physical;
    eDamageSourceKind damageSourceKind = eDamageSourceKind::Skill;
    u8_t sourceSlot = 0u;
    u32_t damageFlags = DamageFlag_OnHit;

    bool_t bApplyOnHitStatus = false;
    StatusEffectApplyDesc onHitStatus{};
    EntityID sharedHitLedgerEntity = NULL_ENTITY;

    std::array<EntityHandle, kMaxPiercingProjectileHits> hitEntities{};
    u16_t hitEntityCount = 0u;
    u16_t uContactOrdinal = 0u;
};
