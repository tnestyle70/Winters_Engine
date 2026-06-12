#include "Game/GameRoom.h"

#include "Game/ServerMinionTuning.h"
#include "Game/SnapshotBuilder.h"
#include "Game/ReplayRecorder.h"
#include "GameRoomSmokeRoster.h"
#include "Network/PacketDispatcher.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/MatchScore.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/JungleAIComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/WaypointPatrolComponent.h"
#include "Shared/GameSim/Champions/Annie/AnnieGameSim.h"
#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"
#include "Shared/GameSim/Champions/Jax/JaxGameSim.h"
#include "Shared/GameSim/Champions/Kalista/KalistaGameSim.h"
#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"
#include "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h"
#include "Shared/GameSim/Champions/MasterYi/MasterYiGameSim.h"
#include "Shared/GameSim/Champions/Riven/RivenGameSim.h"
#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"
#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Champions/Zed/ZedGameSim.h"
#include "Shared/GameSim/Components/AsheSimComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/JaxSimComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MasterYiComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Definitions/StageData.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"

#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Security/LagCompensation.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"

#include "Shared/Network/PacketEnvelope.h"
#include "Shared/Schemas/Generated/cpp/Hello_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyCommand_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyState_generated.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/SpatialIndex.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/TurretAISystem.h"
#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Manager/Navigation/MapWalkableBaker.h"
#include "Manager/Navigation/NavGrid.h"
#include "Manager/Navigation/Pathfinder.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <flatbuffers/flatbuffers.h>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr f32_t kMoveTargetMaxSurfaceDeltaY = 3.f;

    void EnsureMatchScoreEntity(CWorld& world)
    {
        bool_t bFound = false;
        world.ForEach<MatchScoreComponent>(
            [&](EntityID, MatchScoreComponent&)
            {
                bFound = true;
            }
        );
        if (!bFound)
            world.AddComponent<MatchScoreComponent>(
                world.CreateEntity(),
                MatchScoreComponent{}
            );
    }

    bool_t FileExistsForServer(const std::wstring& path)
    {
        const DWORD attr = GetFileAttributesW(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool_t TryResolveExistingServerPath(
        const std::wstring& candidate,
        std::wstring& outPath)
    {
        wchar_t full[MAX_PATH]{};
        const DWORD got = GetFullPathNameW(candidate.c_str(), MAX_PATH, full, nullptr);
        if (got == 0 || got >= MAX_PATH)
            return false;

        if (!FileExistsForServer(full))
            return false;

        outPath = full;
        return true;
    }

    bool_t ResolveServerWMeshPath(std::wstring& outPath)
    {
        outPath.clear();

        std::vector<std::wstring> candidates;
        wchar_t exePath[MAX_PATH]{};
        const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH)
        {
            std::wstring exeDir = exePath;
            const size_t slash = exeDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
            {
                exeDir.resize(slash + 1);
                candidates.push_back(exeDir + L"Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
                candidates.push_back(exeDir + L"..\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
                candidates.push_back(exeDir + L"..\\..\\..\\Client\\Bin\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
                candidates.push_back(exeDir + L"..\\..\\..\\Client\\Bin\\Debug\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
            }
        }

        wchar_t cwd[MAX_PATH]{};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH)
        {
            std::wstring cwdDir = cwd;
            if (!cwdDir.empty() && cwdDir.back() != L'\\' && cwdDir.back() != L'/')
                cwdDir.push_back(L'\\');
            candidates.push_back(cwdDir + L"Client\\Bin\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
        }

        candidates.push_back(L"Client\\Bin\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
        candidates.push_back(L"Client\\Bin\\Debug\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");

        for (const std::wstring& candidate : candidates)
        {
            if (TryResolveExistingServerPath(candidate, outPath))
                return true;
        }

        return false;
    }

    void AssignDefaultBotSkillRanks(SkillRankComponent& ranks, u8_t championLevel)
    {
        ranks = SkillRankComponent{};
        CSkillRankSystem::SyncPointsForLevel(ranks, championLevel);

        static constexpr u8_t kLevelOrder[] =
        {
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::R),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
        };

        const u8_t count = std::min<u8_t>(
            championLevel,
            static_cast<u8_t>(sizeof(kLevelOrder) / sizeof(kLevelOrder[0])));
        for (u8_t i = 0; i < count && ranks.pointsAvailable > 0u; ++i)
            CSkillRankSystem::TryLevelSkill(ranks, kLevelOrder[i]);
    }

    bool_t IsMoveBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::JungleMob ||
            kind == eSpatialKind::Turret ||
            kind == eSpatialKind::Inhibitor ||
            kind == eSpatialKind::Nexus;
    }

    bool_t IsStaticMoveBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::Turret ||
            kind == eSpatialKind::Inhibitor ||
            kind == eSpatialKind::Nexus;
    }

    f32_t ResolveAgentRadius(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<SpatialAgentComponent>(entity))
            return (std::max)(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);

        return 0.5f;
    }

    f32_t ResolveServerMinionAttackRange(
        CWorld& world,
        EntityID self,
        EntityID target,
        const MinionStateComponent& state)
    {
        return state.attackRange +
            ResolveAgentRadius(world, self) +
            ResolveAgentRadius(world, target);
    }

    void FaceServerMinionTowardDirection(TransformComponent& transform, const Vec3& vDirection)
    {
        const f32_t fLenSq =
            vDirection.x * vDirection.x +
            vDirection.z * vDirection.z;
        if (fLenSq <= 0.0001f)
            return;

        Vec3 vRotation = transform.GetRotation();
        vRotation.y = static_cast<f32_t>(std::atan2(-vDirection.x, -vDirection.z));
        transform.SetRotation(vRotation);
    }

    void FaceServerMinionTowardTarget(
        TransformComponent& transform,
        const Vec3& vSource,
        const Vec3& vTarget)
    {
        FaceServerMinionTowardDirection(
            transform,
            Vec3{ vTarget.x - vSource.x, 0.f, vTarget.z - vSource.z });
    }

    bool_t IsSeparatingCandidate(
        const Vec3& vCurrent, const Vec3& vCandidate, const Vec3& vBlockerPos, f32_t minDistSq)
    {
        const f32_t currentDistSq = WintersMath::DistanceSqXZ(vCurrent, vBlockerPos);

        if (currentDistSq >= minDistSq)
            return false;

        const f32_t candidateDistSq = WintersMath::DistanceSqXZ(vCandidate, vBlockerPos);
        return candidateDistSq > currentDistSq + 0.0001f;
    }

    bool_t IsAvoidanceBlockedByEntity(
        CWorld& world,
        EntityID self,
        u8_t selfTeam,
        const Vec3& current,
        const Vec3& candidate,
        f32_t radius,
        EntityID other)
    {
        constexpr u8_t kUnknownSpatialTeam = 0xffu;
        constexpr f32_t kAvoidancePadding = 0.05f;

        if (other == self ||
            !world.HasComponent<TransformComponent>(other) ||
            !world.HasComponent<SpatialAgentComponent>(other))
        {
            return false;
        }

        const auto& agent = world.GetComponent<SpatialAgentComponent>(other);
        if (!IsMoveBlockingKind(agent.kind))
            return false;

        if (world.HasComponent<HealthComponent>(other))
        {
            const auto& health = world.GetComponent<HealthComponent>(other);
            if (health.bIsDead || health.fCurrent <= 0.f)
                return false;
        }

        const Vec3 otherPos = world.GetComponent<TransformComponent>(other).GetPosition();
        const f32_t minDist = radius + (std::max)(0.2f, agent.radius) + kAvoidancePadding;
        const f32_t minDistSq = minDist * minDist;
        const f32_t candidateDistSq = WintersMath::DistanceSqXZ(candidate, otherPos);
        if (candidateDistSq >= minDistSq)
            return false;

        const bool_t bSameTeam =
            selfTeam != kUnknownSpatialTeam &&
            agent.team == selfTeam &&
            agent.team != static_cast<u8_t>(eTeam::Neutral);
        if (bSameTeam)
        {
            if (agent.kind == eSpatialKind::Minion)
                return false;

            const f32_t currentDistSq = WintersMath::DistanceSqXZ(current, otherPos);
            if (candidateDistSq + 0.0001f >= currentDistSq)
                return false;
        }

        return !IsSeparatingCandidate(current, candidate, otherPos, minDistSq);
    }

    bool_t IsAvoidanceCandidateClear(
        CWorld& world,
        EntityID self,
        const Vec3& current,
        const Vec3& candidate,
        f32_t radius)
    {
        constexpr u8_t kUnknownSpatialTeam = 0xffu;
        const u8_t selfTeam = world.HasComponent<SpatialAgentComponent>(self)
            ? world.GetComponent<SpatialAgentComponent>(self).team
            : kUnknownSpatialTeam;

        if (CSpatialIndex* pSpatial = world.Get_SpatialIndex())
        {
            constexpr f32_t kAvoidancePadding = 0.05f;
            constexpr f32_t kStaleIndexMargin = 0.25f;
            const f32_t candidateStep =
                std::sqrt(WintersMath::DistanceSqXZ(current, candidate));
            const f32_t queryRadius =
                radius + kAvoidancePadding + candidateStep + kStaleIndexMargin;
            const u32_t moveBlockerMask =
                SpatialMask(eSpatialKind::JungleMob) |
                SpatialMask(eSpatialKind::Turret) |
                SpatialMask(eSpatialKind::Inhibitor) |
                SpatialMask(eSpatialKind::Nexus);

            std::vector<EntityID> candidates;
            candidates.reserve(16);
            pSpatial->QueryRadius(candidate, queryRadius, moveBlockerMask, 0u, candidates);
            for (EntityID other : candidates)
            {
                if (IsAvoidanceBlockedByEntity(
                    world, self, selfTeam, current, candidate, radius, other))
                {
                    return false;
                }
            }

            return true;
        }

        const auto entities = DeterministicEntityIterator<SpatialAgentComponent>::CollectSorted(world);
        for (EntityID other : entities)
        {
            if (IsAvoidanceBlockedByEntity(
                world, self, selfTeam, current, candidate, radius, other))
            {
                return false;
            }
        }

        return true;
    }

    constexpr u32_t kStructureKindNexus =
        static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
    constexpr u32_t kStructureKindInhibitor =
        static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Inhibitor);
    constexpr u32_t kStructureKindTurret =
        static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret);
    constexpr u32_t kLaneTop = static_cast<u32_t>(Winters::Map::eLane::Top);
    constexpr u32_t kLaneMid = static_cast<u32_t>(Winters::Map::eLane::Mid);
    constexpr u32_t kLaneBot = static_cast<u32_t>(Winters::Map::eLane::Bot);
    constexpr u32_t kLaneBase = static_cast<u32_t>(Winters::Map::eLane::Base);
    constexpr u16_t kTurretProjectileKind = 100;
    constexpr u8_t kServerMinionRoleRanged = 1u;
    constexpr f32_t kServerMinionRangedProjectileSpeed = 14.f;
    constexpr f32_t kServerMinionRangedProjectileHitRadius = 0.45f;
    constexpr f32_t kServerMinionRangedProjectileStartForwardOffset = 0.45f;
    constexpr f32_t kServerMinionRangedProjectileStartHeight = 0.85f;
    constexpr f32_t kServerMinionRangedProjectileTargetHeight = 0.85f;
    constexpr f32_t kServerMinionRangedProjectileMaxDistancePadding = 2.f;
    constexpr f32_t kDefaultStructureRadius = 1.5f;
    constexpr f32_t kStageFountainForwardFromTwin = -0.4f;
    constexpr f32_t kStageFountainSideFromTwin = -6.5f;
    constexpr f32_t kStageFountainSlotSpacing = 3.f;
    constexpr int32_t kStageChampionSpawnWalkableSearchRadius = 16;
    constexpr f32_t kServerNavGridStageBoundsPadding = 4.f;
    constexpr int32_t kServerNavGridSeedCoverageRadius = 24;
    constexpr f32_t kChampionAIInitialDecisionDelaySec = 0.35f;
    constexpr f32_t kChampionAISafeAnchorBehindTurret = 3.f;
    constexpr f32_t kDefaultChampionRespawnDelaySec = 5.f;
    struct StageBaseAnchors
    {
        Vec3 nexus{};
        Vec3 twinCenter{};
        bool_t bHasNexus = false;
        bool_t bHasTwinCenter = false;
    };

    struct StageGameplayBounds
    {
        f32_t minX = (std::numeric_limits<f32_t>::max)();
        f32_t minZ = (std::numeric_limits<f32_t>::max)();
        f32_t maxX = -(std::numeric_limits<f32_t>::max)();
        f32_t maxZ = -(std::numeric_limits<f32_t>::max)();
        bool_t bAny = false;
    };

    void IncludeStageGameplayBoundsPoint(StageGameplayBounds& bounds, const Vec3& p)
    {
        bounds.minX = (std::min)(bounds.minX, p.x);
        bounds.minZ = (std::min)(bounds.minZ, p.z);
        bounds.maxX = (std::max)(bounds.maxX, p.x);
        bounds.maxZ = (std::max)(bounds.maxZ, p.z);
        bounds.bAny = true;
    }

    bool_t BuildStageGameplayBounds(
        const Winters::Map::StageData& stage,
        StageGameplayBounds& outBounds)
    {
        outBounds = StageGameplayBounds{};

        for (const auto& waypoint : stage.minionWaypoints)
        {
            IncludeStageGameplayBoundsPoint(
                outBounds,
                Vec3{ waypoint.px, waypoint.py, waypoint.pz });
        }

        for (const auto& structure : stage.structures)
        {
            if (structure.bVisible == 0u)
                continue;

            IncludeStageGameplayBoundsPoint(
                outBounds,
                Vec3{ structure.px, structure.py, structure.pz });
        }

        return outBounds.bAny;
    }

    bool_t DoesServerNavGridCoverStageBounds(
        const Engine::CNavGrid& navGrid,
        const Winters::Map::StageData& stage,
        f32_t padding,
        StageGameplayBounds& outBounds)
    {
        if (!BuildStageGameplayBounds(stage, outBounds))
            return true;

        const f32_t navMinX = navGrid.Get_OriginX();
        const f32_t navMinZ = navGrid.Get_OriginZ();
        const f32_t navMaxX =
            navMinX + Engine::CNavGrid::kCellCountX * Engine::CNavGrid::kCellSize;
        const f32_t navMaxZ =
            navMinZ + Engine::CNavGrid::kCellCountY * Engine::CNavGrid::kCellSize;

        return outBounds.minX >= navMinX + padding &&
            outBounds.minZ >= navMinZ + padding &&
            outBounds.maxX <= navMaxX - padding &&
            outBounds.maxZ <= navMaxZ - padding;
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

    const char* ServerMinionDebugStateName(MinionStateComponent::State state)
    {
        switch (state)
        {
        case MinionStateComponent::Idle: return "Idle";
        case MinionStateComponent::LaneMove: return "LaneMove";
        case MinionStateComponent::Chase: return "Chase";
        case MinionStateComponent::Attack: return "Attack";
        case MinionStateComponent::Dead: return "Dead";
        default: return "Unknown";
        }
    }

    Engine::CNavGrid::Cell ResolveDebugCell(const Engine::CNavGrid* pGrid, const Vec3& pos)
    {
        if (!pGrid)
            return Engine::CNavGrid::Cell{ -1, -1 };

        return pGrid->WorldToCell(pos);
    }

    void OutputServerMinionPathDebug(
        const Engine::CNavGrid* pMoveGrid,
        const Engine::CNavGrid* pPathGrid,
        u64_t tickIndex,
        EntityID entity,
        const MinionStateComponent& state,
        const Vec3& vPos,
        const Vec3& vTarget,
        const Vec3& vResolvedTarget,
        u16_t pathCount,
        u32_t pathBuildBudget,
        bool_t bBuilt)
    {
        static u32_t s_minionPathLogCount = 0u;
        if (s_minionPathLogCount >= 256u)
            return;

        const Engine::CNavGrid::Cell posMoveCell = ResolveDebugCell(pMoveGrid, vPos);
        const Engine::CNavGrid::Cell posPathCell = ResolveDebugCell(pPathGrid, vPos);
        const Engine::CNavGrid::Cell targetMoveCell = ResolveDebugCell(pMoveGrid, vTarget);
        const Engine::CNavGrid::Cell targetPathCell = ResolveDebugCell(pPathGrid, vTarget);
        const Engine::CNavGrid::Cell resolvedMoveCell = ResolveDebugCell(pMoveGrid, vResolvedTarget);
        const Engine::CNavGrid::Cell resolvedPathCell = ResolveDebugCell(pPathGrid, vResolvedTarget);

        char msg[640]{};
        sprintf_s(
            msg,
            "[MinionMove][Path] tick=%llu entity=%u team=%u lane=%u result=%s "
            "pos=(%.2f,%.2f) moveCell=(%d,%d) pathCell=(%d,%d) "
            "target=(%.2f,%.2f) targetMoveCell=(%d,%d) targetPathCell=(%d,%d) "
            "resolved=(%.2f,%.2f) resolvedMoveCell=(%d,%d) resolvedPathCell=(%d,%d) pathCount=%u budget=%u\n",
            static_cast<unsigned long long>(tickIndex),
            static_cast<u32_t>(entity),
            static_cast<u32_t>(state.team),
            static_cast<u32_t>(state.lane),
            bBuilt ? "built" : "failed",
            vPos.x,
            vPos.z,
            posMoveCell.x,
            posMoveCell.y,
            posPathCell.x,
            posPathCell.y,
            vTarget.x,
            vTarget.z,
            targetMoveCell.x,
            targetMoveCell.y,
            targetPathCell.x,
            targetPathCell.y,
            vResolvedTarget.x,
            vResolvedTarget.z,
            resolvedMoveCell.x,
            resolvedMoveCell.y,
            resolvedPathCell.x,
            resolvedPathCell.y,
            static_cast<u32_t>(pathCount),
            pathBuildBudget);
        OutputServerAITrace(msg);
        ++s_minionPathLogCount;
    }

    void OutputServerMinionStuckDebug(
        const char* pReason,
        const Engine::CNavGrid* pMoveGrid,
        u64_t tickIndex,
        EntityID entity,
        const MinionStateComponent& state,
        const Vec3& vPos,
        const Vec3& vGoal,
        const Vec3* pWaypoint)
    {
        static u32_t s_minionStuckLogCount = 0u;
        if (s_minionStuckLogCount >= 256u)
            return;

        const Engine::CNavGrid::Cell posCell = ResolveDebugCell(pMoveGrid, vPos);
        const Engine::CNavGrid::Cell goalCell = ResolveDebugCell(pMoveGrid, vGoal);
        const Engine::CNavGrid::Cell waypointCell = pWaypoint
            ? ResolveDebugCell(pMoveGrid, *pWaypoint)
            : Engine::CNavGrid::Cell{ -1, -1 };

        char msg[512]{};
        sprintf_s(
            msg,
            "[MinionMove][Stuck] tick=%llu entity=%u team=%u lane=%u state=%s reason=%s "
            "blocked=%u pos=(%.2f,%.2f) posCell=(%d,%d) "
            "goal=(%.2f,%.2f) goalCell=(%d,%d) "
            "path=%u/%u waypointCell=(%d,%d)\n",
            static_cast<unsigned long long>(tickIndex),
            static_cast<u32_t>(entity),
            static_cast<u32_t>(state.team),
            static_cast<u32_t>(state.lane),
            ServerMinionDebugStateName(state.current),
            pReason ? pReason : "unknown",
            static_cast<u32_t>(state.BlockedMoveFrames),
            vPos.x,
            vPos.z,
            posCell.x,
            posCell.y,
            vGoal.x,
            vGoal.z,
            goalCell.x,
            goalCell.y,
            static_cast<u32_t>(state.PathIndex),
            static_cast<u32_t>(state.PathCount),
            waypointCell.x,
            waypointCell.y);
        OutputServerAITrace(msg);
        ++s_minionStuckLogCount;
    }

    void OutputServerNavGridSummary(const char* pLabel, const Engine::CNavGrid& navGrid)
    {
        char msg[256]{};
        sprintf_s(
            msg,
            "[ServerNav] %s origin=(%.2f,%.2f) walkable=%u hash=%08X\n",
            pLabel ? pLabel : "grid",
            navGrid.Get_OriginX(),
            navGrid.Get_OriginZ(),
            navGrid.CountWalkableCells(),
            navGrid.ComputeContentHash());
        OutputServerAITrace(msg);
    }

    bool_t TryResolveServerAuthoredNavGridPath(
        const wchar_t* pStagePath,
        std::wstring& outPath);

    bool_t TryLoadServerAuthoredNavGrid(
        const wchar_t* pStagePath,
        std::unique_ptr<Engine::CNavGrid>& outGrid);

    void PushUniqueServerPath(std::vector<std::wstring>& paths, const std::wstring& path)
    {
        if (path.empty())
            return;

        for (const std::wstring& existing : paths)
        {
            if (_wcsicmp(existing.c_str(), path.c_str()) == 0)
                return;
        }

        paths.push_back(path);
    }

    void EnsureServerTrailingSlash(std::wstring& path)
    {
        if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
            path.push_back(L'\\');
    }

    void PushWorkspaceDataPathCandidate(
        std::vector<std::wstring>& paths,
        const std::wstring& startDir,
        const wchar_t* pFileName)
    {
        if (!pFileName || pFileName[0] == L'\0')
            return;

        std::wstring base = startDir;
        EnsureServerTrailingSlash(base);

        for (u32_t depth = 0; depth < 8 && !base.empty(); ++depth)
        {
            if (FileExistsForServer(base + L"Winters.sln"))
            {
                PushUniqueServerPath(paths, base + L"Data\\" + pFileName);
                return;
            }

            std::wstring trimmed = base;
            while (!trimmed.empty() && (trimmed.back() == L'\\' || trimmed.back() == L'/'))
                trimmed.pop_back();

            const size_t slash = trimmed.find_last_of(L"\\/");
            if (slash == std::wstring::npos)
                break;

            base = trimmed.substr(0, slash + 1);
        }
    }

    bool_t TryResolveServerAuthoredNavGridPath(
        const wchar_t* pStagePath,
        std::wstring& outPath)
    {
        outPath.clear();

        std::vector<std::wstring> candidates{};
        if (pStagePath && pStagePath[0] != L'\0')
        {
            std::wstring fromStage = pStagePath;
            const size_t dot = fromStage.find_last_of(L'.');
            if (dot != std::wstring::npos)
                fromStage.resize(dot);
            fromStage += L".navgrid";
            PushUniqueServerPath(candidates, fromStage);
        }

        wchar_t exePath[MAX_PATH]{};
        const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH)
        {
            std::wstring exeDir = exePath;
            const size_t slash = exeDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
            {
                exeDir.resize(slash + 1);
                PushWorkspaceDataPathCandidate(candidates, exeDir, L"Stage1.navgrid");
            }
        }

        wchar_t cwd[MAX_PATH]{};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH)
        {
            std::wstring cwdDir = cwd;
            if (!cwdDir.empty() && cwdDir.back() != L'\\' && cwdDir.back() != L'/')
                cwdDir.push_back(L'\\');
            PushWorkspaceDataPathCandidate(candidates, cwdDir, L"Stage1.navgrid");
        }

        for (const std::wstring& candidate : candidates)
        {
            if (TryResolveExistingServerPath(candidate, outPath))
            {
                std::wstring msg = L"[ServerNav] authored navgrid path=" + outPath + L"\n";
                OutputServerAITraceW(msg.c_str());
                return true;
            }
        }

        return false;
    }

    bool_t TryLoadServerAuthoredNavGrid(
        const wchar_t* pStagePath,
        std::unique_ptr<Engine::CNavGrid>& outGrid)
    {
        outGrid.reset();

        std::wstring navGridPath{};
        if (!TryResolveServerAuthoredNavGridPath(pStagePath, navGridPath))
            return false;

        outGrid = Engine::CNavGrid::LoadFromFile(navGridPath.c_str());
        if (!outGrid)
        {
            std::wstring msg = L"[ServerNav] authored navgrid load failed path=" + navGridPath + L"\n";
            OutputServerAITraceW(msg.c_str());
            return false;
        }

        return true;
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

    eTeam StageTeamToGameTeam(u32_t team)
    {
        switch (static_cast<Winters::Map::eTeam>(team))
        {
        case Winters::Map::eTeam::Red:
            return eTeam::Red;
        case Winters::Map::eTeam::Neutral:
            return eTeam::Neutral;
        case Winters::Map::eTeam::Blue:
        default:
            return eTeam::Blue;
        }
    }

    u8_t ResolveServerWaypointLane(eTeam team, u8_t lane)
    {
        return CServerMinionWaveRuntime::ResolveWaypointLane(team, lane);
    }

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

    f32_t ResolveStageStructureMaxHp(u32_t kind)
    {
        if (kind == kStructureKindNexus)
            return 5500.f;
        if (kind == kStructureKindInhibitor)
            return 4000.f;
        return 3000.f;
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

    f32_t ResolveStageJungleMaxHp(u32_t subKind)
    {
        switch (subKind)
        {
        case 0u:
            return 8000.f;
        case 1u:
            return 5000.f;
        default:
            return 1500.f;
        }
    }

    f32_t ResolveStageJungleRadius(u32_t subKind)
    {
        switch (subKind)
        {
        case 0u:
            return 2.5f;
        case 1u:
            return 2.2f;
        case 2u:
        case 3u:
        case 5u:
            return 1.2f;
        case 4u:
        case 6u:
        case 7u:
            return 1.0f;
        case 8u:
        case 9u:
        case 10u:
            return 0.7f;
        default:
            return 1.0f;
        }
    }

    f32_t ResolveStageJungleAttackRange(u32_t subKind)
    {
        switch (subKind)
        {
        case 0u: return 4.0f;
        case 1u: return 3.0f;
        case 2u:
        case 3u:
        case 5u: return 2.0f;
        case 8u:
        case 9u:
        case 10u: return 1.4f;
        default: return 1.7f;
        }
    }

    f32_t ResolveStageJungleAttackDamage(u32_t subKind)
    {
        switch (subKind)
        {
        case 0u: return 120.f;
        case 1u: return 90.f;
        case 2u:
        case 3u: return 65.f;
        case 5u: return 60.f;
        case 8u:
        case 9u:
        case 10u: return 25.f;
        default: return 45.f;
        }
    }

    f32_t ResolveStageJungleAttackCooldown(u32_t subKind)
    {
        switch (subKind)
        {
        case 0u: return 1.2f;
        case 1u: return 1.5f;
        case 8u:
        case 9u:
        case 10u: return 1.25f;
        default: return 1.4f;
        }
    }

    f32_t ResolveStageJungleMoveSpeed(u32_t subKind)
    {
        switch (subKind)
        {
        case 0u: return 2.5f;
        case 1u: return 4.0f;
        case 8u:
        case 9u:
        case 10u: return 4.5f;
        default: return 4.0f;
        }
    }

    VisibilityComponent BuildServerVisibleToAll()
    {
        VisibilityComponent visibility{};
        visibility.teamVisibilityMask = static_cast<u8_t>(
            (1u << TeamByte(eTeam::Blue)) |
            (1u << TeamByte(eTeam::Red)));
        return visibility;
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

    bool_t TryResolveServerMinionTargetCandidate(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        u8_t myLane,
        const Vec3& myPos,
        f32_t maxRange,
        EntityID candidate,
        Vec3& outPos,
        f32_t& outDistSq)
    {
        if (world.HasComponent<PracticeDummyTag>(candidate))
            return false;
        if (candidate == self || !IsAliveHealth(world, candidate))
            return false;
        if (!world.HasComponent<TransformComponent>(candidate))
            return false;
        if (world.HasComponent<MinionComponent>(candidate) &&
            world.GetComponent<MinionComponent>(candidate).laneType != myLane)
        {
            return false;
        }
        if (world.HasComponent<MinionStateComponent>(candidate) &&
            world.GetComponent<MinionStateComponent>(candidate).lane != myLane)
        {
            return false;
        }
        if (world.HasComponent<StructureComponent>(candidate) &&
            !world.HasComponent<TargetableTag>(candidate))
        {
            return false;
        }
        if (world.HasComponent<StructureComponent>(candidate))
        {
            const StructureComponent& structure = world.GetComponent<StructureComponent>(candidate);
            if (structure.lane != myLane && structure.lane != kLaneBase)
                return false;
        }

        eTeam targetTeam = eTeam::Neutral;
        if (!TryResolveCombatTeam(world, candidate, targetTeam))
            return false;
        if (targetTeam == myTeam || targetTeam == eTeam::Neutral)
            return false;
        if (!GameplayStateQuery::CanBeTargetedBy(world, self, candidate))
            return false;

        outPos = world.GetComponent<TransformComponent>(candidate).GetPosition();
        outDistSq = WintersMath::DistanceSqXZ(myPos, outPos);

        f32_t resolvedMaxRange = maxRange;
        if (world.HasComponent<StructureComponent>(candidate))
        {
            resolvedMaxRange += ResolveAgentRadius(world, candidate) +
                ServerMinionTuning::kStructureAcquireRangePadding;
        }

        const f32_t maxRangeSq = resolvedMaxRange * resolvedMaxRange;
        return outDistSq <= maxRangeSq;
    }

    bool_t TryResolveServerMinionTargetPriority(
        CWorld& world,
        EntityID entity,
        i32_t& outPriority)
    {
        if (world.HasComponent<MinionComponent>(entity))
        {
            outPriority = 0;
            return true;
        }
        if (world.HasComponent<StructureComponent>(entity))
        {
            outPriority = 1;
            return true;
        }
        if (world.HasComponent<ChampionComponent>(entity))
        {
            outPriority = 2;
            return true;
        }
        return false;
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

    bool_t IsServerRangedMinion(const MinionStateComponent& state)
    {
        return state.type == kServerMinionRoleRanged;
    }

    eProjectileKind ResolveServerMinionRangedProjectileKind(eTeam team)
    {
        return team == eTeam::Red
            ? eProjectileKind::MinionRangedBasicRed
            : eProjectileKind::MinionRangedBasicBlue;
    }

    bool_t IsServerMinionRangedProjectileKind(eProjectileKind kind)
    {
        return kind == eProjectileKind::MinionRangedBasicBlue ||
            kind == eProjectileKind::MinionRangedBasicRed;
    }

    Vec3 OffsetLaneStartAlong(const Vec3& start, const Vec3& next, f32_t offset)
    {
        const Vec3 dir = WintersMath::DirectionXZ(
            start,
            next,
            Vec3{ 1.f, 0.f, 0.f },
            std::numeric_limits<f32_t>::epsilon());
        return Vec3{
            start.x + dir.x * offset,
            start.y,
            start.z + dir.z * offset
        };
    }

    eChampion ResolveAnimationChampion(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).id;
        if (world.HasComponent<StatComponent>(entity))
            return world.GetComponent<StatComponent>(entity).championId;
        return eChampion::NONE;
    }

    u8_t ResolveAnimationSlot(eNetAnimId animId)
    {
        switch (animId)
        {
        case eNetAnimId::SkillQ:
            return static_cast<u8_t>(eSkillSlot::Q);
        case eNetAnimId::SkillW:
            return static_cast<u8_t>(eSkillSlot::W);
        case eNetAnimId::SkillE:
            return static_cast<u8_t>(eSkillSlot::E);
        case eNetAnimId::SkillR:
            return static_cast<u8_t>(eSkillSlot::R);
        case eNetAnimId::BasicAttack:
        default:
            return static_cast<u8_t>(eSkillSlot::BasicAttack);
        }
    }

    void StartReplicatedAnimation(CWorld& world, EntityID entity, eNetAnimId animId,
        const TickContext& tc)
    {
        auto& anim = world.HasComponent<NetAnimationComponent>(entity)
            ? world.GetComponent<NetAnimationComponent>(entity)
            : world.AddComponent<NetAnimationComponent>(entity, NetAnimationComponent{});

        const eChampion champion = ResolveAnimationChampion(world, entity);
        const u8_t slot = ResolveAnimationSlot(animId);
        const ChampionSkillTimingDefaults timing =
            ChampionGameDataDB::ResolveSkillTiming(champion, slot);

        ++anim.actionSeq;
        anim.animId = static_cast<u16_t>(animId);
        anim.animPhaseFrame = 0;
        anim.animStartTick = tc.tickIndex;
        anim.playbackRateQ8 = EncodeSkillPlaybackRateQ8(timing.animPlaySpeed);
        anim.flags = 1;
    }

    EntityID FindClosestEnemyCombatTarget(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        u8_t myLane,
        const Vec3& myPos,
        f32_t maxRange)
    {
        EntityID best = NULL_ENTITY;
        i32_t bestPriority = std::numeric_limits<i32_t>::max();
        const f32_t maxRangeSq = maxRange * maxRange;
        f32_t bestDistSq = maxRangeSq;

        auto tryTarget = [&](EntityID entity, i32_t priority)
        {
            Vec3 pos{};
            f32_t distSq = 0.f;
            if (!TryResolveServerMinionTargetCandidate(
                world,
                self,
                myTeam,
                myLane,
                myPos,
                maxRange,
                entity,
                pos,
                distSq))
            {
                return;
            }

            if (priority < bestPriority || (priority == bestPriority && distSq < bestDistSq))
            {
                bestPriority = priority;
                bestDistSq = distSq;
                best = entity;
            }
        };

        if (CSpatialIndex* pSpatial = world.Get_SpatialIndex())
        {
            const u32_t targetMask =
                SpatialMask(eSpatialKind::Minion) |
                SpatialMask(eSpatialKind::Turret) |
                SpatialMask(eSpatialKind::Inhibitor) |
                SpatialMask(eSpatialKind::Nexus) |
                SpatialMask(eSpatialKind::Champion);
            std::vector<EntityID> candidates;
            const f32_t queryRange =
                maxRange + ServerMinionTuning::kStructureAcquireRangePadding;
            candidates.reserve(64);
            pSpatial->QueryRadius(
                myPos,
                queryRange,
                targetMask,
                1u << TeamByte(myTeam),
                candidates);

            std::sort(candidates.begin(), candidates.end());
            candidates.erase(
                std::unique(candidates.begin(), candidates.end()),
                candidates.end());

            for (EntityID candidate : candidates)
            {
                i32_t priority = 0;
                if (TryResolveServerMinionTargetPriority(world, candidate, priority))
                    tryTarget(candidate, priority);
            }

            return best;
        }

        world.ForEach<MinionComponent>(
            std::function<void(EntityID, MinionComponent&)>(
                [&](EntityID entity, MinionComponent&) { tryTarget(entity, 0); }));
        world.ForEach<StructureComponent>(
            std::function<void(EntityID, StructureComponent&)>(
                [&](EntityID entity, StructureComponent&) { tryTarget(entity, 1); }));
        world.ForEach<ChampionComponent>(
            std::function<void(EntityID, ChampionComponent&)>(
                [&](EntityID entity, ChampionComponent&) { tryTarget(entity, 2); }));

        return best;
    }

    f32_t ResolveCombatRadius(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<SpatialAgentComponent>(entity))
            return std::max(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);
        if (world.HasComponent<StructureComponent>(entity))
            return 1.5f;
        if (world.HasComponent<MinionComponent>(entity) ||
            world.HasComponent<MinionStateComponent>(entity))
            return 0.45f;
        return 0.65f;
    }

    EntityID FindSkillProjectileHitTarget(
        CWorld& world,
        const SkillProjectileComponent& projectile,
        const Vec3& start,
        const Vec3& end,
        Vec3& outHitPos)
    {
        EntityID bestTarget = NULL_ENTITY;
        f32_t bestT = 1.f;

        world.ForEach<HealthComponent, TransformComponent>(
            std::function<void(EntityID, HealthComponent&, TransformComponent&)>(
                [&](EntityID entity, HealthComponent& health, TransformComponent& transform)
                {
                    const bool_t bYasuoTornado =
                        projectile.kind == eProjectileKind::Tornado;
                    if (entity == projectile.sourceEntity ||
                        !world.IsAlive(entity) ||
                        health.bIsDead ||
                        health.fCurrent <= 0.f)
                    {
                        return;
                    }

                    eTeam targetTeam = eTeam::Neutral;
                    if (!TryResolveCombatTeam(world, entity, targetTeam))
                        return;
                    if (targetTeam == projectile.sourceTeam &&
                        targetTeam != eTeam::Neutral)
                    {
                        return;
                    }
                    if (!GameplayStateQuery::CanReceiveProjectileHit(
                        world,
                        projectile.sourceEntity,
                        entity))
                    {
                        return;
                    }

                    const Vec3 targetPos = transform.GetPosition();
                    f32_t t = 0.f;
                    const f32_t distSq = WintersMath::DistanceSqPointToSegmentXZ(
                        targetPos,
                        start,
                        end,
                        &t,
                        std::numeric_limits<f32_t>::epsilon());
                    const f32_t projectileRadius = bYasuoTornado
                        ? std::max(projectile.hitRadius, 2.25f)
                        : projectile.hitRadius;
                    const f32_t radius = projectileRadius + ResolveCombatRadius(world, entity);
                    if (distSq <= radius * radius && t <= bestT)
                    {
                        bestTarget = entity;
                        bestT = t;
                        outHitPos = Vec3{
                            start.x + (end.x - start.x) * t,
                            targetPos.y + 1.0f,
                            start.z + (end.z - start.z) * t
                        };
                    }
                }));

        return bestTarget;
    }

    void LogSkillProjectileEvent(
        const char* state,
        EntityID projectileEntity,
        const SkillProjectileComponent& projectile,
        EntityID targetEntity,
        const Vec3& pos)
    {
        static u32_t s_logCount = 0;
        if (s_logCount >= 128u)
            return;

        char msg[256]{};
        sprintf_s(msg,
            "[SkillProjectile] %s kind=%u source=%u projectile=%u target=%u pos=(%.2f,%.2f,%.2f) traveled=%.2f\n",
            state ? state : "-",
            static_cast<u32_t>(projectile.kind),
            static_cast<u32_t>(projectile.sourceEntity),
            static_cast<u32_t>(projectileEntity),
            static_cast<u32_t>(targetEntity),
            pos.x,
            pos.y,
            pos.z,
            projectile.traveledDistance);
        OutputServerAITrace(msg);
        ++s_logCount;
    }
}

std::unique_ptr<CGameRoom> CGameRoom::Create(u32_t roomId)
{
    auto room = std::unique_ptr<CGameRoom>(new CGameRoom(roomId));
    room->m_pExecutor = CDefaultCommandExecutor::Create();
    room->m_pSnapBuilder = CSnapshotBuilder::Create();
    room->m_pLagCompensation = std::make_unique<CLagCompensation>();
    room->m_pReplayRecorder = CReplayRecorder::Create(roomId, 30);
    room->InitializeServerSimSystems();
    return room;
}

CGameRoom::CGameRoom(u32_t roomId)
    : m_roomId(roomId)
{
    InitializeLobbySlots();
}

CGameRoom::~CGameRoom()
{
    Stop();
}

void CGameRoom::Start()
{
    if (m_bRunning.exchange(true))
        return;

    m_tickThread = std::thread(&CGameRoom::TickThread, this);
}

void CGameRoom::Stop()
{
    const bool_t bWasRunning = m_bRunning.exchange(false);
    if (bWasRunning && m_tickThread.joinable())
        m_tickThread.join();

    FinalizeReplayRecorder();
}

void CGameRoom::Phase_ServerDeathAndRespawn(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    const auto entities = DeterministicEntityIterator<RespawnComponent>::CollectSorted(m_world);
    for (EntityID entity : entities)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<RespawnComponent>(entity) ||
            !m_world.HasComponent<HealthComponent>(entity) ||
            !m_world.HasComponent<ChampionComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& respawn = m_world.GetComponent<RespawnComponent>(entity);
        auto& health = m_world.GetComponent<HealthComponent>(entity);
        auto& champion = m_world.GetComponent<ChampionComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);

        const bool_t bDead = health.bIsDead || health.fCurrent <= 0.f;
        if (!bDead)
        {
            if (respawn.bPending)
            {
                respawn.bPending = false;
                respawn.respawnTimer = 0.f;
                if (!m_world.HasComponent<TargetableTag>(entity))
                    m_world.AddComponent<TargetableTag>(entity);
            }
            continue;
        }

        health.fCurrent = 0.f;
        health.bIsDead = true;
        champion.hp = 0.f;
        champion.maxHp = health.fMaximum;

        if (!respawn.bPending)
        {
            respawn.bPending = true;
            respawn.respawnTimer = respawn.respawnDelay > 0.f
                ? respawn.respawnDelay
                : kDefaultChampionRespawnDelaySec;

            if (m_world.HasComponent<TargetableTag>(entity))
                m_world.RemoveComponent<TargetableTag>(entity);

            if (m_world.HasComponent<MoveTargetComponent>(entity))
            {
                auto& moveTarget = m_world.GetComponent<MoveTargetComponent>(entity);
                moveTarget.bHasTarget = false;
                moveTarget.pathCount = 0;
                moveTarget.pathIndex = 0;
            }

            if (m_world.HasComponent<SkillStateComponent>(entity))
            {
                auto& skillState = m_world.GetComponent<SkillStateComponent>(entity);
                for (u8_t i = 0; i < 5; ++i)
                {
                    skillState.slots[i].currentStage = 0;
                    skillState.slots[i].stageWindow = 0.f;
                }
            }

            if (m_world.HasComponent<ChampionAIComponent>(entity))
            {
                auto& ai = m_world.GetComponent<ChampionAIComponent>(entity);
                ai.state = eChampionAIState::Dead;
                ai.lastAction = eChampionAIAction::Retreat;
                ai.lockedChampion = NULL_ENTITY;
                ai.targetMinion = NULL_ENTITY;
                ai.targetStructure = NULL_ENTITY;
                ai.alliedWave = NULL_ENTITY;
                ai.comboTarget = NULL_ENTITY;
                ai.comboStep = 0u;
                ai.bWaveJoined = false;
                ai.bStructureWaveTanking = false;
                ai.bInsideEnemyTurretDanger = false;
            }

            StartReplicatedAnimation(m_world, entity, eNetAnimId::Death, tc);

            static u32_t s_deathLogCount = 0;
            if (s_deathLogCount < 64u)
            {
                char msg[224]{};
                sprintf_s(msg,
                    "[Respawn] death tick=%llu entity=%u champion=%u team=%u respawn=%.2f\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(champion.id),
                    static_cast<u32_t>(champion.team),
                    respawn.respawnTimer);
                OutputServerAITrace(msg);
                ++s_deathLogCount;
            }
        }

        if (respawn.respawnTimer > 0.f)
        {
            respawn.respawnTimer -= tc.fDt;
            if (respawn.respawnTimer > 0.f)
                continue;
        }

        health.fCurrent = health.fMaximum;
        health.bIsDead = false;
        champion.hp = health.fCurrent;
        champion.maxHp = health.fMaximum;
        transform.SetPosition(respawn.spawnPos);

        if (!m_world.HasComponent<TargetableTag>(entity))
            m_world.AddComponent<TargetableTag>(entity);
        if (m_world.HasComponent<MoveTargetComponent>(entity))
        {
            auto& moveTarget = m_world.GetComponent<MoveTargetComponent>(entity);
            moveTarget.bHasTarget = false;
            moveTarget.pathCount = 0;
            moveTarget.pathIndex = 0;
        }

        if (m_world.HasComponent<ChampionAIComponent>(entity))
        {
            auto& ai = m_world.GetComponent<ChampionAIComponent>(entity);
            ai.state = eChampionAIState::MoveToOuterTurret;
            ai.lastAction = eChampionAIAction::MoveToSafeAnchor;
            ai.lockedChampion = NULL_ENTITY;
            ai.targetMinion = NULL_ENTITY;
            ai.targetStructure = NULL_ENTITY;
            ai.alliedWave = NULL_ENTITY;
            ai.comboTarget = NULL_ENTITY;
            ai.comboStep = 0u;
            ai.bWaveJoined = false;
            ai.bStructureWaveTanking = false;
            ai.bInsideEnemyTurretDanger = false;
            ai.decisionTimer = 0.25f;
        }

        respawn.bPending = false;
        respawn.respawnTimer = 0.f;

        if (champion.id == eChampion::SYLAS &&
            m_world.HasComponent<PracticeDummyTag>(entity) &&
            m_world.HasComponent<WaypointPatrolComponent>(entity))
        {
            auto& patrol = m_world.GetComponent<WaypointPatrolComponent>(entity);
            patrol.currentIndex = 1;
            patrol.direction = 1;
            patrol.bActive = true;
        }

        StartReplicatedAnimation(m_world, entity, eNetAnimId::Idle, tc);

        static u32_t s_respawnLogCount = 0;
        if (s_respawnLogCount < 64u)
        {
            char msg[256]{};
            sprintf_s(msg,
                "[Respawn] revive tick=%llu entity=%u champion=%u team=%u pos=(%.2f,%.2f,%.2f) hp=%.2f\n",
                static_cast<unsigned long long>(tc.tickIndex),
                static_cast<u32_t>(entity),
                static_cast<u32_t>(champion.id),
                static_cast<u32_t>(champion.team),
                respawn.spawnPos.x,
                respawn.spawnPos.y,
                respawn.spawnPos.z,
                health.fCurrent);
            OutputServerAITrace(msg);
            ++s_respawnLogCount;
        }
    }
}

void CGameRoom::InitializeServerSimSystems()
{
    m_world.Initialize_Spatial(LoLSpatialGridDesc());
    EnsureMatchScoreEntity(m_world);
    m_pSpatialSystem = Engine::CSpatialHashSystem::Create();
    m_pTurretAI = Engine::CTurretAISystem::Create();

    RegisterDefaultChampionSkillScalingTables();

    AnnieGameSim::RegisterHooks();
    AsheGameSim::RegisterHooks();
    FioraGameSim::RegisterHooks();
    IreliaGameSim::RegisterHooks();
    JaxGameSim::RegisterHooks();
    KalistaGameSim::RegisterHooks();
    LeeSinGameSim::RegisterHooks();
    KindredGameSim::RegisterHooks();
    MasterYiGameSim::RegisterHooks();
    RivenGameSim::RegisterHooks();
    SylasGameSim::RegisterHooks();
    ViegoGameSim::RegisterHooks();
    YoneGameSim::RegisterHooks();
    YasuoGameSim::RegisterHooks();
    ZedGameSim::RegisterHooks();
}

void CGameRoom::InitializeServerWalkableGrid(const Winters::Map::StageData* pStage, const wchar_t* pStagePath)
{
    m_pPathNavGrid.reset();

    std::unique_ptr<Engine::CNavGrid> authoredGrid{};
    if (TryLoadServerAuthoredNavGrid(pStagePath, authoredGrid))
    {
        bool_t bUseAuthoredGrid = true;
        StageGameplayBounds stageBounds{};
        if (pStage &&
            !DoesServerNavGridCoverStageBounds(
                *authoredGrid,
                *pStage,
                kServerNavGridStageBoundsPadding,
                stageBounds))
        {
            char msg[320]{};
            sprintf_s(
                msg,
                "[ServerNav] authored navgrid rejected: stage bounds x=(%.2f,%.2f) z=(%.2f,%.2f) outside origin=(%.2f,%.2f) size=(%.2f,%.2f)\n",
                stageBounds.minX,
                stageBounds.maxX,
                stageBounds.minZ,
                stageBounds.maxZ,
                authoredGrid->Get_OriginX(),
                authoredGrid->Get_OriginZ(),
                Engine::CNavGrid::kCellCountX * Engine::CNavGrid::kCellSize,
                Engine::CNavGrid::kCellCountY * Engine::CNavGrid::kCellSize);
            OutputServerAITrace(msg);
            bUseAuthoredGrid = false;
        }

        if (bUseAuthoredGrid)
        {
            m_pNavGrid = std::move(authoredGrid);
            OutputServerNavGridSummary("authored navgrid loaded", *m_pNavGrid);
            BuildServerPathNavGrid();
            return;
        }
    }

    OutputServerAITrace("[ServerNav] authored navgrid missing or rejected; fallback bake will not match yellow debug cells\n");

    std::vector<Vec3> seeds{};
    seeds.reserve(128);

    f32_t minX = (std::numeric_limits<f32_t>::max)();
    f32_t minZ = (std::numeric_limits<f32_t>::max)();
    f32_t maxX = -(std::numeric_limits<f32_t>::max)();
    f32_t maxZ = -(std::numeric_limits<f32_t>::max)();

    auto includeBounds = [&](const Vec3& p)
    {
        minX = (std::min)(minX, p.x);
        minZ = (std::min)(minZ, p.z);
        maxX = (std::max)(maxX, p.x);
        maxZ = (std::max)(maxZ, p.z);
    };

    auto addSeed = [&](const Vec3& p)
    {
        seeds.push_back(p);
        includeBounds(p);
    };

    if (pStage)
    {
        for (const auto& waypoint : pStage->minionWaypoints)
            addSeed(Vec3{ waypoint.px, waypoint.py, waypoint.pz });

        for (const auto& structure : pStage->structures)
            includeBounds(Vec3{ structure.px, structure.py, structure.pz });

        for (const auto& jungle : pStage->jungles)
            includeBounds(Vec3{ jungle.px, jungle.py, jungle.pz });

        for (const LobbySlotState& slot : m_lobbySlots)
        {
            if (!slot.bHuman && !slot.bBot)
                continue;

            Vec3 spawn{};
            if (TryResolveStageFountainSpawn(
                *pStage,
                slot.slotId,
                static_cast<eTeam>(slot.team),
                spawn))
            {
                addSeed(spawn);
            }
        }
    }

    for (const LobbySlotState& slot : m_lobbySlots)
    {
        if (!slot.bHuman && !slot.bBot)
            continue;

        if (IsRedSylasSmokeDummySlot(slot))
        {
            addSeed(GetRedSylasSmokeDummyPosition());
            for (u8_t i = 0; i < GetRedSylasSmokePatrolPointCount(); ++i)
                addSeed(GetRedSylasSmokePatrolPoint(i));
            continue;
        }

        addSeed(GetGameSimRosterSpawnPosition(slot.slotId, slot.team, slot.bBot));
    }

    if (seeds.empty())
        addSeed(Vec3{ 0.f, 1.f, 0.f });

    m_pMapSurfaceSampler = std::make_unique<Engine::CMapSurfaceSampler>();

    std::wstring wmeshPath;
    const bool_t bResolvedWMesh = ResolveServerWMeshPath(wmeshPath);
    const Mat4 mapWorld =
        Mat4::Scale(Vec3{ -0.01f, 0.01f, 0.01f }) *
        Mat4::RotationY(DirectX::XMConvertToRadians(-135.f)) *
        Mat4::Translation(Vec3{ 0.f, 0.f, 0.f });

    const bool_t bSurfaceLoaded =
        bResolvedWMesh &&
        m_pMapSurfaceSampler->LoadFromWMesh(wmeshPath.c_str(), mapWorld);

    if (bSurfaceLoaded && !pStage)
    {
        includeBounds(Vec3{ m_pMapSurfaceSampler->GetMinX(), 0.f, m_pMapSurfaceSampler->GetMinZ() });
        includeBounds(Vec3{ m_pMapSurfaceSampler->GetMaxX(), 0.f, m_pMapSurfaceSampler->GetMaxZ() });
    }

    constexpr f32_t kNavPadding = 8.f;
    minX -= kNavPadding;
    minZ -= kNavPadding;
    maxX += kNavPadding;
    maxZ += kNavPadding;

    const f32_t gridWorldX = Engine::CNavGrid::kCellCountX * Engine::CNavGrid::kCellSize;
    const f32_t gridWorldZ = Engine::CNavGrid::kCellCountY * Engine::CNavGrid::kCellSize;
    const f32_t centerX = (minX + maxX) * 0.5f;
    const f32_t centerZ = (minZ + maxZ) * 0.5f;
    m_pNavGrid = Engine::CNavGrid::Create(
        centerX - gridWorldX * 0.5f,
        centerZ - gridWorldZ * 0.5f);

    if (!bSurfaceLoaded)
    {
        m_pNavGrid->SetAllWalkable(true);
        m_pMapSurfaceSampler.reset();
        OutputServerAITrace("[ServerNav] wmesh load failed; terrain walls disabled; structures-only nav fallback\n");
        OutputServerNavGridSummary("fallback authored grid", *m_pNavGrid);
        BuildServerPathNavGrid();
        return;
    }

    Engine::MapWalkableBakeDesc desc{};
    const bool_t bBaked = Engine::CMapWalkableBaker::BakeIntoNavGrid(
        *m_pMapSurfaceSampler,
        *m_pNavGrid,
        seeds,
        desc);

    bool_t bSeedsCovered = bBaked;
    if (bSeedsCovered)
    {
        for (const Vec3& seed : seeds)
        {
            const Engine::CNavGrid::Cell cell = m_pNavGrid->WorldToCell(seed);
            Engine::CNavGrid::Cell nearest{};
            if (m_pNavGrid->IsWalkable(cell.x, cell.y) ||
                m_pNavGrid->TryFindNearestWalkableCell(
                    cell,
                    kServerNavGridSeedCoverageRadius,
                    nearest))
            {
                continue;
            }

            char seedMsg[192]{};
            sprintf_s(
                seedMsg,
                "[ServerNav] terrain bake seed uncovered pos=(%.2f,%.2f) cell=(%d,%d)\n",
                seed.x,
                seed.z,
                cell.x,
                cell.y);
            OutputServerAITrace(seedMsg);
            bSeedsCovered = false;
            break;
        }
    }

    if (!bBaked || !bSeedsCovered)
    {
        m_pNavGrid->SetAllWalkable(true);
        OutputServerAITrace("[ServerNav] terrain bake failed or missed gameplay seeds; fallback all-walkable grid\n");
        OutputServerNavGridSummary("fallback authored grid", *m_pNavGrid);
        BuildServerPathNavGrid();
        return;
    }

    char msg[224]{};
    sprintf_s(msg,
        "[ServerNav] walkable grid baked cells=%u seeds=%zu hash=%08X\n",
        m_pNavGrid->CountWalkableCells(),
        seeds.size(),
        m_pNavGrid->ComputeContentHash());
    OutputServerAITrace(msg);
    BuildServerPathNavGrid();
}

void CGameRoom::CarveServerStructuresOnNavGrid()
{
    if (!m_pNavGrid)
        return;

    u32_t carvedStructures = 0;
    m_world.ForEach<StructureComponent, TransformComponent>(
        std::function<void(EntityID, StructureComponent&, TransformComponent&)>(
            [&](EntityID, StructureComponent& structure, TransformComponent& transform)
            {
                const f32_t radius = ResolveStageStructureRadius(structure.kind, structure.tier);

                const Vec3 pos = transform.GetPosition();
                const Engine::CNavGrid::Cell center = m_pNavGrid->WorldToCell(pos);
                const int32_t rCells = static_cast<int32_t>(std::ceil(radius / Engine::CNavGrid::kCellSize));
                for (int32_t dy = -rCells; dy <= rCells; ++dy)
                {
                    for (int32_t dx = -rCells; dx <= rCells; ++dx)
                    {
                        if (dx * dx + dy * dy <= rCells * rCells)
                            m_pNavGrid->SetWalkable(center.x + dx, center.y + dy, false);
                    }
                }

                ++carvedStructures;
            }));

    char msg[192]{};
    sprintf_s(msg,
        "[ServerNav] structures carved=%u walkable=%u hash=%08X\n",
        carvedStructures,
        m_pNavGrid->CountWalkableCells(),
        m_pNavGrid->ComputeContentHash());
    OutputServerAITrace(msg);
    BuildServerPathNavGrid();
}

void CGameRoom::BuildServerPathNavGrid()
{
    if (!m_pNavGrid)
    {
        m_pPathNavGrid.reset();
        m_pMinionLaneNavGrid.reset();
        return;
    }
    m_pPathNavGrid = m_pNavGrid->BuildInflated(ServerMinionTuning::kPathAgentRadius);
    m_pMinionLaneNavGrid = m_pNavGrid->BuildInflated(ServerMinionTuning::kMinionLaneClearanceRadius);

    if (!m_pPathNavGrid || !m_pMinionLaneNavGrid)
    {
        OutputServerAITrace("[ServerNav] path grid or minion lane grid inflate failed\n");
        return;
    }

    Engine::CPathfinder::PrewarmReachabilityCache(m_pPathNavGrid.get());
    Engine::CPathfinder::PrewarmReachabilityCache(m_pMinionLaneNavGrid.get());
}

bool_t CGameRoom::LoadServerStageData(
    Winters::Map::StageData& outStage,
    std::wstring& outPath) const
{
    outStage.Clear();
    outPath.clear();

    std::vector<std::wstring> candidates;

    wchar_t exePath[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH)
    {
        std::wstring exeDir = exePath;
        const size_t slash = exeDir.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
        {
            exeDir.resize(slash + 1);
            PushWorkspaceDataPathCandidate(candidates, exeDir, L"Stage1.dat");
        }
    }

    wchar_t cwd[MAX_PATH]{};
    const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
    if (cwdLen > 0 && cwdLen < MAX_PATH)
        PushWorkspaceDataPathCandidate(candidates, cwd, L"Stage1.dat");

    for (const std::wstring& candidate : candidates)
    {
        if (Winters::Map::LoadStageDataFromFile(candidate.c_str(), outStage))
        {
            outPath = candidate;
            return true;
        }
    }

    return false;
}

void CGameRoom::CacheServerMinionWaypoints(const Winters::Map::StageData& stage)
{
    m_serverMinionWaves.CacheWaypoints(stage);
}

void CGameRoom::RebuildServerMinionFlowFields()
{
    const Engine::CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    m_serverMinionWaves.RebuildFlowFields(pGrid);
}

void CGameRoom::Phase_ServerMinionWave(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame || !m_bGameplayObjectsSpawned)
        return;

    m_serverMinionWaves.TickWave(
        tc.tickIndex,
        [this](const CServerMinionWaveRuntime::SpawnRequest& request)
        {
            SpawnServerMinion(request.team, request.roleType, request.lane, request.pos);
        });
}

void CGameRoom::Phase_ServerMinionAI(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    const auto minions =
        DeterministicEntityIterator<MinionStateComponent>::CollectSorted(m_world);

    u32_t PathBuildBudget = ServerMinionTuning::kPathBuildBudgetPerTick;

    for (EntityID entity : minions)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<MinionStateComponent>(entity) ||
            !m_world.HasComponent<MinionComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& state = m_world.GetComponent<MinionStateComponent>(entity);
        auto& minion = m_world.GetComponent<MinionComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);

        if (m_world.HasComponent<HealthComponent>(entity))
        {
            auto& hp = m_world.GetComponent<HealthComponent>(entity);
            minion.hp = hp.fCurrent;
            minion.maxHp = hp.fMaximum;
            if (hp.bIsDead || hp.fCurrent <= 0.f)
            {
                if (state.current != MinionStateComponent::Dead)
                {
                    state.current = MinionStateComponent::Dead;
                    state.deathTimer = 1.2f;
                    StartReplicatedAnimation(m_world, entity, eNetAnimId::Death, tc);
                }
                else if (state.deathTimer > 0.f)
                {
                    state.deathTimer -= tc.fDt;
                }

                if (state.deathTimer <= 0.f)
                {
                    const NetEntityId netId = m_entityMap.ToNet(entity);
                    if (netId != NULL_NET_ENTITY)
                        m_entityMap.Unbind(netId);
                    m_world.DestroyEntity(entity);
                }
                continue;
            }
        }

        if (state.attackCooldown > 0.f)
        {
            state.attackCooldown -= tc.fDt;
            if (state.attackCooldown < 0.f)
                state.attackCooldown = 0.f;
        }
        if (state.targetScanCooldown > 0.f)
        {
            state.targetScanCooldown -= tc.fDt;
            if (state.targetScanCooldown < 0.f)
                state.targetScanCooldown = 0.f;
        }

        const Vec3 pos = transform.GetPosition();
        EntityID target = NULL_ENTITY;
        if (state.attackTargetId != NULL_ENTITY)
        {
            Vec3 targetPos{};
            f32_t targetDistSq = 0.f;
            if (TryResolveServerMinionTargetCandidate(
                m_world,
                entity,
                minion.team,
                state.lane,
                pos,
                state.sightRange,
                state.attackTargetId,
                targetPos,
                targetDistSq))
            {
                target = state.attackTargetId;
            }
            else
            {
                state.attackTargetId = NULL_ENTITY;
            }
        }

        if (target == NULL_ENTITY && state.targetScanCooldown <= 0.f)
        {
            target = FindClosestEnemyCombatTarget(
                m_world, entity, minion.team, state.lane, pos, state.sightRange);

            const f32_t scanInterval = state.targetScanInterval > 0.f
                ? state.targetScanInterval
                : ServerMinionTuning::kTargetScanIntervalSec;
            state.targetScanCooldown = scanInterval;
        }

        bool_t bMoved = false;
        if (target != NULL_ENTITY && m_world.HasComponent<TransformComponent>(target))
        {
            const Vec3 targetPos = m_world.GetComponent<TransformComponent>(target).GetPosition();
            const f32_t distSq = WintersMath::DistanceSqXZ(pos, targetPos);
            const f32_t effectiveAttackRange =
                ResolveServerMinionAttackRange(m_world, entity, target, state);
            const f32_t rangeSq = effectiveAttackRange * effectiveAttackRange;
            state.attackTargetId = target;

            if (distSq <= rangeSq)
            {
                state.current = MinionStateComponent::Attack;
                FaceServerMinionTowardTarget(transform, pos, targetPos);
                if (state.attackCooldown <= 0.f)
                {
                    eTeam sourceTeam = minion.team;
                    (void)TryResolveCombatTeam(m_world, entity, sourceTeam);

                    if (IsServerRangedMinion(state))
                    {
                        const Vec3 projectileDir = NormalizeXZOrForward(
                            Vec3{ targetPos.x - pos.x, 0.f, targetPos.z - pos.z },
                            sourceTeam);
                        const Vec3 projectileStart{
                            pos.x + projectileDir.x * kServerMinionRangedProjectileStartForwardOffset,
                            pos.y + kServerMinionRangedProjectileStartHeight,
                            pos.z + projectileDir.z * kServerMinionRangedProjectileStartForwardOffset
                        };

                        SkillProjectileComponent projectile{};
                        projectile.sourceEntity = entity;
                        projectile.targetEntity = target;
                        projectile.sourceTeam = sourceTeam;
                        projectile.kind = ResolveServerMinionRangedProjectileKind(sourceTeam);
                        projectile.skillId = static_cast<u16_t>(projectile.kind);
                        projectile.currentPos = projectileStart;
                        projectile.direction = projectileDir;
                        projectile.speed = kServerMinionRangedProjectileSpeed;
                        projectile.maxDistance =
                            effectiveAttackRange + kServerMinionRangedProjectileMaxDistancePadding;
                        projectile.hitRadius = kServerMinionRangedProjectileHitRadius;
                        projectile.damage = state.attackDamage;

                        const EntityID projectileEntity = m_world.CreateEntity();
                        m_world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

                        TransformComponent projectileTransform{};
                        projectileTransform.SetPosition(projectileStart);
                        m_world.AddComponent<TransformComponent>(
                            projectileEntity,
                            projectileTransform);
                    }
                    else
                    {
                        DamageRequest request{};
                        request.source = entity;
                        request.target = target;
                        request.sourceTeam = sourceTeam;
                        request.type = eDamageType::Physical;
                        request.flatAmount = state.attackDamage;
                        request.flags = DamageFlag_OnHit;
                        EnqueueDamageRequest(m_world, request);
                    }

                    state.attackCooldown = state.attackCooldownMax;
                    state.bHitFired = true;
                    StartReplicatedAnimation(m_world, entity, eNetAnimId::BasicAttack, tc);

                    static u32_t s_minionAttackLogCount = 0;
                    if (s_minionAttackLogCount < 128u)
                    {
                        const char* pTargetKind = m_world.HasComponent<StructureComponent>(target)
                            ? "structure"
                            : (m_world.HasComponent<MinionComponent>(target) ? "minion" : "champion");
                        char msg[288]{};
                        sprintf_s(msg,
                            "[MinionAI] attack tick=%llu entity=%u team=%u lane=%u target=%u targetKind=%s pos=(%.2f,%.2f,%.2f) targetPos=(%.2f,%.2f,%.2f)\n",
                            static_cast<unsigned long long>(tc.tickIndex),
                            static_cast<u32_t>(entity),
                            static_cast<u32_t>(minion.team),
                            static_cast<u32_t>(state.lane),
                            static_cast<u32_t>(target),
                            pTargetKind,
                            pos.x,
                            pos.y,
                            pos.z,
                            targetPos.x,
                            targetPos.y,
                            targetPos.z);
                        OutputServerAITrace(msg);
                        ++s_minionAttackLogCount;
                    }
                }
            }
            else
            {
                (void)TryMoveServerMinionToward(
                    entity,
                    state,
                    transform,
                    targetPos,
                    effectiveAttackRange,
                    tc,
                    PathBuildBudget,
                    bMoved,
                    MinionStateComponent::Chase);
            }
        }
        else
        {
            Vec3 laneTarget{};
            bool_t bHasLaneTarget = false;
            const u8_t waypointLane = ResolveServerWaypointLane(minion.team, state.lane);
            const u32_t waypointCount = GetServerMinionWaypointCount(minion.team, waypointLane);
            if (waypointCount > 0u && state.currentWaypoint < waypointCount)
            {
                laneTarget = GetServerMinionWaypoint(minion.team, waypointLane, state.currentWaypoint);
                const f32_t arriveSq = 0.8f * 0.8f;
                if (WintersMath::DistanceSqXZ(pos, laneTarget) <= arriveSq)
                {
                    ++state.currentWaypoint;
                    if (state.currentWaypoint < waypointCount)
                    {
                        laneTarget = GetServerMinionWaypoint(minion.team, waypointLane, state.currentWaypoint);
                        bHasLaneTarget = true;
                    }
                }
                else
                {
                    bHasLaneTarget = true;
                }
            }

            state.attackTargetId = NULL_ENTITY;
            if (bHasLaneTarget)
            {
                if (!TryMoveServerMinionByFlowFields(entity, state, transform, laneTarget, tc, bMoved))
                {
                    (void)TryMoveServerMinionToward(
                        entity,
                        state,
                        transform,
                        laneTarget,
                        0.8f,
                        tc,
                        PathBuildBudget,
                        bMoved,
                        MinionStateComponent::LaneMove);
                }
            }
            else
            {
                state.current = MinionStateComponent::Idle;
            }
        }

        auto& anim = m_world.HasComponent<NetAnimationComponent>(entity)
            ? m_world.GetComponent<NetAnimationComponent>(entity)
            : m_world.AddComponent<NetAnimationComponent>(entity, NetAnimationComponent{});
        if (bMoved)
        {
            anim.animId = static_cast<u16_t>(eNetAnimId::Run);
            anim.animPhaseFrame = static_cast<u16_t>(tc.tickIndex & 0xffffu);
        }
        else if (state.current != MinionStateComponent::Attack)
        {
            anim.animId = static_cast<u16_t>(eNetAnimId::Idle);
        }
    }
}

void CGameRoom::Phase_ServerMinionDepenetration(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    const auto minions =
        DeterministicEntityIterator<MinionStateComponent>::CollectSorted(m_world);

    for (EntityID entity : minions)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<MinionStateComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        MinionStateComponent& state = m_world.GetComponent<MinionStateComponent>(entity);
        if (state.current == MinionStateComponent::Dead)
            continue;

        TransformComponent& transform = m_world.GetComponent<TransformComponent>(entity);
        const Vec3 vPos = transform.GetPosition();
        const f32_t fStep = (std::max)(0.08f, state.moveSpeed * tc.fDt);

        Vec3 vResolved{};
        if (!TryResolveMinionDepenetrationStep(entity, vPos, fStep, tc, vResolved))
            continue;

        const Vec3 vActualMove{ vResolved.x - vPos.x, 0.f, vResolved.z - vPos.z };
        transform.SetPosition(vResolved);
        FaceServerMinionTowardDirection(transform, vActualMove);

        if (state.BlockedMoveFrames > 0u)
            state.BlockedMoveFrames = 0u;
    }
}

void CGameRoom::Phase_ServerTurretAI(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    if (m_pSpatialSystem)
        m_pSpatialSystem->Execute(m_world, tc.fDt);
    if (m_pTurretAI)
        m_pTurretAI->Execute(m_world, tc.fDt);
}

void CGameRoom::Phase_ServerProjectiles(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    const auto projectiles =
        DeterministicEntityIterator<TurretProjectileComponent>::CollectSorted(m_world);

    for (EntityID entity : projectiles)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<TurretProjectileComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& projectile = m_world.GetComponent<TurretProjectileComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);
        const Vec3 pos = projectile.currentPos;
        const NetEntityId currentProjectileNet = m_entityMap.ToNet(entity);
        const bool_t bTargetAlive =
            projectile.targetEntity != NULL_ENTITY &&
            m_world.IsAlive(projectile.targetEntity) &&
            m_world.HasComponent<TransformComponent>(projectile.targetEntity) &&
            IsAliveHealth(m_world, projectile.targetEntity) &&
            GameplayStateQuery::CanReceiveProjectileHit(
                m_world,
                projectile.sourceEntity,
                projectile.targetEntity);

        if (!bTargetAlive)
        {
            if (currentProjectileNet != NULL_NET_ENTITY)
            {
                ReplicatedEventComponent hit{};
                hit.kind = eReplicatedEventKind::ProjectileHit;
                hit.sourceEntity = projectile.sourceEntity;
                hit.targetEntity = NULL_ENTITY;
                hit.projectileEntity = entity;
                hit.projectileKind = kTurretProjectileKind;
                hit.position = pos;
                hit.bDestroyed = true;
                hit.startTick = tc.tickIndex;
                EnqueueReplicatedEvent(m_world, hit);
            }

            m_world.DestroyEntity(entity);
            continue;
        }

        if (currentProjectileNet == NULL_NET_ENTITY)
        {
            const NetEntityId projectileNet = m_entityMap.IssueNew(entity);
            NetEntityIdComponent net{};
            net.netId = projectileNet;
            if (!m_world.HasComponent<NetEntityIdComponent>(entity))
                m_world.AddComponent<NetEntityIdComponent>(entity, net);

            Vec3 dir{ 0.f, 0.f, 1.f };
            const Vec3 targetPos =
                m_world.GetComponent<TransformComponent>(projectile.targetEntity).GetPosition();
            dir = NormalizeXZOrForward(
                Vec3{ targetPos.x - pos.x, 0.f, targetPos.z - pos.z },
                eTeam::Neutral);

            ReplicatedEventComponent spawn{};
            spawn.kind = eReplicatedEventKind::ProjectileSpawn;
            spawn.sourceEntity = projectile.sourceEntity;
            spawn.targetEntity = projectile.targetEntity;
            spawn.projectileEntity = entity;
            spawn.projectileKind = kTurretProjectileKind;
            spawn.position = pos;
            spawn.direction = dir;
            spawn.speed = projectile.speed;
            spawn.maxDistance = 48.f;
            spawn.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, spawn);

            static u32_t s_turretProjectileLogCount = 0;
            if (s_turretProjectileLogCount < 64u)
            {
                char msg[192]{};
                sprintf_s(msg,
                    "[TurretAI] projectile tick=%llu source=%u target=%u projectile=%u pos=(%.2f,%.2f,%.2f)\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(projectile.sourceEntity),
                    static_cast<u32_t>(projectile.targetEntity),
                    static_cast<u32_t>(entity),
                    pos.x,
                    pos.y,
                    pos.z);
                OutputServerAITrace(msg);
                ++s_turretProjectileLogCount;
            }
        }

        const Vec3 targetPos = m_world.GetComponent<TransformComponent>(
            projectile.targetEntity).GetPosition();
        const Vec3 targetAim{ targetPos.x, targetPos.y + 1.2f, targetPos.z };
        const Vec3 delta{
            targetAim.x - pos.x,
            targetAim.y - pos.y,
            targetAim.z - pos.z
        };
        const f32_t distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        const f32_t hitRadiusSq = projectile.hitRadius * projectile.hitRadius;

        if (distSq <= hitRadiusSq)
        {
            eTeam sourceTeam = eTeam::Neutral;
            (void)TryResolveCombatTeam(m_world, projectile.sourceEntity, sourceTeam);

            DamageRequest request{};
            request.source = projectile.sourceEntity;
            request.target = projectile.targetEntity;
            request.sourceTeam = sourceTeam;
            request.type = eDamageType::Physical;
            request.flatAmount = projectile.damage;
            request.skillId = kTurretProjectileKind;
            EnqueueDamageRequest(m_world, request);

            ReplicatedEventComponent hit{};
            hit.kind = eReplicatedEventKind::ProjectileHit;
            hit.sourceEntity = projectile.sourceEntity;
            hit.targetEntity = projectile.targetEntity;
            hit.projectileEntity = entity;
            hit.projectileKind = kTurretProjectileKind;
            hit.position = targetAim;
            hit.bDestroyed = true;
            hit.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, hit);
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t dist = std::sqrt(distSq);
        if (dist <= std::numeric_limits<f32_t>::epsilon())
        {
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t step = projectile.speed * tc.fDt;
        const f32_t t = (step >= dist) ? 1.f : (step / dist);
        Vec3 next{
            pos.x + delta.x * t,
            pos.y + delta.y * t,
            pos.z + delta.z * t
        };
        projectile.currentPos = next;
        transform.SetPosition(next);
    }

    const auto skillProjectiles =
        DeterministicEntityIterator<SkillProjectileComponent>::CollectSorted(m_world);

    for (EntityID entity : skillProjectiles)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<SkillProjectileComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& projectile = m_world.GetComponent<SkillProjectileComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);

        if (!projectile.bSpawned)
        {
            const NetEntityId projectileNet = m_entityMap.IssueNew(entity);
            NetEntityIdComponent net{};
            net.netId = projectileNet;
            if (!m_world.HasComponent<NetEntityIdComponent>(entity))
                m_world.AddComponent<NetEntityIdComponent>(entity, net);

            projectile.bSpawned = true;

            ReplicatedEventComponent spawn{};
            spawn.kind = eReplicatedEventKind::ProjectileSpawn;
            spawn.sourceEntity = projectile.sourceEntity;
            spawn.targetEntity = projectile.targetEntity;
            spawn.projectileEntity = entity;
            spawn.projectileKind = static_cast<u16_t>(projectile.kind);
            spawn.position = projectile.currentPos;
            spawn.direction = projectile.direction;
            spawn.speed = projectile.speed;
            spawn.maxDistance = projectile.maxDistance;
            spawn.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, spawn);
            if (!IsServerMinionRangedProjectileKind(projectile.kind))
                LogSkillProjectileEvent(
                    "spawn",
                    entity,
                    projectile,
                    projectile.targetEntity,
                    projectile.currentPos);
            continue;
        }

        if (!IsAliveHealth(m_world, projectile.sourceEntity) ||
            projectile.speed <= 0.f ||
            projectile.maxDistance <= 0.f)
        {
            ReplicatedEventComponent hit{};
            hit.kind = eReplicatedEventKind::ProjectileHit;
            hit.sourceEntity = projectile.sourceEntity;
            hit.projectileEntity = entity;
            hit.projectileKind = static_cast<u16_t>(projectile.kind);
            hit.position = projectile.currentPos;
            hit.bDestroyed = true;
            hit.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, hit);
            m_world.DestroyEntity(entity);
            continue;
        }

        if (projectile.targetEntity != NULL_ENTITY)
        {
            const bool_t bTargetAlive =
                m_world.IsAlive(projectile.targetEntity) &&
                m_world.HasComponent<TransformComponent>(projectile.targetEntity) &&
                IsAliveHealth(m_world, projectile.targetEntity) &&
                GameplayStateQuery::CanReceiveProjectileHit(
                    m_world,
                    projectile.sourceEntity,
                    projectile.targetEntity);

            if (!bTargetAlive)
            {
                ReplicatedEventComponent hit{};
                hit.kind = eReplicatedEventKind::ProjectileHit;
                hit.sourceEntity = projectile.sourceEntity;
                hit.targetEntity = NULL_ENTITY;
                hit.projectileEntity = entity;
                hit.projectileKind = static_cast<u16_t>(projectile.kind);
                hit.position = projectile.currentPos;
                hit.bDestroyed = true;
                hit.startTick = tc.tickIndex;
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
                continue;
            }

            const Vec3 targetPos =
                m_world.GetComponent<TransformComponent>(projectile.targetEntity).GetPosition();
            const Vec3 targetAim{
                targetPos.x,
                targetPos.y + kServerMinionRangedProjectileTargetHeight,
                targetPos.z
            };
            const Vec3 delta{
                targetAim.x - projectile.currentPos.x,
                targetAim.y - projectile.currentPos.y,
                targetAim.z - projectile.currentPos.z
            };
            const f32_t distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            const f32_t hitRadiusSq = projectile.hitRadius * projectile.hitRadius;

            if (distSq <= hitRadiusSq)
            {
                DamageRequest request{};
                request.source = projectile.sourceEntity;
                request.target = projectile.targetEntity;
                request.sourceTeam = projectile.sourceTeam;
                request.type = eDamageType::Physical;
                request.flatAmount = projectile.damage;
                request.skillId = projectile.skillId;
                request.rank = projectile.rank;
                request.flags = DamageFlag_OnHit;
                EnqueueDamageRequest(m_world, request);

                ReplicatedEventComponent hit{};
                hit.kind = eReplicatedEventKind::ProjectileHit;
                hit.sourceEntity = projectile.sourceEntity;
                hit.targetEntity = projectile.targetEntity;
                hit.projectileEntity = entity;
                hit.projectileKind = static_cast<u16_t>(projectile.kind);
                hit.position = targetAim;
                hit.bDestroyed = true;
                hit.startTick = tc.tickIndex;
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
                continue;
            }

            const f32_t dist = std::sqrt(distSq);
            if (dist <= std::numeric_limits<f32_t>::epsilon())
            {
                m_world.DestroyEntity(entity);
                continue;
            }

            const f32_t remaining = projectile.maxDistance - projectile.traveledDistance;
            if (remaining <= 0.f)
            {
                ReplicatedEventComponent hit{};
                hit.kind = eReplicatedEventKind::ProjectileHit;
                hit.sourceEntity = projectile.sourceEntity;
                hit.targetEntity = NULL_ENTITY;
                hit.projectileEntity = entity;
                hit.projectileKind = static_cast<u16_t>(projectile.kind);
                hit.position = projectile.currentPos;
                hit.bDestroyed = true;
                hit.startTick = tc.tickIndex;
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
                continue;
            }

            const f32_t step = std::min(projectile.speed * tc.fDt, remaining);
            const f32_t actualStep = (step >= dist) ? dist : step;
            const f32_t t = actualStep / dist;
            const Vec3 next{
                projectile.currentPos.x + delta.x * t,
                projectile.currentPos.y + delta.y * t,
                projectile.currentPos.z + delta.z * t
            };
            projectile.currentPos = next;
            projectile.traveledDistance += actualStep;
            transform.SetPosition(next);

            if (projectile.traveledDistance >= projectile.maxDistance - 0.001f)
            {
                ReplicatedEventComponent hit{};
                hit.kind = eReplicatedEventKind::ProjectileHit;
                hit.sourceEntity = projectile.sourceEntity;
                hit.targetEntity = NULL_ENTITY;
                hit.projectileEntity = entity;
                hit.projectileKind = static_cast<u16_t>(projectile.kind);
                hit.position = next;
                hit.bDestroyed = true;
                hit.startTick = tc.tickIndex;
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
            }
            continue;
        }

        const Vec3 start = projectile.currentPos;
        const f32_t remaining = projectile.maxDistance - projectile.traveledDistance;
        const f32_t step = std::min(projectile.speed * tc.fDt, remaining);
        const Vec3 end{
            start.x + projectile.direction.x * step,
            start.y + projectile.direction.y * step,
            start.z + projectile.direction.z * step
        };

        Vec3 hitPos = end;
        const EntityID target = FindSkillProjectileHitTarget(
            m_world,
            projectile,
            start,
            end,
            hitPos);

        if (target != NULL_ENTITY)
        {
            if (projectile.kind == eProjectileKind::Tornado)
                YasuoGameSim::ApplyTornadoAirborne(m_world, tc, projectile.sourceEntity, target);
            if (projectile.kind == eProjectileKind::LeeSinQ)
                LeeSinGameSim::ApplySonicWaveMark(m_world, projectile.sourceEntity, target);
            if (projectile.kind == eProjectileKind::SylasChain)
                SylasGameSim::ApplyChainHit(m_world, tc, projectile.sourceEntity, target);
            if (projectile.bApplyOnHitStatus)
                GameplayStatus::ApplyStatusEffect(m_world, target, projectile.onHitStatus, tc);

            DamageRequest request{};
            request.source = projectile.sourceEntity;
            request.target = target;
            request.sourceTeam = projectile.sourceTeam;
            request.type = projectile.kind == eProjectileKind::SylasChain
                ? eDamageType::Magic
                : eDamageType::Physical;
            request.flatAmount = projectile.damage;
            request.skillId = projectile.skillId;
            request.rank = projectile.rank;
            request.flags = DamageFlag_OnHit;
            EnqueueDamageRequest(m_world, request);

            ReplicatedEventComponent hit{};
            hit.kind = eReplicatedEventKind::ProjectileHit;
            hit.sourceEntity = projectile.sourceEntity;
            hit.targetEntity = target;
            hit.projectileEntity = entity;
            hit.projectileKind = static_cast<u16_t>(projectile.kind);
            hit.position = hitPos;
            hit.bDestroyed = true;
            hit.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, hit);
            LogSkillProjectileEvent("hit", entity, projectile, target, hitPos);
            m_world.DestroyEntity(entity);
            continue;
        }

        projectile.currentPos = end;
        projectile.traveledDistance += step;
        transform.SetPosition(end);

        if (projectile.traveledDistance >= projectile.maxDistance - 0.001f)
        {
            ReplicatedEventComponent hit{};
            hit.kind = eReplicatedEventKind::ProjectileHit;
            hit.sourceEntity = projectile.sourceEntity;
            hit.projectileEntity = entity;
            hit.projectileKind = static_cast<u16_t>(projectile.kind);
            hit.position = end;
            hit.bDestroyed = true;
            hit.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, hit);
            LogSkillProjectileEvent("expire", entity, projectile, NULL_ENTITY, end);
            m_world.DestroyEntity(entity);
        }
    }
}

Vec3 CGameRoom::GetSpawnPositionForLobbySlot(const LobbySlotState& slot) const
{
    if (IsRedSylasSmokeDummySlot(slot))
        return GetRedSylasSmokeDummyPosition();

    Winters::Map::StageData stage{};
    std::wstring stagePath;
    if (LoadServerStageData(stage, stagePath))
    {
        Vec3 stageSpawn{};
        if (TryResolveStageFountainSpawn(
            stage,
            slot.slotId,
            static_cast<eTeam>(slot.team),
            stageSpawn))
        {
            Vec3 walkableStageSpawn = stageSpawn;
            if (TryResolveServerWalkablePosition(
                stageSpawn,
                kStageChampionSpawnWalkableSearchRadius,
                walkableStageSpawn))
            {
                return walkableStageSpawn;
            }
        }
    }

    const Vec3 fallbackSpawn = GetGameSimRosterSpawnPosition(slot.slotId, slot.team, slot.bBot);
    Vec3 walkableFallbackSpawn = fallbackSpawn;
    if (TryResolveServerWalkablePosition(
        fallbackSpawn,
        kStageChampionSpawnWalkableSearchRadius,
        walkableFallbackSpawn))
    {
        return walkableFallbackSpawn;
    }

    return fallbackSpawn;
}

u64_t CGameRoom::ResolveServerGameTimeMs(u64_t iServerTick)
{
    return (iServerTick * 1000ull) / DeterministicTime::kTicksPerSecond;
}

bool CGameRoom::DebugSetHealthByNetId(NetEntityId netId, f32_t value)
{
    std::lock_guard stateLock(m_stateMutex);

    const EntityID entity = m_entityMap.FromNet(netId);
    if (entity == NULL_ENTITY || !m_world.HasComponent<HealthComponent>(entity))
        return false;

    auto& health = m_world.GetComponent<HealthComponent>(entity);
    health.fCurrent = value;
    health.bIsDead = (health.fCurrent <= 0.f);

    if (m_world.HasComponent<ChampionComponent>(entity))
    {
        auto& champion = m_world.GetComponent<ChampionComponent>(entity);
        champion.hp = health.fCurrent;
    }

    char msg[160]{};
    sprintf_s(msg,
        "[GameRoom] Debug hp netId=%u entity=%u value=%.2f\n",
        netId,
        static_cast<u32_t>(entity),
        value);
    OutputServerAITrace(msg);
    return true;
}

bool_t CGameRoom::IsWalkableXZ(const Vec3& pos) const
{
    const Engine::CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid)
        return true;

    const Engine::CNavGrid::Cell cell = pGrid->WorldToCell(pos);
    return pGrid->IsWalkable(cell.x, cell.y);
}

bool_t CGameRoom::SegmentWalkableXZ(const Vec3& from, const Vec3& to, f32_t radiusWorld) const
{
    const Engine::CNavGrid* pGrid = m_pNavGrid.get();
    if (!pGrid)
        return true;

    return pGrid->SegmentWalkable(from, to, (std::max)(0.f, radiusWorld));
}

bool_t CGameRoom::TryClampMoveSegmentXZ(
    const Vec3& vFrom,
    const Vec3& vDesired,
    f32_t fRadiusWorld,
    Vec3& vOutPosition) const
{
    const Engine::CNavGrid* pGrid = m_pNavGrid.get();
    vOutPosition = vDesired;
    if (!pGrid)
        return true;

    const f32_t fRadius = (std::max)(0.f, fRadiusWorld);
    if (pGrid->SegmentWalkable(vFrom, vDesired, fRadius))
        return true;

    const Engine::CNavGrid::Cell fromCell = pGrid->WorldToCell(vFrom);
    if (!pGrid->IsWalkable(fromCell.x, fromCell.y))
    {
        Engine::CNavGrid::Cell nearest{};
        if (!pGrid->TryFindNearestWalkableCell(fromCell, 16, nearest))
        {
            vOutPosition = vFrom;
            return false;
        }
        vOutPosition = pGrid->CellToWorld(nearest.x, nearest.y);
        if (!TrySampleHeight(vOutPosition.x, vOutPosition.z, vOutPosition.y))
            vOutPosition.y = vFrom.y;
        return true;
    }

    f32_t fLow = 0.f;
    f32_t fHigh = 1.f;
    for (u32_t i = 0; i < 12u; ++i)
    {
        const f32_t fMid = (fLow + fHigh) * 0.5f;
        const Vec3 vProbe{
            vFrom.x + (vDesired.x - vFrom.x) * fMid,
            vFrom.y + (vDesired.y - vFrom.y) * fMid,
            vFrom.z + (vDesired.z - vFrom.z) * fMid
        };

        if (pGrid->SegmentWalkable(vFrom, vProbe, fRadius))
            fLow = fMid;
        else
            fHigh = fMid;
    }

    if (fLow <= 0.001f)
    {
        vOutPosition = vFrom;
        return false;
    }

    vOutPosition = Vec3{
        vFrom.x + (vDesired.x - vFrom.x) * fLow,
        vFrom.y + (vDesired.y - vFrom.y) * fLow,
        vFrom.z + (vDesired.z - vFrom.z) * fLow
    };
    return true;
}

bool_t CGameRoom::TryResolveMoveTarget(const Vec3& from, const Vec3& rawTarget, Vec3& outTarget) const
{
    auto ApplySafeMoveHeight = [&](Vec3& ioPos, f32_t fallbackY)
        {
            f32_t sampledY = fallbackY;
            if (!TrySampleHeight(ioPos.x, ioPos.z, sampledY))
            {
                ioPos.y = fallbackY;
                return;
            }

            const f32_t surfaceDeltaY = sampledY - from.y;
            if (std::fabs(surfaceDeltaY) <= kMoveTargetMaxSurfaceDeltaY)
            {
                ioPos.y = sampledY;
                return;
            }

            static u32_t s_badSurfaceLogCount = 0;
            if (s_badSurfaceLogCount < 64u)
            {
                char msg[224]{};
                sprintf_s(
                    msg,
                    "[ServerNav] resolve reject-surface-y fromY=%.3f sampledY=%.3f delta=%.3f xz=(%.3f,%.3f)\n",
                    from.y,
                    sampledY,
                    surfaceDeltaY,
                    ioPos.x,
                    ioPos.z);
                OutputServerAITrace(msg);
                ++s_badSurfaceLogCount;
            }
            ioPos.y = fallbackY;
        };

    const Engine::CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid)
    {
        outTarget = rawTarget;
        ApplySafeMoveHeight(outTarget, from.y);
        return true;
    }

    Engine::CNavGrid::Cell start = pGrid->WorldToCell(from);
    if (!pGrid->IsWalkable(start.x, start.y))
    {
        Engine::CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 16, nearestStart))
            return false;
        static u32_t s_startBlockedLogCount = 0;
        if (s_startBlockedLogCount < 64u)
        {
            char msg[256]{};
            sprintf_s(msg,
                "[ServerNav] start blocked from=(%.2f,%.2f) cell=(%d,%d) nearest=(%d,%d)\n",
                from.x,
                from.z,
                start.x,
                start.y,
                nearestStart.x,
                nearestStart.y);
            OutputServerAITrace(msg);
            ++s_startBlockedLogCount;
        }
        start = nearestStart;
    }

    const Engine::CNavGrid::Cell rawGoal = pGrid->WorldToCell(rawTarget);
    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
    {
        static u32_t s_outOfBoundsLogCount = 0;
        if (s_outOfBoundsLogCount < 32u)
        {
            char msg[256]{};
            sprintf_s(msg,
                "[ServerNav] move reject reason=out-of-nav-bounds from=(%.2f,%.2f) target=(%.2f,%.2f) cell=(%d,%d) origin=(%.2f,%.2f)\n",
                from.x,
                from.z,
                rawTarget.x,
                rawTarget.z,
                rawGoal.x,
                rawGoal.y,
                pGrid->Get_OriginX(),
                pGrid->Get_OriginZ());
            OutputServerAITrace(msg);
            ++s_outOfBoundsLogCount;
        }
        return false;
    }

    Engine::CNavGrid::Cell resolved{};
    std::vector<Engine::CNavGrid::Cell> path{};
    if (!Engine::CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolved,
        &path))
    {
        return false;
    }

    outTarget = pGrid->CellToWorld(resolved.x, resolved.y);
    ApplySafeMoveHeight(outTarget, from.y);

    if (resolved.x != rawGoal.x || resolved.y != rawGoal.y)
    {
        static u32_t s_correctionLogCount = 0;
        if (s_correctionLogCount < 64u)
        {
            char msg[224]{};
            sprintf_s(msg,
                "[ServerNav] bfs-corrected move goal raw=(%d,%d) resolved=(%d,%d) path=%zu\n",
                rawGoal.x,
                rawGoal.y,
                resolved.x,
                resolved.y,
                path.size());
            OutputServerAITrace(msg);
            ++s_correctionLogCount;
        }
    }

    return true;
}

bool_t CGameRoom::TryBuildMovePath(
    const Vec3& from,
    const Vec3& rawTarget,
    Vec3* pOutWaypoints,
    u16_t maxWaypoints,
    u16_t& outWaypointCount,
    Vec3& outTarget) const
{
    outWaypointCount = 0;
    outTarget = rawTarget;
    if (!pOutWaypoints || maxWaypoints == 0)
        return false;

    auto ApplySafeMoveHeight = [&](Vec3& ioPos, f32_t fallbackY)
        {
            f32_t sampledY = fallbackY;
            if (!TrySampleHeight(ioPos.x, ioPos.z, sampledY))
            {
                ioPos.y = fallbackY;
                return;
            }

            const f32_t surfaceDeltaY = sampledY - from.y;
            if (std::fabs(surfaceDeltaY) <= kMoveTargetMaxSurfaceDeltaY)
            {
                ioPos.y = sampledY;
                return;
            }

            static u32_t s_badSurfaceLogCount = 0;
            if (s_badSurfaceLogCount < 64u)
            {
                char msg[224]{};
                sprintf_s(
                    msg,
                    "[ServerNav] reject-surface-y fromY=%.3f sampledY=%.3f delta=%.3f xz=(%.3f,%.3f)\n",
                    from.y,
                    sampledY,
                    surfaceDeltaY,
                    ioPos.x,
                    ioPos.z);
                OutputServerAITrace(msg);
                ++s_badSurfaceLogCount;
            }
            ioPos.y = fallbackY;
        };

    const Engine::CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid)
    {
        ApplySafeMoveHeight(outTarget, from.y);
        pOutWaypoints[outWaypointCount++] = outTarget;
        static u32_t s_yawTraceNoGridCount = 0;
        if (s_yawTraceNoGridCount < 64u)
        {
            char msg[384]{};
            sprintf_s(
                msg,
                "[YawTrace][ServerPath] mode=no-grid from=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) pathCount=%u\n",
                from.x,
                from.y,
                from.z,
                rawTarget.x,
                rawTarget.y,
                rawTarget.z,
                outTarget.x,
                outTarget.y,
                outTarget.z,
                static_cast<u32_t>(outWaypointCount));
            OutputServerAITrace(msg);
            ++s_yawTraceNoGridCount;
        }
        return true;
    }

    Engine::CNavGrid::Cell start = pGrid->WorldToCell(from);
    if (!pGrid->IsWalkable(start.x, start.y))
    {
        Engine::CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 16, nearestStart))
            return false;
        start = nearestStart;
    }

    const Engine::CNavGrid::Cell rawGoal = pGrid->WorldToCell(rawTarget);
    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
        return false;

    if (pGrid->IsWalkable(rawGoal.x, rawGoal.y) &&
        pGrid->SegmentWalkable(from, rawTarget, 0.f))
    {
        outTarget = rawTarget;
        ApplySafeMoveHeight(outTarget, from.y);
        pOutWaypoints[outWaypointCount++] = outTarget;
        static u32_t s_yawTraceDirectCount = 0;
        if (s_yawTraceDirectCount < 512u)
        {
            char msg[512]{};
            sprintf_s(
                msg,
                "[YawTrace][ServerPath] mode=direct from=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) path0=(%.3f,%.3f,%.3f) start=(%d,%d) goal=(%d,%d) pathCount=%u\n",
                from.x,
                from.y,
                from.z,
                rawTarget.x,
                rawTarget.y,
                rawTarget.z,
                outTarget.x,
                outTarget.y,
                outTarget.z,
                pOutWaypoints[0].x,
                pOutWaypoints[0].y,
                pOutWaypoints[0].z,
                start.x,
                start.y,
                rawGoal.x,
                rawGoal.y,
                static_cast<u32_t>(outWaypointCount));
            OutputServerAITrace(msg);
            ++s_yawTraceDirectCount;
        }
        return true;
    }

    Engine::CNavGrid::Cell resolved{};
    std::vector<Engine::CNavGrid::Cell> path{};
    if (!Engine::CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolved,
        &path))
    {
        return false;
    }

    outTarget = pGrid->CellToWorld(resolved.x, resolved.y);
    ApplySafeMoveHeight(outTarget, from.y);

    Engine::CNavGrid::Cell lastAppended{ -1, -1 };
    auto AppendCell = [&](Engine::CNavGrid::Cell cell) -> bool_t
        {
            if (cell.x == lastAppended.x && cell.y == lastAppended.y)
                return true;
            if (outWaypointCount >= maxWaypoints)
                return false;

            Vec3 waypoint = pGrid->CellToWorld(cell.x, cell.y);
            ApplySafeMoveHeight(waypoint, outTarget.y);

            pOutWaypoints[outWaypointCount++] = waypoint;
            lastAppended = cell;
            return true;
        };

    const std::vector<Engine::CNavGrid::Cell> smoothedPath = SmoothServerPathCells(*pGrid, path);
    if (smoothedPath.size() <= 1)
    {
        const bool_t bAppended = AppendCell(resolved);
        static u32_t s_yawTraceSinglePathCount = 0;
        if (bAppended && s_yawTraceSinglePathCount < 512u)
        {
            const Vec3 firstWaypoint = outWaypointCount > 0 ? pOutWaypoints[0] : Vec3{};
            char msg[640]{};
            sprintf_s(
                msg,
                "[YawTrace][ServerPath] mode=path-single from=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) path0=(%.3f,%.3f,%.3f) start=(%d,%d) goal=(%d,%d) corrected=(%d,%d) rawPath=%zu smoothedPath=%zu pathCount=%u\n",
                from.x,
                from.y,
                from.z,
                rawTarget.x,
                rawTarget.y,
                rawTarget.z,
                outTarget.x,
                outTarget.y,
                outTarget.z,
                firstWaypoint.x,
                firstWaypoint.y,
                firstWaypoint.z,
                start.x,
                start.y,
                rawGoal.x,
                rawGoal.y,
                resolved.x,
                resolved.y,
                path.size(),
                smoothedPath.size(),
                static_cast<u32_t>(outWaypointCount));
            OutputServerAITrace(msg);
            ++s_yawTraceSinglePathCount;
        }
        return bAppended;
    }

    for (size_t i = 1; i < smoothedPath.size(); ++i)
    {
        if (!AppendCell(smoothedPath[i]))
            return false;
    }

    static u32_t s_yawTracePathCount = 0;
    if (s_yawTracePathCount < 512u)
    {
        const Vec3 firstWaypoint = outWaypointCount > 0 ? pOutWaypoints[0] : Vec3{};
        char msg[640]{};
        sprintf_s(
            msg,
            "[YawTrace][ServerPath] mode=path from=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) path0=(%.3f,%.3f,%.3f) start=(%d,%d) goal=(%d,%d) corrected=(%d,%d) rawPath=%zu smoothedPath=%zu pathCount=%u\n",
            from.x,
            from.y,
            from.z,
            rawTarget.x,
            rawTarget.y,
            rawTarget.z,
            outTarget.x,
            outTarget.y,
            outTarget.z,
            firstWaypoint.x,
            firstWaypoint.y,
            firstWaypoint.z,
            start.x,
            start.y,
            rawGoal.x,
            rawGoal.y,
            resolved.x,
            resolved.y,
            path.size(),
            smoothedPath.size(),
            static_cast<u32_t>(outWaypointCount));
        OutputServerAITrace(msg);
        ++s_yawTracePathCount;
    }

    return outWaypointCount > 0;
}

bool_t CGameRoom::TrySampleHeight(f32_t x, f32_t z, f32_t& outY) const
{
    if (!m_pMapSurfaceSampler)
        return false;

    f32_t height = 0.f;
    if (!m_pMapSurfaceSampler->SampleHeight(x, z, height))
        return false;

    outY = height + 0.05f;
    return true;
}

void CGameRoom::SpawnChampionsFromLobby()
{
    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        LobbySlotState& slot = m_lobbySlots[i];
        if (!slot.bHuman && !slot.bBot)
            continue;
        if (slot.netId != NULL_NET_ENTITY)
            continue;

        SpawnChampionForLobbySlot(slot);
    }
}

void CGameRoom::SpawnServerGameplayObjects()
{
    if (m_bGameplayObjectsSpawned)
        return;

    m_bGameplayObjectsSpawned = true;

    Winters::Map::StageData stage{};
    std::wstring stagePath;
    const bool_t bLoadedStage = LoadServerStageData(stage, stagePath);
    if (bLoadedStage)
    {
        CacheServerMinionWaypoints(stage);
        InitializeServerWalkableGrid(&stage, stagePath.c_str());

        u32_t spawnedStructures = 0;
        for (const auto& entry : stage.structures)
        {
            if (SpawnServerStructureFromStageEntry(entry) != NULL_ENTITY)
                ++spawnedStructures;
        }

        u32_t spawnedJungles = 0;
        for (const auto& entry : stage.jungles)
        {
            if (SpawnServerJungleFromStageEntry(entry) != NULL_ENTITY)
                ++spawnedJungles;
        }

        wchar_t stageMsg[512]{};
        swprintf_s(stageMsg,
            L"[GameRoom] Stage1 loaded for server sim: %ls structures=%u jungles=%u waypoints=%zu\n",
            stagePath.c_str(),
            spawnedStructures,
            spawnedJungles,
            stage.minionWaypoints.size());
        OutputServerAITraceW(stageMsg);

        if (spawnedStructures == 0)
        {
            OutputServerAITrace("[GameRoom] Stage has no server structures; using fallback structures\n");
        }
        else
        {
            CarveServerStructuresOnNavGrid();
            SanitizeServerMoversOnNavGrid();
            SanitizeServerWaypointPatrolsOnNavGrid();
            SanitizeServerMinionWaypointsOnNavGrid();
            RebuildServerMinionFlowFields();
            RefreshChampionAIGoals();
            m_serverMinionWaves.ResetWaveSchedule();

            char msg[160]{};
            sprintf_s(msg,
                "[GameRoom] Server gameplay objects spawned. entities=%u\n",
                m_world.GetEntityCount());
            OutputServerAITrace(msg);
            return;
        }
    }
    else
    {
        OutputServerAITrace("[GameRoom] Stage1.dat not found for server sim; using fallback objects\n");
        InitializeServerWalkableGrid(nullptr, nullptr);
    }

    SpawnServerStructure(eTeam::Blue, kStructureKindTurret, 0, kLaneMid,
        Vec3{ 18.f, 1.f, 0.f }, Vec3{}, 3000.f, 1.f, true, false, false);
    SpawnServerStructure(eTeam::Blue, kStructureKindTurret, 1, kLaneMid,
        Vec3{ 25.f, 1.f, 0.f }, Vec3{}, 3000.f, 1.f, true, false, false);
    SpawnServerStructure(eTeam::Blue, kStructureKindNexus, 3, kLaneMid,
        Vec3{ 32.f, 1.f, 0.f }, Vec3{}, 5500.f, 1.f, false, true, false);

    SpawnServerStructure(eTeam::Red, kStructureKindTurret, 0, kLaneMid,
        Vec3{ -18.f, 1.f, 0.f }, Vec3{}, 3000.f, 1.f, true, false, false);
    SpawnServerStructure(eTeam::Red, kStructureKindTurret, 1, kLaneMid,
        Vec3{ -25.f, 1.f, 0.f }, Vec3{}, 3000.f, 1.f, true, false, false);
    SpawnServerStructure(eTeam::Red, kStructureKindNexus, 3, kLaneMid,
        Vec3{ -32.f, 1.f, 0.f }, Vec3{}, 5500.f, 1.f, false, true, false);

    CarveServerStructuresOnNavGrid();
    SanitizeServerMoversOnNavGrid();
    SanitizeServerWaypointPatrolsOnNavGrid();
    SanitizeServerMinionWaypointsOnNavGrid();
    RebuildServerMinionFlowFields();
    RefreshChampionAIGoals();
    m_serverMinionWaves.ResetWaveSchedule();

    char msg[160]{};
    sprintf_s(msg,
        "[GameRoom] Server gameplay objects spawned. entities=%u\n",
        m_world.GetEntityCount());
    OutputServerAITrace(msg);
}

EntityID CGameRoom::SpawnServerStructureFromStageEntry(
    const Winters::Map::StructureEntry& entry)
{
    if (entry.bVisible == 0u)
        return NULL_ENTITY;

    const u32_t kind = entry.subKind;
    const bool_t bTurret = kind == kStructureKindTurret;
    const bool_t bNexus = kind == kStructureKindNexus;
    const bool_t bInhibitor = kind == kStructureKindInhibitor;
    if (!bTurret && !bNexus && !bInhibitor)
        return NULL_ENTITY;

    const eTeam team = StageTeamToGameTeam(entry.team);
    const Vec3 pos{ entry.px, entry.py, entry.pz };
    const u8_t resolvedLane = ResolveServerStructureLane(team, kind, entry.tier, pos);
    if (entry.lane != resolvedLane)
    {
        char msg[256]{};
        sprintf_s(msg,
            "[GameRoom] structure lane remap team=%u kind=%u tier=%u stageLane=%u lane=%u pos=(%.2f,%.2f,%.2f)\n",
            static_cast<u32_t>(team),
            kind,
            entry.tier,
            entry.lane,
            static_cast<u32_t>(resolvedLane),
            pos.x,
            pos.y,
            pos.z);
        OutputServerAITrace(msg);
    }

    return SpawnServerStructure(
        team,
        kind,
        entry.tier,
        resolvedLane,
        pos,
        Vec3{ entry.rx, entry.ry, entry.rz },
        ResolveStageStructureMaxHp(kind),
        entry.scale,
        bTurret,
        bNexus,
        bInhibitor);
}

EntityID CGameRoom::SpawnServerJungleFromStageEntry(
    const Winters::Map::JungleEntry& entry)
{
    if (entry.bVisible == 0u)
        return NULL_ENTITY;

    const EntityID entity = m_world.CreateEntity();

    TransformComponent transform{};
    transform.SetPosition(Vec3{ entry.px, entry.py, entry.pz });
    transform.SetRotation(Vec3{ entry.rx, entry.ry, entry.rz });
    transform.SetScale(entry.scale > 0.f ? entry.scale : 1.f);
    m_world.AddComponent<TransformComponent>(entity, transform);

    const f32_t maxHp = ResolveStageJungleMaxHp(entry.subKind);

    JungleComponent jungle{};
    jungle.subKind = entry.subKind;
    jungle.campId = entry.campId;
    jungle.hp = maxHp;
    jungle.maxHp = maxHp;
    m_world.AddComponent<JungleComponent>(entity, jungle);
    m_world.AddComponent<JungleMonsterTag>(entity);

    HealthComponent health{};
    health.fCurrent = maxHp;
    health.fMaximum = maxHp;
    m_world.AddComponent<HealthComponent>(entity, health);

    const f32_t attackDamage = ResolveStageJungleAttackDamage(entry.subKind);
    const f32_t attackCooldown = ResolveStageJungleAttackCooldown(entry.subKind);
    const f32_t attackSpeed = 1.f / attackCooldown;

    StatComponent stat{};
    stat.championId = eChampion::NONE;
    stat.level = 1;
    stat.hpMax = maxHp;
    stat.baseAd = attackDamage;
    stat.ad = attackDamage;
    stat.baseArmor = 20.f;
    stat.armor = stat.baseArmor;
    stat.baseMr = 20.f;
    stat.mr = stat.baseMr;
    stat.baseAttackSpeed = attackSpeed;
    stat.attackSpeedRatio = attackSpeed;
    stat.attackSpeed = attackSpeed;
    stat.attackRange = ResolveStageJungleAttackRange(entry.subKind);
    stat.moveSpeed = ResolveStageJungleMoveSpeed(entry.subKind);
    stat.bDirty = false;
    m_world.AddComponent<StatComponent>(entity, stat);

    m_world.AddComponent<SkillStateComponent>(entity, SkillStateComponent{});
    m_world.AddComponent<JungleAIComponent>(entity, JungleAIComponent{});

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::JungleMob;
    spatial.team = TeamByte(eTeam::Neutral);
    spatial.radius = ResolveStageJungleRadius(entry.subKind);
    m_world.AddComponent<SpatialAgentComponent>(entity, spatial);

    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, 2.0f, spatial.radius };
    collider.vOffset = { 0.f, 1.0f, 0.f };
    collider.bIsTrigger = false;
    m_world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = 10.f;
    m_world.AddComponent<VisionSourceComponent>(entity, vision);
    m_world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    m_world.AddComponent<TargetableTag>(entity);

    NetAnimationComponent anim{};
    anim.animId = static_cast<u16_t>(eNetAnimId::Idle);
    m_world.AddComponent<NetAnimationComponent>(entity, anim);

    const NetEntityId netId = m_entityMap.IssueNew(entity);
    NetEntityIdComponent netEntity{};
    netEntity.netId = netId;
    m_world.AddComponent<NetEntityIdComponent>(entity, netEntity);

    return entity;
}

EntityID CGameRoom::SpawnServerStructure(eTeam team, u32_t kind, u32_t tier, u32_t lane,
    const Vec3& pos, const Vec3& rotation, f32_t maxHp, f32_t scale,
    bool_t bTurret, bool_t bNexus, bool_t bInhibitor)
{
    const EntityID entity = m_world.CreateEntity();

    TransformComponent transform{};
    transform.SetPosition(pos);
    transform.SetRotation(rotation);
    transform.SetScale(scale > 0.f ? scale : 1.f);
    m_world.AddComponent<TransformComponent>(entity, transform);

    StructureComponent structure{};
    structure.team = team;
    structure.kind = kind;
    structure.tier = tier;
    structure.lane = lane;
    structure.hp = maxHp;
    structure.maxHp = maxHp;
    m_world.AddComponent<StructureComponent>(entity, structure);

    HealthComponent health{};
    health.fCurrent = maxHp;
    health.fMaximum = maxHp;
    m_world.AddComponent<HealthComponent>(entity, health);

    if (bTurret)
    {
        TurretComponent turret{};
        turret.team = team;
        turret.hp = maxHp;
        turret.maxHp = maxHp;
        turret.tier = static_cast<u8_t>(tier);
        turret.laneType = static_cast<u8_t>(lane);
        m_world.AddComponent<TurretComponent>(entity, turret);

        TurretAIComponent ai{};
        ai.attackRange = 7.75f;
        ai.attackCooldownMax = 1.0f;
        ai.attackDamage = (tier == static_cast<u32_t>(Winters::Map::eTurretTier::Nexus))
            ? 180.f
            : 150.f;
        ai.projectileSpeed = 18.f;
        m_world.AddComponent<TurretAIComponent>(entity, ai);
    }

    if (bNexus)
        m_world.AddComponent<NexusTag>(entity);
    if (bInhibitor)
        m_world.AddComponent<InhibitorTag>(entity);

    SpatialAgentComponent spatial{};
    if (bTurret)
        spatial.kind = eSpatialKind::Turret;
    else if (bInhibitor)
        spatial.kind = eSpatialKind::Inhibitor;
    else
        spatial.kind = eSpatialKind::Nexus;
    spatial.team = TeamByte(team);
    spatial.radius = ResolveStageStructureRadius(kind, tier);
    m_world.AddComponent<SpatialAgentComponent>(entity, spatial);

    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, 2.5f, spatial.radius };
    collider.vOffset = { 0.f, 1.25f, 0.f };
    collider.bIsTrigger = false;
    m_world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = bTurret ? 12.f : 10.f;
    vision.bTrueSight = bTurret;
    m_world.AddComponent<VisionSourceComponent>(entity, vision);
    m_world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    m_world.AddComponent<TargetableTag>(entity);

    NetAnimationComponent anim{};
    anim.animId = static_cast<u16_t>(eNetAnimId::Idle);
    m_world.AddComponent<NetAnimationComponent>(entity, anim);

    const NetEntityId netId = m_entityMap.IssueNew(entity);
    NetEntityIdComponent netEntity{};
    netEntity.netId = netId;
    m_world.AddComponent<NetEntityIdComponent>(entity, netEntity);

    return entity;
}

EntityID CGameRoom::SpawnServerMinion(eTeam team, u8_t roleType, u8_t lane, const Vec3& pos)
{
    const EntityID entity = m_world.CreateEntity();

    Vec3 spawnPos = pos;
    (void)TryResolveServerWalkablePosition(pos, 16, spawnPos);

    TransformComponent transform{};
    transform.SetPosition(spawnPos);
    transform.SetScale(0.006f);
    m_world.AddComponent<TransformComponent>(entity, transform);

    MinionStateComponent state{};
    state.current = MinionStateComponent::LaneMove;
    state.currentWaypoint = ResolveServerMinionStartWaypoint(team, lane, spawnPos);
    state.team = team;
    state.type = roleType;
    state.lane = lane;
    state.moveSpeed = (roleType == 3) ? 5.0f : ((roleType == 2) ? 3.5f : 4.0f);
    state.attackRange = (roleType == 1) ? 8.0f : ((roleType == 2) ? 10.0f : 1.5f);
    state.sightRange = (roleType == 0) ? 12.f : ((roleType == 2) ? 16.f : 14.f);
    state.attackDamage = (roleType == 3) ? 100.f : ((roleType == 2) ? 40.f : ((roleType == 1) ? 30.f : 20.f));
    state.attackCooldownMax = (roleType == 1) ? 1.2f : 1.0f;
    state.targetScanInterval = ServerMinionTuning::kTargetScanIntervalSec;
    const u32_t scanBucket =
        (static_cast<u32_t>(entity) * 1103515245u +
            static_cast<u32_t>(lane) * 2246822519u +
            static_cast<u32_t>(roleType) * 3266489917u) %
        ServerMinionTuning::kTargetScanStaggerBuckets;
    state.targetScanCooldown =
        state.targetScanInterval *
        (static_cast<f32_t>(scanBucket) /
            static_cast<f32_t>(ServerMinionTuning::kTargetScanStaggerBuckets));
    m_world.AddComponent<MinionStateComponent>(entity, state);

    const f32_t maxHp = (roleType == 3) ? 1000.f : 450.f;
    HealthComponent health{};
    health.fCurrent = maxHp;
    health.fMaximum = maxHp;
    m_world.AddComponent<HealthComponent>(entity, health);

    MinionComponent minion{};
    minion.team = team;
    minion.laneType = lane;
    minion.roleType = roleType;
    minion.hp = maxHp;
    minion.maxHp = maxHp;
    m_world.AddComponent<MinionComponent>(entity, minion);

    VelocityComponent velocity{};
    m_world.AddComponent<VelocityComponent>(entity, velocity);

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::Minion;
    spatial.team = TeamByte(team);
    spatial.radius = 0.5f;
    m_world.AddComponent<SpatialAgentComponent>(entity, spatial);

    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, 1.0f, spatial.radius };
    collider.vOffset = { 0.f, 0.5f, 0.f };
    collider.bIsTrigger = false;
    m_world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = state.sightRange;
    m_world.AddComponent<VisionSourceComponent>(entity, vision);
    m_world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    m_world.AddComponent<TargetableTag>(entity);

    NetAnimationComponent anim{};
    anim.animId = static_cast<u16_t>(eNetAnimId::Run);
    m_world.AddComponent<NetAnimationComponent>(entity, anim);

    const NetEntityId netId = m_entityMap.IssueNew(entity);
    NetEntityIdComponent netEntity{};
    netEntity.netId = netId;
    m_world.AddComponent<NetEntityIdComponent>(entity, netEntity);

    return entity;
}

bool_t CGameRoom::TryResolveServerWalkablePosition(const Vec3& vRawPos,
    int32_t maxRadius, Vec3& vOutPos) const
{
    const Engine::CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();

    if (!pGrid)
    {
        vOutPos = vRawPos;
        return true;
    }

    const Engine::CNavGrid::Cell cell = pGrid->WorldToCell(vRawPos);

    if (pGrid->IsWalkable(cell.x, cell.y))
    {
        vOutPos = vRawPos;
        return true;
    }

    Engine::CNavGrid::Cell nearest{};
    if (!pGrid->TryFindNearestWalkableCell(cell, maxRadius, nearest))
        return false;

    vOutPos = pGrid->CellToWorld(nearest.x, nearest.y);
    if (!TrySampleHeight(vOutPos.x, vOutPos.z, vOutPos.y))
        vOutPos.y = vRawPos.y;

    return true;
}

void CGameRoom::SanitizeServerMoversOnNavGrid()
{
    u32_t corrected = 0u;
    m_world.ForEach<SpatialAgentComponent, TransformComponent>(
        std::function<void(EntityID, SpatialAgentComponent&, TransformComponent&)>(
            [&](EntityID entity, SpatialAgentComponent& agent, TransformComponent& transform)
            {
                if (agent.kind != eSpatialKind::Champion &&
                    agent.kind != eSpatialKind::Minion)
                {
                    return;
                }

                const Vec3 pos = transform.GetPosition();
                Vec3 resolved{};
                if (!TryResolveServerWalkablePosition(pos, 16, resolved))
                    return;
                if (WintersMath::DistanceSqXZ(pos, resolved) <= 0.0001f)
                    return;

                transform.SetPosition(resolved);
                if (m_world.HasComponent<RespawnComponent>(entity))
                    m_world.GetComponent<RespawnComponent>(entity).spawnPos = resolved;
                ++corrected;
            }));

    if (corrected > 0u)
    {
        char msg[160]{};
        sprintf_s(msg, "[ServerNav] sanitized movers corrected=%u\n", corrected);
        OutputServerAITrace(msg);
    }
}

void CGameRoom::SanitizeServerWaypointPatrolsOnNavGrid()
{
    m_world.ForEach<WaypointPatrolComponent, TransformComponent>(
        std::function<void(EntityID, WaypointPatrolComponent&, TransformComponent&)>(
            [&](EntityID entity, WaypointPatrolComponent& patrol, TransformComponent& transform)
            {
                Vec3 resolvedPos{};
                if (TryResolveServerWalkablePosition(transform.GetPosition(), 16, resolvedPos))
                {
                    transform.SetPosition(resolvedPos);
                    if (m_world.HasComponent<RespawnComponent>(entity))
                        m_world.GetComponent<RespawnComponent>(entity).spawnPos = resolvedPos;
                }

                const u8_t count = (std::min)(patrol.pointCount, kWaypointPatrolMaxPoints);
                for (u8_t i = 0; i < count; ++i)
                {
                    Vec3 resolvedPoint{};
                    if (TryResolveServerWalkablePosition(patrol.points[i], 16, resolvedPoint))
                        patrol.points[i] = resolvedPoint;
                }
            }));
}

void CGameRoom::SanitizeServerMinionWaypointsOnNavGrid()
{
    m_serverMinionWaves.SanitizeWaypoints(
        [this](const Vec3& rawPos, int32_t maxRadius, Vec3& outPos)
        {
            return TryResolveServerWalkablePosition(rawPos, maxRadius, outPos);
        },
        [](const char* pText)
        {
            OutputServerAITrace(pText);
        });
}

u32_t CGameRoom::ResolveServerMinionStartWaypoint(eTeam team, u8_t lane, const Vec3& vSpawnPos) const
{
    const u8_t waypointLane = ResolveServerWaypointLane(team, lane);
    const u32_t waypointCount = GetServerMinionWaypointCount(team, waypointLane);
    if (waypointCount <= 1u)
        return 0u;

    for (u32_t index = 1u; index < waypointCount; ++index)
    {
        Vec3 resolvedTarget{};
        if (TryResolveMoveTarget(
            vSpawnPos,
            GetServerMinionWaypoint(team, waypointLane, index),
            resolvedTarget))
        {
            return index;
        }
    }

    return 1u;
}

bool_t CGameRoom::TryResolveMinionDepenetrationStep(
    EntityID entity,
    const Vec3& vPos,
    f32_t fStep,
    const TickContext& tc,
    Vec3& vOutNext)
{
    if (!m_world.HasComponent<SpatialAgentComponent>(entity))
        return false;

    const SpatialAgentComponent& self = m_world.GetComponent<SpatialAgentComponent>(entity);
    const f32_t selfRadius = (std::max)(0.2f, self.radius);

    const u32_t blockerMask =
        SpatialMask(eSpatialKind::Minion) |
        SpatialMask(eSpatialKind::JungleMob) |
        SpatialMask(eSpatialKind::Turret) |
        SpatialMask(eSpatialKind::Inhibitor) |
        SpatialMask(eSpatialKind::Nexus);

    std::vector<EntityID> blockers;
    blockers.reserve(32);

    if (CSpatialIndex* pSpatial = m_world.Get_SpatialIndex())
    {
        pSpatial->QueryRadius(vPos, selfRadius + 3.0f, blockerMask, 0u, blockers);
    }
    else
    {
        blockers = DeterministicEntityIterator<SpatialAgentComponent>::CollectSorted(m_world);
    }

    Vec3 vPush{};
    u32_t blockerCount = 0u;
    u32_t staticCount = 0u;
    u32_t dynamicCount = 0u;
    u32_t softMinionCount = 0u;

    for (EntityID other : blockers)
    {
        if (other == entity ||
            !m_world.HasComponent<SpatialAgentComponent>(other) ||
            !m_world.HasComponent<TransformComponent>(other))
        {
            continue;
        }

        const SpatialAgentComponent& agent = m_world.GetComponent<SpatialAgentComponent>(other);
        const bool_t bSoftMinion = agent.kind == eSpatialKind::Minion;
        if (!bSoftMinion && !IsMoveBlockingKind(agent.kind))
            continue;

        if (m_world.HasComponent<HealthComponent>(other))
        {
            const HealthComponent& health = m_world.GetComponent<HealthComponent>(other);
            if (health.bIsDead || health.fCurrent <= 0.f)
                continue;
        }

        const Vec3 otherPos = m_world.GetComponent<TransformComponent>(other).GetPosition();
        Vec3 vAway{ vPos.x - otherPos.x, 0.f, vPos.z - otherPos.z };
        f32_t distSq = vAway.x * vAway.x + vAway.z * vAway.z;

        if (distSq <= 0.0001f)
        {
            const u32_t hash =
                static_cast<u32_t>(entity) * 73856093u ^
                static_cast<u32_t>(other) * 19349663u;
            vAway = Vec3{
                (hash & 1u) ? 1.f : -1.f,
                0.f,
                (hash & 2u) ? 1.f : -1.f };
            distSq = vAway.x * vAway.x + vAway.z * vAway.z;
        }

        const bool_t bStatic = IsStaticMoveBlockingKind(agent.kind);
        const f32_t otherRadius = (std::max)(0.2f, agent.radius);
        const f32_t padding = bStatic ? 0.20f : (bSoftMinion ? 0.f : 0.04f);
        const f32_t radiusScale = bSoftMinion
            ? ServerMinionTuning::kMinionSoftSeparationRadiusScale
            : 1.f;
        const f32_t minDist = (selfRadius + otherRadius) * radiusScale + padding;
        if (distSq >= minDist * minDist)
            continue;

        const f32_t dist = std::sqrt(distSq);
        const f32_t penetration = minDist - dist;
        const f32_t weight = bStatic
            ? 1.0f
            : (bSoftMinion ? ServerMinionTuning::kMinionSoftSeparationWeight : 0.55f);

        vPush.x += (vAway.x / dist) * penetration * weight;
        vPush.z += (vAway.z / dist) * penetration * weight;

        ++blockerCount;
        if (bStatic)
            ++staticCount;
        else if (bSoftMinion)
            ++softMinionCount;
        else
            ++dynamicCount;
    }

    const f32_t pushLenSq = vPush.x * vPush.x + vPush.z * vPush.z;
    if (pushLenSq <= 0.0001f)
        return false;

    const f32_t pushLen = std::sqrt(pushLenSq);
    const f32_t maxPushStep = softMinionCount > 0u && staticCount == 0u && dynamicCount == 0u
        ? ServerMinionTuning::kMinionSoftSeparationMaxStep
        : 0.35f;
    const f32_t pushStep = (std::min)((std::max)(0.08f, fStep), maxPushStep);
    const Vec3 vCandidate{
        vPos.x + (vPush.x / pushLen) * pushStep,
        vPos.y,
        vPos.z + (vPush.z / pushLen) * pushStep
    };

    Vec3 vGuarded = vCandidate;
    if (!TryClampMoveSegmentXZ(
        vPos,
        vCandidate,
        ServerMinionTuning::kMinionLaneClearanceRadius,
        vGuarded))
    {
        return false;
    }

    if (!TrySampleHeight(vGuarded.x, vGuarded.z, vGuarded.y))
        vGuarded.y = vPos.y;

    static u32_t s_minionDepenetrationLogCount = 0u;
    if (s_minionDepenetrationLogCount < 256u)
    {
        const Engine::CNavGrid::Cell posCell = ResolveDebugCell(m_pNavGrid.get(), vPos);
        const Engine::CNavGrid::Cell nextCell = ResolveDebugCell(m_pNavGrid.get(), vGuarded);

        char msg[512]{};
        sprintf_s(
            msg,
            "[MinionMove][Depenetrate] tick=%llu entity=%u posCell=(%d,%d) nextCell=(%d,%d) "
            "push=(%.3f,%.3f) blockers=%u static=%u dynamic=%u softMinion=%u from=(%.2f,%.2f) to=(%.2f,%.2f)\n",
            static_cast<unsigned long long>(tc.tickIndex),
            static_cast<u32_t>(entity),
            posCell.x,
            posCell.y,
            nextCell.x,
            nextCell.y,
            vPush.x,
            vPush.z,
            blockerCount,
            staticCount,
            dynamicCount,
            softMinionCount,
            vPos.x,
            vPos.z,
            vGuarded.x,
            vGuarded.z);
        OutputServerAITrace(msg);
        ++s_minionDepenetrationLogCount;
    }

    vOutNext = vGuarded;
    return WintersMath::DistanceSqXZ(vPos, vOutNext) > 0.0001f;
}

bool_t CGameRoom::TryMoveServerMinionToward(
    EntityID entity,
    MinionStateComponent& state,
    TransformComponent& transform,
    const Vec3& vTarget,
    f32_t fArriveRadius,
    TickContext& tc,
    u32_t& PathBuildBudget,
    bool_t& outMoved,
    MinionStateComponent::State moveState)
{
    state.PathRebuildCooldown = (std::max)(0.f, state.PathRebuildCooldown - tc.fDt);

    const Vec3 vPos = transform.GetPosition();
    const f32_t fResolvedArriveRadius = (std::max)(0.1f, fArriveRadius);
    if (WintersMath::DistanceSqXZ(vPos, vTarget) <= fResolvedArriveRadius * fResolvedArriveRadius)
    {
        state.current = moveState;
        state.PathCount = 0u;
        state.PathIndex = 0u;
        state.BlockedMoveFrames = 0u;
        return true;
    }

    Vec3 vMoveGoal = vTarget;
    if (!SegmentWalkableXZ(vPos, vTarget, ServerMinionTuning::kMinionLaneClearanceRadius))
    {
        const bool_t bTargetMoved =
            WintersMath::DistanceSqXZ(state.PathTarget, vTarget) >
            ServerMinionTuning::kPathTargetRefreshDistanceSq;

        const bool_t bNeedPath =
            state.PathCount == 0u ||
            state.PathIndex >= state.PathCount ||
            bTargetMoved ||
            state.BlockedMoveFrames >= ServerMinionTuning::kBlockedFramesBeforeRepath;

        if (bNeedPath &&
            state.PathRebuildCooldown <= 0.f &&
            PathBuildBudget > 0u)
        {
            Vec3 vResolvedTarget = vTarget;
            u16_t PathCount = 0u;
            const bool_t bPathBuilt = TryBuildServerMinionMovePath(
                vPos,
                vTarget,
                state.PathWaypoints,
                MinionStateComponent::PathMaxWaypoints,
                PathCount,
                vResolvedTarget);
            if (bPathBuilt)
            {
                state.PathTarget = vTarget;
                state.PathResolvedTarget = vResolvedTarget;
                state.PathCount = PathCount;
                state.PathIndex = 0u;
                state.BlockedMoveFrames = 0u;
            }

            const Engine::CNavGrid* pPathDebugGrid = m_pMinionLaneNavGrid ? m_pMinionLaneNavGrid.get() : m_pPathNavGrid.get();
            if (!pPathDebugGrid)
                pPathDebugGrid = m_pNavGrid.get();

            OutputServerMinionPathDebug(
                m_pNavGrid.get(),
                pPathDebugGrid,
                tc.tickIndex,
                entity,
                state,
                vPos,
                vTarget,
                vResolvedTarget,
                PathCount,
                PathBuildBudget,
                bPathBuilt);

            --PathBuildBudget;
            state.PathRebuildCooldown =
                moveState == MinionStateComponent::Chase
                ? ServerMinionTuning::kChasePathRebuildIntervalSec
                : ServerMinionTuning::kLanePathRebuildIntervalSec;
        }

        if (state.PathCount == 0u || state.PathIndex >= state.PathCount)
        {
            vMoveGoal = vTarget;
        }
        else
        {
            while (state.PathIndex + 1u < state.PathCount &&
                WintersMath::DistanceSqXZ(vPos, state.PathWaypoints[state.PathIndex]) <=
                ServerMinionTuning::kPathWaypointArriveRadius *
                ServerMinionTuning::kPathWaypointArriveRadius)
            {
                ++state.PathIndex;
            }

            vMoveGoal = state.PathWaypoints[state.PathIndex];
        }
    }
    else
    {
        state.PathCount = 0u;
        state.PathIndex = 0u;
    }

    const Vec3 vToGoal{ vMoveGoal.x - vPos.x, 0.f, vMoveGoal.z - vPos.z };
    if ((vToGoal.x * vToGoal.x + vToGoal.z * vToGoal.z) <= 0.0001f)
        return false;

    const Vec3 vDir = NormalizeXZOrForward(vToGoal, state.team);
    const f32_t fStep = state.moveSpeed * tc.fDt;

    Vec3 vNext{};
    if (!TryResolveMinionMoveStep(entity, vPos, vDir, fStep, tc, vNext))
    {
        Vec3 vDepenetrated{};
        if (TryResolveMinionDepenetrationStep(entity, vPos, fStep, tc, vDepenetrated))
        {
            const Vec3 vActualMove{ vDepenetrated.x - vPos.x, 0.f, vDepenetrated.z - vPos.z };
            transform.SetPosition(vDepenetrated);
            FaceServerMinionTowardDirection(transform, vActualMove);
            state.current = moveState;
            state.BlockedMoveFrames = 0u;
            outMoved = true;
            return true;
        }

        ++state.BlockedMoveFrames;

        const Vec3* pWaypoint =
            state.PathCount > 0u && state.PathIndex < state.PathCount
            ? &state.PathWaypoints[state.PathIndex]
            : nullptr;
        OutputServerMinionStuckDebug(
            "toward-resolve-failed",
            m_pNavGrid.get(),
            tc.tickIndex,
            entity,
            state,
            vPos,
            vMoveGoal,
            pWaypoint);
        return false;
    }

    const Vec3 vActualMove{ vNext.x - vPos.x, 0.f, vNext.z - vPos.z };
    transform.SetPosition(vNext);
    FaceServerMinionTowardDirection(transform, vActualMove);
    state.current = moveState;
    state.BlockedMoveFrames = 0u;
    outMoved = true;
    return true;
}

bool_t CGameRoom::TryMoveServerMinionByFlowFields(
    EntityID entity,
    MinionStateComponent& state,
    TransformComponent& transform,
    const Vec3& vLaneTarget,
    TickContext& tc,
    bool_t& outMoved)
{
    state.PathRebuildCooldown = (std::max)(0.f, state.PathRebuildCooldown - tc.fDt);

    if (state.PathCount > 0u && state.PathIndex < state.PathCount)
        return false;

    const Vec3 vPos = transform.GetPosition();
    const f32_t fPrevLaneDistSq = WintersMath::DistanceSqXZ(vPos, vLaneTarget);

    Vec3 vDir{};
    if (!m_serverMinionWaves.TryResolveFlowDirection(
        state.team,
        state.lane,
        vPos,
        vDir))
    {
        return false;
    }

    const f32_t fLenSq = vDir.x * vDir.x + vDir.z * vDir.z;
    if (fLenSq <= 0.0001f)
        return false;

    const f32_t fStep = state.moveSpeed * tc.fDt;
    Vec3 vNext{};
    if (!TryResolveMinionMoveStep(entity, vPos, vDir, fStep, tc, vNext))
    {
        Vec3 vDepenetrated{};
        if (TryResolveMinionDepenetrationStep(entity, vPos, fStep, tc, vDepenetrated))
        {
            const Vec3 vActualMove{ vDepenetrated.x - vPos.x, 0.f, vDepenetrated.z - vPos.z };
            transform.SetPosition(vDepenetrated);
            FaceServerMinionTowardDirection(transform, vActualMove);
            state.current = MinionStateComponent::LaneMove;
            state.BlockedMoveFrames = 0u;
            outMoved = true;
            return true;
        }

        ++state.BlockedMoveFrames;
        OutputServerMinionStuckDebug(
            "flow-resolve-failed",
            m_pNavGrid.get(),
            tc.tickIndex,
            entity,
            state,
            vPos,
            vLaneTarget,
            nullptr);
        return false;
    }

    const f32_t fNextLaneDistSq = WintersMath::DistanceSqXZ(vNext, vLaneTarget);
    const bool_t bProgressed =
        fNextLaneDistSq + ServerMinionTuning::kFlowFieldProgressSlackSq < fPrevLaneDistSq;

    if (!bProgressed)
    {
        ++state.BlockedMoveFrames;
        if (state.BlockedMoveFrames >= ServerMinionTuning::kFlowFieldStallFramesBeforePathFallback)
        {
            static u32_t s_flowFieldFallbackLogCount = 0;
            if (s_flowFieldFallbackLogCount < 64u)
            {
                char msg[256]{};
                sprintf_s(msg,
                    "[MinionAI] flow fallback reason=stall entity=%u team=%u lane=%u blocked=%u pos=(%.2f,%.2f) target=(%.2f,%.2f)\n",
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(state.team),
                    static_cast<u32_t>(state.lane),
                    static_cast<u32_t>(state.BlockedMoveFrames),
                    vPos.x,
                    vPos.z,
                    vLaneTarget.x,
                    vLaneTarget.z);
                OutputServerAITrace(msg);
                ++s_flowFieldFallbackLogCount;
            }

            return false;
        }
    }
    else
    {
        state.BlockedMoveFrames = 0u;
    }

    const Vec3 vActualMove{ vNext.x - vPos.x, 0.f, vNext.z - vPos.z };
    transform.SetPosition(vNext);
    FaceServerMinionTowardDirection(transform, vActualMove);
    state.current = MinionStateComponent::LaneMove;
    state.PathCount = 0u;
    state.PathIndex = 0u;
    outMoved = true;
    return true;
}

bool_t CGameRoom::TryResolveMinionMoveStep(
    EntityID entity,
    const Vec3& vPos,
    const Vec3& vDesiredDir,
    f32_t fStep,
    const TickContext& tc,
    Vec3& vOutNext)
{
    static constexpr f32_t kAngles[] =
    {
        0.f,
        0.610865f, -0.610865f,
        1.22173f, -1.22173f,
        1.570796f, -1.570796f
    };

    const f32_t fRadius = ResolveAgentRadius(m_world, entity);
    u32_t actorBlocked = 0u;
    u32_t navBlocked = 0u;

    for (const f32_t fAngle : kAngles)
    {
        const Vec3 vDir = WintersMath::RotateXZ(vDesiredDir, fAngle);
        const Vec3 vCandidate{
            vPos.x + vDir.x * fStep,
            vPos.y,
            vPos.z + vDir.z * fStep
        };

        if (!IsAvoidanceCandidateClear(m_world, entity, vPos, vCandidate, fRadius))
        {
            ++actorBlocked;
            continue;
        }

        Vec3 vGuarded = vCandidate;
        if (!TryClampMoveSegmentXZ(
            vPos,
            vCandidate,
            ServerMinionTuning::kMinionLaneClearanceRadius,
            vGuarded))
        {
            ++navBlocked;
            continue;
        }

        f32_t fSurfaceY = 0.f;
        if (TrySampleHeight(vGuarded.x, vGuarded.z, fSurfaceY))
            vGuarded.y = fSurfaceY;

        const bool_t bAngleAdjusted = std::fabs(fAngle) > 0.001f;
        const bool_t bClamped =
            WintersMath::DistanceSqXZ(vCandidate, vGuarded) > 0.0001f;
        if (bAngleAdjusted || bClamped || actorBlocked > 0u || navBlocked > 0u)
        {
            static u32_t s_minionResolveLogCount = 0u;
            if (s_minionResolveLogCount < 512u)
            {
                const Engine::CNavGrid::Cell posCell = ResolveDebugCell(m_pNavGrid.get(), vPos);
                const Engine::CNavGrid::Cell candidateCell = ResolveDebugCell(m_pNavGrid.get(), vCandidate);
                const Engine::CNavGrid::Cell guardedCell = ResolveDebugCell(m_pNavGrid.get(), vGuarded);

                char msg[640]{};
                sprintf_s(
                    msg,
                    "[MinionMove][Resolve] tick=%llu entity=%u posCell=(%d,%d) "
                    "desiredDir=(%.3f,%.3f) angle=%.3f selectedDir=(%.3f,%.3f) "
                    "candidate=(%.2f,%.2f) candidateCell=(%d,%d) "
                    "guarded=(%.2f,%.2f) guardedCell=(%d,%d) "
                    "actorBlocked=%u navBlocked=%u clamped=%u\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(entity),
                    posCell.x,
                    posCell.y,
                    vDesiredDir.x,
                    vDesiredDir.z,
                    fAngle,
                    vDir.x,
                    vDir.z,
                    vCandidate.x,
                    vCandidate.z,
                    candidateCell.x,
                    candidateCell.y,
                    vGuarded.x,
                    vGuarded.z,
                    guardedCell.x,
                    guardedCell.y,
                    actorBlocked,
                    navBlocked,
                    bClamped ? 1u : 0u);
                OutputServerAITrace(msg);
                ++s_minionResolveLogCount;
            }
        }

        vOutNext = vGuarded;
        return true;
    }

    static u32_t s_minionResolveFailLogCount = 0u;
    if (s_minionResolveFailLogCount < 256u)
    {
        const Engine::CNavGrid::Cell posCell = ResolveDebugCell(m_pNavGrid.get(), vPos);

        char msg[384]{};
        sprintf_s(
            msg,
            "[MinionMove][ResolveFail] tick=%llu entity=%u pos=(%.2f,%.2f) posCell=(%d,%d) "
            "desiredDir=(%.3f,%.3f) step=%.3f radius=%.2f actorBlocked=%u navBlocked=%u candidates=%u\n",
            static_cast<unsigned long long>(tc.tickIndex),
            static_cast<u32_t>(entity),
            vPos.x,
            vPos.z,
            posCell.x,
            posCell.y,
            vDesiredDir.x,
            vDesiredDir.z,
            fStep,
            fRadius,
            actorBlocked,
            navBlocked,
            static_cast<u32_t>(sizeof(kAngles) / sizeof(kAngles[0])));
        OutputServerAITrace(msg);
        ++s_minionResolveFailLogCount;
    }

    return false;
}

bool_t CGameRoom::TryBuildServerMinionMovePath(
    const Vec3& vStart,
    const Vec3& vGoal,
    Vec3* pOutWaypoints,
    u16_t maxWayPoints,
    u16_t& outCount,
    Vec3& outResolvedGoal) const
{
    outCount = 0u;
    outResolvedGoal = vGoal;
    if (!pOutWaypoints || maxWayPoints == 0u)
        return false;

    auto ApplyPathHeight = [&](Vec3& ioPos, f32_t fallbackY)
        {
            f32_t sampledY = fallbackY;
            if (!TrySampleHeight(ioPos.x, ioPos.z, sampledY))
            {
                ioPos.y = fallbackY;
                return;
            }

            if (std::fabs(sampledY - vStart.y) <= kMoveTargetMaxSurfaceDeltaY)
                ioPos.y = sampledY;
            else
                ioPos.y = fallbackY;
        };

    const Engine::CNavGrid* pGrid = m_pMinionLaneNavGrid ? m_pMinionLaneNavGrid.get() : m_pPathNavGrid.get();
    if (!pGrid)
        pGrid = m_pNavGrid.get();

    if (!pGrid)
    {
        ApplyPathHeight(outResolvedGoal, vStart.y);
        pOutWaypoints[outCount++] = outResolvedGoal;
        return true;
    }

    Engine::CNavGrid::Cell start = pGrid->WorldToCell(vStart);
    if (!pGrid->IsWalkable(start.x, start.y))
    {
        Engine::CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 16, nearestStart))
            return false;
        start = nearestStart;
    }

    const Engine::CNavGrid::Cell rawGoal = pGrid->WorldToCell(vGoal);
    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
        return false;

    if (pGrid->IsWalkable(rawGoal.x, rawGoal.y) &&
        pGrid->SegmentWalkable(vStart, vGoal, 0.f))
    {
        outResolvedGoal = vGoal;
        ApplyPathHeight(outResolvedGoal, vStart.y);
        pOutWaypoints[outCount++] = outResolvedGoal;
        return true;
    }

    Engine::CNavGrid::Cell resolvedGoal{};
    std::vector<Engine::CNavGrid::Cell> path{};
    if (!Engine::CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolvedGoal,
        &path))
    {
        return false;
    }

    outResolvedGoal = pGrid->CellToWorld(resolvedGoal.x, resolvedGoal.y);
    ApplyPathHeight(outResolvedGoal, vStart.y);

    Engine::CNavGrid::Cell lastAppended{ -1, -1 };
    auto AppendCell = [&](Engine::CNavGrid::Cell cell) -> bool_t
        {
            if (cell.x == lastAppended.x && cell.y == lastAppended.y)
                return true;
            if (outCount >= maxWayPoints)
                return false;

            Vec3 waypoint = pGrid->CellToWorld(cell.x, cell.y);
            ApplyPathHeight(waypoint, outResolvedGoal.y);

            pOutWaypoints[outCount++] = waypoint;
            lastAppended = cell;
            return true;
        };

    const std::vector<Engine::CNavGrid::Cell> smoothedPath = SmoothServerPathCells(*pGrid, path);
    if (smoothedPath.size() <= 1u)
        return AppendCell(resolvedGoal);

    for (size_t i = 1u; i < smoothedPath.size(); ++i)
    {
        if (!AppendCell(smoothedPath[i]))
            return false;
    }

    return outCount > 0u;
}

u32_t CGameRoom::GetServerMinionWaypointCount(eTeam team, u8_t lane) const
{
    return m_serverMinionWaves.GetWaypointCount(team, lane);
}

Vec3 CGameRoom::GetServerMinionWaypoint(eTeam team, u8_t lane, u32_t index) const
{
    return m_serverMinionWaves.GetWaypoint(team, lane, index);
}

u8_t CGameRoom::ResolveServerStructureLane(
    eTeam team,
    u32_t kind,
    u32_t tier,
    const Vec3& pos) const
{
    if (kind == kStructureKindNexus ||
        (kind == kStructureKindTurret &&
            tier == static_cast<u32_t>(Winters::Map::eTurretTier::Nexus)))
    {
        return static_cast<u8_t>(kLaneBase);
    }

    static constexpr u8_t kPhysicalLanes[] =
    {
        static_cast<u8_t>(kLaneTop),
        static_cast<u8_t>(kLaneMid),
        static_cast<u8_t>(kLaneBot),
    };

    u8_t bestLane = static_cast<u8_t>(kLaneMid);
    f32_t bestScore = std::numeric_limits<f32_t>::max();

    for (u8_t lane : kPhysicalLanes)
    {
        const u8_t waypointLane = ResolveServerWaypointLane(team, lane);
        const u32_t waypointCount = GetServerMinionWaypointCount(team, waypointLane);
        if (waypointCount == 0u)
            continue;

        f32_t score = std::numeric_limits<f32_t>::max();
        if (waypointCount >= 2u)
        {
            for (u32_t i = 1u; i < waypointCount; ++i)
            {
                f32_t t = 0.f;
                score = std::min(score, WintersMath::DistanceSqPointToSegmentXZ(
                    pos,
                    GetServerMinionWaypoint(team, waypointLane, i - 1u),
                    GetServerMinionWaypoint(team, waypointLane, i),
                    &t,
                    std::numeric_limits<f32_t>::epsilon()));
            }
        }
        else
        {
            score = WintersMath::DistanceSqXZ(pos, GetServerMinionWaypoint(team, waypointLane, 0u));
        }

        if (score < bestScore)
        {
            bestScore = score;
            bestLane = lane;
        }
    }

    return bestLane;
}

EntityID CGameRoom::SpawnChampionForLobbySlot(LobbySlotState& slot)
{
    const EntityID entity = m_world.CreateEntity();

    const Vec3 spawnPos = GetSpawnPositionForLobbySlot(slot);

    TransformComponent transform{};
    transform.SetPosition(spawnPos);
    m_world.AddComponent<TransformComponent>(entity, transform);

    const ChampionStatsDef statsDef =
        CChampionStatsRegistry::Instance().Resolve(slot.champion);
    StatComponent stat = CStatSystem::BuildBaseStats(statsDef, 6);
    stat.hpMax = ResolveServerChampionMaxHpForSlot(slot, stat.hpMax);
    m_world.AddComponent<StatComponent>(entity, stat);

    HealthComponent health{};
    health.fCurrent = stat.hpMax;
    health.fMaximum = stat.hpMax;
    health.bIsDead = false;
    m_world.AddComponent<HealthComponent>(entity, health);

    RespawnComponent respawn{};
    respawn.spawnPos = spawnPos;
    respawn.respawnDelay = kDefaultChampionRespawnDelaySec;
    m_world.AddComponent<RespawnComponent>(entity, respawn);

    SkillStateComponent skillState{};
    m_world.AddComponent<SkillStateComponent>(entity, skillState);

    CExperienceSystem::InitializeChampionExperience(m_world, entity, stat.level);

    SkillRankComponent skillRank{};
    if (slot.bBot && !slot.bDummy)
        AssignDefaultBotSkillRanks(skillRank, stat.level);
    else
        CSkillRankSystem::SyncPointsForLevel(skillRank, stat.level);
    m_world.AddComponent<SkillRankComponent>(entity, skillRank);

    GoldComponent gold{};
    gold.amount = 10000;
    m_world.AddComponent<GoldComponent>(entity, gold);

    InventoryComponent inventory{};
    m_world.AddComponent<InventoryComponent>(entity, inventory);

    RuneLoadoutComponent runeLoadout{};
    runeLoadout.eRunes[0] = eRuneId::LethalTempo;
    runeLoadout.iCount = 1u;
    m_world.AddComponent<RuneLoadoutComponent>(entity, runeLoadout);
    m_world.AddComponent<RuneRuntimeComponent>(entity, RuneRuntimeComponent{});

    ChampionScoreComponent score{};
    m_world.AddComponent<ChampionScoreComponent>(entity, score);

    SummonerSpellStateComponent summonerSpellState{};
    m_world.AddComponent<SummonerSpellStateComponent>(entity, summonerSpellState);

    ChampionComponent champion{};
    champion.id = slot.champion;
    champion.team = static_cast<eTeam>(slot.team);
    champion.hp = health.fCurrent;
    champion.maxHp = health.fMaximum;
    champion.mana = stat.manaMax;
    champion.maxMana = stat.manaMax;
    champion.moveSpeed = stat.moveSpeed;
    champion.level = stat.level;
    m_world.AddComponent<ChampionComponent>(entity, champion);

    if (slot.champion == eChampion::YASUO)
        m_world.AddComponent<YasuoStateComponent>(entity, YasuoStateComponent{});
    if (slot.champion == eChampion::ASHE)
        m_world.AddComponent<AsheSimComponent>(entity, AsheSimComponent{});
    if (slot.champion == eChampion::ANNIE)
        m_world.AddComponent<AnnieSimComponent>(entity, AnnieSimComponent{});
    if (slot.champion == eChampion::FIORA)
        m_world.AddComponent<FioraSimComponent>(entity, FioraSimComponent{});
    if (slot.champion == eChampion::JAX)
        m_world.AddComponent<JaxSimComponent>(entity, JaxSimComponent{});
    if (slot.champion == eChampion::VIEGO)
        m_world.AddComponent<ViegoSimComponent>(entity, ViegoSimComponent{});
    if (slot.champion == eChampion::YONE)
        m_world.AddComponent<YoneSimComponent>(entity, YoneSimComponent{});
    if (slot.champion == eChampion::LEESIN)
        m_world.AddComponent<LeeSinSimComponent>(entity, LeeSinSimComponent{});
    if (slot.champion == eChampion::KINDRED)
        m_world.AddComponent<KindredSimComponent>(entity, KindredSimComponent{});
    if (slot.champion == eChampion::MASTERYI)
        m_world.AddComponent<MasterYiSimComponent>(entity, MasterYiSimComponent{});

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::Champion;
    spatial.team = slot.team;
    spatial.radius = statsDef.spatialRadius;
    m_world.AddComponent<SpatialAgentComponent>(entity, spatial);

    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, 1.8f, spatial.radius };
    collider.vOffset = { 0.f, 0.9f, 0.f };
    collider.bIsTrigger = false;
    m_world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = statsDef.sightRange;
    m_world.AddComponent<VisionSourceComponent>(entity, vision);
    m_world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    m_world.AddComponent<TargetableTag>(entity);
    if (slot.bDummy)
        m_world.AddComponent<PracticeDummyTag>(entity);
    if (IsRedSylasSmokeDummySlot(slot))
    {
        WaypointPatrolComponent patrol{};
        patrol.pointCount = GetRedSylasSmokePatrolPointCount();
        for (u8_t i = 0; i < patrol.pointCount; ++i)
            patrol.points[i] = GetRedSylasSmokePatrolPoint(i);
        patrol.currentIndex = 1;
        patrol.direction = 1;
        patrol.mode = eWaypointPatrolMode::PingPong;
        patrol.arriveRadius = 0.35f;
        patrol.bActive = true;
        m_world.AddComponent<WaypointPatrolComponent>(entity, patrol);
    }

    if (slot.bBot && !slot.bDummy)
    {
        ChampionAIComponent ai{};
        const ChampionAIProfile& profile = GetChampionAIProfile(slot.champion);
        ai.champion = slot.champion;
        ai.team = static_cast<eTeam>(slot.team);
        ai.difficulty = slot.botDifficulty;
        ai.lane = ResolveInitialBotLane(slot);
        ai.decisionTimer = kChampionAIInitialDecisionDelaySec;
        ai.retreatGoal = spawnPos;
        ai.championScanRange = profile.championScanRange;
        ai.minionScanRange = profile.minionScanRange;
        ai.structureScanRange = profile.structureScanRange;
        ai.leashRange = profile.leashRange;
        ai.retreatHpRatio = profile.retreatHpRatio;
        ai.reengageHpRatio = profile.reengageHpRatio;

        const u8_t waypointLane = ResolveServerWaypointLane(ai.team, ai.lane);
        const u32_t waypointCount = GetServerMinionWaypointCount(ai.team, waypointLane);
        ai.laneGoal = waypointCount > 0u
            ? GetServerMinionWaypoint(ai.team, waypointLane, waypointCount / 2u)
            : GetGameSimLaneGatherPosition(ai.lane, slot.team);
        ai.safeAnchor = ResolveChampionAISafeAnchor(ai.team, ai.lane);
        ai.retreatGoal = ai.safeAnchor;

        m_world.AddComponent<ChampionAIComponent>(entity, ai);
    }

    NetAnimationComponent anim{};
    anim.animId = static_cast<u16_t>(eNetAnimId::Idle);
    anim.animPhaseFrame = 0;
    m_world.AddComponent<NetAnimationComponent>(entity, anim);

    slot.netId = m_entityMap.IssueNew(entity);

    NetEntityIdComponent netEntity{};
    netEntity.netId = slot.netId;
    m_world.AddComponent<NetEntityIdComponent>(entity, netEntity);

    if (slot.bHuman && slot.sessionId != 0)
        m_sessionToEntity[slot.sessionId] = entity;

    char spawnMsg[320]{};
    sprintf_s(spawnMsg,
        "[GameRoom] SpawnLobby slot=%u team=%u human=%u bot=%u dummy=%u champ=%u netId=%u entity=%u pos=(%.2f,%.2f,%.2f) aiDelay=%.2f\n",
        static_cast<u32_t>(slot.slotId),
        static_cast<u32_t>(slot.team),
        slot.bHuman ? 1u : 0u,
        slot.bBot ? 1u : 0u,
        slot.bDummy ? 1u : 0u,
        static_cast<u32_t>(slot.champion),
        slot.netId,
        static_cast<u32_t>(entity),
        spawnPos.x,
        spawnPos.y,
        spawnPos.z,
        (slot.bBot && !slot.bDummy) ? kChampionAIInitialDecisionDelaySec : 0.f);
    OutputServerAITrace(spawnMsg);

    return entity;
}
