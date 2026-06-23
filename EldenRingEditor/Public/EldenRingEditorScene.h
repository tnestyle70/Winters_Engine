#pragma once

#include "IScene.h"
#include "World/EditorTransaction.h"
#include "World/WorldCellDocument.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class CEldenRingEditorScene final : public IScene
{
public:
	~CEldenRingEditorScene() = default;

	static std::unique_ptr<CEldenRingEditorScene> Create();

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t deltaTime) override;
	void OnRender() override;
	void OnImGui() override;

private:
	CEldenRingEditorScene() = default;

	void DrawViewportPanel();
	void DrawContentBrowserPanel();
	void DrawWorldCellPanel();
	void DrawWorldOutlinerPanel();
	void DrawDetailsPanel();
	void DrawTransactionPanel();
	void DrawFxGraphPanel();
	void DrawSequencerPanel();
	void DrawWorldPartitionPanel();
	void DrawBossTestingPanel();
	void DrawLogPanel();

	void AddSeedPlacement();
	void DeleteSelectedPlacement();
	void CommitDetailsTransformEdit();
	void CancelDetailsTransformEdit();
	void EnsureValidSelection();
	void RunFxGraphValidation();
	void RunSequenceValidation();
	void RunWorldPartitionProbe();
	void RunBossHitboxProbe();
	size_t FindPlacementIndex(u32_t placementId) const;
	u32_t FindDeleteFallbackSelection(size_t deletingIndex, u32_t deletingId) const;

	CWorldCellDocument m_worldCellDocument;
	CEditorTransaction m_transaction;
	u32_t m_selectedPlacementId = 0;
	std::string m_statusMessage = "World cell contract ready.";

	char m_cellLoadPath[512] = {};
	char m_cellSavePath[512] = {};
	char m_seedName[128] = {};
	char m_seedWMeshPath[512] = {};
	char m_fxGraphPath[512] = {};
	char m_sequencePath[512] = {};
	char m_worldPartitionPath[512] = {};

	bool m_bDetailsTransformEditing = false;
	u32_t m_detailsTransformEditingId = 0;
	WorldPlacement m_detailsTransformBefore;
	WorldPlacement m_detailsTransformDraft;

	std::string m_fxGraphStatus = "FX graph validator ready.";
	std::vector<std::string> m_fxGraphIssues;
	u32_t m_fxGraphEmitterCount = 0;
	u32_t m_fxGraphCompiledCount = 0;

	std::string m_sequenceStatus = "Sequencer validator ready.";
	std::vector<std::string> m_sequenceIssues;
	u32_t m_sequenceTrackCount = 0;
	u32_t m_sequenceKeyCount = 0;
	f64_t m_sequenceDurationSec = 0.0;

	std::string m_worldPartitionStatus = "World partition probe ready.";
	std::vector<std::string> m_worldPartitionRows;
	u32_t m_worldPartitionCellCount = 0;
	u32_t m_worldPartitionVisibleInstances = 0;

	std::string m_bossTestingStatus = "Boss hitbox probe ready.";
	std::vector<std::string> m_bossTestingRows;
};
