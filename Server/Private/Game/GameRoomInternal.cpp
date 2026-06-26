#include "GameRoomInternal.h"

#include "Game/ServerMinionWaveRuntime.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/World.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/StageData.h"

#include <limits>

namespace
{
    constexpr f32_t kDefaultStructureRadius = 1.5f;
    constexpr f32_t kStageFountainForwardFromTwin = -0.4f;
    constexpr f32_t kStageFountainSideFromTwin = -6.5f;
    constexpr f32_t kStageFountainSlotSpacing = 3.f;

    struct StageBaseAnchors
    {
        Vec3 nexus{};
        Vec3 twinCenter{};
        bool_t bHasNexus = false;
        bool_t bHasTwinCenter = false;
    };

    bool_t TryResolveStageBaseAnchors(
        const Winters::Map::StageData& stage,
        eTeam team,
        StageBaseAnchors& outAnchors)
    {
        if (team != eTeam::Blue && team != eTeam::Red)
            return false;

        outAnchors = StageBaseAnchors{};

        const u32_t stageTeam = static_cast<u32_t>(
            team == eTeam::Red ? Winters::Map::eTeam::Red : Winters::Map::eTeam::Blue);
        const u32_t nexusTier = static_cast<u32_t>(Winters::Map::eTurretTier::Nexus);

        Vec3 twinSum{};
        u32_t twinCount = 0;

        for (const auto& entry : stage.structures)
        {
            if (entry.bVisible == 0u || entry.team != stageTeam)
                continue;

            const Vec3 pos{ entry.px, entry.py, entry.pz };
            if (entry.subKind == kStructureKindNexus)
            {
                outAnchors.nexus = pos;
                outAnchors.bHasNexus = true;
            }
            else if (entry.subKind == kStructureKindTurret && entry.tier == nexusTier)
            {
                twinSum.x += pos.x;
                twinSum.y += pos.y;
                twinSum.z += pos.z;
                ++twinCount;
            }
        }

        if (twinCount > 0u)
        {
            const f32_t invCount = 1.f / static_cast<f32_t>(twinCount);
            outAnchors.twinCenter = Vec3{
                twinSum.x * invCount,
                twinSum.y * invCount,
                twinSum.z * invCount
            };
            outAnchors.bHasTwinCenter = true;
        }

        return outAnchors.bHasNexus && outAnchors.bHasTwinCenter;
    }

}

u8_t TeamByte(eTeam team)
{
    return static_cast<u8_t>(team);
}

void OutputServerAITrace(const char* pText)
{
    if (!pText)
        return;

    WintersOutputAIDebugStringA(pText);
}

void OutputServerAITraceW(const wchar_t* pText)
{
    if (!pText)
        return;

    WintersOutputAIDebugStringW(pText);
}

Vec3 NormalizeXZOrForward(const Vec3& v, eTeam team)
{
    const Vec3 fallback =
        (team == eTeam::Blue) ? Vec3{ -1.f, 0.f, 0.f } : Vec3{ 1.f, 0.f, 0.f };
    return WintersMath::NormalizeXZ(
        v,
        fallback,
        std::numeric_limits<f32_t>::epsilon());
}

std::vector<Engine::CNavGrid::Cell> SmoothServerPathCells(
    const Engine::CNavGrid& navGrid,
    const std::vector<Engine::CNavGrid::Cell>& path)
{
    if (path.size() <= 2)
        return path;

    std::vector<Engine::CNavGrid::Cell> smoothed{};
    smoothed.reserve(path.size());
    smoothed.push_back(path.front());

    size_t anchor = 0;
    while (anchor + 1u < path.size())
    {
        size_t best = anchor + 1u;
        for (size_t probe = path.size() - 1u; probe > anchor + 1u; --probe)
        {
            if (navGrid.LineCellsWalkableForRadius(path[anchor], path[probe], 0.f))
            {
                best = probe;
                break;
            }
        }

        smoothed.push_back(path[best]);
        anchor = best;
    }

    return smoothed;
}

u8_t ResolveServerWaypointLane(eTeam team, u8_t lane)
{
    return CServerMinionWaveRuntime::ResolveWaypointLane(team, lane);
}

bool_t TryResolveStageFountainSpawn(
    const Winters::Map::StageData& stage,
    u8_t slotId,
    eTeam team,
    Vec3& outSpawn)
{
    StageBaseAnchors anchors{};
    if (!TryResolveStageBaseAnchors(stage, team, anchors))
        return false;

    const Vec3 forward = WintersMath::NormalizeXZ(
        Vec3{
            anchors.twinCenter.x - anchors.nexus.x,
            0.f,
            anchors.twinCenter.z - anchors.nexus.z
        },
        Vec3{ 1.f, 0.f, 0.f },
        std::numeric_limits<f32_t>::epsilon());
    const Vec3 right{ -forward.z, 0.f, forward.x };
    const f32_t slotOffset =
        kStageFountainForwardFromTwin +
        static_cast<f32_t>(slotId % 5u) * kStageFountainSlotSpacing;

    outSpawn = Vec3{
        anchors.twinCenter.x + forward.x * slotOffset + right.x * kStageFountainSideFromTwin,
        1.f,
        anchors.twinCenter.z + forward.z * slotOffset + right.z * kStageFountainSideFromTwin
    };
    return true;
}

f32_t ResolveStageStructureRadius(u32_t kind, u32_t tier)
{
    if (kind == kStructureKindNexus)
        return 2.8f;
    if (kind == kStructureKindInhibitor)
        return 1.8f;
    if (kind == kStructureKindTurret)
    {
        const u32_t nexusTier =
            static_cast<u32_t>(Winters::Map::eTurretTier::Nexus);
        return (tier == nexusTier) ? 1.8f : 1.5f;
    }
    return kDefaultStructureRadius;
}

bool_t TryResolveCombatTeam(CWorld& world, EntityID entity, eTeam& outTeam)
{
    if (world.HasComponent<ChampionComponent>(entity))
    {
        outTeam = world.GetComponent<ChampionComponent>(entity).team;
        return true;
    }
    if (world.HasComponent<MinionComponent>(entity))
    {
        outTeam = world.GetComponent<MinionComponent>(entity).team;
        return true;
    }
    if (world.HasComponent<MinionStateComponent>(entity))
    {
        outTeam = world.GetComponent<MinionStateComponent>(entity).team;
        return true;
    }
    if (world.HasComponent<TurretComponent>(entity))
    {
        outTeam = world.GetComponent<TurretComponent>(entity).team;
        return true;
    }
    if (world.HasComponent<StructureComponent>(entity))
    {
        outTeam = world.GetComponent<StructureComponent>(entity).team;
        return true;
    }
    return false;
}

bool_t IsAliveHealth(CWorld& world, EntityID entity)
{
    if (entity == NULL_ENTITY || !world.IsAlive(entity))
        return false;
    if (!world.HasComponent<HealthComponent>(entity))
        return true;

    const HealthComponent& hp = world.GetComponent<HealthComponent>(entity);
    return !hp.bIsDead && hp.fCurrent > 0.f;
}

void StartReplicatedAction(CWorld& world, EntityID entity, eActionStateId actionId,
    const TickContext& tc, u8_t stage)
{
    StartActionState(world, entity, actionId, tc.tickIndex, stage);
}

void SetReplicatedPose(CWorld& world, EntityID entity, ePoseStateId poseId,
    const TickContext& tc)
{
    SetPoseState(world, entity, poseId, tc.tickIndex);
}
