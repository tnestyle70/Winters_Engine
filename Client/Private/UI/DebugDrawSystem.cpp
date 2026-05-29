#include "UI/DebugDrawSystem.h"
#include "Scene/Scene_InGame.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/CoreComponents.h"  // ColliderComponent (vOffset / vHalfExtents)
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "Manager/Structure_Manager.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Navigation/NavGrid.h"
#include "DynamicCamera.h"
#include "EngineConfig.h"                   // g_iWinSizeX / g_iWinSizeY
#include "WintersMath.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <algorithm>
#include <cmath>
#include <cstdio>

// ─────────────────────────────────────────────────────────────
//  TU 전용 Debug 헬퍼 (Scene_InGame.cpp L1156-1222 에서 이관)
// ─────────────────────────────────────────────────────────────
namespace
{
    bool WorldToScreen(const DirectX::XMMATRIX& mVP, const Vec3& w, ImVec2& out)
    {
        DirectX::XMVECTOR v = DirectX::XMVectorSet(w.x, w.y, w.z, 1.f);
        v = DirectX::XMVector4Transform(v, mVP);
        const f32_t wComp = DirectX::XMVectorGetW(v);
        if (wComp <= 0.01f) return false;
        const f32_t nx = DirectX::XMVectorGetX(v) / wComp;
        const f32_t ny = DirectX::XMVectorGetY(v) / wComp;
        out.x = (nx * 0.5f + 0.5f) * static_cast<f32_t>(g_iWinSizeX);
        out.y = (1.f - (ny * 0.5f + 0.5f)) * static_cast<f32_t>(g_iWinSizeY);
        return true;
    }

    void DrawWireBox(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP,
        const Vec3& center, const Vec3& halfExt, ImU32 col, f32_t thickness = 1.5f)
    {
        Vec3 c[8] = {
            {center.x - halfExt.x, center.y - halfExt.y, center.z - halfExt.z},
            {center.x + halfExt.x, center.y - halfExt.y, center.z - halfExt.z},
            {center.x + halfExt.x, center.y - halfExt.y, center.z + halfExt.z},
            {center.x - halfExt.x, center.y - halfExt.y, center.z + halfExt.z},
            {center.x - halfExt.x, center.y + halfExt.y, center.z - halfExt.z},
            {center.x + halfExt.x, center.y + halfExt.y, center.z - halfExt.z},
            {center.x + halfExt.x, center.y + halfExt.y, center.z + halfExt.z},
            {center.x - halfExt.x, center.y + halfExt.y, center.z + halfExt.z},
        };
        ImVec2 s[8]; bool ok[8];
        for (int i = 0; i < 8; ++i) ok[i] = WorldToScreen(mVP, c[i], s[i]);
        auto L = [&](int a, int b) { if (ok[a] && ok[b]) pDraw->AddLine(s[a], s[b], col, thickness); };
        L(0, 1); L(1, 2); L(2, 3); L(3, 0);
        L(4, 5); L(5, 6); L(6, 7); L(7, 4);
        L(0, 4); L(1, 5); L(2, 6); L(3, 7);
    }

    void DrawWireCylinder(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP,
        const Vec3& base, f32_t r, f32_t h, ImU32 col, int segments = 24)
    {
        const f32_t step = WintersMath::kTwoPi / segments;
        ImVec2 botPts[48], mIDPts[48], topPts[48];
        bool   botOk[48], mIDOk[48], topOk[48];
        for (int i = 0; i < segments; ++i)
        {
            const f32_t a = step * i;
            const f32_t cx = base.x + cosf(a) * r;
            const f32_t cz = base.z + sinf(a) * r;
            botOk[i] = WorldToScreen(mVP, { cx, base.y,           cz }, botPts[i]);
            mIDOk[i] = WorldToScreen(mVP, { cx, base.y + h * 0.5f, cz }, mIDPts[i]);
            topOk[i] = WorldToScreen(mVP, { cx, base.y + h,       cz }, topPts[i]);
        }
        for (int i = 0; i < segments; ++i)
        {
            const int j = (i + 1) % segments;
            if (botOk[i] && botOk[j]) pDraw->AddLine(botPts[i], botPts[j], col, 1.5f);
            if (mIDOk[i] && mIDOk[j]) pDraw->AddLine(mIDPts[i], mIDPts[j], col, 1.0f);
            if (topOk[i] && topOk[j]) pDraw->AddLine(topPts[i], topPts[j], col, 1.5f);
        }
        for (int k = 0; k < 4; ++k)
        {
            const int i = (segments / 4) * k;
            if (botOk[i] && topOk[i]) pDraw->AddLine(botPts[i], topPts[i], col, 1.5f);
        }
    }

    bool DrawWorldLine(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP,
        const Vec3& a, const Vec3& b, ImU32 col, f32_t thickness = 2.f)
    {
        ImVec2 sa{}, sb{};
        if (!WorldToScreen(mVP, a, sa) || !WorldToScreen(mVP, b, sb))
            return false;

        pDraw->AddLine(sa, sb, col, thickness);
        return true;
    }

    Vec3 DebugMinionForwardFromYaw(f32_t fYaw)
    {
        return Vec3{ -sinf(fYaw), 0.f, -cosf(fYaw) };
    }

    Vec3 NormalizeDebugXZOrFallback(const Vec3& v, const Vec3& fallback)
    {
        const f32_t fLenSq = v.x * v.x + v.z * v.z;
        if (fLenSq <= 0.0001f)
            return fallback;

        const f32_t fInvLen = 1.f / std::sqrt(fLenSq);
        return Vec3{ v.x * fInvLen, 0.f, v.z * fInvLen };
    }

    ImU32 MinionDebugColor(eTeam team)
    {
        switch (team)
        {
        case eTeam::Blue:    return 0xFFFFAA40u;
        case eTeam::Red:     return 0xFF5050FFu;
        case eTeam::Neutral: return 0xFFC8C8C8u;
        default:             return 0xFFC8C8C8u;
        }
    }
}

namespace UI
{
    void CDebugDrawSystem::Render(CWorld& world, CScene_InGame* pScene, const Mat4& matVP)
    {
        if (!pScene || !pScene->IsShowRenderDebug()) return;

        ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
        if (!pDraw) return;

        const DirectX::XMMATRIX mVP = matVP.ToXMMATRIX();
        if (pScene->IsDbgShowNavGrid())    DrawNavGrid(world, pScene, pDraw, mVP);
        if (pScene->IsDbgShowPathNavGrid()) DrawPathNavGrid(world, pScene, pDraw, mVP);
        if (pScene->IsDbgShowStructures()) DrawStructures(world, pScene, pDraw, mVP);
        if (pScene->IsDbgShowColliders())  DrawColliders(world, pScene, pDraw, mVP);
        if (pScene->IsDbgShowChampions())  DrawChampions(world, pScene, pDraw, mVP);
        if (pScene->IsDbgShowMinionMovement()) DrawMinionMovement(world, pScene, pDraw, mVP);
    }

    // R-8: GetOrigin() 없음 → Get_OriginX/Z 분리 + int32_t
    void CDebugDrawSystem::DrawNavGrid(CWorld&, CScene_InGame* pScene,
        ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
    {
        const CNavGrid* pGrid = pScene->GetNavGrid();
        const CDynamicCamera* pCam = pScene->GetCameraPtr();
        if (!pGrid || !pCam) return;

        const Vec3 camPos = pCam->GetEye();
        const f32_t radius = pScene->GetDbgNavRadius();
        const f32_t cs = CNavGrid::kCellSize;
        const f32_t ox = pGrid->Get_OriginX();
        const f32_t oz = pGrid->Get_OriginZ();

        const int32_t minIX = (int32_t)((camPos.x - radius - ox) / cs);
        const int32_t maxIX = (int32_t)((camPos.x + radius - ox) / cs);
        const int32_t minIY = (int32_t)((camPos.z - radius - oz) / cs);
        const int32_t maxIY = (int32_t)((camPos.z + radius - oz) / cs);

        for (int32_t iy = minIY; iy <= maxIY; ++iy)
            for (int32_t ix = minIX; ix <= maxIX; ++ix)
            {
                if (ix < 0 || iy < 0
                    || ix >= (int32_t)CNavGrid::kCellCountX
                    || iy >= (int32_t)CNavGrid::kCellCountY) continue;
                if (pGrid->IsWalkable(ix, iy)) continue;
                const f32_t x = ox + (ix + 0.5f) * cs;
                const f32_t z = oz + (iy + 0.5f) * cs;
                DrawWireBox(pDraw, mVP, { x, 0.1f, z }, { cs * 0.5f, 0.1f, cs * 0.5f }, 0x8000FFFFu, 1.f);
            }
    }

    void CDebugDrawSystem::DrawPathNavGrid(CWorld&, CScene_InGame* pScene,
        ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
    {
        const CNavGrid* pGrid = pScene->GetPathNavGrid();
        const CDynamicCamera* pCam = pScene->GetCameraPtr();
        if (!pGrid || !pCam) return;

        const Vec3 camPos = pCam->GetEye();
        const f32_t radius = pScene->GetDbgNavRadius();
        const f32_t cs = CNavGrid::kCellSize;
        const f32_t ox = pGrid->Get_OriginX();
        const f32_t oz = pGrid->Get_OriginZ();

        const int32_t minIX = (int32_t)((camPos.x - radius - ox) / cs);
        const int32_t maxIX = (int32_t)((camPos.x + radius - ox) / cs);
        const int32_t minIY = (int32_t)((camPos.z - radius - oz) / cs);
        const int32_t maxIY = (int32_t)((camPos.z + radius - oz) / cs);

        for (int32_t iy = minIY; iy <= maxIY; ++iy)
            for (int32_t ix = minIX; ix <= maxIX; ++ix)
            {
                if (ix < 0 || iy < 0
                    || ix >= (int32_t)CNavGrid::kCellCountX
                    || iy >= (int32_t)CNavGrid::kCellCountY) continue;
                if (pGrid->IsWalkable(ix, iy)) continue;
                const f32_t x = ox + (ix + 0.5f) * cs;
                const f32_t z = oz + (iy + 0.5f) * cs;
                DrawWireBox(pDraw, mVP, { x, 0.18f, z }, { cs * 0.5f, 0.1f, cs * 0.5f }, 0x800080FFu, 1.25f);
            }
    }

    void CDebugDrawSystem::DrawStructures(CWorld&, CScene_InGame*,
        ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
    {
        const uint32_t iCount = CStructure_Manager::Get()->Get_Count();
        for (uint32_t i = 0; i < iCount; ++i)
        {
            TransformComponent* pTf = CStructure_Manager::Get()->Get_Transform(i);
            if (!pTf) continue;
            const Vec3 p = pTf->GetLocalPosition();
            DrawWireBox(pDraw, mVP, { p.x, 1.0f, p.z }, { 1.5f, 1.0f, 1.5f }, 0xFFFF8000u, 1.5f);
        }
        const uint32_t jCount = CJungle_Manager::Get()->Get_Count();
        for (uint32_t i = 0; i < jCount; ++i)
        {
            TransformComponent* pTf = CJungle_Manager::Get()->Get_Transform(i);
            if (!pTf) continue;
            const Vec3 p = pTf->GetLocalPosition();
            DrawWireCylinder(pDraw, mVP, { p.x, p.y, p.z }, 1.0f, 2.0f, 0xFF00FF00u);
        }
    }

    // R-9, R-10: col.vOffset / col.vHalfExtents
    void CDebugDrawSystem::DrawColliders(CWorld& world, CScene_InGame*,
        ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
    {
        world.ForEach<TransformComponent, ColliderComponent>(
            [&](EntityID, TransformComponent& tf, ColliderComponent& col)
            {
                const Vec3 c{
                    tf.m_LocalPosition.x + col.vOffset.x,
                    tf.m_LocalPosition.y + col.vOffset.y,
                    tf.m_LocalPosition.z + col.vOffset.z
                };
                DrawWireBox(pDraw, mVP, c, col.vHalfExtents, 0xFFFFFFFFu, 1.f);
            });
    }
    void CDebugDrawSystem::DrawChampions(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
    {
        constexpr f32_t kRadius = 1.2f;
        constexpr f32_t kHeight = 3.0f;

        w.ForEach<ChampionComponent, TransformComponent>(
            [&](EntityID /*e*/, ChampionComponent& cc, TransformComponent& tf)
            {
                const Vec3& vC = tf.m_LocalPosition;
                const Vec3 vBase{ vC.x, vC.y - kHeight * 0.5f, vC.z };

                ImU32 col;
                switch (cc.team)
                {
                case eTeam::Blue:    col = 0xFFFF8C3Cu; break; // RGB(60,140,255)
                case eTeam::Red:     col = 0xFF4646FFu; break; // RGB(255,70,70)
                case eTeam::Neutral: col = 0xFFC8C8C8u; break; // RGB(200,200,200)
                default:             col = 0xFFC8C8C8u; break;
                }

                DrawWireCylinder(pDraw, mVP, vBase, kRadius, kHeight, col);
            });
    }

    void CDebugDrawSystem::DrawMinionMovement(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
    {
        const CNavGrid* pGrid = s->GetPathNavGrid() ? s->GetPathNavGrid() : s->GetNavGrid();
        constexpr f32_t kDefaultRadius = 0.5f;
        constexpr f32_t kHeight = 1.4f;

        w.ForEach<MinionStateComponent, TransformComponent>(
            [&](EntityID entity, MinionStateComponent& state, TransformComponent& tf)
            {
                const Vec3 vPos = tf.GetPosition();
                const ImU32 col = MinionDebugColor(state.team);
                const f32_t fRadius = w.HasComponent<SpatialAgentComponent>(entity)
                    ? (std::max)(kDefaultRadius, w.GetComponent<SpatialAgentComponent>(entity).radius)
                    : kDefaultRadius;

                DrawWireCylinder(
                    pDraw,
                    mVP,
                    Vec3{ vPos.x, vPos.y, vPos.z },
                    fRadius,
                    kHeight,
                    col,
                    16);

                CNavGrid::Cell currentCell{ -1, -1 };
                if (pGrid)
                {
                    currentCell = pGrid->WorldToCell(vPos);
                    if (pGrid->IsInBounds(currentCell.x, currentCell.y))
                    {
                        const Vec3 vCellCenter = pGrid->CellToWorld(currentCell.x, currentCell.y);
                        const ImU32 cellCol = pGrid->IsWalkable(currentCell.x, currentCell.y)
                            ? 0xA000FF80u
                            : 0xA00000FFu;
                        DrawWireBox(
                            pDraw,
                            mVP,
                            Vec3{ vCellCenter.x, vPos.y + 0.08f, vCellCenter.z },
                            Vec3{ CNavGrid::kCellSize * 0.5f, 0.08f, CNavGrid::kCellSize * 0.5f },
                            cellCol,
                            1.75f);
                    }
                }

                const Vec3 vForward = DebugMinionForwardFromYaw(tf.GetRotation().y);
                const bool_t bHasWaypoint =
                    state.PathCount > 0u &&
                    state.PathIndex < state.PathCount;
                const Vec3 vPathDir = bHasWaypoint
                    ? Vec3{
                        state.PathWaypoints[state.PathIndex].x - vPos.x,
                        0.f,
                        state.PathWaypoints[state.PathIndex].z - vPos.z }
                    : Vec3{};
                const Vec3 vDir = NormalizeDebugXZOrFallback(vPathDir, vForward);

                const Vec3 vLineStart{ vPos.x, vPos.y + 0.75f, vPos.z };
                const Vec3 vLineEnd{
                    vLineStart.x + vDir.x * 1.6f,
                    vLineStart.y,
                    vLineStart.z + vDir.z * 1.6f
                };
                DrawWorldLine(pDraw, mVP, vLineStart, vLineEnd, 0xFF00FFFFu, 2.5f);

                if (pGrid)
                {
                    const Vec3 vProbe{
                        vPos.x + vDir.x * CNavGrid::kCellSize * 2.f,
                        vPos.y,
                        vPos.z + vDir.z * CNavGrid::kCellSize * 2.f
                    };
                    const CNavGrid::Cell nextCell = pGrid->WorldToCell(vProbe);
                    if (pGrid->IsInBounds(nextCell.x, nextCell.y))
                    {
                        const Vec3 vNextCenter = pGrid->CellToWorld(nextCell.x, nextCell.y);
                        DrawWireBox(
                            pDraw,
                            mVP,
                            Vec3{ vNextCenter.x, vPos.y + 0.2f, vNextCenter.z },
                            Vec3{ CNavGrid::kCellSize * 0.5f, 0.12f, CNavGrid::kCellSize * 0.5f },
                            0xFF00FFFFu,
                            1.5f);
                    }
                }

                if (bHasWaypoint)
                {
                    const Vec3 vWaypoint = state.PathWaypoints[state.PathIndex];
                    DrawWorldLine(
                        pDraw,
                        mVP,
                        Vec3{ vPos.x, vPos.y + 1.05f, vPos.z },
                        Vec3{ vWaypoint.x, vWaypoint.y + 1.05f, vWaypoint.z },
                        0xFFFF00FFu,
                        1.8f);
                }

                ImVec2 labelPos{};
                if (WorldToScreen(mVP, Vec3{ vPos.x, vPos.y + 2.05f, vPos.z }, labelPos))
                {
                    char label[128]{};
                    sprintf_s(
                        label,
                        "m%u cell=%d,%d path=%u/%u block=%u",
                        static_cast<u32_t>(entity),
                        currentCell.x,
                        currentCell.y,
                        static_cast<u32_t>(state.PathIndex),
                        static_cast<u32_t>(state.PathCount),
                        static_cast<u32_t>(state.BlockedMoveFrames));
                    pDraw->AddText(labelPos, 0xFFFFFFFFu, label);
                }
            });
    }
}
