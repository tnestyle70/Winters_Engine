#include "Game/GameRoom.h"

#include "GameRoomInternal.h"
#include "GameRoomSmokeRoster.h"

#include "Game/ServerMinionTuning.h"
#include "Shared/GameSim/Components/RespawnComponent.h"

#include "Shared/GameSim/Components/WaypointPatrolComponent.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Definitions/StageData.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"

#include "ECS/Components/SpatialAgentComponent.h"
#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Manager/Navigation/MapWalkableBaker.h"
#include "Manager/Navigation/NavGrid.h"
#include "Manager/Navigation/Pathfinder.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>
#include <vector>

namespace
{
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

    constexpr f32_t kServerNavGridStageBoundsPadding = 4.f;
    constexpr int32_t kServerNavGridSeedCoverageRadius = 24;

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

        const LobbySlotState* pLobbySlots = GetLobbySlots();
        const u32_t lobbySlotCount = GetLobbySlotCount();
        for (u32_t i = 0; pLobbySlots && i < lobbySlotCount; ++i)
        {
            const LobbySlotState& slot = pLobbySlots[i];
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

    const LobbySlotState* pLobbySlots = GetLobbySlots();
    const u32_t lobbySlotCount = GetLobbySlotCount();
    for (u32_t i = 0; pLobbySlots && i < lobbySlotCount; ++i)
    {
        const LobbySlotState& slot = pLobbySlots[i];
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
