#include "EldenRingEditorScene.h"

#include "Cinematic/CSequenceAsset.h"
#include "FX/Exec/FxExecPlan.h"
#include "FX/Graph/FxGraph.h"
#include "FX/Graph/FxGraphValidator.h"
#include "Physics3D/HitVolume.h"
#include "World/AssetStreamingSystem.h"
#include "World/WorldPartitionSystem.h"

#include <imgui.h>
#include <Windows.h>

#include <cmath>
#include <cstdio>
#include <sstream>
#include <memory>
#include <vector>

namespace
{
	constexpr char kDefaultCellPath[] = "Client/Bin/Resource/EldenRing/World/editor_seed_cell.json";
	constexpr char kDefaultSeedName[] = "SeedPlacement";
	constexpr char kDefaultSeedWMesh[] = "Client/Bin/Resource/EldenRing/Assets/seed.wmesh";
	constexpr char kDefaultFxGraphPath[] = "Client/Bin/Resource/EldenRing/FX/editor_seed.wfx.json";
	constexpr char kDefaultSequencePath[] = "Client/Bin/Resource/EldenRing/Sequences/editor_seed.wseq.json";
	constexpr char kDefaultWorldPartitionPath[] = "Client/Bin/Resource/EldenRing/World/editor_world.json";
	constexpr f32_t kTransformEpsilon = 0.0001f;

	void CopyCString(char* pDestination, size_t destinationSize, const char* pSource)
	{
		if (!pDestination || destinationSize == 0)
			return;

		std::snprintf(pDestination, destinationSize, "%s", pSource ? pSource : "");
	}

	bool SameVec3(const Vec3& lhs, const Vec3& rhs)
	{
		return std::fabs(lhs.x - rhs.x) <= kTransformEpsilon
			&& std::fabs(lhs.y - rhs.y) <= kTransformEpsilon
			&& std::fabs(lhs.z - rhs.z) <= kTransformEpsilon;
	}

	bool SameTransform(const WorldPlacement& lhs, const WorldPlacement& rhs)
	{
		return SameVec3(lhs.position, rhs.position)
			&& SameVec3(lhs.rotationDeg, rhs.rotationDeg)
			&& SameVec3(lhs.scale, rhs.scale);
	}

	const char* PlacementDisplayName(const WorldPlacement& placement)
	{
		if (!placement.name.empty())
			return placement.name.c_str();
		if (!placement.wmesh.empty())
			return placement.wmesh.c_str();
		return "Unnamed Placement";
	}

	u32_t CountSequenceKeys(const CSequenceAsset& asset)
	{
		u32_t count = 0;
		for (const SeqTrack& track : asset.tracks)
		{
			count += static_cast<u32_t>(track.cameraKeys.size());
			count += static_cast<u32_t>(track.animKeys.size());
			count += static_cast<u32_t>(track.fxKeys.size());
			count += static_cast<u32_t>(track.audioKeys.size());
			count += static_cast<u32_t>(track.eventKeys.size());
			count += static_cast<u32_t>(track.visibilityKeys.size());
			count += static_cast<u32_t>(track.timeDilationKeys.size());
		}
		return count;
	}

	std::string FormatCellCounts(const Engine::CellStateCounts& counts)
	{
		char szLine[256] = {};
		std::snprintf(
			szLine,
			sizeof(szLine),
			"states U:%u Q:%u H:%u V:%u",
			counts.uUnloaded,
			counts.uQueued,
			counts.uLoadedHidden,
			counts.uVisible);
		return szLine;
	}
}

std::unique_ptr<CEldenRingEditorScene> CEldenRingEditorScene::Create()
{
	return std::unique_ptr<CEldenRingEditorScene>(new CEldenRingEditorScene());
}

bool CEldenRingEditorScene::OnEnter()
{
	m_worldCellDocument.Clear();
	m_transaction.Clear();
	m_selectedPlacementId = 0;
	m_bDetailsTransformEditing = false;
	m_detailsTransformEditingId = 0;
	CopyCString(m_cellLoadPath, sizeof(m_cellLoadPath), kDefaultCellPath);
	CopyCString(m_cellSavePath, sizeof(m_cellSavePath), kDefaultCellPath);
	CopyCString(m_seedName, sizeof(m_seedName), kDefaultSeedName);
	CopyCString(m_seedWMeshPath, sizeof(m_seedWMeshPath), kDefaultSeedWMesh);
	CopyCString(m_fxGraphPath, sizeof(m_fxGraphPath), kDefaultFxGraphPath);
	CopyCString(m_sequencePath, sizeof(m_sequencePath), kDefaultSequencePath);
	CopyCString(m_worldPartitionPath, sizeof(m_worldPartitionPath), kDefaultWorldPartitionPath);
	m_fxGraphStatus = "FX graph validator ready.";
	m_fxGraphIssues.clear();
	m_fxGraphEmitterCount = 0;
	m_fxGraphCompiledCount = 0;
	m_sequenceStatus = "Sequencer validator ready.";
	m_sequenceIssues.clear();
	m_sequenceTrackCount = 0;
	m_sequenceKeyCount = 0;
	m_sequenceDurationSec = 0.0;
	m_worldPartitionStatus = "World partition probe ready.";
	m_worldPartitionRows.clear();
	m_worldPartitionCellCount = 0;
	m_worldPartitionVisibleInstances = 0;
	m_bossTestingStatus = "Boss hitbox probe ready.";
	m_bossTestingRows.clear();
	m_statusMessage = "World cell contract ready.";

#ifdef _DEBUG
	::OutputDebugStringA("[EldenRingEditorScene] OnEnter\n");
	::OutputDebugStringA("[WorldEditor] Contract panels initialized. Preview/gizmo/ray-pick are deferred.\n");
#endif
	return true;
}

void CEldenRingEditorScene::OnExit()
{
#ifdef _DEBUG
	::OutputDebugStringA("[EldenRingEditorScene] OnExit\n");
#endif
}

void CEldenRingEditorScene::OnUpdate(f32_t)
{}

void CEldenRingEditorScene::OnRender()
{}

void CEldenRingEditorScene::AddSeedPlacement()
{
	if (m_seedWMeshPath[0] == '\0')
	{
		m_statusMessage = "Seed placement requires a .wmesh path.";
		return;
	}

	CancelDetailsTransformEdit();

	WorldPlacement placement;
	placement.id = m_worldCellDocument.AllocPlacementId();
	placement.kind = "Asset";
	placement.name = m_seedName[0] != '\0' ? m_seedName : "SeedPlacement";
	placement.wmesh = m_seedWMeshPath;
	placement.position = Vec3{ 0.f, 0.f, 0.f };
	placement.rotationDeg = Vec3{ 0.f, 0.f, 0.f };
	placement.scale = Vec3{ 1.f, 1.f, 1.f };
	placement.animated = false;
	placement.transformResolved = true;

	const u32_t previousSelection = m_selectedPlacementId;
	m_transaction.Push(std::make_unique<CAddPlacementCommand>(
		&m_worldCellDocument,
		placement,
		&m_selectedPlacementId,
		previousSelection,
		m_worldCellDocument.Placements().size()));

	m_statusMessage = "Added seed placement through transaction.";
}

void CEldenRingEditorScene::DeleteSelectedPlacement()
{
	WorldPlacement* pPlacement = m_worldCellDocument.FindPlacement(m_selectedPlacementId);
	if (!pPlacement)
	{
		m_statusMessage = "No placement selected for delete.";
		return;
	}

	CancelDetailsTransformEdit();

	const size_t originalIndex = FindPlacementIndex(pPlacement->id);
	const u32_t fallbackSelection = FindDeleteFallbackSelection(originalIndex, pPlacement->id);
	const WorldPlacement placement = *pPlacement;

	m_transaction.Push(std::make_unique<CDeletePlacementCommand>(
		&m_worldCellDocument,
		placement,
		&m_selectedPlacementId,
		fallbackSelection,
		originalIndex));

	m_statusMessage = "Deleted placement through transaction.";
}

void CEldenRingEditorScene::CommitDetailsTransformEdit()
{
	if (!m_bDetailsTransformEditing)
		return;

	const u32_t editingId = m_detailsTransformEditingId;
	const WorldPlacement before = m_detailsTransformBefore;
	const WorldPlacement draft = m_detailsTransformDraft;

	m_bDetailsTransformEditing = false;
	m_detailsTransformEditingId = 0;

	if (!m_worldCellDocument.FindPlacement(editingId))
		return;

	if (SameTransform(before, draft))
	{
		m_statusMessage = "Transform unchanged.";
		return;
	}

	m_transaction.Push(std::make_unique<CTransformPlacementCommand>(
		&m_worldCellDocument,
		editingId,
		before.position,
		before.rotationDeg,
		before.scale,
		draft.position,
		draft.rotationDeg,
		draft.scale,
		&m_selectedPlacementId));

	m_statusMessage = "Committed transform through transaction.";
}

void CEldenRingEditorScene::CancelDetailsTransformEdit()
{
	m_bDetailsTransformEditing = false;
	m_detailsTransformEditingId = 0;
}

void CEldenRingEditorScene::EnsureValidSelection()
{
	if (m_selectedPlacementId == 0)
		return;

	if (m_worldCellDocument.FindPlacement(m_selectedPlacementId))
		return;

	m_selectedPlacementId = 0;
	CancelDetailsTransformEdit();
}

size_t CEldenRingEditorScene::FindPlacementIndex(u32_t placementId) const
{
	const std::vector<WorldPlacement>& placements = m_worldCellDocument.Placements();
	for (size_t i = 0; i < placements.size(); ++i)
	{
		if (placements[i].id == placementId)
			return i;
	}
	return placements.size();
}

u32_t CEldenRingEditorScene::FindDeleteFallbackSelection(size_t deletingIndex, u32_t deletingId) const
{
	const std::vector<WorldPlacement>& placements = m_worldCellDocument.Placements();
	if (placements.empty())
		return 0;

	for (size_t i = deletingIndex + 1; i < placements.size(); ++i)
	{
		if (placements[i].id != deletingId)
			return placements[i].id;
	}

	if (deletingIndex > 0)
	{
		for (size_t i = deletingIndex; i > 0; --i)
		{
			if (placements[i - 1].id != deletingId)
				return placements[i - 1].id;
		}
	}

	return 0;
}

void CEldenRingEditorScene::RunFxGraphValidation()
{
	m_fxGraphIssues.clear();
	m_fxGraphEmitterCount = 0;
	m_fxGraphCompiledCount = 0;

	CFxGraph graph;
	std::string error;
	if (!CFxGraph::LoadFromJson(m_fxGraphPath, graph, &error))
	{
		m_fxGraphStatus = "FX graph load failed: " + error;
		m_fxGraphIssues.push_back(m_fxGraphStatus);
		return;
	}

	m_fxGraphEmitterCount = static_cast<u32_t>(graph.emitterGraphs.size());
	if (graph.emitterGraphs.empty())
	{
		m_fxGraphStatus = "FX graph loaded, but no emitterGraphs were found.";
		m_fxGraphIssues.push_back("Add at least one emitter graph before compile.");
		return;
	}

	for (const FxEmitterGraph& emitter : graph.emitterGraphs)
	{
		FxValidationResult validation = CFxGraphValidator::Validate(emitter);
		for (const FxValidationIssue& issue : validation.issues)
		{
			std::ostringstream oss;
			oss << (issue.bError ? "error" : "warning")
				<< " emitter=" << (emitter.strName.empty() ? "<unnamed>" : emitter.strName)
				<< " node=" << issue.nodeId
				<< " " << issue.message;
			m_fxGraphIssues.push_back(oss.str());
		}

		if (!validation.bValid)
			continue;

		CFxExecPlan plan;
		std::string compileError;
		if (CFxGraphCompiler::Compile(emitter, validation.topoOrder, plan, compileError))
		{
			++m_fxGraphCompiledCount;
		}
		else
		{
			std::ostringstream oss;
			oss << "compile error emitter="
				<< (emitter.strName.empty() ? "<unnamed>" : emitter.strName)
				<< " " << compileError;
			m_fxGraphIssues.push_back(oss.str());
		}
	}

	std::ostringstream status;
	status << "FX graph validated: emitters=" << m_fxGraphEmitterCount
		<< " compiled=" << m_fxGraphCompiledCount
		<< " issues=" << m_fxGraphIssues.size();
	m_fxGraphStatus = status.str();
	m_statusMessage = m_fxGraphStatus;
}

void CEldenRingEditorScene::RunSequenceValidation()
{
	m_sequenceIssues.clear();
	m_sequenceTrackCount = 0;
	m_sequenceKeyCount = 0;
	m_sequenceDurationSec = 0.0;

	CSequenceAsset asset;
	if (!CSequenceAsset::LoadFromJson(m_sequencePath, asset))
	{
		m_sequenceStatus = "Sequence load failed.";
		m_sequenceIssues.push_back("Failed to read or parse sequence JSON.");
		return;
	}

	m_sequenceTrackCount = static_cast<u32_t>(asset.tracks.size());
	m_sequenceKeyCount = CountSequenceKeys(asset);
	m_sequenceDurationSec = asset.dDurationSec;

	std::vector<std::string> validationErrors;
	const bool_t bValid = asset.Validate(&validationErrors);
	m_sequenceIssues = validationErrors;

	std::ostringstream status;
	status << "Sequence " << (bValid ? "valid" : "invalid")
		<< ": tracks=" << m_sequenceTrackCount
		<< " keys=" << m_sequenceKeyCount
		<< " duration=" << m_sequenceDurationSec;
	m_sequenceStatus = status.str();
	m_statusMessage = m_sequenceStatus;
}

void CEldenRingEditorScene::RunWorldPartitionProbe()
{
	m_worldPartitionRows.clear();
	m_worldPartitionCellCount = 0;
	m_worldPartitionVisibleInstances = 0;

	std::unique_ptr<Engine::CAssetStreamingSystem> pStreaming = Engine::CAssetStreamingSystem::Create();
	std::unique_ptr<Engine::CWorldPartitionSystem> pPartition = Engine::CWorldPartitionSystem::Create(pStreaming.get());
	if (!pStreaming || !pPartition)
	{
		m_worldPartitionStatus = "World partition probe allocation failed.";
		m_worldPartitionRows.push_back(m_worldPartitionStatus);
		return;
	}

	if (!pPartition->LoadWorld(m_worldPartitionPath))
	{
		m_worldPartitionStatus = "World partition load failed.";
		m_worldPartitionRows.push_back("Path: " + std::string(m_worldPartitionPath));
		return;
	}

	Engine::StreamingSourceComponent source;
	source.vPosition = Vec3{ 0.f, 0.f, 0.f };
	source.fVisibleRadius = 160.f;
	source.fLoadRadius = 256.f;
	source.fUnloadRadius = 320.f;
	source.uPriority = 1u;
	source.bActive = true;
	pPartition->SetSource(1u, source);
	pPartition->Update(0.f);
	pPartition->Update(0.f);
	pPartition->Update(0.f);

	std::vector<Engine::VisibleInstance> visibleInstances;
	pPartition->CollectVisibleInstances(visibleInstances);
	Engine::WorldPartitionDebugStats stats = pPartition->GetDebugStats();

	m_worldPartitionCellCount = stats.uCellCount;
	m_worldPartitionVisibleInstances = static_cast<u32_t>(visibleInstances.size());
	m_worldPartitionRows.push_back(FormatCellCounts(stats.stateCounts));
	m_worldPartitionRows.push_back("sources=" + std::to_string(stats.uSourceCount)
		+ " transitions=" + std::to_string(stats.uTotalTransitions));
	m_worldPartitionRows.push_back("missing required=" + std::to_string(stats.uMissingRequiredAssets)
		+ " optional=" + std::to_string(stats.uMissingOptionalAssets));
	m_worldPartitionRows.push_back("skipped not-placeable=" + std::to_string(stats.uSkippedNotPlaceableInstances)
		+ " missing-asset=" + std::to_string(stats.uSkippedMissingAssetInstances)
		+ " not-ready=" + std::to_string(stats.uSkippedNotReadyInstances));

	for (const Engine::WorldCellRuntime& cell : pPartition->GetCells())
	{
		if (!cell.pDesc)
			continue;
		std::ostringstream oss;
		oss << cell.pDesc->strId
			<< " state=" << Engine::ToString(cell.eState)
			<< " desired=" << Engine::ToString(cell.eDesiredState)
			<< " reason=" << Engine::ToString(cell.eLastReason)
			<< " transitions=" << cell.uTransitionCount;
		m_worldPartitionRows.push_back(oss.str());
		if (m_worldPartitionRows.size() >= 10)
			break;
	}

	std::ostringstream status;
	status << "World partition probe complete: cells=" << m_worldPartitionCellCount
		<< " visibleInstances=" << m_worldPartitionVisibleInstances;
	m_worldPartitionStatus = status.str();
	m_statusMessage = m_worldPartitionStatus;
}

void CEldenRingEditorScene::RunBossHitboxProbe()
{
	m_bossTestingRows.clear();

	const HitVolume hurtbox = WintersPhysics3D::MakeAABB(
		Vec3{ 0.f, 1.f, 0.f },
		Vec3{ 0.6f, 1.f, 0.6f });
	const HitVolume slash = WintersPhysics3D::MakeOBB(
		Vec3{ 0.5f, 1.f, 0.1f },
		Vec3{ 0.9f, 0.35f, 0.25f },
		0.35f);
	const HitVolume shockwave = WintersPhysics3D::MakeSphere(
		Vec3{ 1.7f, 0.2f, 0.f },
		1.25f);
	const HitActiveWindow window = WintersPhysics3D::MakeActiveWindow(18, 26, 8);

	const bool_t bSlashHits = WintersPhysics3D::Overlap(hurtbox, slash);
	const bool_t bShockwaveHits = WintersPhysics3D::Overlap(hurtbox, shockwave);
	m_bossTestingRows.push_back(WintersPhysics3D::ToDebugString(hurtbox));
	m_bossTestingRows.push_back(WintersPhysics3D::ToDebugString(slash));
	m_bossTestingRows.push_back(WintersPhysics3D::ToDebugString(shockwave));
	m_bossTestingRows.push_back(WintersPhysics3D::ToDebugString(window));
	m_bossTestingRows.push_back(std::string("slash overlap=") + (bSlashHits ? "true" : "false"));
	m_bossTestingRows.push_back(std::string("shockwave overlap=") + (bShockwaveHits ? "true" : "false"));
	m_bossTestingRows.push_back("active frames=" + std::to_string(WintersPhysics3D::ActiveWindowLengthFrames(window))
		+ " dodge frames=" + std::to_string(WintersPhysics3D::DodgeWindowLengthFrames(window)));

	m_bossTestingStatus = "Boss hitbox geometry probe complete.";
	m_statusMessage = m_bossTestingStatus;
}

void CEldenRingEditorScene::OnImGui()
{
	EnsureValidSelection();
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None);

	DrawViewportPanel();
	DrawContentBrowserPanel();
	DrawWorldCellPanel();
	DrawWorldOutlinerPanel();
	DrawDetailsPanel();
	DrawTransactionPanel();
	DrawFxGraphPanel();
	DrawSequencerPanel();
	DrawWorldPartitionPanel();
	DrawBossTestingPanel();
	DrawLogPanel();
}

void CEldenRingEditorScene::DrawViewportPanel()
{
	if (!ImGui::Begin("Viewport"))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("Preview pending");
	ImGui::TextDisabled("ModelRenderer preview, ray-pick, and gizmo are deferred for the next WORLD_EDITOR gate.");
	ImGui::End();
}

void CEldenRingEditorScene::DrawContentBrowserPanel()
{
	if (!ImGui::Begin("Content Browser"))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("Asset catalog pending");
	ImGui::TextDisabled(".wmesh scan and drag-drop are intentionally outside this slice.");
	ImGui::End();
}

void CEldenRingEditorScene::DrawWorldCellPanel()
{
	if (!ImGui::Begin("World Cell"))
	{
		ImGui::End();
		return;
	}

	const Vec3& origin = m_worldCellDocument.Origin();
	ImGui::Text("Schema: %s", m_worldCellDocument.Schema().c_str());
	ImGui::Text("Cell: %s", m_worldCellDocument.CellId().c_str());
	ImGui::Text("Area: %u  Block: %u,%u  Variant: %u",
		m_worldCellDocument.Area(),
		m_worldCellDocument.BlockX(),
		m_worldCellDocument.BlockY(),
		m_worldCellDocument.Variant());
	ImGui::Text("Cell Size: %.2fm", m_worldCellDocument.CellSizeMeters());
	ImGui::Text("Origin: %.2f, %.2f, %.2f", origin.x, origin.y, origin.z);
	ImGui::Text("Data Layer: %s", m_worldCellDocument.DataLayer().c_str());
	ImGui::Separator();
	ImGui::Text("Placements: %zu", m_worldCellDocument.Placements().size());
	ImGui::Text("References: %zu", m_worldCellDocument.References().size());

	ImGui::SeparatorText("File");
	ImGui::InputText("Load Path", m_cellLoadPath, sizeof(m_cellLoadPath));
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		CancelDetailsTransformEdit();
		if (m_worldCellDocument.Load(m_cellLoadPath))
		{
			m_transaction.Clear();
			CopyCString(m_cellSavePath, sizeof(m_cellSavePath), m_cellLoadPath);
			m_selectedPlacementId = m_worldCellDocument.Placements().empty()
				? 0
				: m_worldCellDocument.Placements().front().id;
			m_statusMessage = "Loaded world cell document.";
		}
		else
		{
			m_statusMessage = "Failed to load world cell document.";
		}
	}

	ImGui::InputText("Save Path", m_cellSavePath, sizeof(m_cellSavePath));
	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		CommitDetailsTransformEdit();
		if (m_worldCellDocument.Save(m_cellSavePath))
			m_statusMessage = "Saved world cell document.";
		else
			m_statusMessage = "Failed to save world cell document.";
	}

	ImGui::SeparatorText("Seed Placement");
	ImGui::InputText("Seed Name", m_seedName, sizeof(m_seedName));
	ImGui::InputText("Seed WMesh", m_seedWMeshPath, sizeof(m_seedWMeshPath));
	ImGui::BeginDisabled(m_seedWMeshPath[0] == '\0');
	if (ImGui::Button("Add Seed Placement"))
		AddSeedPlacement();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(m_selectedPlacementId == 0);
	if (ImGui::Button("Delete Selected"))
		DeleteSelectedPlacement();
	ImGui::EndDisabled();
	ImGui::End();
}

void CEldenRingEditorScene::DrawWorldOutlinerPanel()
{
	if (!ImGui::Begin("World Outliner"))
	{
		ImGui::End();
		return;
	}

	if (m_worldCellDocument.Placements().empty())
	{
		ImGui::TextDisabled("No placements loaded.");
	}
	else
	{
		for (const WorldPlacement& placement : m_worldCellDocument.Placements())
		{
			char szLabel[256] = {};
			std::snprintf(
				szLabel,
				sizeof(szLabel),
				"#%u %s",
				placement.id,
				PlacementDisplayName(placement));
			if (ImGui::Selectable(szLabel, m_selectedPlacementId == placement.id))
			{
				if (m_selectedPlacementId != placement.id)
					CancelDetailsTransformEdit();
				m_selectedPlacementId = placement.id;
			}
		}
	}

	if (!m_worldCellDocument.References().empty())
	{
		ImGui::SeparatorText("References");
		for (const WorldReference& reference : m_worldCellDocument.References())
		{
			ImGui::TextDisabled("%s  %s",
				reference.kind.c_str(),
				reference.model.c_str());
		}
	}

	ImGui::End();
}

void CEldenRingEditorScene::DrawDetailsPanel()
{
	if (!ImGui::Begin("Details"))
	{
		ImGui::End();
		return;
	}

	const WorldPlacement* pPlacement = m_worldCellDocument.FindPlacement(m_selectedPlacementId);
	if (!pPlacement)
	{
		ImGui::TextDisabled("No placement selected.");
		ImGui::End();
		return;
	}

	ImGui::Text("Placement #%u", pPlacement->id);
	ImGui::Text("Kind: %s", pPlacement->kind.c_str());
	ImGui::Text("Name: %s", pPlacement->name.c_str());
	ImGui::TextWrapped("WMesh: %s", pPlacement->wmesh.c_str());
	ImGui::Separator();

	WorldPlacement displayPlacement = *pPlacement;
	if (m_bDetailsTransformEditing && m_detailsTransformEditingId == pPlacement->id)
		displayPlacement = m_detailsTransformDraft;

	auto beginTransformEdit = [&]()
	{
		if (m_bDetailsTransformEditing && m_detailsTransformEditingId == pPlacement->id)
			return;

		m_bDetailsTransformEditing = true;
		m_detailsTransformEditingId = pPlacement->id;
		m_detailsTransformBefore = *pPlacement;
		m_detailsTransformDraft = *pPlacement;
	};

	bool bCommitTransform = false;
	float position[3] =
	{
		displayPlacement.position.x,
		displayPlacement.position.y,
		displayPlacement.position.z
	};
	if (ImGui::DragFloat3("Position", position, 0.05f))
	{
		beginTransformEdit();
		m_detailsTransformDraft.position = Vec3{ position[0], position[1], position[2] };
		m_statusMessage = "Editing placement position.";
	}
	bCommitTransform = bCommitTransform || ImGui::IsItemDeactivatedAfterEdit();

	float rotationDeg[3] =
	{
		displayPlacement.rotationDeg.x,
		displayPlacement.rotationDeg.y,
		displayPlacement.rotationDeg.z
	};
	if (ImGui::DragFloat3("Rotation", rotationDeg, 0.25f))
	{
		beginTransformEdit();
		m_detailsTransformDraft.rotationDeg = Vec3{ rotationDeg[0], rotationDeg[1], rotationDeg[2] };
		m_statusMessage = "Editing placement rotation.";
	}
	bCommitTransform = bCommitTransform || ImGui::IsItemDeactivatedAfterEdit();

	float scale[3] =
	{
		displayPlacement.scale.x,
		displayPlacement.scale.y,
		displayPlacement.scale.z
	};
	if (ImGui::DragFloat3("Scale", scale, 0.01f, 0.001f, 1000.f))
	{
		beginTransformEdit();
		m_detailsTransformDraft.scale = Vec3{ scale[0], scale[1], scale[2] };
		m_statusMessage = "Editing placement scale.";
	}
	bCommitTransform = bCommitTransform || ImGui::IsItemDeactivatedAfterEdit();

	if (bCommitTransform)
		CommitDetailsTransformEdit();

	ImGui::Text("Animated: %s", pPlacement->animated ? "true" : "false");
	ImGui::Text("Transform Resolved: %s", pPlacement->transformResolved ? "true" : "false");
	ImGui::Separator();
	if (ImGui::Button("Delete Placement"))
		DeleteSelectedPlacement();
	ImGui::End();
}

void CEldenRingEditorScene::DrawTransactionPanel()
{
	if (!ImGui::Begin("Transaction Stack"))
	{
		ImGui::End();
		return;
	}

	ImGui::BeginDisabled(!m_transaction.CanUndo());
	if (ImGui::Button("Undo"))
	{
		CancelDetailsTransformEdit();
		m_transaction.Undo();
		EnsureValidSelection();
		m_statusMessage = "Undo applied.";
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!m_transaction.CanRedo());
	if (ImGui::Button("Redo"))
	{
		CancelDetailsTransformEdit();
		m_transaction.Redo();
		EnsureValidSelection();
		m_statusMessage = "Redo applied.";
	}
	ImGui::EndDisabled();

	ImGui::Text("Undo: %zu  Redo: %zu", m_transaction.UndoCount(), m_transaction.RedoCount());
	ImGui::Separator();

	const std::vector<std::string>& history = m_transaction.History();
	if (history.empty())
	{
		ImGui::TextDisabled("No commands.");
	}
	else
	{
		for (const std::string& label : history)
			ImGui::BulletText("%s", label.c_str());
	}

	ImGui::End();
}

void CEldenRingEditorScene::DrawFxGraphPanel()
{
	if (!ImGui::Begin("FX Graph"))
	{
		ImGui::End();
		return;
	}

	ImGui::InputText("WFX JSON", m_fxGraphPath, sizeof(m_fxGraphPath));
	ImGui::BeginDisabled(m_fxGraphPath[0] == '\0');
	if (ImGui::Button("Load / Validate / Compile"))
		RunFxGraphValidation();
	ImGui::EndDisabled();
	ImGui::Separator();
	ImGui::TextWrapped("%s", m_fxGraphStatus.c_str());
	ImGui::Text("Emitters: %u  Compiled: %u", m_fxGraphEmitterCount, m_fxGraphCompiledCount);
	if (m_fxGraphIssues.empty())
	{
		ImGui::TextDisabled("No validation issues.");
	}
	else
	{
		for (const std::string& issue : m_fxGraphIssues)
			ImGui::BulletText("%s", issue.c_str());
	}

	ImGui::End();
}

void CEldenRingEditorScene::DrawSequencerPanel()
{
	if (!ImGui::Begin("Sequencer"))
	{
		ImGui::End();
		return;
	}

	ImGui::InputText("WSEQ JSON", m_sequencePath, sizeof(m_sequencePath));
	ImGui::BeginDisabled(m_sequencePath[0] == '\0');
	if (ImGui::Button("Load / Validate"))
		RunSequenceValidation();
	ImGui::EndDisabled();
	ImGui::Separator();
	ImGui::TextWrapped("%s", m_sequenceStatus.c_str());
	ImGui::Text("Tracks: %u  Keys: %u  Duration: %.3f",
		m_sequenceTrackCount,
		m_sequenceKeyCount,
		m_sequenceDurationSec);
	if (m_sequenceIssues.empty())
	{
		ImGui::TextDisabled("No validation issues.");
	}
	else
	{
		for (const std::string& issue : m_sequenceIssues)
			ImGui::BulletText("%s", issue.c_str());
	}

	ImGui::End();
}

void CEldenRingEditorScene::DrawWorldPartitionPanel()
{
	if (!ImGui::Begin("World Partition"))
	{
		ImGui::End();
		return;
	}

	ImGui::InputText("World JSON", m_worldPartitionPath, sizeof(m_worldPartitionPath));
	ImGui::BeginDisabled(m_worldPartitionPath[0] == '\0');
	if (ImGui::Button("Load / Probe Source"))
		RunWorldPartitionProbe();
	ImGui::EndDisabled();
	ImGui::Separator();
	ImGui::TextWrapped("%s", m_worldPartitionStatus.c_str());
	ImGui::Text("Cells: %u  Visible Instances: %u",
		m_worldPartitionCellCount,
		m_worldPartitionVisibleInstances);
	if (m_worldPartitionRows.empty())
	{
		ImGui::TextDisabled("No probe rows.");
	}
	else
	{
		for (const std::string& row : m_worldPartitionRows)
			ImGui::BulletText("%s", row.c_str());
	}

	ImGui::End();
}

void CEldenRingEditorScene::DrawBossTestingPanel()
{
	if (!ImGui::Begin("Boss Testing"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Run Hitbox Geometry Probe"))
		RunBossHitboxProbe();
	ImGui::Separator();
	ImGui::TextWrapped("%s", m_bossTestingStatus.c_str());
	if (m_bossTestingRows.empty())
	{
		ImGui::TextDisabled("No hitbox probe rows.");
	}
	else
	{
		for (const std::string& row : m_bossTestingRows)
			ImGui::BulletText("%s", row.c_str());
	}

	ImGui::End();
}

void CEldenRingEditorScene::DrawLogPanel()
{
	if (!ImGui::Begin("Log"))
	{
		ImGui::End();
		return;
	}

	ImGui::TextWrapped("%s", m_statusMessage.c_str());
	ImGui::End();
}
