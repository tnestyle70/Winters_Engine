#include "Server/Private/Game/Factory/ServerChampionEntityFactory.h"

#include "Game/GameRoom.h"
#include "Server/Private/Game/GameRoomInternal.h"
#include "Server/Private/Game/GameRoomSmokeRoster.h"
#include "Server/Private/Game/Factory/ChampionSimComponentTable.h"
#include "Server/Private/Data/LoLGameplayDefinitionPack.h"
#include "Shared/GameSim/Spawn/ChampionGameplayAssembly.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/World.h"

VisibilityComponent BuildServerVisibleToAll()
{
    VisibilityComponent visibility{};
    visibility.teamVisibilityMask = static_cast<u8_t>(
        (1u << TeamByte(eTeam::Blue)) |
        (1u << TeamByte(eTeam::Red)));
    return visibility;
}

EntityID ServerEntityFactory::BuildChampionEntity(
    CWorld& world, const LobbySlotState& slot, const Vec3& spawnPos)
{
    const SpawnObjectDefinitionPack& objectDefs = ServerData::GetLoLSpawnObjectDefinitionPack();
    const GameplayDefinitionPack& definitions = ServerData::GetLoLGameplayDefinitionPack();
    const ChampionGameplayDef* championDef = definitions.FindChampion(slot.champion);

    ChampionAssemblyContext ctx{};
    ctx.champion = slot.champion;
    ctx.team = slot.team;
    ctx.spawnPos = spawnPos;
    ctx.bAssignBotSkillRanks = slot.bBot && !slot.bDummy;
    ctx.loadout = objectDefs.spawnLoadout;
    ctx.pDef = championDef;
    // smoke/dummy 고정 HP override(else 분기는 defaultMaxHp 반환). 0.f를 넘기면 normal=0(override 없음),
    // smoke/dummy=고정값 -> 기존 ResolveServerChampionMaxHpForSlot(slot, stat.hpMax)와 byte-identical.
    ctx.maxHpOverride = ResolveServerChampionMaxHpForSlot(slot, 0.f);

    f32_t spatialRadius = 0.75f;
    f32_t sightRange = 19.f;
    if (championDef)
    {
        spatialRadius = championDef->stats.spatialRadius;
        sightRange = championDef->stats.sightRange;
    }
    else
    {
        ctx.fallbackStats = CChampionStatsRegistry::Instance().Resolve(slot.champion);
        spatialRadius = ctx.fallbackStats.spatialRadius;
        sightRange = ctx.fallbackStats.sightRange;
    }

    const EntityID entity = ChampionGameplayAssembly::Build(world, ctx);

    AttachChampionSimComponents(world, entity, slot.champion);

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::Character;
    spatial.team = slot.team;
    spatial.radius = spatialRadius;
    world.AddComponent<SpatialAgentComponent>(entity, spatial);

    const ChampionColliderProfileDef& colliderProfile = objectDefs.championCollider;
    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, colliderProfile.bodyHeight, spatial.radius };
    collider.vOffset = { 0.f, colliderProfile.bodyOffsetY, 0.f };
    collider.bIsTrigger = false;
    world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = sightRange;
    world.AddComponent<VisionSourceComponent>(entity, vision);
    world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    world.AddComponent<TargetableTag>(entity);

    return entity;
}
