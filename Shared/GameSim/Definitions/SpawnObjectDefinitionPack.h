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

    f32_t ResolveStructureMaxHp(eStructureKind kind) const;
    const JungleCampGameDef* FindJungleCamp(u8_t subKind) const;
    JungleCampGameDef ResolveJungleCamp(u8_t subKind) const;
    const MinionCombatDef* FindMinion(u8_t roleType) const;
    MinionCombatDef ResolveMinion(u8_t roleType) const;
};
