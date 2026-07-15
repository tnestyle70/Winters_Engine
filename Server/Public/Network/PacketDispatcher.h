#pragma once

#include "Network/FrameParser.h"
#include "Shared/Network/PacketEnvelope.h"
#include "WintersTypes.h"

#include <mutex>
#include <unordered_map>

class CGameRoom;

class CPacketDispatcher
{
public:
    static CPacketDispatcher& Instance();

    void DrainFrames(u32_t sessionId, CFrameParser& parser);
    void DispatchFrame(u32_t sessionId, const ParsedFrameOwned& frame);
    void DispatchCommandBatch(u32_t sessionId, const ParsedFrameOwned& frame);
    void DispatchLobbyCommand(u32_t sessionId, const ParsedFrameOwned& frame);
    void DispatchHello(u32_t sessionId, const ParsedFrameOwned& frame);

    void RegisterRoom(u32_t roomId, CGameRoom* pRoom);
    void UnregisterRoom(u32_t roomId, CGameRoom* pExpectedRoom);
    void RouteSession(u32_t sessionId, u32_t roomId);
    void UnrouteSession(u32_t sessionId);

private:
    CPacketDispatcher() = default;
    CPacketDispatcher(const CPacketDispatcher&) = delete;
    CPacketDispatcher& operator=(const CPacketDispatcher&) = delete;

    std::mutex m_mutex;
    std::unordered_map<u32_t, CGameRoom*> m_rooms;
    std::unordered_map<u32_t, u32_t> m_sessionToRoom;
};
