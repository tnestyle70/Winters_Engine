#include "Network/Client/ClientNetwork.h"
#include "Scene/InGameNetworkBridge.h"

#include <Windows.h>
#include <cstdio>
#include <utility>

#include "Network/Client/CommandSerializer.h"
#include "Network/Client/EventApplier.h"
#include "GameInstance.h"
#include "Network/Client/GameSessionClient.h"
#include "Network/Client/SnapshotApplier.h"
#include "Dev/SmokeLog.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"

void CInGameNetworkBridge::Initialize(InGameNetworkBridgeDesc& desc)
{
    const bool_t bUseNetworkRoster = desc.context.bUseNetworkRoster;

    desc.entityIdMap = std::make_unique<EntityIdMap>();
    desc.networkView = nullptr;
    desc.bUsingSharedNetwork = false;

    if (bUseNetworkRoster && CGameSessionClient::Instance().IsConnected())
    {
        desc.bUsingSharedNetwork = true;
        desc.networkView = CGameSessionClient::Instance().GetNetwork();
    }
    else
    {
        desc.ownedNetwork = CClientNetwork::Create();
        desc.networkView = desc.ownedNetwork.get();
    }

    Winters::DevSmoke::Log(
        "[InGameNetwork] init useNetworkRoster=%u shared=%u\n",
        bUseNetworkRoster ? 1u : 0u,
        desc.bUsingSharedNetwork ? 1u : 0u);

    desc.snapshotApplier = CSnapshotApplier::Create();
    desc.eventApplier = CEventApplier::Create();
    desc.commandSerializer = CCommandSerializer::Create();

    if (desc.snapshotApplier)
    {
        auto createChampion = desc.createChampion;
        desc.snapshotApplier->SetOnNewEntityCallback(
            [createChampion](u32_t netId, u8_t championId, u8_t team) -> EntityID
            {
                (void)netId;
                if (!createChampion)
                    return NULL_ENTITY;

                return createChampion(
                    static_cast<eChampion>(championId),
                    static_cast<eTeam>(team));
            });
        //viego soul callback function
        auto updateChampionVisual = desc.updateChampionVisual;
        desc.snapshotApplier->SetOnChampionVisualChangedCallback(
            [updateChampionVisual](EntityID entity, u8_t championId, u8_t team)
            {
                if (updateChampionVisual)
                    updateChampionVisual(entity,
                        static_cast<eChampion>(championId),
                        static_cast<eTeam>(team));
            }
        );
        auto removeNetworkEntity = desc.removeNetworkEntity;
        desc.snapshotApplier->SetOnRemoveEntityCallback(
            [removeNetworkEntity](EntityID entity)
            {
                if (removeNetworkEntity)
                    removeNetworkEntity(entity);
            }
        );
        desc.snapshotApplier->SetOnAuthoritativeSnapshot(
            [](u64_t, u64_t iServerTimeMs, u32_t, u32_t)
            {
                CGameInstance::Get()->UI_SetGameContextServerTimeMs(iServerTimeMs);
            });
    }

    if (!desc.networkView || !desc.snapshotApplier || !desc.eventApplier || !desc.entityIdMap)
        return;

    auto* pWorld = &desc.world;
    auto* pEntityIdMapSlot = &desc.entityIdMap;
    auto* pSnapshotApplierSlot = &desc.snapshotApplier;
    auto* pEventApplierSlot = &desc.eventApplier;
    auto* ppNetworkView = &desc.networkView;
    const GameContext* pContext = &desc.context;
    auto bindLocalEntity = desc.bindLocalEntity;

    CGameSessionClient::FrameCallback frameHandler =
        [pWorld, pEntityIdMapSlot, pSnapshotApplierSlot, pEventApplierSlot,
            ppNetworkView, pContext, bindLocalEntity]
        (ePacketType type, u32_t sequence, const u8_t* payload, u32_t len)
        {
            (void)sequence;

            if (!pEntityIdMapSlot || !*pEntityIdMapSlot ||
                !pSnapshotApplierSlot || !*pSnapshotApplierSlot ||
                !pEventApplierSlot || !*pEventApplierSlot)
            {
                return;
            }

            EntityIdMap& entityMap = **pEntityIdMapSlot;
            CSnapshotApplier& snapshotApplier = **pSnapshotApplierSlot;
            CEventApplier& eventApplier = **pEventApplierSlot;

            if (type == ePacketType::Hello)
            {
                u32_t myNetId = 0;
                u32_t mySessionId = 0;
                snapshotApplier.OnHello(
                    *pWorld,
                    entityMap,
                    payload,
                    len,
                    &myNetId,
                    &mySessionId);

                const GameContext& context = *pContext;
                if (context.bUseNetworkRoster
                    && context.MyNetId != 0
                    && myNetId != 0
                    && context.MyNetId != myNetId)
                {
                    char mismatch[192]{};
                    sprintf_s(mismatch,
                        "[Scene_InGame] netId mismatch roster=%u hello=%u sid=%u\n",
                        context.MyNetId,
                        myNetId,
                        mySessionId);
                    OutputDebugStringA(mismatch);
                }

                const u32_t bindNetId = myNetId != 0
                    ? myNetId
                    : (context.bUseNetworkRoster ? context.MyNetId : 0);
                const u32_t bindSessionId = mySessionId != 0
                    ? mySessionId
                    : (context.bUseNetworkRoster ? context.MySessionId : 0);

                if (context.bUseNetworkRoster
                    && context.MySessionId != 0
                    && mySessionId != 0
                    && context.MySessionId != mySessionId)
                {
                    char mismatch[192]{};
                    sprintf_s(mismatch,
                        "[Scene_InGame] session mismatch rosterSid=%u helloSid=%u helloNet=%u\n",
                        context.MySessionId,
                        mySessionId,
                        myNetId);
                    OutputDebugStringA(mismatch);
                }

                CClientNetwork* pNetworkView = ppNetworkView ? *ppNetworkView : nullptr;
                if (pNetworkView)
                {
                    pNetworkView->SetMyNetEntityId(bindNetId);
                    pNetworkView->SetMySessionId(bindSessionId);
                }

                const EntityID localNetEntity = bindNetId != 0
                    ? entityMap.FromNet(bindNetId)
                    : NULL_ENTITY;
                Winters::DevSmoke::Log(
                    "[InGameNetwork] hello myNet=%u mySid=%u bindNet=%u bindSid=%u entity=%u\n",
                    myNetId,
                    mySessionId,
                    bindNetId,
                    bindSessionId,
                    static_cast<u32_t>(localNetEntity));
                if (localNetEntity != NULL_ENTITY && bindLocalEntity)
                    bindLocalEntity(localNetEntity);
            }
            else if (type == ePacketType::Snapshot)
            {
                static u32_t s_snapshotLogCount = 0;
                if (s_snapshotLogCount < 3u)
                {
                    Winters::DevSmoke::Log(
                        "[InGameNetwork] snapshot len=%u index=%u\n",
                        len,
                        s_snapshotLogCount);
                    ++s_snapshotLogCount;
                }
                snapshotApplier.OnSnapshot(*pWorld, entityMap, payload, len);
            }
            else if (type == ePacketType::Event)
            {
                eventApplier.OnEvent(*pWorld, entityMap, payload, len);
            }
        };

    if (desc.bUsingSharedNetwork)
        CGameSessionClient::Instance().SetGameFrameCallback(std::move(frameHandler));
    else
        desc.networkView->SetFrameCallback(std::move(frameHandler));
    Winters::DevSmoke::Log("[Scene] callbacks registered (snapshot/event/cmd/network)\n");

    if (bUseNetworkRoster)
    {
        Winters::DevSmoke::Log(desc.bUsingSharedNetwork
            ? "[Scene_InGame] Reusing BanPick TCP session.\n"
            : "[Scene_InGame] Network roster active without shared session; local roster only.\n");
    }
    else if (desc.networkView->Connect("127.0.0.1", 9000))
    {
        Winters::DevSmoke::Log("[Scene_InGame] Connected to local Winters server.\n");
    }
    else
    {
        Winters::DevSmoke::Log("[Scene_InGame] Server not reachable; running local-only mode.\n");
    }
}

bool_t CInGameNetworkBridge::Pump(CClientNetwork* pNetworkView, bool_t bUsingSharedNetwork)
{
    const bool_t bNetworkActive = (pNetworkView && pNetworkView->IsConnected());
    if (!bNetworkActive)
        return false;

    if (bUsingSharedNetwork)
        CGameSessionClient::Instance().Pump();
    else
        pNetworkView->PumpReceivedFrames();

    return true;
}

void CInGameNetworkBridge::ReplayLastHelloIfShared(bool_t bUsingSharedNetwork)
{
    if (bUsingSharedNetwork)
        CGameSessionClient::Instance().ReplayLastHelloToGameFrameCallback();
}

void CInGameNetworkBridge::ApplySnapshot(
    CWorld& world,
    CSnapshotApplier* pSnapshotApplier,
    EntityIdMap* pEntityIdMap,
    const u8_t* bytes,
    u32_t len)
{
    if (!pSnapshotApplier || !pEntityIdMap)
        return;

    pSnapshotApplier->OnSnapshot(world, *pEntityIdMap, bytes, len);
}

void CInGameNetworkBridge::Shutdown(
    std::unique_ptr<EntityIdMap>& entityIdMap,
    std::unique_ptr<CClientNetwork>& ownedNetwork,
    CClientNetwork*& networkView,
    bool_t& bUsingSharedNetwork,
    std::unique_ptr<CSnapshotApplier>& snapshotApplier,
    std::unique_ptr<CEventApplier>& eventApplier,
    std::unique_ptr<CCommandSerializer>& commandSerializer)
{
    if (bUsingSharedNetwork)
    {
        CGameSessionClient::Instance().SetGameFrameCallback(nullptr);
    }
    else if (ownedNetwork)
    {
        ownedNetwork->Disconnect();
    }

    commandSerializer.reset();
    eventApplier.reset();
    snapshotApplier.reset();
    ownedNetwork.reset();
    networkView = nullptr;
    bUsingSharedNetwork = false;
    entityIdMap.reset();
}
