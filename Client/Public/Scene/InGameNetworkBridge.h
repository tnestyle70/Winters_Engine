#pragma once

#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameContext.h"

#include <functional>
#include <memory>

class CClientNetwork;
class CCommandSerializer;
class CEventApplier;
class CSnapshotApplier;
class CWorld;
class EntityIdMap;

struct InGameNetworkBridgeDesc
{
    CWorld& world;
    const GameContext& context;
    std::unique_ptr<EntityIdMap>& entityIdMap;
    std::unique_ptr<CClientNetwork>& ownedNetwork;
    CClientNetwork*& networkView;
    bool_t& bUsingSharedNetwork;
    std::unique_ptr<CSnapshotApplier>& snapshotApplier;
    std::unique_ptr<CEventApplier>& eventApplier;
    std::unique_ptr<CCommandSerializer>& commandSerializer;
    std::function<EntityID(eChampion, eTeam)> createChampion;
    //Viego Soul
    std::function<void(EntityID, eChampion, eTeam)> updateChampionVisual;
    std::function<void(EntityID)> removeNetworkEntity;
    std::function<void(EntityID)> bindLocalEntity;
};

class CInGameNetworkBridge final
{
public:
    static void Initialize(InGameNetworkBridgeDesc& desc);
    static bool_t Pump(CClientNetwork* pNetworkView, bool_t bUsingSharedNetwork);
    static void ReplayLastHelloIfShared(bool_t bUsingSharedNetwork);
    static void ApplySnapshot(
        CWorld& world,
        CSnapshotApplier* pSnapshotApplier,
        EntityIdMap* pEntityIdMap,
        const u8_t* bytes,
        u32_t len);
    static void Shutdown(
        std::unique_ptr<EntityIdMap>& entityIdMap,
        std::unique_ptr<CClientNetwork>& ownedNetwork,
        CClientNetwork*& networkView,
        bool_t& bUsingSharedNetwork,
        std::unique_ptr<CSnapshotApplier>& snapshotApplier,
        std::unique_ptr<CEventApplier>& eventApplier,
        std::unique_ptr<CCommandSerializer>& commandSerializer);
};
