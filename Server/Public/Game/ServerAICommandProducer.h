#pragma once

#include "WintersTypes.h"

#include <vector>

class CWorld;
struct GameCommand;
struct LobbySlotState;
struct TickContext;
struct ChampionAIShadowPolicyArtifactV1;

class CServerAICommandProducer final
{
public:
    static void Execute(
        CWorld& world,
        const TickContext& tc,
        std::vector<GameCommand>& outCommands,
        const ChampionAIShadowPolicyArtifactV1* pShadowPolicy = nullptr);

    static u8_t ResolveInitialBotLane(
        const LobbySlotState& slot,
        u8_t rosterFallbackLane);

private:
    CServerAICommandProducer() = delete;
};
