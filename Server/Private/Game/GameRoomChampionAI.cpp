#include "Game/GameRoom.h"
#include "Game/ServerAICommandProducer.h"
#include "Game/ServerMinionWaveRuntime.h"
#include "GameRoomInternal.h"

#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr u8_t kChampionAIMidLane =
        static_cast<u8_t>(Winters::Map::eLane::Mid);
}

void CGameRoom::Phase_ServerBotAI(TickContext& tc)
{
    if (!IsInGamePhase())
        return;

    CServerAICommandProducer::Execute(
        m_world,
        tc,
        m_pendingExecCommands,
        m_pShadowPolicy.get());
}

Vec3 CGameRoom::ResolveChampionAILaneGoal(eTeam team, u8_t lane) const
{
    const u8_t waypointLane = CServerMinionWaveRuntime::ResolveWaypointLane(team, lane);
    const u32_t waypointCount = GetServerMinionWaypointCount(team, waypointLane);
    Vec3 goal = GetGameSimLaneGatherPosition(lane, TeamByte(team));

    if (waypointCount >= 2u)
    {
        const u32_t index = std::max(1u, waypointCount / 2u);
        goal = GetServerMinionWaypoint(team, waypointLane, index);
    }
    else if (waypointCount == 1u)
    {
        goal = GetServerMinionWaypoint(team, waypointLane, 0u);
    }

    goal.y = 1.f;
    return goal;
}

Vec3 CGameRoom::ResolveChampionAISafeAnchor(eTeam team, u8_t lane)
{
    return CChampionAISystem::ResolveSafeAnchor(
        m_world,
        team,
        lane,
        GetGameSimLaneGatherPosition(lane, TeamByte(team)));
}

void CGameRoom::RefreshChampionAIGoals()
{
    m_world.ForEach<ChampionAIComponent>(
        [this](EntityID, ChampionAIComponent& ai)
        {
            if (!ai.bMidDefenseActive)
                ai.activeLane = ai.lane;

            ai.safeAnchor = ResolveChampionAISafeAnchor(ai.team, ai.lane);
            ai.retreatGoal = ai.safeAnchor;
            ai.laneGoal = ResolveChampionAILaneGoal(ai.team, ai.lane);
            ai.midDefenseAnchor = ResolveChampionAISafeAnchor(
                ai.team,
                kChampionAIMidLane);
            const u8_t waypointLane =
                CServerMinionWaveRuntime::ResolveWaypointLane(ai.team, ai.lane);
            char msg[256]{};
            sprintf_s(msg,
                "[ChampionAI] lane goal team=%u champ=%u lane=%u wpLane=%u advance=(%.2f,%.2f,%.2f) safe=(%.2f,%.2f,%.2f)\n",
                static_cast<u32_t>(ai.team),
                static_cast<u32_t>(ai.champion),
                static_cast<u32_t>(ai.lane),
                static_cast<u32_t>(waypointLane),
                ai.laneGoal.x,
                ai.laneGoal.y,
                ai.laneGoal.z,
                ai.safeAnchor.x,
                ai.safeAnchor.y,
                ai.safeAnchor.z);
            OutputServerAITrace(msg);
        });
}
