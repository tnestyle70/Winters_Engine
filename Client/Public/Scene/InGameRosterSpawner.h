#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"

#include <functional>
#include <unordered_map>

class CWorld;
class EntityIdMap;

struct InGameRosterSpawnDesc
{
    CWorld& world;
    EntityIdMap* pEntityIdMap = nullptr;
    bool_t bNetworkAuthoritative = false;
    std::unordered_map<EntityID, Vec3>& networkChampionPrevPos;
    std::function<EntityID(eChampion, eTeam)> createChampion;
    std::function<void(eChampion, EntityID)> assignAlias;
};

struct InGameRosterSpawnResult
{
    bool_t bCreatedAny = false;
    u32_t requestedSlots = 0;
    u32_t createdSlots = 0;
    u32_t humanSlots = 0;
    u32_t botSlots = 0;
    EntityID playerEntity = NULL_ENTITY;
};

class CInGameRosterSpawner final
{
public:
    static void EnsureLocalRosterFallback(MatchContext& context);
    static eChampion ResolvePracticeBotChampion();
    static bool_t IsLocalRosterSlot(const MatchContext& context, const GameRosterSlot& slot);
    static InGameRosterSpawnResult SpawnFromContext(
        InGameRosterSpawnDesc& desc,
        const MatchContext& context);

private:
    static EntityID SpawnSlot(InGameRosterSpawnDesc& desc, const GameRosterSlot& slot);
};
