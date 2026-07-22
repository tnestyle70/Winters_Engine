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
    f32_t speed = 0.f;
    f32_t hitRadius = 0.f;
    f32_t forwardOffset = 0.f;
    f32_t spawnHeight = 0.f;
    f32_t maxDistancePadding = 0.f;
};

struct MinionBehaviorDef
{
    f32_t pathAgentRadius = 0.f;
    f32_t laneClearanceRadius = 0.f;
    f32_t softSeparationRadiusScale = 0.f;
    f32_t softSeparationWeight = 0.f;
    f32_t defaultSeparationWeight = 0.f;
    f32_t softSeparationMaxStep = 0.f;
    f32_t lanePathRebuildIntervalSec = 0.f;
    f32_t chasePathRebuildIntervalSec = 0.f;
    f32_t pathTargetRefreshDistanceSq = 0.f;
    f32_t pathWaypointArriveRadius = 0.f;
    u32_t pathBuildBudgetPerTick = 0u;
    u8_t blockedFramesBeforeRepath = 0u;
    u8_t flowFieldStallFramesBeforePathFallback = 0u;
    f32_t flowFieldProgressSlackSq = 0.f;
    f32_t structureAcquireRangePadding = 0.f;
    f32_t targetScanIntervalSec = 0.f;
    u32_t targetScanStaggerBuckets = 0u;
    u8_t rangedRoleType = 0u;
    f32_t attackExitRangePadding = 0.f;
    f32_t meleeAttackWindupSec = 0.f;
    f32_t rangedAttackWindupSec = 0.f;
    f32_t attackRecoverySec = 0.f;
};

struct MinionSpawnSlotDef
{
    u8_t roleType = 0u;
    f32_t forwardOffset = 0.f;
    f32_t sideOffset = 0.f;
};

// 기본값 = 기존 서버 하드코딩(ServerMinionTuning/GameRoomSpawn/GameRoomUnitAI).
// corpseDeathTimerSec 만 1.5 로 단일화(서버 1.2 vs 공용 1.5 드리프트 해소).
struct MinionWaveDef
{
    u64_t waveIntervalTicks = 0u;
    u64_t initialDelayTicks = 0u;
    u64_t perMinionDelayTicks = 0u;
    u32_t siegeWavePeriod = 0u;
    u32_t timeGrowthCapMinutes = 0u;
    f32_t timeGrowthPerMinute = 0.f;
    f32_t corpseDeathTimerSec = 0.f;
    MinionWaveRangedProjectileDef rangedProjectile{};
    f32_t startX = 0.f;
    MinionSpawnSlotDef formationSlots[6]{};
    u8_t formationSlotCount = 0u;
    MinionSpawnSlotDef siegeSlot{};
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
    MinionBehaviorDef minionBehavior{};
    // aggregate init 호환을 위해 맨 뒤 append. 구 코드젠 팩은 기본값(= 기존 하드코딩)으로 남는다.
    MinionWaveDef minionWave{};

    f32_t ResolveStructureMaxHp(eStructureKind kind) const;
    const JungleCampGameDef* FindJungleCamp(u8_t subKind) const;
    JungleCampGameDef ResolveJungleCamp(u8_t subKind) const;
    const MinionCombatDef* FindMinion(u8_t roleType) const;
    MinionCombatDef ResolveMinion(u8_t roleType) const;
};
