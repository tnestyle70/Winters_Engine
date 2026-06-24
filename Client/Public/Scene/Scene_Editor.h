#pragma once
#include "IScene.h"
#include "Defines.h"
#include "DynamicCamera.h"
#include "Renderer/ModelRenderer.h"
#include "Core/CTransform.h"
#include "ECS/Systems/TransformSystem.h"
#include "Manager/Navigation/NavGrid.h"
#include "Manager/Bush_Manager.h"

#include "Map/MapDataFormats.h"
#include "Manager/Jungle_Manager.h"          // eJungleSub 타입 참조
#include "Manager/Minion_Manager.h"          // eMinionTeam/eMinionWay 참조

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

// CScene_Editor — 맵 오브젝트 배치 전용 씬 (던전스 CEditor 포팅, v3 싱글턴)
//
// 기능:
//  - 탑다운 카메라 (FreeCam, WASD + 마우스)
//  - 3 싱글턴 Manager 에 직접 접근 (CXxx_Manager::Get())
//  - Palette: Structure / Jungle 중 선택 후 맵 좌클릭 = 배치 (Minion 은 런타임 스폰 대상이라 제외)
//  - Hierarchy 트리에서 오브젝트 선택
//  - Inspector: Position / Rotation / Scale / Visible / Delete
//  - File: Save(Ctrl+S) / Load / Switch Stage / Back to InGame(Esc)

class CScene_Editor final : public IScene
{
public:
    CScene_Editor() = default;
    ~CScene_Editor() override = default;

    bool OnEnter()              override;
    void OnExit()               override;
    void OnUpdate(f32_t dt)     override;
    void OnLateUpdate(f32_t dt) override;
    void OnRender()             override;
    void OnImGui()              override;

private:
    //Transform System
    unique_ptr<CTransformSystem> m_pTransformSystem = { nullptr };

    CWorld m_World{};

    // 맵 메시
    ModelRenderer m_Map{};
    CTransform    m_MapTransform{};

    // 카메라
    unique_ptr<CDynamicCamera> m_pCamera{};

    // 편집 상태
    i32_t m_iCurrentStage = 1;
    bool  m_bDirty        = false;

    // 선택
    enum class eCategory { Structure, Jungle, MinionWP, Bush };
    eCategory m_eSelectedCategory = eCategory::Structure;
    i32_t     m_iSelectedIndex    = -1;

    // Palette (Add 모드 + 선택값)
    enum class eAddMode { Structure, Jungle, MinionWP, NavGrid, Bush };
    eAddMode m_eAddMode = eAddMode::Structure;

    Winters::Map::eObjectKind m_PendingStructKind = Winters::Map::eObjectKind::Structure_Turret;
    Winters::Map::eTeam       m_PendingStructTeam = Winters::Map::eTeam::Blue;
    Winters::Map::eTurretTier m_PendingStructTier = Winters::Map::eTurretTier::Outer;
    Winters::Map::eLane       m_PendingStructLane = Winters::Map::eLane::Top;
    CJungle_Manager::eJungleSub          m_PendingJungleSub  = CJungle_Manager::eJungleSub::Baron;
    u32_t                                 m_PendingJungleCamp = 0;

    // MinionWP 편집 상태
    eMinionTeam m_PendingMinionTeam = eMinionTeam::Blue;
    eMinionWay  m_PendingMinionLane = eMinionWay::Top;

    // Bush placement state
    u32_t m_PendingBushId = 1;
    f32_t m_PendingBushRadius = 4.6f;
    f32_t m_PendingBushWidth = 8.f;
    f32_t m_PendingBushHeight = 4.f;
    char  m_szPendingBushAsset[128] =
        "Client/Bin/Resource/Texture/MAP/output/textures/assets/maps/kitpieces/srx/textures/sru_brush.png";

    // 마우스 좌클릭 에지 감지 (GetAsyncKeyState 직접 사용)
    unique_ptr<Engine::CNavGrid> m_pEditorNavGrid;
    int32_t m_iNavBrushRadiusCells = 2;
    bool_t m_bShowNavGridOverlay = true;

    // ImGui 패널
    void Render_MenuBar();
    void Render_Palette();
    void Render_Hierarchy();
    void Render_Inspector();

    // 파일 I/O
    void Save_CurrentStage();
    void Load_CurrentStage();
    void SaveCurrentNavGrid();
    void LoadCurrentNavGrid();

    // 배치
    void Handle_MousePlacement();
    void PaintNavGridAt(const Vec3& worldPos, bool_t bWalkable);
    bool_t TryPickGroundPlane(Vec3& outWorld) const;
    void RenderNavGridOverlay(const Mat4& matVP);

    // 씬 전환
    void Request_BackToInGame();
};
