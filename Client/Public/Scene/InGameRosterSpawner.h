#pragma once

#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameContext.h"

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
    static void EnsureLocalRosterFallback(GameContext& context);
    static bool_t IsLocalRosterSlot(const GameContext& context, const GameRosterSlot& slot);
    static InGameRosterSpawnResult SpawnFromContext(
        InGameRosterSpawnDesc& desc,
        const GameContext& context);

private:
    static EntityID SpawnSlot(InGameRosterSpawnDesc& desc, const GameRosterSlot& slot);
};
