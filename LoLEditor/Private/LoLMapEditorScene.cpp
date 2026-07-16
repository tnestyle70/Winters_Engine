#include <Windows.h>

#include "LoLMapEditorScene.h"
#include "LoLEditorApp.h"

#include "Core/CInput.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
	namespace WMap = Winters::Map;

	const char* const kTeamItems[] = { "Blue", "Red", "Neutral" };
	const char* const kTierItems[] = { "Outer", "Inner", "Inhibitor", "Nexus" };
	const char* const kLaneItems[] = { "Top", "Mid", "Bot", "Base" };
	const char* const kStructKindItems[] = { "Nexus", "Inhibitor", "Turret" };
	const char* const kMinionTeamItems[] = { "Blue", "Red" };
	const char* const kMinionLaneItems[] = { "Top", "Mid", "Bot" };

	const char* StructureKindLabel(WMap::eObjectKind kind)
	{
		switch (kind)
		{
		case WMap::eObjectKind::Structure_Nexus:     return "Nexus";
		case WMap::eObjectKind::Structure_Inhibitor: return "Inhibitor";
		case WMap::eObjectKind::Structure_Turret:    return "Turret";
		default:                                     return "Structure";
		}
	}

	bool ComboU32(const char* pLabel, u32_t& value, const char* const* items, int count)
	{
		int current = static_cast<int>(value);
		if (current < 0 || current >= count)
			current = 0;

		if (!ImGui::Combo(pLabel, &current, items, count))
			return false;

		value = static_cast<u32_t>(current);
		return true;
	}

	bool WorldToScreen(const DirectX::XMMATRIX& mVP, const Vec3& w, ImVec2& out)
	{
		DirectX::XMVECTOR v = DirectX::XMVectorSet(w.x, w.y, w.z, 1.f);
		v = DirectX::XMVector4Transform(v, mVP);
		const f32_t wComp = DirectX::XMVectorGetW(v);
		if (wComp <= 0.01f)
			return false;

		const f32_t nx = DirectX::XMVectorGetX(v) / wComp;
		const f32_t ny = DirectX::XMVectorGetY(v) / wComp;
		out.x = (nx * 0.5f + 0.5f) * static_cast<f32_t>(LoLEditor::kWindowWidth);
		out.y = (1.f - (ny * 0.5f + 0.5f)) * static_cast<f32_t>(LoLEditor::kWindowHeight);
		return true;
	}

	ImU32 TeamColor(u32_t team)
	{
		switch (team)
		{
		case 0:  return IM_COL32(80, 160, 255, 255);
		case 1:  return IM_COL32(255, 100, 100, 255);
		default: return IM_COL32(200, 200, 200, 255);
		}
	}

	f32_t SnapDownToNavCell(f32_t value)
	{
		return std::floor(value / Engine::CNavGrid::kCellSize) * Engine::CNavGrid::kCellSize;
	}
}

bool CLoLMapEditorScene::OnEnter()
{
	// 맵 메시 — Scene_InGame/CScene_Editor와 동일 상수
	m_Map.Initialize("Texture/MAP/output/sr_base_flip.wmesh", L"Shaders/Mesh3D.hlsl");
	m_MapTransform.SetPosition(0.f, 0.f, 0.f);
	// InGame 과 동일한 X mirror 적용 (lol2gltf --flipX false 무력화 우회).
	m_MapTransform.SetScale({ -0.01f, 0.01f, 0.01f });
	m_MapTransform.SetRotation({ 0.f, DirectX::XMConvertToRadians(-135.f), 0.f });

	m_pCamera = std::make_unique<CLoLEditorCamera>();
	m_pCamera->Ready(
		{ 0.f, 20.f, -0.1f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f },
		DirectX::XMConvertToRadians(45.f),
		static_cast<f32_t>(LoLEditor::kWindowWidth) / static_cast<f32_t>(LoLEditor::kWindowHeight),
		0.1f, 2000.f);

	LoadAll();
	return true;
}

void CLoLMapEditorScene::OnExit()
{
}

void CLoLMapEditorScene::OnUpdate(f32_t dt)
{
	auto& input = CInput::Get();

	if (m_pCamera && !ImGui::GetIO().WantCaptureKeyboard)
		m_pCamera->Update(dt, input);

	if (!ImGui::GetIO().WantCaptureKeyboard &&
		input.IsKeyDown(VK_CONTROL) && input.IsKeyPressed('S'))
	{
		SaveAll();
	}

	HandleMousePlacement();
}

void CLoLMapEditorScene::OnRender()
{
	if (!m_pCamera)
		return;

	const Mat4 matVP = m_pCamera->GetViewProjection();

	m_Map.UpdateCamera(matVP);
	m_Map.UpdateTransform(m_MapTransform.GetWorldMatrix());
	m_Map.Render();

	RenderMarkers(matVP);
	RenderNavGridOverlay(matVP);
}

void CLoLMapEditorScene::OnImGui()
{
	DrawMenuBar();
	DrawPalette();
	DrawHierarchy();
	DrawInspector();
}

void CLoLMapEditorScene::DrawMenuBar()
{
	if (!ImGui::BeginMainMenuBar())
		return;

	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Save Stage + NavGrid", "Ctrl+S"))
			SaveAll();

		if (ImGui::MenuItem("Load Stage + NavGrid"))
			LoadAll();

		if (ImGui::MenuItem("Verify Roundtrip"))
		{
			std::wstring msg;
			const bool_t bOk = m_document.VerifyRoundtrip(msg);
			m_LastRoundtripMessage = std::wstring(L"roundtrip ") + (bOk ? L"" : L"FAILED: ") + msg;
#ifdef _DEBUG
			::OutputDebugStringW((L"[LoLEditor] " + m_LastRoundtripMessage + L"\n").c_str());
#endif
		}

		ImGui::Separator();

		if (ImGui::MenuItem("New Stage"))
		{
			m_document.NewStage(m_iCurrentStage);
			m_iSelectedIndex = -1;
			m_StatusMessage = "new empty stage";
		}

		ImGui::EndMenu();
	}

	ImGui::SetNextItemWidth(90.f);
	int stage = m_iCurrentStage;
	if (ImGui::InputInt("Stage", &stage) && stage >= 1)
		m_iCurrentStage = stage;

	const auto& data = m_document.Data();
	ImGui::Text("| S=%zu J=%zu W=%zu B=%zu%s | %s",
		data.structures.size(), data.jungles.size(),
		data.minionWaypoints.size(), data.bushes.size(),
		m_document.IsDirty() ? " *" : "",
		m_StatusMessage.c_str());

	if (!m_LastRoundtripMessage.empty())
		ImGui::Text("| %ls", m_LastRoundtripMessage.c_str());

	ImGui::EndMainMenuBar();
}

void CLoLMapEditorScene::DrawPalette()
{
	if (!ImGui::Begin("Palette"))
	{
		ImGui::End();
		return;
	}

	int addMode = static_cast<int>(m_eAddMode);
	ImGui::RadioButton("Structure", &addMode, static_cast<int>(eAddMode::Structure)); ImGui::SameLine();
	ImGui::RadioButton("Jungle", &addMode, static_cast<int>(eAddMode::Jungle)); ImGui::SameLine();
	ImGui::RadioButton("MinionWP", &addMode, static_cast<int>(eAddMode::MinionWP)); ImGui::SameLine();
	ImGui::RadioButton("NavGrid", &addMode, static_cast<int>(eAddMode::NavGrid)); ImGui::SameLine();
	ImGui::RadioButton("Bush", &addMode, static_cast<int>(eAddMode::Bush));
	m_eAddMode = static_cast<eAddMode>(addMode);

	ImGui::Separator();

	switch (m_eAddMode)
	{
	case eAddMode::Structure:
	{
		int kind = 2;
		if (m_PendingStructKind == WMap::eObjectKind::Structure_Nexus) kind = 0;
		else if (m_PendingStructKind == WMap::eObjectKind::Structure_Inhibitor) kind = 1;
		if (ImGui::Combo("Kind", &kind, kStructKindItems, 3))
		{
			m_PendingStructKind =
				(kind == 0) ? WMap::eObjectKind::Structure_Nexus :
				(kind == 1) ? WMap::eObjectKind::Structure_Inhibitor :
				              WMap::eObjectKind::Structure_Turret;
		}
		ComboU32("Team", m_PendingStructTeam, kTeamItems, 3);
		if (m_PendingStructKind == WMap::eObjectKind::Structure_Turret)
		{
			ComboU32("Tier", m_PendingStructTier, kTierItems, 4);
			ComboU32("Lane", m_PendingStructLane, kLaneItems, 4);
		}
		break;
	}
	case eAddMode::Jungle:
	{
		ImGui::InputScalar("SubKind(raw)", ImGuiDataType_U32, &m_PendingJungleSub);
		ImGui::InputScalar("CampId", ImGuiDataType_U32, &m_PendingJungleCamp);
		break;
	}
	case eAddMode::MinionWP:
	{
		ComboU32("Team", m_PendingMinionTeam, kMinionTeamItems, 2);
		ComboU32("Lane", m_PendingMinionLane, kMinionLaneItems, 3);
		ImGui::Text("current lane waypoints: %u",
			CountWaypoints(m_PendingMinionTeam, m_PendingMinionLane));
		break;
	}
	case eAddMode::NavGrid:
	{
		ImGui::SliderInt("Brush radius (cells)", &m_iNavBrushRadiusCells, 1, 16);
		bool bShow = m_bShowNavGridOverlay;
		if (ImGui::Checkbox("Show overlay", &bShow))
			m_bShowNavGridOverlay = bShow;
		ImGui::TextUnformatted("LMB drag = block / RMB drag = walkable");
		break;
	}
	case eAddMode::Bush:
	{
		ImGui::InputScalar("BushId", ImGuiDataType_U32, &m_PendingBushId);
		ImGui::DragFloat("Radius", &m_PendingBushRadius, 0.1f, 0.1f, 64.f);
		ImGui::DragFloat("Width", &m_PendingBushWidth, 0.1f, 0.1f, 64.f);
		ImGui::DragFloat("Height", &m_PendingBushHeight, 0.1f, 0.1f, 64.f);
		ImGui::InputText("Asset", m_szPendingBushAsset, sizeof(m_szPendingBushAsset));
		break;
	}
	}

	ImGui::End();
}

void CLoLMapEditorScene::DrawHierarchy()
{
	if (!ImGui::Begin("Hierarchy"))
	{
		ImGui::End();
		return;
	}

	auto& data = m_document.Data();

	if (ImGui::CollapsingHeader("Structures", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (size_t i = 0; i < data.structures.size(); ++i)
		{
			const bool bSelected =
				m_eSelectedCategory == eCategory::Structure &&
				m_iSelectedIndex == static_cast<i32_t>(i);

			char label[96] = {};
			snprintf(label, sizeof(label), "%s##s%zu", data.structures[i].name, i);
			if (ImGui::Selectable(label, bSelected))
			{
				m_eSelectedCategory = eCategory::Structure;
				m_iSelectedIndex = static_cast<i32_t>(i);
			}
		}
	}

	if (ImGui::CollapsingHeader("Jungles", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (size_t i = 0; i < data.jungles.size(); ++i)
		{
			const bool bSelected =
				m_eSelectedCategory == eCategory::Jungle &&
				m_iSelectedIndex == static_cast<i32_t>(i);

			char label[96] = {};
			snprintf(label, sizeof(label), "%s (camp %u)##j%zu",
				data.jungles[i].name, data.jungles[i].campId, i);
			if (ImGui::Selectable(label, bSelected))
			{
				m_eSelectedCategory = eCategory::Jungle;
				m_iSelectedIndex = static_cast<i32_t>(i);
			}
		}
	}

	if (ImGui::CollapsingHeader("Minion Waypoints"))
	{
		for (size_t i = 0; i < data.minionWaypoints.size(); ++i)
		{
			const auto& wp = data.minionWaypoints[i];
			const bool bSelected =
				m_eSelectedCategory == eCategory::MinionWP &&
				m_iSelectedIndex == static_cast<i32_t>(i);

			char label[96] = {};
			snprintf(label, sizeof(label), "T%u L%u #%u##w%zu", wp.team, wp.lane, wp.order, i);
			if (ImGui::Selectable(label, bSelected))
			{
				m_eSelectedCategory = eCategory::MinionWP;
				m_iSelectedIndex = static_cast<i32_t>(i);
			}
		}
	}

	if (ImGui::CollapsingHeader("Bushes"))
	{
		for (size_t i = 0; i < data.bushes.size(); ++i)
		{
			const bool bSelected =
				m_eSelectedCategory == eCategory::Bush &&
				m_iSelectedIndex == static_cast<i32_t>(i);

			char label[96] = {};
			snprintf(label, sizeof(label), "%s (id %u)##b%zu",
				data.bushes[i].name, data.bushes[i].bushId, i);
			if (ImGui::Selectable(label, bSelected))
			{
				m_eSelectedCategory = eCategory::Bush;
				m_iSelectedIndex = static_cast<i32_t>(i);
			}
		}
	}

	ImGui::End();
}

void CLoLMapEditorScene::DrawInspector()
{
	if (!ImGui::Begin("Inspector"))
	{
		ImGui::End();
		return;
	}

	auto& data = m_document.Data();
	const size_t idx = static_cast<size_t>(m_iSelectedIndex);
	bool bDeleted = false;

	switch (m_eSelectedCategory)
	{
	case eCategory::Structure:
	{
		if (m_iSelectedIndex < 0 || idx >= data.structures.size())
			break;

		auto& e = data.structures[idx];
		if (ImGui::InputText("Name", e.name, sizeof(e.name))) m_document.MarkDirty();
		if (ComboU32("Team", e.team, kTeamItems, 3)) m_document.MarkDirty();
		if (ComboU32("Tier", e.tier, kTierItems, 4)) m_document.MarkDirty();
		if (ComboU32("Lane", e.lane, kLaneItems, 4)) m_document.MarkDirty();

		f32_t pos[3] = { e.px, e.py, e.pz };
		if (ImGui::DragFloat3("Position", pos, 0.1f))
		{
			e.px = pos[0]; e.py = pos[1]; e.pz = pos[2];
			m_document.MarkDirty();
		}

		f32_t rot[3] = { e.rx, e.ry, e.rz };
		if (ImGui::DragFloat3("Rotation", rot, 0.01f))
		{
			e.rx = rot[0]; e.ry = rot[1]; e.rz = rot[2];
			m_document.MarkDirty();
		}

		if (ImGui::DragFloat("Scale", &e.scale, 0.01f, 0.001f, 100.f)) m_document.MarkDirty();

		bool bVisible = e.bVisible != 0;
		if (ImGui::Checkbox("Visible", &bVisible))
		{
			e.bVisible = bVisible ? 1u : 0u;
			m_document.MarkDirty();
		}

		if (ImGui::Button("Delete"))
		{
			data.structures.erase(data.structures.begin() + idx);
			bDeleted = true;
		}
		break;
	}
	case eCategory::Jungle:
	{
		if (m_iSelectedIndex < 0 || idx >= data.jungles.size())
			break;

		auto& e = data.jungles[idx];
		if (ImGui::InputText("Name", e.name, sizeof(e.name))) m_document.MarkDirty();
		if (ImGui::InputScalar("SubKind(raw)", ImGuiDataType_U32, &e.subKind)) m_document.MarkDirty();
		if (ImGui::InputScalar("CampId", ImGuiDataType_U32, &e.campId)) m_document.MarkDirty();

		f32_t pos[3] = { e.px, e.py, e.pz };
		if (ImGui::DragFloat3("Position", pos, 0.1f))
		{
			e.px = pos[0]; e.py = pos[1]; e.pz = pos[2];
			m_document.MarkDirty();
		}

		if (ImGui::DragFloat("Scale", &e.scale, 0.01f, 0.001f, 100.f)) m_document.MarkDirty();

		bool bVisible = e.bVisible != 0;
		if (ImGui::Checkbox("Visible", &bVisible))
		{
			e.bVisible = bVisible ? 1u : 0u;
			m_document.MarkDirty();
		}

		if (ImGui::Button("Delete"))
		{
			data.jungles.erase(data.jungles.begin() + idx);
			bDeleted = true;
		}
		break;
	}
	case eCategory::MinionWP:
	{
		if (m_iSelectedIndex < 0 || idx >= data.minionWaypoints.size())
			break;

		auto& e = data.minionWaypoints[idx];
		ImGui::Text("Team %u / Lane %u / Order %u", e.team, e.lane, e.order);

		f32_t pos[3] = { e.px, e.py, e.pz };
		if (ImGui::DragFloat3("Position", pos, 0.1f))
		{
			e.px = pos[0]; e.py = pos[1]; e.pz = pos[2];
			m_document.MarkDirty();
		}

		if (ImGui::Button("Delete"))
		{
			const u32_t team = e.team;
			const u32_t lane = e.lane;
			data.minionWaypoints.erase(data.minionWaypoints.begin() + idx);
			RenormalizeWaypointOrders(team, lane);
			bDeleted = true;
		}
		break;
	}
	case eCategory::Bush:
	{
		if (m_iSelectedIndex < 0 || idx >= data.bushes.size())
			break;

		auto& e = data.bushes[idx];
		if (ImGui::InputText("Name", e.name, sizeof(e.name))) m_document.MarkDirty();
		if (ImGui::InputScalar("BushId", ImGuiDataType_U32, &e.bushId)) m_document.MarkDirty();

		f32_t pos[3] = { e.px, e.py, e.pz };
		if (ImGui::DragFloat3("Position", pos, 0.1f))
		{
			e.px = pos[0]; e.py = pos[1]; e.pz = pos[2];
			m_document.MarkDirty();
		}

		if (ImGui::DragFloat("Yaw", &e.yaw, 0.01f)) m_document.MarkDirty();
		if (ImGui::DragFloat("Radius", &e.radius, 0.1f, 0.1f, 64.f)) m_document.MarkDirty();
		if (ImGui::DragFloat("Width", &e.width, 0.1f, 0.1f, 64.f)) m_document.MarkDirty();
		if (ImGui::DragFloat("Height", &e.height, 0.1f, 0.1f, 64.f)) m_document.MarkDirty();
		if (ImGui::InputText("Asset", e.assetPath, sizeof(e.assetPath))) m_document.MarkDirty();

		bool bVisible = e.bVisible != 0;
		if (ImGui::Checkbox("Visible", &bVisible))
		{
			e.bVisible = bVisible ? 1u : 0u;
			m_document.MarkDirty();
		}

		if (ImGui::Button("Delete"))
		{
			data.bushes.erase(data.bushes.begin() + idx);
			bDeleted = true;
		}
		break;
	}
	}

	if (bDeleted)
	{
		m_iSelectedIndex = -1;
		m_document.MarkDirty();
	}

	ImGui::End();
}

void CLoLMapEditorScene::SaveAll()
{
	if (!m_document.SaveStage())
	{
		m_StatusMessage = "stage save FAILED";
		return;
	}

	if (m_pEditorNavGrid)
	{
		wchar_t navPath[MAX_PATH] = {};
		if (CLoLStageDocument::ResolveNavGridPath(m_iCurrentStage, navPath, MAX_PATH))
		{
			if (!Engine::CNavGrid::SaveToFile(navPath, *m_pEditorNavGrid))
			{
				m_StatusMessage = "navgrid save FAILED";
				return;
			}
		}
	}

	m_StatusMessage = "saved";
#ifdef _DEBUG
	::OutputDebugStringA("[LoLEditor] stage+navgrid saved\n");
#endif
}

void CLoLMapEditorScene::LoadAll()
{
	if (!m_document.LoadStage(m_iCurrentStage))
	{
		m_document.NewStage(m_iCurrentStage);
		m_StatusMessage = "load failed -> new empty stage";
	}
	else
	{
		m_StatusMessage = "loaded";
	}

	m_iSelectedIndex = -1;
	LoadOrCreateNavGrid();
}

void CLoLMapEditorScene::LoadOrCreateNavGrid()
{
	m_pEditorNavGrid.reset();

	wchar_t navPath[MAX_PATH] = {};
	if (CLoLStageDocument::ResolveNavGridPath(m_iCurrentStage, navPath, MAX_PATH))
		m_pEditorNavGrid = Engine::CNavGrid::LoadFromFile(navPath);

	if (!m_pEditorNavGrid)
		m_pEditorNavGrid = CreateStageBoundsNavGrid();
}

std::unique_ptr<Engine::CNavGrid> CLoLMapEditorScene::CreateStageBoundsNavGrid() const
{
	bool bHasPoint = false;
	f32_t fMinX = 0.f, fMaxX = 0.f, fMinZ = 0.f, fMaxZ = 0.f;

	auto includePoint = [&](f32_t x, f32_t z)
	{
		if (!bHasPoint)
		{
			bHasPoint = true;
			fMinX = fMaxX = x;
			fMinZ = fMaxZ = z;
			return;
		}
		fMinX = (x < fMinX) ? x : fMinX;
		fMaxX = (x > fMaxX) ? x : fMaxX;
		fMinZ = (z < fMinZ) ? z : fMinZ;
		fMaxZ = (z > fMaxZ) ? z : fMaxZ;
	};

	const auto& data = m_document.Data();
	for (const auto& e : data.structures)      includePoint(e.px, e.pz);
	for (const auto& e : data.jungles)         includePoint(e.px, e.pz);
	for (const auto& e : data.minionWaypoints) includePoint(e.px, e.pz);

	const f32_t gridWorldX = Engine::CNavGrid::kCellCountX * Engine::CNavGrid::kCellSize;
	const f32_t gridWorldZ = Engine::CNavGrid::kCellCountY * Engine::CNavGrid::kCellSize;

	f32_t originX = -gridWorldX * 0.5f;
	f32_t originZ = -gridWorldZ * 0.5f;

	if (bHasPoint)
	{
		const f32_t centerX = (fMinX + fMaxX) * 0.5f;
		const f32_t centerZ = (fMinZ + fMaxZ) * 0.5f;
		originX = SnapDownToNavCell(centerX - gridWorldX * 0.5f);
		originZ = SnapDownToNavCell(centerZ - gridWorldZ * 0.5f);
	}

	std::unique_ptr<Engine::CNavGrid> pNavGrid = Engine::CNavGrid::Create(originX, originZ);
	if (pNavGrid)
		pNavGrid->SetAllWalkable(true);

	return pNavGrid;
}

void CLoLMapEditorScene::HandleMousePlacement()
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

	auto& data = m_document.Data();

	switch (m_eAddMode)
	{
	case eAddMode::Structure:
	{
		u32_t tier = m_PendingStructTier;
		u32_t lane = m_PendingStructLane;
		if (m_PendingStructKind != WMap::eObjectKind::Structure_Turret)
		{
			tier = static_cast<u32_t>(WMap::eTurretTier::None);
			lane = static_cast<u32_t>(WMap::eLane::Base);
		}

		WMap::StructureEntry e{};
		snprintf(e.name, sizeof(e.name), "%s_%02zu",
			StructureKindLabel(m_PendingStructKind), data.structures.size());
		e.subKind = static_cast<u32_t>(m_PendingStructKind);
		e.team = m_PendingStructTeam;
		e.tier = tier;
		e.lane = lane;
		e.px = ground.x; e.py = ground.y; e.pz = ground.z;
		data.structures.push_back(e);

		m_eSelectedCategory = eCategory::Structure;
		m_iSelectedIndex = static_cast<i32_t>(data.structures.size()) - 1;
		m_document.MarkDirty();
		break;
	}
	case eAddMode::Jungle:
	{
		WMap::JungleEntry e{};
		snprintf(e.name, sizeof(e.name), "Jungle_%02zu", data.jungles.size());
		e.subKind = m_PendingJungleSub;
		e.campId = m_PendingJungleCamp;
		e.px = ground.x; e.py = ground.y; e.pz = ground.z;
		data.jungles.push_back(e);

		m_eSelectedCategory = eCategory::Jungle;
		m_iSelectedIndex = static_cast<i32_t>(data.jungles.size()) - 1;
		m_document.MarkDirty();
		break;
	}
	case eAddMode::MinionWP:
	{
		AppendWaypoint(m_PendingMinionTeam, m_PendingMinionLane, ground);
		m_eSelectedCategory = eCategory::MinionWP;
		m_iSelectedIndex = static_cast<i32_t>(data.minionWaypoints.size()) - 1;
		m_document.MarkDirty();
		break;
	}
	case eAddMode::Bush:
	{
		WMap::BushEntry e{};
		snprintf(e.name, sizeof(e.name), "Bush_%02zu", data.bushes.size());
		e.bushId = m_PendingBushId;
		e.px = ground.x; e.py = ground.y; e.pz = ground.z;
		e.radius = m_PendingBushRadius;
		e.width = m_PendingBushWidth;
		e.height = m_PendingBushHeight;
		strncpy_s(e.assetPath, m_szPendingBushAsset, _TRUNCATE);
		data.bushes.push_back(e);

		m_eSelectedCategory = eCategory::Bush;
		m_iSelectedIndex = static_cast<i32_t>(data.bushes.size()) - 1;
		m_document.MarkDirty();
		break;
	}
	case eAddMode::NavGrid:
		break;
	}
}

bool_t CLoLMapEditorScene::TryPickGroundPlane(Vec3& outWorld) const
{
	if (!m_pCamera)
		return false;

	const auto& input = CInput::Get();
	const CInput::MouseRay ray = input.GetMouseWorldRay(
		*m_pCamera,
		static_cast<i32_t>(LoLEditor::kWindowWidth),
		static_cast<i32_t>(LoLEditor::kWindowHeight));

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

void CLoLMapEditorScene::PaintNavGridAt(const Vec3& worldPos, bool_t bWalkable)
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

	m_document.MarkDirty();
}

void CLoLMapEditorScene::RenderMarkers(const Mat4& matVP) const
{
	ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
	const DirectX::XMMATRIX mVP = matVP.ToXMMATRIX();
	const auto& data = m_document.Data();

	for (size_t i = 0; i < data.structures.size(); ++i)
	{
		const auto& e = data.structures[i];
		ImVec2 s{};
		if (!WorldToScreen(mVP, { e.px, e.py, e.pz }, s))
			continue;

		pDraw->AddCircleFilled(s, 7.f, TeamColor(e.team));
		if (m_eSelectedCategory == eCategory::Structure &&
			m_iSelectedIndex == static_cast<i32_t>(i))
		{
			pDraw->AddCircle(s, 11.f, IM_COL32(255, 255, 0, 255), 0, 2.f);
		}
		pDraw->AddText(ImVec2(s.x + 10.f, s.y - 8.f), IM_COL32_WHITE, e.name);
	}

	for (size_t i = 0; i < data.jungles.size(); ++i)
	{
		const auto& e = data.jungles[i];
		ImVec2 s{};
		if (!WorldToScreen(mVP, { e.px, e.py, e.pz }, s))
			continue;

		pDraw->AddCircleFilled(s, 6.f, IM_COL32(120, 220, 120, 255));
		if (m_eSelectedCategory == eCategory::Jungle &&
			m_iSelectedIndex == static_cast<i32_t>(i))
		{
			pDraw->AddCircle(s, 10.f, IM_COL32(255, 255, 0, 255), 0, 2.f);
		}

		char lbl[80] = {};
		snprintf(lbl, sizeof(lbl), "%s(%u)", e.name, e.campId);
		pDraw->AddText(ImVec2(s.x + 9.f, s.y - 8.f), IM_COL32_WHITE, lbl);
	}

	for (size_t i = 0; i < data.bushes.size(); ++i)
	{
		const auto& e = data.bushes[i];
		ImVec2 s{};
		if (!WorldToScreen(mVP, { e.px, e.py, e.pz }, s))
			continue;

		pDraw->AddCircle(s, 8.f, IM_COL32(60, 160, 60, 255), 0, 2.f);
		if (m_eSelectedCategory == eCategory::Bush &&
			m_iSelectedIndex == static_cast<i32_t>(i))
		{
			pDraw->AddCircle(s, 12.f, IM_COL32(255, 255, 0, 255), 0, 2.f);
		}
		pDraw->AddText(ImVec2(s.x + 10.f, s.y - 8.f), IM_COL32_WHITE, e.name);
	}

	// 편집 중인 팀/레인 웨이포인트 폴리라인
	{
		std::vector<const WMap::MinionWaypointEntry*> lanePoints;
		for (const auto& wp : data.minionWaypoints)
		{
			if (wp.team == m_PendingMinionTeam && wp.lane == m_PendingMinionLane)
				lanePoints.push_back(&wp);
		}
		std::sort(lanePoints.begin(), lanePoints.end(),
			[](const WMap::MinionWaypointEntry* a, const WMap::MinionWaypointEntry* b)
			{
				return a->order < b->order;
			});

		const ImU32 colLine = (m_PendingMinionTeam == 0)
			? IM_COL32(80, 160, 255, 220) : IM_COL32(255, 100, 100, 220);
		const ImU32 colDot = IM_COL32(255, 255, 0, 255);

		ImVec2 prev{};
		bool havePrev = false;
		for (const auto* pW : lanePoints)
		{
			ImVec2 s{};
			if (!WorldToScreen(mVP, { pW->px, pW->py, pW->pz }, s))
			{
				havePrev = false;
				continue;
			}

			if (havePrev)
				pDraw->AddLine(prev, s, colLine, 2.f);

			pDraw->AddCircleFilled(s, 6.f, colDot);

			char lbl[8] = {};
			snprintf(lbl, sizeof(lbl), "%u", pW->order);
			pDraw->AddText(ImVec2(s.x + 8.f, s.y - 8.f), IM_COL32_WHITE, lbl);

			prev = s;
			havePrev = true;
		}
	}
}

void CLoLMapEditorScene::RenderNavGridOverlay(const Mat4& matVP) const
{
	if (!m_bShowNavGridOverlay || !m_pEditorNavGrid)
		return;

	ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
	const DirectX::XMMATRIX mVP = matVP.ToXMMATRIX();
	const f32_t half = Engine::CNavGrid::kCellSize * 0.5f;
	const ImU32 color = IM_COL32(255, 230, 0, 210);

	for (u32_t y = 0; y < Engine::CNavGrid::kCellCountY; ++y)
	{
		for (u32_t x = 0; x < Engine::CNavGrid::kCellCountX; ++x)
		{
			if (m_pEditorNavGrid->IsWalkable(static_cast<int32_t>(x), static_cast<int32_t>(y)))
				continue;

			const Vec3 center = m_pEditorNavGrid->CellToWorld(
				static_cast<int32_t>(x), static_cast<int32_t>(y));
			const Vec3 p0{ center.x - half, 0.05f, center.z - half };
			const Vec3 p1{ center.x + half, 0.05f, center.z - half };
			const Vec3 p2{ center.x + half, 0.05f, center.z + half };
			const Vec3 p3{ center.x - half, 0.05f, center.z + half };

			ImVec2 s0{}, s1{}, s2{}, s3{};
			if (!WorldToScreen(mVP, p0, s0) ||
				!WorldToScreen(mVP, p1, s1) ||
				!WorldToScreen(mVP, p2, s2) ||
				!WorldToScreen(mVP, p3, s3))
			{
				continue;
			}

			pDraw->AddQuad(s0, s1, s2, s3, color, 1.f);
		}
	}
}

u32_t CLoLMapEditorScene::CountWaypoints(u32_t team, u32_t lane) const
{
	u32_t count = 0;
	for (const auto& wp : m_document.Data().minionWaypoints)
	{
		if (wp.team == team && wp.lane == lane)
			++count;
	}
	return count;
}

void CLoLMapEditorScene::AppendWaypoint(u32_t team, u32_t lane, const Vec3& pos)
{
	WMap::MinionWaypointEntry e{};
	e.team = team;
	e.lane = lane;
	e.order = CountWaypoints(team, lane);
	e.px = pos.x; e.py = pos.y; e.pz = pos.z;
	m_document.Data().minionWaypoints.push_back(e);
}

void CLoLMapEditorScene::RenormalizeWaypointOrders(u32_t team, u32_t lane)
{
	std::vector<WMap::MinionWaypointEntry*> lanePoints;
	for (auto& wp : m_document.Data().minionWaypoints)
	{
		if (wp.team == team && wp.lane == lane)
			lanePoints.push_back(&wp);
	}

	std::sort(lanePoints.begin(), lanePoints.end(),
		[](const WMap::MinionWaypointEntry* a, const WMap::MinionWaypointEntry* b)
		{
			return a->order < b->order;
		});

	for (u32_t i = 0; i < static_cast<u32_t>(lanePoints.size()); ++i)
		lanePoints[i]->order = i;
}
