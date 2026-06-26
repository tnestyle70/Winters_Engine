#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include "Scene/Scene_Editor.h"
#include "Scene/Scene_InGame.h"
#include "Manager/Structure_Manager.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Manager/Bush_Manager.h"
#include "Core/CInput.h"
#include "WintersPaths.h"
#include "GameInstance.h"
#include "Map/MapDataIO.h"
#include <cmath>

namespace
{
    namespace WMap = Winters::Map;

    struct NavGridStageBounds
    {
        bool_t bHasPoint = false;
        f32_t fMinX = 0.f;
        f32_t fMaxX = 0.f;
        f32_t fMinZ = 0.f;
        f32_t fMaxZ = 0.f;
    };

    void IncludeNavGridBoundsPoint(NavGridStageBounds& bounds, const Vec3& pos)
    {
        if (!bounds.bHasPoint)
        {
            bounds.bHasPoint = true;
            bounds.fMinX = bounds.fMaxX = pos.x;
            bounds.fMinZ = bounds.fMaxZ = pos.z;
            return;
        }

        if (pos.x < bounds.fMinX) bounds.fMinX = pos.x;
        if (pos.x > bounds.fMaxX) bounds.fMaxX = pos.x;
        if (pos.z < bounds.fMinZ) bounds.fMinZ = pos.z;
        if (pos.z > bounds.fMaxZ) bounds.fMaxZ = pos.z;
    }

    f32_t SnapDownToNavCell(f32_t value)
    {
        return std::floor(value / Engine::CNavGrid::kCellSize) * Engine::CNavGrid::kCellSize;
    }

    unique_ptr<Engine::CNavGrid> CreateStageBoundsNavGrid()
    {
        NavGridStageBounds bounds{};

        const uint32_t structureCount = CStructure_Manager::Get()->Get_Count();
        for (uint32_t i = 0; i < structureCount; ++i)
        {
            if (TransformComponent* pTf = CStructure_Manager::Get()->Get_Transform(i))
                IncludeNavGridBoundsPoint(bounds, pTf->GetPosition());
        }

        const uint32_t jungleCount = CJungle_Manager::Get()->Get_Count();
        for (uint32_t i = 0; i < jungleCount; ++i)
        {
            if (TransformComponent* pTf = CJungle_Manager::Get()->Get_Transform(i))
                IncludeNavGridBoundsPoint(bounds, pTf->GetPosition());
        }

        for (uint32_t team = 0; team < static_cast<uint32_t>(eMinionTeam::End); ++team)
        {
            for (uint32_t lane = 0; lane < static_cast<uint32_t>(eMinionWay::End); ++lane)
            {
                const eMinionTeam minionTeam = static_cast<eMinionTeam>(team);
                const eMinionWay minionLane = static_cast<eMinionWay>(lane);
                const uint32_t wpCount = CMinion_Manager::Get()->Get_WaypointCount(minionTeam, minionLane);
                for (uint32_t i = 0; i < wpCount; ++i)
                {
                    if (const Vec3* pWP = CMinion_Manager::Get()->Get_WaypointPtr(minionTeam, minionLane, i))
                        IncludeNavGridBoundsPoint(bounds, *pWP);
                }
            }
        }

        const f32_t gridWorldX = Engine::CNavGrid::kCellCountX * Engine::CNavGrid::kCellSize;
        const f32_t gridWorldZ = Engine::CNavGrid::kCellCountY * Engine::CNavGrid::kCellSize;

        f32_t originX = -gridWorldX * 0.5f;
        f32_t originZ = -gridWorldZ * 0.5f;

        if (bounds.bHasPoint)
        {
            const f32_t centerX = (bounds.fMinX + bounds.fMaxX) * 0.5f;
            const f32_t centerZ = (bounds.fMinZ + bounds.fMaxZ) * 0.5f;
            originX = SnapDownToNavCell(centerX - gridWorldX * 0.5f);
            originZ = SnapDownToNavCell(centerZ - gridWorldZ * 0.5f);
        }

        unique_ptr<Engine::CNavGrid> pNavGrid = Engine::CNavGrid::Create(originX, originZ);
        if (pNavGrid)
            pNavGrid->SetAllWalkable(true);

        return pNavGrid;
    }
}

bool CScene_Editor::OnEnter()
{
    //Transform System 
    m_pTransformSystem = CTransformSystem::Create();

    // ── Manager 싱글턴 초기화 (Editor 전용 CWorld 주입) ──
    CStructure_Manager::Get()->Initialize(&m_World);
    CJungle_Manager::Get()->Initialize(&m_World);
    CMinion_Manager::Get()->Initialize(&m_World);
    CBush_Manager::Get()->Initialize(&m_World);

    // 맵 메시 (InGame 과 동일)
    m_Map.Initialize("Texture/MAP/output/sr_base_flip.wmesh",
        L"Shaders/Mesh3D.hlsl");
    m_MapTransform.SetPosition(0.f, 0.f, 0.f);
    // InGame 과 동일한 X mirror 적용 (lol2gltf --flipX false 무력화 우회).
    m_MapTransform.SetScale({ -0.01f, 0.01f, 0.01f });
    m_MapTransform.SetRotation({ 0.f, DirectX::XMConvertToRadians(-135.f), 0.f });

    // 탑다운 카메라
    m_pCamera = CDynamicCamera::Create(
        { 0.f,20.f, -0.1f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });

    if (m_pCamera) m_pCamera->SetFollowMode(false);

    // 싱글턴 Manager 는 CGameApp 에서 이미 Initialize 됨 → 현재 Stage 자동 로드
    Load_CurrentStage();
    LoadCurrentNavGrid();

    // 에디터는 WP 편집 전용 — 스폰 비활성
    CMinion_Manager::Get()->Set_Enabled(false);
    CMinion_Manager::Get()->Set_EditLane(m_PendingMinionTeam, m_PendingMinionLane);

    return true;
}

void CScene_Editor::OnExit() 
{
    CStructure_Manager::Get()->Shutdown();
    CJungle_Manager::Get()->Shutdown();
    CMinion_Manager::Get()->Shutdown();
    CBush_Manager::Get()->Shutdown();
}

void CScene_Editor::OnUpdate(f32_t dt)
{
    auto& input = CInput::Get();
    if (m_pCamera) m_pCamera->Update(dt, input);

    if (input.IsKeyDown(VK_CONTROL) && input.IsKeyPressed('S'))
    {
        Save_CurrentStage();
        SaveCurrentNavGrid();
    }

    // ESC → Scene 전환은 self-destruct 를 유발 → 즉시 return 필수 (use-after-free 방지)
    if (input.IsKeyPressed(VK_ESCAPE))
    {
        Request_BackToInGame();
        return;
    }

    Handle_MousePlacement();
    CJungle_Manager::Get()->Update(dt);

    //ECS Transform Update
    if (m_pTransformSystem) 
        m_pTransformSystem->Execute(m_World, dt);
}

void CScene_Editor::OnLateUpdate(f32_t /*dt*/) {}

void CScene_Editor::OnRender()
{
    if (!m_pCamera) return;
    const Mat4 matVP = m_pCamera->GetViewProjection();

    m_Map.UpdateCamera(matVP);
    m_Map.UpdateTransform(m_MapTransform.GetWorldMatrix());
    m_Map.Render();

    CStructure_Manager::Get()->Render(matVP);
    CJungle_Manager::Get()->Render(matVP);
    CBush_Manager::Get()->RenderEditorOverlay(
        matVP,
        (m_eSelectedCategory == eCategory::Bush) ? m_iSelectedIndex : -1);
    RenderNavGridOverlay(matVP);

    // 편집 중인 레인의 웨이포인트를 스크린 공간 라인스트립 + 점 + 인덱스로 시각화
    const eMinionTeam t = m_PendingMinionTeam;
    const eMinionWay  l = m_PendingMinionLane;
    const u32_t n = CMinion_Manager::Get()->Get_WaypointCount(t, l);
    if (n > 0)
    {
        ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
        const DirectX::XMMATRIX mVP = matVP.ToXMMATRIX();

        auto WorldToScreen = [&](const Vec3& w, ImVec2& out) -> bool
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
        };

        const ImU32 colLine = (t == eMinionTeam::Blue) ? IM_COL32(80,160,255,220) : IM_COL32(255,100,100,220);
        const ImU32 colDot  = IM_COL32(255,255,0,255);
        ImVec2 prev{}; bool havePrev = false;
        for (u32_t i = 0; i < n; ++i)
        {
            const Vec3* pW = CMinion_Manager::Get()->Get_WaypointPtr(t, l, i);
            if (!pW) continue;
            ImVec2 s{};
            if (!WorldToScreen(*pW, s)) { havePrev = false; continue; }
            if (havePrev) pDraw->AddLine(prev, s, colLine, 2.f);
            pDraw->AddCircleFilled(s, 6.f, colDot);
            char lbl[8]; snprintf(lbl, sizeof(lbl), "%u", i);
            pDraw->AddText(ImVec2(s.x + 8, s.y - 8), IM_COL32_WHITE, lbl);
            prev = s; havePrev = true;
        }
    }
}

void CScene_Editor::OnImGui()
{
    Render_MenuBar();
    Render_Palette();
    Render_Hierarchy();
    Render_Inspector();
}

void CScene_Editor::Render_MenuBar()
{
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save Stage + NavGrid", "Ctrl+S"))
        {
            Save_CurrentStage();
            SaveCurrentNavGrid();
        }
        if (ImGui::MenuItem("Load Stage + NavGrid"))
        {
            Load_CurrentStage();
            LoadCurrentNavGrid();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Back to InGame", "Esc")) Request_BackToInGame();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Stage"))
    {
        for (i32_t i = 1; i <= 8; ++i)
        {
            char label[32];
            sprintf_s(label, "Stage %d%s", i, (i == m_iCurrentStage) ? "  [current]" : "");
            if (ImGui::MenuItem(label))
            {
                if (m_bDirty)
                {
                    Save_CurrentStage();
                    SaveCurrentNavGrid();
                }
                m_iCurrentStage = i;
                Load_CurrentStage();
                LoadCurrentNavGrid();
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::TextDisabled("S:%u J:%u B:%u",
        CStructure_Manager::Get()->Get_Count(),
        CJungle_Manager::Get()->Get_Count(),
        CBush_Manager::Get()->Get_Count());
    ImGui::Separator();
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::EndMainMenuBar();
}

void CScene_Editor::Render_Palette()
{
    ImGui::SetNextWindowPos(ImVec2(0, 22), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Palette")) { ImGui::End(); return; }

    const char* modes[] = { "Structure", "Jungle", "MinionWP", "NavGrid", "Bush" };
    int modeIdx = static_cast<int>(m_eAddMode);
    if (ImGui::Combo("AddMode", &modeIdx, modes, IM_ARRAYSIZE(modes)))
        m_eAddMode = static_cast<eAddMode>(modeIdx);

    ImGui::Separator();

    if (m_eAddMode == eAddMode::Structure)
    {
        static const WMap::eObjectKind kindTable[] = {
            WMap::eObjectKind::Structure_Nexus,
            WMap::eObjectKind::Structure_Inhibitor,
            WMap::eObjectKind::Structure_Turret };
        const char* kinds[] = { "Nexus", "Inhibitor", "Turret" };
        int kindIdx = 0;
        for (int i = 0; i < 3; ++i) if (kindTable[i] == m_PendingStructKind) { kindIdx = i; break; }
        if (ImGui::Combo("Kind", &kindIdx, kinds, 3)) m_PendingStructKind = kindTable[kindIdx];

        int teamIdx = (m_PendingStructTeam == Winters::Map::eTeam::Blue) ? 0 : 1;
        const char* teams[] = { "Blue", "Red" };
        if (ImGui::Combo("Team", &teamIdx, teams, 2))
            m_PendingStructTeam = (teamIdx == 0) ? Winters::Map::eTeam::Blue : Winters::Map::eTeam::Red;

        // Turret 선택 시에만 Tier/Lane 표시 — Nexus/Inhibitor 는 Add 시 None/Base 강제
        if (m_PendingStructKind == WMap::eObjectKind::Structure_Turret)
        {
            const char* tiers[] = { "Outer", "Inner", "Inhibitor", "Nexus" };
            int tierIdx = static_cast<int>(m_PendingStructTier);
            if (tierIdx < 0 || tierIdx > 3) tierIdx = 0;
            if (ImGui::Combo("Tier", &tierIdx, tiers, 4))
                m_PendingStructTier = static_cast<Winters::Map::eTurretTier>(tierIdx);

            const char* lanes[] = { "Top", "Mid", "Bot", "Base" };
            int laneIdx = static_cast<int>(m_PendingStructLane);
            if (laneIdx < 0 || laneIdx > 3) laneIdx = 0;
            if (ImGui::Combo("Lane", &laneIdx, lanes, 4))
                m_PendingStructLane = static_cast<Winters::Map::eLane>(laneIdx);
        }

        f32_t s = CStructure_Manager::Get()->Get_DefaultScale();
        if (ImGui::DragFloat("DefaultScale", &s, 0.0005f, 0.001f, 1.f))
            CStructure_Manager::Get()->Set_DefaultScale(s);
    }
    else if (m_eAddMode == eAddMode::Jungle)
    {
        static const CJungle_Manager::eJungleSub subTable[] = {
            CJungle_Manager::eJungleSub::Razorbeak,
            CJungle_Manager::eJungleSub::RazorbeakMini,
            CJungle_Manager::eJungleSub::Wolf,
            CJungle_Manager::eJungleSub::WolfMini,
            CJungle_Manager::eJungleSub::RedBuff,
            CJungle_Manager::eJungleSub::BlueBuff,
            CJungle_Manager::eJungleSub::Gromp,
            CJungle_Manager::eJungleSub::Krug,
            CJungle_Manager::eJungleSub::KrugMini,
            CJungle_Manager::eJungleSub::Dragon,
            CJungle_Manager::eJungleSub::Baron };
        const char* subs[] = {
            "Razorbeak Big",
            "Razorbeak Small",
            "Wolf Big",
            "Wolf Small",
            "RedBuff",
            "BlueBuff",
            "Gromp",
            "Krug Big",
            "Krug Small",
            "Dragon",
            "Baron" };
        constexpr int subCount = static_cast<int>(sizeof(subTable) / sizeof(subTable[0]));
        int idx = 0;
        for (int i = 0; i < subCount; ++i) if (subTable[i] == m_PendingJungleSub) { idx = i; break; }
        if (ImGui::Combo("Sub", &idx, subs, subCount))
            m_PendingJungleSub = subTable[idx];

        int camp = static_cast<int>(m_PendingJungleCamp);
        if (ImGui::InputInt("CampId", &camp))
            m_PendingJungleCamp = (camp < 0) ? 0 : static_cast<u32_t>(camp);
    }
    else if (m_eAddMode == eAddMode::MinionWP)
    {
        const char* teams[] = { "Blue", "Red" };
        int teamIdx = static_cast<int>(m_PendingMinionTeam);
        if (ImGui::Combo("Team", &teamIdx, teams, 2))
            m_PendingMinionTeam = static_cast<eMinionTeam>(teamIdx);

        const char* lanes[] = { "Top", "Mid", "Bot" };
        int laneIdx = static_cast<int>(m_PendingMinionLane);
        if (ImGui::Combo("Lane", &laneIdx, lanes, 3))
            m_PendingMinionLane = static_cast<eMinionWay>(laneIdx);

        CMinion_Manager::Get()->Set_EditLane(m_PendingMinionTeam, m_PendingMinionLane);

        const u32_t n = CMinion_Manager::Get()->Get_WaypointCount(
            m_PendingMinionTeam, m_PendingMinionLane);
        ImGui::Text("Waypoints in lane: %u", n);

        if (ImGui::Button("Clear Lane"))
            CMinion_Manager::Get()->Clear_Waypoints(m_PendingMinionTeam, m_PendingMinionLane);
        ImGui::SameLine();
        if (ImGui::Button("Reset Defaults"))
            CMinion_Manager::Get()->LoadDefaults();

        ImGui::TextDisabled("Click map = append WP to selected lane");
    }
    else if (m_eAddMode == eAddMode::NavGrid)
    {
        ImGui::TextUnformatted("Left drag = blocked");
        ImGui::TextUnformatted("Right drag = walkable");
        ImGui::SliderInt("Brush Radius", &m_iNavBrushRadiusCells, 0, 8);
        ImGui::Checkbox("Show Overlay", &m_bShowNavGridOverlay);

        if (m_pEditorNavGrid)
        {
            ImGui::Text("Walkable cells: %u", m_pEditorNavGrid->CountWalkableCells());
        }

        if (ImGui::Button("Save NavGrid"))
            SaveCurrentNavGrid();

        ImGui::SameLine();

        if (ImGui::Button("Load NavGrid"))
            LoadCurrentNavGrid();
    }
    else if (m_eAddMode == eAddMode::Bush)
    {
        int bushId = static_cast<int>(m_PendingBushId);
        if (ImGui::InputInt("BushId", &bushId))
            m_PendingBushId = (bushId < 0) ? 0u : static_cast<u32_t>(bushId);

        ImGui::DragFloat("Radius", &m_PendingBushRadius, 0.1f, 0.1f, 50.f, "%.2f");
        ImGui::DragFloat("Width", &m_PendingBushWidth, 0.1f, 0.1f, 80.f, "%.2f");
        ImGui::DragFloat("Height", &m_PendingBushHeight, 0.1f, 0.1f, 40.f, "%.2f");
        ImGui::InputText("Asset", m_szPendingBushAsset, IM_ARRAYSIZE(m_szPendingBushAsset));

        CBush_Manager::Get()->Set_DefaultRadius(m_PendingBushRadius);
        CBush_Manager::Get()->Set_DefaultWidth(m_PendingBushWidth);
        CBush_Manager::Get()->Set_DefaultHeight(m_PendingBushHeight);
        CBush_Manager::Get()->Set_DefaultAssetPath(m_szPendingBushAsset);

        ImGui::Text("Bush count: %u", CBush_Manager::Get()->Get_Count());
        ImGui::TextDisabled("Left-click map = add bush volume");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Left-click on map = Add / Paint");
    ImGui::TextDisabled("WASD + mouse = Camera / Esc = Back");
    ImGui::End();
}

void CScene_Editor::Render_Hierarchy()
{
    ImGui::SetNextWindowPos(ImVec2(0, 460), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hierarchy")) { ImGui::End(); return; }

    auto drawCategory = [&](eCategory cat, const char* label, u32_t count, auto getName)
    {
        if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen)) return;
        for (u32_t i = 0; i < count; ++i)
        {
            ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_Leaf
                | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (m_eSelectedCategory == cat && m_iSelectedIndex == (i32_t)i)
                f |= ImGuiTreeNodeFlags_Selected;
            ImGui::TreeNodeEx((void*)(intptr_t)(static_cast<int>(cat) * 10000 + i), f,
                "%s", getName(i));
            if (ImGui::IsItemClicked())
            {
                m_eSelectedCategory = cat;
                m_iSelectedIndex    = static_cast<i32_t>(i);
            }
        }
        ImGui::TreePop();
    };

    drawCategory(eCategory::Structure, "Structure",
        CStructure_Manager::Get()->Get_Count(),
        [&](u32_t i) { return CStructure_Manager::Get()->Get_Name(i); });
    drawCategory(eCategory::Jungle, "Jungle",
        CJungle_Manager::Get()->Get_Count(),
        [&](u32_t i) { return CJungle_Manager::Get()->Get_Name(i); });
    drawCategory(eCategory::Bush, "Bush",
        CBush_Manager::Get()->Get_Count(),
        [&](u32_t i) { return CBush_Manager::Get()->Get_Name(i); });

    // Minion 웨이포인트 — 현재 편집 중인 레인만 표시 (Palette 의 Team/Lane 과 연동)
    {
        const eMinionTeam t = m_PendingMinionTeam;
        const eMinionWay  l = m_PendingMinionLane;
        const u32_t n = CMinion_Manager::Get()->Get_WaypointCount(t, l);

        char header[64];
        sprintf_s(header, "MinionWP [%s/%s] (%u)",
            (t == eMinionTeam::Blue) ? "Blue" : "Red",
            (l == eMinionWay::Top) ? "Top" : (l == eMinionWay::Mid) ? "Mid" : "Bot",
            n);

        if (ImGui::TreeNodeEx(header, ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (u32_t i = 0; i < n; ++i)
            {
                ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (m_eSelectedCategory == eCategory::MinionWP && m_iSelectedIndex == (i32_t)i)
                    f |= ImGuiTreeNodeFlags_Selected;
                ImGui::TreeNodeEx((void*)(intptr_t)(30000 + i), f, "WP[%u]", i);
                if (ImGui::IsItemClicked())
                {
                    m_eSelectedCategory = eCategory::MinionWP;
                    m_iSelectedIndex    = static_cast<i32_t>(i);
                }
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void CScene_Editor::Render_Inspector()
{
    ImGui::SetNextWindowPos(ImVec2(1040, 22), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inspector")) { ImGui::End(); return; }

    if (m_iSelectedIndex < 0)
    {
        ImGui::TextDisabled("Select from Hierarchy");
        ImGui::End();
        return;
    }

    // MinionWP 는 Transform 이 아닌 Vec3 직접 편집 — 별도 경로로 조기 반환
    if (m_eSelectedCategory == eCategory::MinionWP)
    {
        const u32_t idx = static_cast<u32_t>(m_iSelectedIndex);
        const Vec3* pWP = CMinion_Manager::Get()->Get_WaypointPtr(
            m_PendingMinionTeam, m_PendingMinionLane, idx);
        if (!pWP) { ImGui::TextDisabled("(invalid WP)"); ImGui::End(); return; }

        Vec3 pos = *pWP;
        ImGui::Text("MinionWP[%u]", idx);
        ImGui::Separator();
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f, -500.f, 500.f, "%.2f"))
        {
            CMinion_Manager::Get()->Set_Waypoint(
                m_PendingMinionTeam, m_PendingMinionLane, idx, pos);
            m_bDirty = true;
        }

        ImGui::Separator();
        if (ImGui::Button("Delete", ImVec2(-1, 0)))
        {
            if (CMinion_Manager::Get()->Remove_Waypoint(
                m_PendingMinionTeam, m_PendingMinionLane, idx))
            {
                m_iSelectedIndex = -1;
                m_bDirty = true;
            }
        }
        ImGui::End();
        return;
    }

    if (m_eSelectedCategory == eCategory::Bush)
    {
        const u32_t idx = static_cast<u32_t>(m_iSelectedIndex);
        if (idx >= CBush_Manager::Get()->Get_Count())
        {
            ImGui::TextDisabled("(invalid Bush)");
            ImGui::End();
            return;
        }

        ImGui::Text("%s", CBush_Manager::Get()->Get_Name(idx));
        ImGui::Separator();

        int bushId = static_cast<int>(CBush_Manager::Get()->Get_BushId(idx));
        if (ImGui::InputInt("BushId", &bushId))
        {
            CBush_Manager::Get()->Set_BushId(idx, (bushId < 0) ? 0u : static_cast<u32_t>(bushId));
            m_bDirty = true;
        }

        Vec3 pos = CBush_Manager::Get()->Get_Position(idx);
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
        {
            CBush_Manager::Get()->Set_Position(idx, pos);
            m_bDirty = true;
        }

        f32_t yawDeg = DirectX::XMConvertToDegrees(CBush_Manager::Get()->Get_Yaw(idx));
        if (ImGui::DragFloat("Yaw(deg)", &yawDeg, 1.0f))
        {
            CBush_Manager::Get()->Set_Yaw(idx, DirectX::XMConvertToRadians(yawDeg));
            m_bDirty = true;
        }

        f32_t radius = CBush_Manager::Get()->Get_Radius(idx);
        if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 50.f, "%.2f"))
        {
            CBush_Manager::Get()->Set_Radius(idx, radius);
            m_bDirty = true;
        }

        f32_t width = 0.f;
        f32_t height = 0.f;
        f32_t scale = 1.f;
        CBush_Manager::Get()->Get_VisualSize(idx, width, height, scale);
        bool_t bSizeChanged = false;
        bSizeChanged |= ImGui::DragFloat("Width", &width, 0.1f, 0.1f, 80.f, "%.2f");
        bSizeChanged |= ImGui::DragFloat("Height", &height, 0.1f, 0.1f, 40.f, "%.2f");
        bSizeChanged |= ImGui::DragFloat("Scale", &scale, 0.01f, 0.01f, 20.f, "%.2f");
        if (bSizeChanged)
        {
            CBush_Manager::Get()->Set_VisualSize(idx, width, height, scale);
            m_bDirty = true;
        }

        int renderKind = static_cast<int>(CBush_Manager::Get()->Get_RenderKind(idx));
        const char* renderKinds[] = { "Billboard", "Mesh" };
        if (ImGui::Combo("RenderKind", &renderKind, renderKinds, IM_ARRAYSIZE(renderKinds)))
        {
            CBush_Manager::Get()->Set_RenderKind(
                idx,
                static_cast<Winters::Map::eBushRenderKind>(renderKind));
            m_bDirty = true;
        }

        char assetPath[128]{};
        strncpy_s(assetPath, CBush_Manager::Get()->Get_AssetPath(idx), _TRUNCATE);
        if (ImGui::InputText("Asset", assetPath, IM_ARRAYSIZE(assetPath)))
        {
            CBush_Manager::Get()->Set_AssetPath(idx, assetPath);
            m_bDirty = true;
        }

        bool visible = CBush_Manager::Get()->Get_Visible(idx) != 0;
        if (ImGui::Checkbox("Visible", &visible))
        {
            CBush_Manager::Get()->Set_Visible(idx, visible);
            m_bDirty = true;
        }

        ImGui::Separator();
        if (ImGui::Button("Delete", ImVec2(-1, 0)))
        {
            if (CBush_Manager::Get()->Remove_At(idx))
            {
                m_iSelectedIndex = -1;
                m_bDirty = true;
            }
        }

        ImGui::End();
        return;
    }

    TransformComponent* pTf = nullptr;   // ← CTransform* 대신
    const char* pName = nullptr;
    const u32_t         idx = static_cast<u32_t>(m_iSelectedIndex);
    bool_t              bVis = false;
    bool_t              bHasVis = false;

    switch (m_eSelectedCategory)
    {
    case eCategory::Structure:
        pTf = CStructure_Manager::Get()->Get_Transform(idx);
        pName = CStructure_Manager::Get()->Get_Name(idx);
        bVis = CStructure_Manager::Get()->Get_Visible(idx);
        bHasVis = (pTf != nullptr);
        break;
    case eCategory::Jungle:
        pTf = CJungle_Manager::Get()->Get_Transform(idx);
        pName = CJungle_Manager::Get()->Get_Name(idx);
        bVis = CJungle_Manager::Get()->Get_Visible(idx);
        bHasVis = (pTf != nullptr);
        break;
    }

    if (!pTf) { ImGui::TextDisabled("(invalid)"); ImGui::End(); return; }

    ImGui::Text("%s", pName ? pName : "?");
    ImGui::Separator();

    Vec3 pos = pTf->GetPosition();
    if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) { pTf->SetPosition(pos); m_bDirty = true; }

    Vec3 rot = pTf->GetRotation();
    Vec3 rotDeg = {
        DirectX::XMConvertToDegrees(rot.x),
        DirectX::XMConvertToDegrees(rot.y),
        DirectX::XMConvertToDegrees(rot.z) };
    if (ImGui::DragFloat3("Rotation(deg)", &rotDeg.x, 1.0f))
    {
        pTf->SetRotation({
            DirectX::XMConvertToRadians(rotDeg.x),
            DirectX::XMConvertToRadians(rotDeg.y),
            DirectX::XMConvertToRadians(rotDeg.z) });
        m_bDirty = true;
    }

    Vec3  scl = pTf->GetScale();
    f32_t s = scl.x;
    if (ImGui::DragFloat("Scale", &s, 0.001f, 0.001f, 10.f))
    {
        pTf->SetScale(s);     // SetScale(f32_t) 오버로드 (수정 1 에서 추가)
        m_bDirty = true;
    }

    if (bHasVis)
    {
        bool b = (bVis != 0);
        if (ImGui::Checkbox("Visible", &b))
        {
            switch (m_eSelectedCategory)
            {
            case eCategory::Structure: CStructure_Manager::Get()->Set_Visible(idx, b); break;
            case eCategory::Jungle:    CJungle_Manager::Get()->Set_Visible(idx, b);    break;
            }
            m_bDirty = true;
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Delete", ImVec2(-1, 0)))
    {
        bool removed = false;
        switch (m_eSelectedCategory)
        {
        case eCategory::Structure: removed = CStructure_Manager::Get()->Remove_At(idx); break;
        case eCategory::Jungle:    removed = CJungle_Manager::Get()->Remove_At(idx);    break;
        }
        if (removed) { m_iSelectedIndex = -1; m_bDirty = true; }
    }

    ImGui::End();
}

void CScene_Editor::Handle_MousePlacement()
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    Vec3 ground{};
    if (!TryPickGroundPlane(ground))
        return;

    auto& input = CInput::Get();
    if (m_eAddMode == eAddMode::NavGrid)
    {
        if (input.IsLButtonDown())
            PaintNavGridAt(ground, false);

        if (input.IsRButtonDown())
            PaintNavGridAt(ground, true);

        return;
    }

    if (!input.IsLButtonPressed())
        return;

    i32_t newIdx = -1;
    switch (m_eAddMode)
    {
    case eAddMode::Structure:
    {
        Winters::Map::eTurretTier tier = m_PendingStructTier;
        Winters::Map::eLane       lane = m_PendingStructLane;
        if (m_PendingStructKind != Winters::Map::eObjectKind::Structure_Turret)
        {
            tier = Winters::Map::eTurretTier::None;
            lane = Winters::Map::eLane::Base;
        }
        newIdx = CStructure_Manager::Get()->Add_At(
            m_PendingStructKind,
            static_cast<::eTeam>(m_PendingStructTeam),
            tier, lane, ground);

        if (newIdx >= 0) { m_eSelectedCategory = eCategory::Structure; m_iSelectedIndex = newIdx; }
        break;
    }
    case eAddMode::Jungle:
        newIdx = CJungle_Manager::Get()->Add_At(m_PendingJungleSub, m_PendingJungleCamp, ground);
        if (newIdx >= 0) { m_eSelectedCategory = eCategory::Jungle; m_iSelectedIndex = newIdx; }
        break;

    case eAddMode::MinionWP:
    {
        newIdx = CMinion_Manager::Get()->Add_Waypoint(
            m_PendingMinionTeam, m_PendingMinionLane, ground);
        if (newIdx >= 0)
        {
            m_eSelectedCategory = eCategory::MinionWP;
            m_iSelectedIndex    = newIdx;
        }
        break;
    }
    case eAddMode::Bush:
    {
        newIdx = CBush_Manager::Get()->Add_At(
            m_PendingBushId,
            ground,
            m_PendingBushRadius,
            m_szPendingBushAsset,
            Winters::Map::eBushRenderKind::Billboard);
        if (newIdx >= 0)
        {
            CBush_Manager::Get()->Set_VisualSize(
                static_cast<u32_t>(newIdx),
                m_PendingBushWidth,
                m_PendingBushHeight,
                1.f);
            m_eSelectedCategory = eCategory::Bush;
            m_iSelectedIndex = newIdx;
        }
        break;
    }
    case eAddMode::NavGrid:
        break;
    }
    if (newIdx >= 0) m_bDirty = true;
}

void CScene_Editor::PaintNavGridAt(const Vec3& worldPos, bool_t bWalkable)
{
    if (!m_pEditorNavGrid)
        return;

    const Engine::CNavGrid::Cell center = m_pEditorNavGrid->WorldToCell(worldPos);
    const int32_t radius = m_iNavBrushRadiusCells;

    for (int32_t y = center.y - radius; y <= center.y + radius; ++y)
    {
        for (int32_t x = center.x - radius; x <= center.x + radius; ++x)
        {
            const int32_t dx = x - center.x;
            const int32_t dy = y - center.y;
            if (dx * dx + dy * dy > radius * radius)
                continue;

            m_pEditorNavGrid->SetWalkable(x, y, bWalkable);
        }
    }

    m_bDirty = true;
}

bool_t CScene_Editor::TryPickGroundPlane(Vec3& outWorld) const
{
    if (!m_pCamera)
        return false;

    const auto& input = CInput::Get();
    const CInput::MouseRay ray = input.GetMouseWorldRay(
        *m_pCamera,
        static_cast<i32_t>(g_iWinSizeX),
        static_cast<i32_t>(g_iWinSizeY));

    if (fabsf(ray.Dir.y) < 1e-4f)
        return false;

    const f32_t t = -ray.Origin.y / ray.Dir.y;
    if (t < 0.f)
        return false;

    outWorld = {
        ray.Origin.x + ray.Dir.x * t,
        0.f,
        ray.Origin.z + ray.Dir.z * t
    };
    return true;
}

void CScene_Editor::RenderNavGridOverlay(const Mat4& matVP)
{
    if (!m_bShowNavGridOverlay || !m_pEditorNavGrid)
        return;

    ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
    const DirectX::XMMATRIX mVP = matVP.ToXMMATRIX();
    const f32_t half = Engine::CNavGrid::kCellSize * 0.5f;
    const ImU32 color = IM_COL32(255, 230, 0, 210);

    auto WorldToScreen = [&](const Vec3& w, ImVec2& out) -> bool
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
    };

    for (u32_t y = 0; y < Engine::CNavGrid::kCellCountY; ++y)
    {
        for (u32_t x = 0; x < Engine::CNavGrid::kCellCountX; ++x)
        {
            if (m_pEditorNavGrid->IsWalkable(static_cast<int32_t>(x), static_cast<int32_t>(y)))
                continue;

            const Vec3 center = m_pEditorNavGrid->CellToWorld(static_cast<int32_t>(x), static_cast<int32_t>(y));
            const Vec3 p0{ center.x - half, 0.05f, center.z - half };
            const Vec3 p1{ center.x + half, 0.05f, center.z - half };
            const Vec3 p2{ center.x + half, 0.05f, center.z + half };
            const Vec3 p3{ center.x - half, 0.05f, center.z + half };

            ImVec2 s0{}, s1{}, s2{}, s3{};
            if (!WorldToScreen(p0, s0) ||
                !WorldToScreen(p1, s1) ||
                !WorldToScreen(p2, s2) ||
                !WorldToScreen(p3, s3))
            {
                continue;
            }

            pDraw->AddQuad(s0, s1, s2, s3, color, 1.f);
        }
    }
}

void CScene_Editor::Save_CurrentStage()
{
    wchar_t stagePath[MAX_PATH] = {};
    if (!CMapDataIO::Get_StagePathW(m_iCurrentStage, stagePath, MAX_PATH))
    {
        MessageBoxW(nullptr, L"경로 조립 실패 (GetModuleFileNameW)",
            L"Editor Save", MB_OK | MB_ICONERROR);
        return;
    }
    if (FAILED(CMapDataIO::Save_Stage(stagePath)))
    {
        wchar_t m[MAX_PATH + 64];
        swprintf_s(m, L"저장 실패!\n%s", stagePath);
        MessageBoxW(nullptr, m, L"Editor Save", MB_OK | MB_ICONERROR);
        return;
    }
    m_bDirty = false;
    wchar_t ok[MAX_PATH + 128];
    swprintf_s(ok, L"Stage%d.dat 저장 완료!\n\nS=%u J=%u B=%u\n경로: %s",
        m_iCurrentStage,
        CStructure_Manager::Get()->Get_Count(),
        CJungle_Manager::Get()->Get_Count(),
        CBush_Manager::Get()->Get_Count(),
        stagePath);
    MessageBoxW(nullptr, ok, L"Editor Save", MB_OK | MB_ICONINFORMATION);
}

void CScene_Editor::Load_CurrentStage()
{
    wchar_t stagePath[MAX_PATH] = {};
    if (!CMapDataIO::Get_StagePathW(m_iCurrentStage, stagePath, MAX_PATH)) return;

    // 실패해도 Manager 는 Clear() 된 상태 — 빈 맵 허용
    CMapDataIO::Load_Stage(stagePath);

    m_iSelectedIndex = -1;
    m_bDirty = false;
}

void CScene_Editor::SaveCurrentNavGrid()
{
    if (!m_pEditorNavGrid)
        return;

    wchar_t navPath[MAX_PATH] = {};
    if (!CMapDataIO::GetNavGridPathW(m_iCurrentStage, navPath, MAX_PATH))
        return;

    Engine::CNavGrid::SaveToFile(navPath, *m_pEditorNavGrid);
}

void CScene_Editor::LoadCurrentNavGrid()
{
    wchar_t navPath[MAX_PATH] = {};
    if (CMapDataIO::GetNavGridPathW(m_iCurrentStage, navPath, MAX_PATH))
        m_pEditorNavGrid = Engine::CNavGrid::LoadFromFile(navPath);

    if (!m_pEditorNavGrid)
    {
        m_pEditorNavGrid = CreateStageBoundsNavGrid();
    }
}

void CScene_Editor::Request_BackToInGame()
{
    if (m_bDirty)
    {
        const int r = MessageBoxW(nullptr,
            L"저장되지 않은 변경사항이 있습니다.\n저장하고 나갈까요?",
            L"Back to InGame", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL) return;
        if (r == IDYES)
        {
            Save_CurrentStage();
            SaveCurrentNavGrid();
        }
    }
    using namespace Engine;
    auto pScene = unique_ptr<IScene>(new CScene_InGame());
    CGameInstance::Get()->Change_Scene((uint32_t)eSceneID::InGame, std::move(pScene));
}
