#include "Network/PacketDispatcher.h"

#include "Game/GameRoom.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/Schemas/Generated/cpp/Command_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyCommand_generated.h"

#include <flatbuffers/flatbuffers.h>

CPacketDispatcher& CPacketDispatcher::Instance()
{
    static CPacketDispatcher s_inst;
    return s_inst;
}

void CPacketDispatcher::RegisterRoom(u32_t roomId, CGameRoom* pRoom)
{
    std::lock_guard lk(m_mutex);
    m_rooms[roomId] = pRoom;
}

void CPacketDispatcher::RouteSession(u32_t sessionId, u32_t roomId)
{
    std::lock_guard lk(m_mutex);
    m_sessionToRoom[sessionId] = roomId;
}

void CPacketDispatcher::UnrouteSession(u32_t sessionId)
{
    std::lock_guard lk(m_mutex);
    m_sessionToRoom.erase(sessionId);
}

void CPacketDispatcher::DrainFrames(u32_t sessionId, CFrameParser& parser)
{
    ParsedFrameOwned frame{};
    for (;;)
    {
        const eFrameParseResult result = parser.TryPop(frame);
        if (result == eFrameParseResult::NeedMore)
            return;

        if (result == eFrameParseResult::Invalid)
        {
            CSession_Manager::Get()->OnDisconnect(sessionId);
            return;
        }

        switch (frame.type)
        {
        case ePacketType::CommandBatch:
            DispatchCommandBatch(sessionId, frame);
            break;
        case ePacketType::LobbyCommand:
            DispatchLobbyCommand(sessionId, frame);
            break;
        case ePacketType::Hello:
            DispatchHello(sessionId, frame);
            break;
        case ePacketType::Heartbeat:
            break;
        default:
            if (auto pSession = CSession_Manager::Get()->Find(sessionId))
                pSession->FlagSuspicious();
            break;
        }
    }
}

void CPacketDispatcher::DispatchCommandBatch(u32_t sessionId, const ParsedFrameOwned& frame)
{
    flatbuffers::Verifier verifier(frame.payload.data(), frame.payload.size());
    if (!Shared::Schema::VerifyCommandBatchBuffer(verifier))
    {
        if (auto pSession = CSession_Manager::Get()->Find(sessionId))
            pSession->FlagSuspicious();
        return;
    }

    const auto* batch = Shared::Schema::GetCommandBatch(frame.payload.data());

    CGameRoom* pRoom = nullptr;
    {
        std::lock_guard lk(m_mutex);
        auto routeIt = m_sessionToRoom.find(sessionId);
        if (routeIt == m_sessionToRoom.end())
            return;

        auto roomIt = m_rooms.find(routeIt->second);
        if (roomIt == m_rooms.end())
            return;

        pRoom = roomIt->second;
    }

    if (pRoom)
        pRoom->OnCommandBatch(sessionId, batch);
}

void CPacketDispatcher::DispatchLobbyCommand(u32_t sessionId, const ParsedFrameOwned& frame)
{
    flatbuffers::Verifier verifier(frame.payload.data(), frame.payload.size());
    if (!Shared::Schema::VerifyLobbyCommandBuffer(verifier))
    {
        if (auto pSession = CSession_Manager::Get()->Find(sessionId))
            pSession->FlagSuspicious();
        return;
    }

    const auto* command = Shared::Schema::GetLobbyCommand(frame.payload.data());

    CGameRoom* pRoom = nullptr;
    {
        std::lock_guard lk(m_mutex);
        auto routeIt = m_sessionToRoom.find(sessionId);
        if (routeIt == m_sessionToRoom.end())
            return;

        auto roomIt = m_rooms.find(routeIt->second);
        if (roomIt == m_rooms.end())
            return;

        pRoom = roomIt->second;
    }

    if (pRoom)
        pRoom->OnLobbyCommand(sessionId, command);
}

void CPacketDispatcher::DispatchHello(u32_t sessionId, const ParsedFrameOwned& frame)
{
    (void)sessionId;
    (void)frame;
}
