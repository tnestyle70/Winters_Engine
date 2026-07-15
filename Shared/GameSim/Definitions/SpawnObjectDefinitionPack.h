#pragma once

#include <cstddef>

#include "Shared/GameSim/Definitions/ChampionColliderProfileDef.h"
#include "Shared/GameSim/Definitions/DataPackManifest.h"
#include "Shared/GameSim/Definitions/JungleCampGameDef.h"
#include "Shared/GameSim/Definitions/MinionCombatDef.h"
#include "Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h"
#include "Shared/GameSim/Definitions/StructureGameDef.h"

struct JungleCampGameDefEntry
{
    u8_t subKind = 0u;
    JungleCampGameDef value{};
};

struct MinionCombatDefEntry
{
    u8_t roleType = 0u;
    MinionCombatDef value{};
};

struct MinionWaveRangedProjectileDef
{
    f32_t speed = 14.f;
    f32_t hitRadius = 0.45f;
    f32_t forwardOffset = 0.45f;
    f32_t spawnHeight = 0.85f;
    f32_t maxDistancePadding = 2.f;
};

// 기본값 = 기존 서버 하드코딩(ServerMinionTuning/GameRoomSpawn/GameRoomUnitAI).
// corpseDeathTimerSec 만 1.5 로 단일화(서버 1.2 vs 공용 1.5 드리프트 해소).
struct MinionWaveDef
{
    u64_t waveIntervalTicks = 900u;
    u64_t initialDelayTicks = 300u;
    u64_t perMinionDelayTicks = 10u;
    u32_t siegeWavePeriod = 3u;
    u32_t timeGrowthCapMinutes = 30u;
    f32_t timeGrowthPerMinute = 0.025f;
    f32_t corpseDeathTimerSec = 1.5f;
    MinionWaveRangedProjectileDef rangedProjectile{};
};

struct SpawnObjectDefinitionPack
{
    DataPackManifest manifest{};
    SpawnLoadoutPolicyDef spawnLoadout{};
    ChampionColliderProfileDef championCollider{};
    StructureGameDef structure{};
    JungleCampGameDef defaultJungleCamp{};
    const JungleCampGameDefEntry* jungleCamps = nullptr;
    std::size_t jungleCampCount = 0u;
    MinionCombatDef defaultMinion{};
    const MinionCombatDefEntry* minions = nullptr;
    std::size_t minionCount = 0u;
    // aggregate init 호환을 위해 맨 뒤 append. 구 코드젠 팩은 기본값(= 기존 하드코딩)으로 남는다.
    MinionWaveDef minionWave{};

    f32_t ResolveStructureMaxHp(eStructureKind kind) const;
    const JungleCampGameDef* FindJungleCamp(u8_t subKind) const;
    JungleCampGameDef ResolveJungleCamp(u8_t subKind) const;
    const MinionCombatDef* FindMinion(u8_t roleType) const;
    MinionCombatDef ResolveMinion(u8_t roleType) const;
};
