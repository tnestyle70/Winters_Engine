#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"
#include "WintersMath.h"

struct ChampionAIPerception
{
    u64_t factTick = 0u;

    EntityID enemyChampion = NULL_ENTITY;
    EntityID lowHpEnemyChampion = NULL_ENTITY;
    EntityID diveTarget = NULL_ENTITY;
    EntityID enemyMinion = NULL_ENTITY;
    EntityID enemyStructure = NULL_ENTITY;
    EntityID alliedWave = NULL_ENTITY;
    EntityID abilityTarget = NULL_ENTITY;
    EntityID mobilityTarget = NULL_ENTITY;
    EntityID lastSeenEnemyChampion = NULL_ENTITY;

    f32_t selfHpRatio = 1.f;
    f32_t enemyHpRatio = 1.f;
    // V1 names are retained for schema compatibility. These values are the
    // observable purchase value of inventory items, never wallet gold.
    f32_t selfGold = 0.f;
    f32_t enemyGold = 0.f;
    u8_t selfLevel = 1u;
    u8_t enemyLevel = 1u;
    f32_t enemyDistance = 999.f;
    f32_t lowHpEnemyRatio = 1.f;
    f32_t lowHpEnemyDistance = 999.f;
    f32_t attackRange = 1.5f;
    f32_t waveDistance = 999.f;
    f32_t turretDanger = 0.f;
    Vec3 midDefenseAnchor{};
    Vec3 lastSeenEnemyChampionPos{};
    u64_t lastSeenEnemyChampionTick = 0u;
    f32_t lastSeenEnemyAgeSec = 0.f;
    f32_t lastSeenEnemyConfidence = 0.f;
    u32_t activeSkillMask = 0u;
    bool_t bEnemyChampionTargetable = false;
    bool_t bCanMove = true;
    bool_t bCanAttack = true;
    bool_t bCanCast = true;
    bool_t bAlliedWaveNearby = false;
    bool_t bStructureWaveTanking = false;
    bool_t bInsideEnemyTurretDanger = false;
    bool_t bAlliedOuterTurretLost = false;
    bool_t bMidLaneTurretLost = false;
    bool_t bHasLastSeenEnemyChampion = false;
    bool_t bSelfInventoryValueComplete = true;
    bool_t bEnemyInventoryValueComplete = true;
};
