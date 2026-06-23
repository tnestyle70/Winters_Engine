#pragma once

#include "ECS/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/JungleCampGameDef.h"
#include "Shared/GameSim/Definitions/StructureGameDef.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <array>

struct SpawnObjectDefinitionPack;
namespace Winters::Map
{
    struct JungleEntry;
    struct StructureEntry;
}

struct WorldBootstrapStructureSpawnRequest
{
    eTeam team = eTeam::Blue;
    u32_t kind = 0u;
    u32_t tier = 0u;
    u32_t lane = 0u;
    Vec3 position{};
    Vec3 rotation{};
    f32_t maxHp = 0.f;
    f32_t scale = 1.f;
    bool_t bTurret = false;
    bool_t bNexus = false;
    bool_t bInhibitor = false;
};

struct WorldBootstrapJungleSpawnRequest
{
    u32_t subKind = 0u;
    u32_t campId = 0u;
    Vec3 position{};
    Vec3 rotation{};
    f32_t scale = 1.f;
    JungleCampGameDef combat{};
};

class CWorldBootstrap final
{
public:
    static eTeam ResolveStageTeam(u32_t stageTeam);

    static eStructureKind ResolveStructureKind(
        u32_t kind,
        u32_t nexusKind,
        u32_t inhibitorKind);

    static bool_t TryBuildStageStructureRequest(
        const SpawnObjectDefinitionPack& objectDefs,
        const Winters::Map::StructureEntry& entry,
        u8_t resolvedLane,
        u32_t turretKind,
        u32_t nexusKind,
        u32_t inhibitorKind,
        WorldBootstrapStructureSpawnRequest& outRequest);

    static bool_t TryBuildStageJungleRequest(
        const SpawnObjectDefinitionPack& objectDefs,
        const Winters::Map::JungleEntry& entry,
        WorldBootstrapJungleSpawnRequest& outRequest);

    static std::array<WorldBootstrapStructureSpawnRequest, 6> BuildFallbackStructures(
        const SpawnObjectDefinitionPack& objectDefs,
        u32_t turretKind,
        u32_t nexusKind,
        u32_t laneMid);
};
