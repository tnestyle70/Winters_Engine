#pragma once

#include "IScene.h"
#include "Core/CTransform.h"
#include "Manager/Navigation/NavGrid.h"
#include "Renderer/CCamera.h"
#include "Renderer/ModelRenderer.h"
#include "World/LoLStageDocument.h"

#include <memory>
#include <string>

class CLoLEditorCamera final : public CCamera
{
public:
	CLoLEditorCamera() = default;
	~CLoLEditorCamera() override = default;
};

// CLoLMapEditorScene — CScene_Editor 포팅.
// 매니저 4종/ECS 비의존: CLoLStageDocument(StageData)를 직접 편집한다.
class CLoLMapEditorScene final : public IScene
{
public:
	CLoLMapEditorScene() = default;
	~CLoLMapEditorScene() override = default;

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t dt) override;
	void OnRender() override;
	void OnImGui() override;

private:
	enum class eCategory : i32_t { Structure, Jungle, MinionWP, Bush };
	enum class eAddMode : i32_t { Structure, Jungle, MinionWP, NavGrid, Bush };

	// ImGui 패널
	void DrawMenuBar();
	void DrawPalette();
	void DrawHierarchy();
	void DrawInspector();

	// 파일 I/O
	void SaveAll();
	void LoadAll();
	void LoadOrCreateNavGrid();
	std::unique_ptr<Engine::CNavGrid> CreateStageBoundsNavGrid() const;

	// 배치 / 피킹
	void HandleMousePlacement();
	bool_t TryPickGroundPlane(Vec3& outWorld) const;
	void PaintNavGridAt(const Vec3& worldPos, bool_t bWalkable);

	// 오버레이
	void RenderMarkers(const Mat4& matVP) const;
	void RenderNavGridOverlay(const Mat4& matVP) const;

	// 웨이포인트 (문서 기반)
	u32_t CountWaypoints(u32_t team, u32_t lane) const;
	void AppendWaypoint(u32_t team, u32_t lane, const Vec3& pos);
	void RenormalizeWaypointOrders(u32_t team, u32_t lane);

	CLoLStageDocument m_document;

	ModelRenderer m_Map{};
	CTransform    m_MapTransform{};
	std::unique_ptr<CLoLEditorCamera> m_pCamera;
	std::unique_ptr<Engine::CNavGrid> m_pEditorNavGrid;

	i32_t m_iCurrentStage = 1;

	eCategory m_eSelectedCategory = eCategory::Structure;
	i32_t     m_iSelectedIndex    = -1;

	eAddMode m_eAddMode = eAddMode::Structure;

	Winters::Map::eObjectKind m_PendingStructKind = Winters::Map::eObjectKind::Structure_Turret;
	u32_t m_PendingStructTeam = 0;
	u32_t m_PendingStructTier = 0;
	u32_t m_PendingStructLane = 0;

	u32_t m_PendingJungleSub  = 0;
	u32_t m_PendingJungleCamp = 0;

	u32_t m_PendingMinionTeam = 0;
	u32_t m_PendingMinionLane = 0;

	u32_t m_PendingBushId = 1;
	f32_t m_PendingBushRadius = 4.6f;
	f32_t m_PendingBushWidth = 8.f;
	f32_t m_PendingBushHeight = 4.f;
	char  m_szPendingBushAsset[128] =
		"Texture/MAP/output/textures/assets/maps/kitpieces/srx/textures/sru_brush.png";

	i32_t  m_iNavBrushRadiusCells = 2;
	bool_t m_bShowNavGridOverlay = true;

	std::string  m_StatusMessage = "LoL map editor ready.";
	std::wstring m_LastRoundtripMessage;
};
