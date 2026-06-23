#include "Game/WorldBootstrap.h"

#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Definitions/SpawnObjectDefinitionPack.h"

eTeam CWorldBootstrap::ResolveStageTeam(u32_t stageTeam)
{
    switch (static_cast<Winters::Map::eTeam>(stageTeam))
    {
    case Winters::Map::eTeam::Red:
        return eTeam::Red;
    case Winters::Map::eTeam::Neutral:
        return eTeam::Neutral;
    case Winters::Map::eTeam::Blue:
    default:
        return eTeam::Blue;
    }
}

eStructureKind CWorldBootstrap::ResolveStructureKind(
    u32_t kind,
    u32_t nexusKind,
    u32_t inhibitorKind)
{
    if (kind == nexusKind)
        return eStructureKind::Nexus;
    if (kind == inhibitorKind)
        return eStructureKind::Inhibitor;
    return eStructureKind::Turret;
}

bool_t CWorldBootstrap::TryBuildStageStructureRequest(
    const SpawnObjectDefinitionPack& objectDefs,
    const Winters::Map::StructureEntry& entry,
    u8_t resolvedLane,
    u32_t turretKind,
    u32_t nexusKind,
    u32_t inhibitorKind,
    WorldBootstrapStructureSpawnRequest& outRequest)
{
    if (entry.bVisible == 0u)
        return false;

    const u32_t kind = entry.subKind;
    const bool_t bTurret = kind == turretKind;
    const bool_t bNexus = kind == nexusKind;
    const bool_t bInhibitor = kind == inhibitorKind;
    if (!bTurret && !bNexus && !bInhibitor)
        return false;

    outRequest = WorldBootstrapStructureSpawnRequest{};
    outRequest.team = ResolveStageTeam(entry.team);
    outRequest.kind = kind;
    outRequest.tier = entry.tier;
    outRequest.lane = resolvedLane;
    outRequest.position = Vec3{ entry.px, entry.py, entry.pz };
    outRequest.rotation = Vec3{ entry.rx, entry.ry, entry.rz };
    outRequest.maxHp = objectDefs.ResolveStructureMaxHp(
        ResolveStructureKind(kind, nexusKind, inhibitorKind));
    outRequest.scale = entry.scale;
    outRequest.bTurret = bTurret;
    outRequest.bNexus = bNexus;
    outRequest.bInhibitor = bInhibitor;
    return true;
}

bool_t CWorldBootstrap::TryBuildStageJungleRequest(
    const SpawnObjectDefinitionPack& objectDefs,
    const Winters::Map::JungleEntry& entry,
    WorldBootstrapJungleSpawnRequest& outRequest)
{
    if (entry.bVisible == 0u)
        return false;

    outRequest = WorldBootstrapJungleSpawnRequest{};
    outRequest.subKind = entry.subKind;
    outRequest.campId = entry.campId;
    outRequest.position = Vec3{ entry.px, entry.py, entry.pz };
    outRequest.rotation = Vec3{ entry.rx, entry.ry, entry.rz };
    outRequest.scale = entry.scale > 0.f ? entry.scale : 1.f;
    outRequest.combat = objectDefs.ResolveJungleCamp(static_cast<u8_t>(entry.subKind));
    return true;
}

std::array<WorldBootstrapStructureSpawnRequest, 6> CWorldBootstrap::BuildFallbackStructures(
    const SpawnObjectDefinitionPack& objectDefs,
    u32_t turretKind,
    u32_t nexusKind,
    u32_t laneMid)
{
    const f32_t fallbackTurretMaxHp =
        objectDefs.ResolveStructureMaxHp(eStructureKind::Turret);
    const f32_t fallbackNexusMaxHp =
        objectDefs.ResolveStructureMaxHp(eStructureKind::Nexus);

    return { {
        { eTeam::Blue, turretKind, 0u, laneMid, Vec3{ 18.f, 1.f, 0.f }, Vec3{}, fallbackTurretMaxHp, 1.f, true, false, false },
        { eTeam::Blue, turretKind, 1u, laneMid, Vec3{ 25.f, 1.f, 0.f }, Vec3{}, fallbackTurretMaxHp, 1.f, true, false, false },
        { eTeam::Blue, nexusKind, 3u, laneMid, Vec3{ 32.f, 1.f, 0.f }, Vec3{}, fallbackNexusMaxHp, 1.f, false, true, false },
        { eTeam::Red, turretKind, 0u, laneMid, Vec3{ -18.f, 1.f, 0.f }, Vec3{}, fallbackTurretMaxHp, 1.f, true, false, false },
        { eTeam::Red, turretKind, 1u, laneMid, Vec3{ -25.f, 1.f, 0.f }, Vec3{}, fallbackTurretMaxHp, 1.f, true, false, false },
        { eTeam::Red, nexusKind, 3u, laneMid, Vec3{ -32.f, 1.f, 0.f }, Vec3{}, fallbackNexusMaxHp, 1.f, false, true, false },
    } };
}
