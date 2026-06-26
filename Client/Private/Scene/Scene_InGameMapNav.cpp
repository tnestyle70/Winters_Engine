// Scene_InGameMapNav.cpp — CScene_InGame의 맵 surface 샘플링/NavGrid 빌드/이동 타겟 해석 책임 TU.
// Stage 1 (mechanical split): Scene_InGame.cpp에서 verbatim 이동. 동작/시그니처/호출순서 불변.
// 설계: .md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md
#define _CRT_SECURE_NO_WARNINGS

#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"
#include "Network/Client/EventApplier.h"
#include "Network/Client/GameSessionClient.h"
#include "Network/Client/SnapshotApplier.h"
#include "Replay/ReplayPlayer.h"

#include <Windows.h>
#include "Scene/GameplayQuery.h"
#include "Scene/InGameRosterSpawner.h"
#include "Scene/RenderVisibilityFilter.h"
#include "Scene/Scene_InGame.h"
#include "Scene/Scene_InGameInternal.h"
#include "Scene/Scene_Editor.h"
#include "Manager/Structure_Manager.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Map/MapDataIO.h"
#include "Core/CInput.h"
#include "WintersPaths.h"
#include "GameInstance.h"
#include "ECS/Components/CoreComponents.h"   // ColliderComponent
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "ECS/Systems/MCTSSystem.h"
#include "ECS/Systems/NavigationThrottleSystem.h"
#include "ECS/Systems/YoneSoulSpawnSystem.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/ConcealmentVolumeIndex.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/SpatialIndex.h"
#include "ProfilerAPI.h"
#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Manager/Navigation/MapWalkableBaker.h"
#include "Manager/Navigation/Pathfinder.h"

// [Phase T] UI Panels + DebugDrawSystem
#include "UI/AIDebugPanel.h"
#include "UI/CombatDebugPanel.h"
#include "UI/MapTunerPanel.h"
#include "UI/RenderDebug.h"
#include "UI/DebugDrawSystem.h"
#include "UI/SkillTimingPanel.h"
#include "UI/ChampionTuner.h"
#include "UI/EffectTuner.h"
#include "UI/WfxEffectToolPanel.h"
#include "UI/MinimapPanel.h"
#include "Network/Client/NetworkEventTrace.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"

#include "Resource/Animator.h"
#include "Resource/Animation.h"

#include "GameObject/ChampionDef.h"
#include "GameObject/Champion/Zed/ZedFxPresets.h"
#include "GameObject/Champion/Annie/Annie_Components.h"
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Irelia/Irelia_Skills.h"
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Kalista/Kalista_Skills.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "GameObject/Champion/Yasuo/Yasuo_Tuning.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "GameObject/ChampionSpawnService.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionModuleBootstrap.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/SkillDefVisualDataAdapter.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Dev/SmokeLog.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/PoseStateComponent.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/ReplicatedStateComponent.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/SkillDefGameDataAdapter.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/LobbyTypes_generated.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

// [Phase T-8] FX / Status / Irelia Blade / Ult Wave
#include "ECS/Systems/StatusEffectSystem.h"
#include "Shared/GameSim/Components/GameplayComponents.h"   // Stun/Slow/Disarm
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "Renderer/FxStaticMeshRenderer.h"

#include "ECS/Components/MeshGroupVisibilityComponent.h"

#include "RHI/IRHIDevice.h"
#include "RHI/RHITextureLoader.h"
#include "RHI/RHITypes.h"
#include "Renderer/FogOfWarRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <functional>
#include <vector>

namespace
{
    constexpr f32_t kMoveTargetMaxSurfaceDeltaY = 3.f;

    std::vector<CNavGrid::Cell> SmoothClientMovePathCells(
        const CNavGrid& navGrid,
        const std::vector<CNavGrid::Cell>& path)
    {
        if (path.size() <= 2)
            return path;

        std::vector<CNavGrid::Cell> smoothed{};
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

    bool_t IsFacingCandidateOpposedToIntent(
        const Vec3& origin,
        const Vec3& intentTarget,
        const Vec3& candidate)
    {
        const Vec3 intent{
            intentTarget.x - origin.x,
            0.f,
            intentTarget.z - origin.z
        };
        const Vec3 candidateDir{
            candidate.x - origin.x,
            0.f,
            candidate.z - origin.z
        };
        const f32_t intentLenSq = intent.x * intent.x + intent.z * intent.z;
        const f32_t candidateLenSq =
            candidateDir.x * candidateDir.x + candidateDir.z * candidateDir.z;
        if (intentLenSq <= 0.0001f || candidateLenSq <= 0.0001f)
            return false;

        const f32_t dot = intent.x * candidateDir.x + intent.z * candidateDir.z;
        const f32_t minDot = -0.10f * std::sqrt(intentLenSq * candidateLenSq);
        return dot < minDot;
    }
}

void CScene_InGame::InitializeMapSurfaceSampler(bool_t bMapLoaded, const wchar_t* pSurfaceMeshPath)
{
    m_pMapSurfaceSampler.reset();
    if (!bMapLoaded)
        return;

    unique_ptr<Engine::CMapSurfaceSampler> sampler(new Engine::CMapSurfaceSampler());
    wchar_t surfacePath[MAX_PATH]{};
    const wchar_t* pPath = (pSurfaceMeshPath && pSurfaceMeshPath[0] != L'\0')
        ? pSurfaceMeshPath
        : L"Texture/MAP/output/sr_base_flip.wmesh";
    if (!WintersResolveContentPath(
        pPath,
        surfacePath,
        MAX_PATH) ||
        !sampler->LoadFromWMesh(surfacePath, m_MapTransform.GetWorldMatrix()))
    {
        return;
    }

    m_pMapSurfaceSampler = std::move(sampler);
}

unique_ptr<CNavGrid> CScene_InGame::CreateMapNavGrid() const
{
    const f32_t gridWorldX = CNavGrid::kCellCountX * CNavGrid::kCellSize;
    const f32_t gridWorldZ = CNavGrid::kCellCountY * CNavGrid::kCellSize;
    f32_t centerX = 0.f;
    f32_t centerZ = 0.f;

    if (m_pMapSurfaceSampler && m_pMapSurfaceSampler->IsReady())
    {
        centerX = (m_pMapSurfaceSampler->GetMinX() + m_pMapSurfaceSampler->GetMaxX()) * 0.5f;
        centerZ = (m_pMapSurfaceSampler->GetMinZ() + m_pMapSurfaceSampler->GetMaxZ()) * 0.5f;
    }

    return CNavGrid::Create(
        centerX - gridWorldX * 0.5f,
        centerZ - gridWorldZ * 0.5f);
}

void CScene_InGame::BakeMapWalkableNavGrid()
{
    if (!m_pMapSurfaceSampler || !m_pNavGrid)
        return;

    std::vector<Vec3> seeds{};
    seeds.reserve(64);

    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<TransformComponent>(m_PlayerEntity))
    {
        seeds.push_back(m_World.GetComponent<TransformComponent>(m_PlayerEntity).GetPosition());
    }

    m_World.ForEach<ChampionComponent, TransformComponent>(
        function<void(EntityID, ChampionComponent&, TransformComponent&)>(
            [&](EntityID, ChampionComponent&, TransformComponent& tf)
            {
                seeds.push_back(tf.GetPosition());
            }));

    for (u32_t team = 0; team < 2u; ++team)
    {
        for (u32_t lane = 0; lane < 3u; ++lane)
        {
            const Vec3* pWaypoints = nullptr;
            u32_t count = 0;
            CMinion_Manager::GetWayPoints(
                static_cast<eMinionTeam>(team),
                static_cast<eMinionWay>(lane),
                &pWaypoints,
                &count);

            if (!pWaypoints || count == 0)
                continue;

            for (u32_t i = 0; i < count; ++i)
                seeds.push_back(pWaypoints[i]);
        }
    }

    if (seeds.empty())
        seeds.push_back(Vec3{ 0.f, 0.f, 0.f });

    Engine::MapWalkableBakeDesc desc{};
    desc.playableBaseY = m_fNavPlayableBaseY;
    desc.playableHeightBand = m_fNavPlayableHeightBand;
    desc.minNormalY = m_fNavMinNormalY;
    desc.maxStepHeight = m_fNavMaxStepHeight;

    const bool_t bBaked = Engine::CMapWalkableBaker::BakeIntoNavGrid(
        *m_pMapSurfaceSampler,
        *m_pNavGrid,
        seeds,
        desc);

}

void CScene_InGame::RebuildMapWalkableNavGridForDebug()
{
    wchar_t navGridPath[MAX_PATH] = {};
    if (CMapDataIO::GetNavGridPathW(1, navGridPath, MAX_PATH))
        m_pNavGrid = Engine::CNavGrid::LoadFromFile(navGridPath);

    if (!m_pNavGrid)
    {
        m_pNavGrid = CreateMapNavGrid();
        m_pNavGrid->SetAllWalkable(true);
    }

    Mark_StructuresOnNavGrid();
}

void CScene_InGame::RebuildClientPathNavGrid()
{
    if (!m_pNavGrid)
    {
        m_pPathNavGrid.reset();
        return;
    }

    m_pPathNavGrid = m_pNavGrid->BuildInflated(0.5f);
    if (!m_pPathNavGrid)
    {
        return;
    }

    CPathfinder::PrewarmReachabilityCache(m_pPathNavGrid.get());

}

bool_t CScene_InGame::TryProjectToMapSurface(Vec3& ioPos, f32_t fYOffset) const
{
    if (!m_pMapSurfaceSampler)
        return false;

    f32_t height = 0.f;
    if (!m_pMapSurfaceSampler->SampleHeight(ioPos.x, ioPos.z, height))
        return false;

    ioPos.y = height + fYOffset;
    return true;
}

bool_t CScene_InGame::TryResolveNearestWalkablePosition(
    const Vec3& rawPos,
    Vec3& outPos,
    int32_t maxRadius) const
{
    const CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid)
    {
        outPos = rawPos;
        return true;
    }

    const CNavGrid::Cell cell = pGrid->WorldToCell(rawPos);
    if (pGrid->IsWalkable(cell.x, cell.y))
    {
        outPos = rawPos;
        (void)TryProjectToMapSurface(outPos, 0.05f);
        return true;
    }

    CNavGrid::Cell nearest{};
    if (!pGrid->TryFindNearestWalkableCell(cell, maxRadius, nearest))
        return false;

    outPos = pGrid->CellToWorld(nearest.x, nearest.y);
    if (!TryProjectToMapSurface(outPos, 0.05f))
        outPos.y = rawPos.y;


    return true;
}

bool_t CScene_InGame::TryResolveWalkableMoveTarget(
    const Vec3& rawTarget,
    Vec3& outTarget,
    Vec3* pOutFirstWaypoint) const
{
    const CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid || !m_pPlayerTransform)
        return false;

    const Vec3 playerPos = m_pPlayerTransform->GetPosition();
    CNavGrid::Cell start = pGrid->WorldToCell(playerPos);
    const CNavGrid::Cell rawGoal = pGrid->WorldToCell(rawTarget);

    auto ProjectMoveTarget = [&](Vec3& ioTarget)
        {
            if (!TryProjectToMapSurface(ioTarget, 0.05f))
            {
                ioTarget.y = playerPos.y;
                return;
            }

            const f32_t surfaceDeltaY = ioTarget.y - playerPos.y;
            if (std::fabs(surfaceDeltaY) <= kMoveTargetMaxSurfaceDeltaY)
                return;

            ioTarget.y = playerPos.y;
        };

    if (!pGrid->IsWalkable(start.x, start.y))
    {
        CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 8, nearestStart))
            return false;
        start = nearestStart;
    }

    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
        return false;

    const bool_t bRawGoalWalkable = pGrid->IsWalkable(rawGoal.x, rawGoal.y);
    const bool_t bRawSegmentWalkable = pGrid->SegmentWalkable(playerPos, rawTarget, 0.f);

    if (bRawGoalWalkable && bRawSegmentWalkable)
    {
        outTarget = rawTarget;
        ProjectMoveTarget(outTarget);
        if (pOutFirstWaypoint)
            *pOutFirstWaypoint = outTarget;
        return true;
    }

    CNavGrid::Cell resolved{};
    std::vector<CNavGrid::Cell> path{};
    if (!CPathfinder::TryFindNearestReachableGoal(
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
    ProjectMoveTarget(outTarget);

    if (pOutFirstWaypoint)
    {
        *pOutFirstWaypoint = outTarget;
        const std::vector<CNavGrid::Cell> smoothedPath =
            SmoothClientMovePathCells(*pGrid, path);
        if (smoothedPath.size() > 1)
        {
            Vec3 waypoint = pGrid->CellToWorld(
                smoothedPath[1].x,
                smoothedPath[1].y);
            ProjectMoveTarget(waypoint);

            Vec3 intentFacingTarget = rawTarget;
            ProjectMoveTarget(intentFacingTarget);
            const bool_t bFirstWaypointOpposed = IsFacingCandidateOpposedToIntent(
                playerPos,
                intentFacingTarget,
                waypoint);
            *pOutFirstWaypoint = bFirstWaypointOpposed ? intentFacingTarget : waypoint;
        }
    }


    return true;
}

bool_t CScene_InGame::IsWalkableMoveSegment(const Vec3& from, const Vec3& to, f32_t radiusWorld) const
{
    const CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid)
        return true;

    return pGrid->SegmentWalkable(from, to, m_pPathNavGrid ? 0.f : radiusWorld);
}

Vec3 CScene_InGame::ResolveMouseMapSurfacePos() const
{
    if (!m_pCamera)
        return Vec3{};

    const auto& input = CInput::Get();
    const i32_t screenW = static_cast<i32_t>(g_iWinSizeX);
    const i32_t screenH = static_cast<i32_t>(g_iWinSizeY);
    const CInput::MouseRay ray =
        input.GetMouseWorldRay(*m_pCamera, screenW, screenH);
    Vec3 ground = input.GetMouseGroundPos(*m_pCamera, screenW, screenH);
    bool_t bProjected = false;

    if (m_pMapSurfaceSampler && m_pMapSurfaceSampler->IsReady())
    {
        if (std::fabs(ray.Dir.y) > 0.0001f)
        {
            f32_t t = -ray.Origin.y / ray.Dir.y;
            bool_t bSurfaceHit = std::isfinite(t) && t >= 0.f;
            f32_t height = 0.f;

            for (u32_t i = 0; bSurfaceHit && i < 6u; ++i)
            {
                const Vec3 p{
                    ray.Origin.x + ray.Dir.x * t,
                    ray.Origin.y + ray.Dir.y * t,
                    ray.Origin.z + ray.Dir.z * t
                };
                if (!m_pMapSurfaceSampler->SampleHeight(p.x, p.z, height))
                {
                    bSurfaceHit = false;
                    break;
                }

                const f32_t nextT = (height - ray.Origin.y) / ray.Dir.y;
                if (!std::isfinite(nextT) || nextT < 0.f)
                {
                    bSurfaceHit = false;
                    break;
                }

                if (std::fabs(nextT - t) <= 0.001f)
                {
                    t = nextT;
                    break;
                }
                t = nextT;
            }

            if (bSurfaceHit)
            {
                const Vec3 p{
                    ray.Origin.x + ray.Dir.x * t,
                    ray.Origin.y + ray.Dir.y * t,
                    ray.Origin.z + ray.Dir.z * t
                };
                if (m_pMapSurfaceSampler->SampleHeight(p.x, p.z, height))
                {
                    ground = { p.x, height, p.z };
                    bProjected = true;
                }
            }
        }
    }

    if (!bProjected)
        bProjected = TryProjectToMapSurface(ground, 0.f);

    return ground;
}

void CScene_InGame::ProjectGameplayActorsToMapSurface()
{
    if (!m_pMapSurfaceSampler)
        return;

    m_World.ForEach<ChampionComponent, TransformComponent>(
        function<void(EntityID, ChampionComponent&, TransformComponent&)>(
            [&](EntityID entity, ChampionComponent&, TransformComponent& tf)
            {
                Vec3 pos = tf.GetPosition();
                if (!TryProjectToMapSurface(pos, 0.05f))
                    return;

                tf.SetPosition(pos);
                if (entity == m_PlayerEntity)
                {
                    SetPlayerPosition(pos);
                    m_vPlayerDest.y = pos.y;
                }
            }));

    m_World.ForEach<MinionStateComponent, TransformComponent>(
        function<void(EntityID, MinionStateComponent&, TransformComponent&)>(
            [&](EntityID, MinionStateComponent& state, TransformComponent& tf)
            {
                if (state.current == MinionStateComponent::Dead)
                    return;

                Vec3 pos = tf.GetPosition();
                if (TryProjectToMapSurface(pos, 0.02f))
                    tf.SetPosition(pos);
            }));
}

void CScene_InGame::Mark_StructuresOnNavGrid()
{
    if (!m_pNavGrid)
        return;
    const uint32_t iCount = CStructure_Manager::Get()->Get_Count();
    for (uint32_t i = 0; i < iCount; ++i)
    {
        TransformComponent* pTf = CStructure_Manager::Get()->Get_Transform(i);
        if (!pTf)
            continue;
        const Vec3 vPos = pTf->GetLocalPosition();
        f32_t radius = 2.f;
        EntityID entity = CStructure_Manager::Get()->Get_EntityAt(i);
        if (m_World.HasComponent<StructureComponent>(entity))
        {
            auto& sc = m_World.GetComponent<StructureComponent>(entity);
            if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Nexus))
                radius = 4.f;
            else if (sc.kind == static_cast<uint32_t>(Winters::Map::eObjectKind::Structure_Inhibitor))
                radius = 3.f;
        }
        const CNavGrid::Cell center = m_pNavGrid->WorldToCell(vPos);
        const int32_t rCells = static_cast<int32_t>(radius / CNavGrid::kCellSize);
        for (int32_t dy = -rCells; dy <= rCells; ++dy)
             {
            for (int32_t dx = -rCells; dx <= rCells; ++dx)
            {
                if (dx * dx + dy * dy <= rCells * rCells)
                    m_pNavGrid->SetWalkable(center.x + dx, center.y + dy, false);
            }
        }
    }
    RebuildClientPathNavGrid();
}
