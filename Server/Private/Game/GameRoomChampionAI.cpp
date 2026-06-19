#include "Game/GameRoom.h"
#include "Game/ServerMinionWaveRuntime.h"
#include "GameRoomInternal.h"

#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace
{
    constexpr f32_t kChampionAISafeAnchorBehindTurret = 3.f;

    bool_t IsValidChampionAILane(u8_t lane)
    {
        return lane == static_cast<u8_t>(kLaneTop) ||
            lane == static_cast<u8_t>(kLaneMid) ||
            lane == static_cast<u8_t>(kLaneBot);
    }
}

void CGameRoom::Phase_ServerBotAI(TickContext& tc)
{
    if (!IsInGamePhase())
        return;

    CChampionAISystem::Execute(m_world, tc, m_pendingExecCommands);
}

u8_t CGameRoom::ResolveInitialBotLane(const LobbySlotState& slot) const
{
    if (!slot.bBot || slot.bDummy)
        return GetGameSimRosterLane(slot.slotId);

    if (IsValidChampionAILane(slot.botLane))
        return slot.botLane;

    static constexpr u8_t kBotLanes[] =
    {
        static_cast<u8_t>(kLaneTop),
        static_cast<u8_t>(kLaneMid),
        static_cast<u8_t>(kLaneBot),
    };

    const u32_t seed =
        static_cast<u32_t>(slot.slotId) * 1103515245u ^
        static_cast<u32_t>(slot.team) * 2654435761u ^
        static_cast<u32_t>(slot.botDifficulty) * 2246822519u ^
        static_cast<u32_t>(slot.champion) * 3266489917u;

    return kBotLanes[seed % static_cast<u32_t>(sizeof(kBotLanes) / sizeof(kBotLanes[0]))];
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
    Vec3 best = GetGameSimLaneGatherPosition(lane, TeamByte(team));
    bool_t bFound = false;
    f32_t bestScore = std::numeric_limits<f32_t>::max();
    const u8_t waypointLane = CServerMinionWaveRuntime::ResolveWaypointLane(team, lane);
    const u32_t waypointCount = GetServerMinionWaypointCount(team, waypointLane);

    auto getWaypoint = [&](u32_t index)
    {
        return GetServerMinionWaypoint(team, waypointLane, index);
    };

    auto scoreLaneDistance = [&](const Vec3& pos) -> f32_t
        {
            if (waypointCount >= 2u)
            {
                f32_t score = std::numeric_limits<f32_t>::max();
                for (u32_t i = 1u; i < waypointCount; ++i)
                {
                    f32_t t = 0.f;
                    const f32_t distSq = WintersMath::DistanceSqPointToSegmentXZ(
                        pos,
                        getWaypoint(i - 1u),
                        getWaypoint(i),
                        &t,
                        std::numeric_limits<f32_t>::epsilon());
                    score = std::min(score, distSq);
                }
                return score;
            }

            if (waypointCount == 1u)
                return WintersMath::DistanceSqXZ(pos, getWaypoint(0u));

            return WintersMath::DistanceSqXZ(pos, best);
        };

    m_world.ForEach<StructureComponent, TransformComponent>(
        [&](EntityID, StructureComponent& structure, TransformComponent& transform)
        {
            if (structure.team != team)
                return;
            if (structure.kind != kStructureKindTurret)
                return;
            if (structure.tier != static_cast<u32_t>(Winters::Map::eTurretTier::Outer))
                return;
            if (structure.lane != lane)
                return;

            const Vec3 towerPos = transform.GetPosition();
            const f32_t score = scoreLaneDistance(towerPos);
            if (score < bestScore)
            {
                bestScore = score;
                best = towerPos;
                bFound = true;
            }
        });

    if (bFound && waypointCount >= 2u)
    {
        const Vec3 start = getWaypoint(0u);
        const Vec3 next = getWaypoint(1u);
        const Vec3 laneDir = NormalizeXZOrForward(
            Vec3{ next.x - start.x, 0.f, next.z - start.z },
            team);
        best = Vec3{
            best.x - laneDir.x * kChampionAISafeAnchorBehindTurret,
            best.y,
            best.z - laneDir.z * kChampionAISafeAnchorBehindTurret
        };
    }
    else if (bFound)
    {
        best.x += (team == eTeam::Blue)
            ? -kChampionAISafeAnchorBehindTurret
            : kChampionAISafeAnchorBehindTurret;
    }
    else if (waypointCount > 1u)
    {
        best = getWaypoint(1u);
    }
    else if (waypointCount > 0u)
    {
        best = getWaypoint(0u);
    }

    best.y = 1.f;
    return best;
}

void CGameRoom::RefreshChampionAIGoals()
{
    m_world.ForEach<ChampionAIComponent>(
        [this](EntityID, ChampionAIComponent& ai)
        {
            ai.safeAnchor = ResolveChampionAISafeAnchor(ai.team, ai.lane);
            ai.retreatGoal = ai.safeAnchor;
            ai.laneGoal = ResolveChampionAILaneGoal(ai.team, ai.lane);
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
