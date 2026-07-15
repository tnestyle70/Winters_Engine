#include "UI/AttackSpeedLab.h"

#include "ECS/World.h"
#include "GameObject/ChampionDef.h"
#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"
#include "Network/Client/GameSessionClient.h"
#include "Network/Client/SnapshotApplier.h"
#include "Scene/Scene_InGame.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "WintersPaths.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#include <imgui.h>
#pragma pop_macro("new")

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace
{
	using json = nlohmann::json;

	constexpr wchar_t kTuningResourcePath[] =
		L"Resource/Config/Practice/attack_speed_tuning.json";
	constexpr wchar_t kPracticeWriteAnchorPath[] =
		L"Resource/Config/Practice/practice_balance_overrides.json";
	constexpr wchar_t kTuningFileName[] = L"attack_speed_tuning.json";
	constexpr size_t kChampionSlotCount = 256u;
	constexpr f32_t kAttackSpeedMin = 0.8f;
	constexpr f32_t kAttackSpeedMax = 2.5f;
	constexpr f32_t kAnimCorrectionMin = 0.25f;
	constexpr f32_t kAnimCorrectionMax = 3.f;
	constexpr f32_t kObservedTolerance = 0.005f;

	struct LabEntry
	{
		f32_t fTargetAttackSpeed = 1.f;
		f32_t fAnimCorrectionScale = 1.f;
		bool_t bSeeded = false;
	};

	struct AppliedTuning
	{
		eChampion eChampionId = eChampion::END;
		f32_t fTargetAttackSpeed = 1.f;
		f32_t fAnimCorrectionScale = 1.f;
	};

	enum class eControlRequestKind : u8_t
	{
		None = 0u,
		TakeRosterChampion,
		FreshReplace,
	};

	struct LabState
	{
		EntityID selectedEntity = NULL_ENTITY;
		u64_t uPendingEntityHandle = 0u;
		u32_t uPendingCommandSequence = 0u;
		f32_t fPendingAttackSpeed = 0.f;
		f32_t fPendingAnimCorrectionScale = 1.f;
		eChampion ePendingChampion = eChampion::END;
		eControlRequestKind ePendingControlKind = eControlRequestKind::None;
		u32_t uPendingControlSequence = 0u;
		NetEntityId uPendingSourceNet = NULL_NET_ENTITY;
		NetEntityId uPendingTargetNet = NULL_NET_ENTITY;
		eChampion eRequestedChampion = eChampion::IRELIA;
		bool_t bPendingControlAckedWithoutConfirmation = false;
		std::string status = "Press 8 to load the tuning JSON.";
		std::array<LabEntry, kChampionSlotCount> entries{};
	};

	LabState g_Lab{};
	std::unordered_map<u64_t, AppliedTuning> g_AppliedByEntity{};

	size_t SlotOf(eChampion champion)
	{
		return static_cast<size_t>(static_cast<u8_t>(champion));
	}

	std::string NormalizeChampionKey(const char* pName)
	{
		std::string key{};
		if (!pName)
			return key;
		for (const unsigned char ch : std::string{ pName })
		{
			if (std::isalnum(ch) != 0)
				key.push_back(static_cast<char>(std::toupper(ch)));
		}
		return key;
	}

	std::string ChampionJsonKey(eChampion champion)
	{
		return NormalizeChampionKey(GetChampionDisplayName(champion));
	}

	eChampion FindChampionByJsonKey(const std::string& sourceKey)
	{
		const std::string normalized = NormalizeChampionKey(sourceKey.c_str());
		for (u16_t raw = 1u; raw < 255u; ++raw)
		{
			const eChampion champion = static_cast<eChampion>(raw);
			if (!FindChampionDef(champion))
				continue;
			if (ChampionJsonKey(champion) == normalized)
				return champion;
		}
		return eChampion::END;
	}

	f32_t ResolveCanonicalAttackSpeedBase(
		const StatComponent& stat,
		eChampion champion)
	{
		if (std::isfinite(stat.baseAttackSpeed) && stat.baseAttackSpeed > 0.001f)
			return stat.baseAttackSpeed;
		if (std::isfinite(stat.attackSpeedRatio) && stat.attackSpeedRatio > 0.001f)
			return stat.attackSpeedRatio;

		const ChampionStatsDef canonical = ChampionGameDataDB::ResolveStats(champion);
		if (std::isfinite(canonical.baseAttackSpeed) && canonical.baseAttackSpeed > 0.001f)
			return canonical.baseAttackSpeed;
		if (std::isfinite(canonical.attackSpeedRatio) && canonical.attackSpeedRatio > 0.001f)
			return canonical.attackSpeedRatio;
		return 1.f;
	}

	LabEntry& EntryOf(eChampion champion)
	{
		LabEntry& entry = g_Lab.entries[SlotOf(champion)];
		if (!entry.bSeeded)
		{
			const ChampionStatsDef stats = ChampionGameDataDB::ResolveStats(champion);
			const f32_t base =
				std::isfinite(stats.baseAttackSpeed) && stats.baseAttackSpeed > 0.001f
				? stats.baseAttackSpeed
				: 1.f;
			entry.fTargetAttackSpeed = std::clamp(
				base,
				kAttackSpeedMin,
				kAttackSpeedMax);
			entry.fAnimCorrectionScale = 1.f;
			entry.bSeeded = true;
		}
		return entry;
	}

	std::filesystem::path ResolveTuningPath()
	{
		wchar_t resolved[1024]{};
		if (WintersResolveContentPath(
			kTuningResourcePath,
			resolved,
			static_cast<u32_t>(std::size(resolved))))
		{
			return std::filesystem::path{ resolved };
		}

		wchar_t anchor[1024]{};
		if (WintersResolveContentPath(
			kPracticeWriteAnchorPath,
			anchor,
			static_cast<u32_t>(std::size(anchor))))
		{
			return std::filesystem::path{ anchor }.parent_path() / kTuningFileName;
		}
		return {};
	}

	bool_t TryDecodeTuningJson(
		const json& root,
		std::array<LabEntry, kChampionSlotCount>& outEntries,
		std::string& outError)
	{
		if (!root.is_object() || root.value("version", 0) != 1)
		{
			outError = "version must be 1";
			return false;
		}
		if (!root.contains("champions") || !root["champions"].is_array())
		{
			outError = "champions must be an array";
			return false;
		}

		for (const json& source : root["champions"])
		{
			if (!source.is_object() ||
				!source.contains("champion") ||
				!source["champion"].is_string())
			{
				outError = "every champion entry needs a string champion key";
				return false;
			}

			const f32_t target = source.value("targetAttackSpeed", 1.f);
			const f32_t correction = source.value("animSpeedMul", 1.f);
			if (!std::isfinite(target) ||
				target < kAttackSpeedMin ||
				target > kAttackSpeedMax)
			{
				outError = "targetAttackSpeed must be within 0.8..2.5";
				return false;
			}
			if (!std::isfinite(correction) ||
				correction < kAnimCorrectionMin ||
				correction > kAnimCorrectionMax)
			{
				outError = "animSpeedMul must be within 0.25..3.0";
				return false;
			}

			const eChampion champion = FindChampionByJsonKey(
				source["champion"].get<std::string>());
			if (champion == eChampion::END)
				continue;

			LabEntry& entry = outEntries[SlotOf(champion)];
			entry.fTargetAttackSpeed = target;
			entry.fAnimCorrectionScale = correction;
			entry.bSeeded = true;
		}
		return true;
	}

	bool_t LoadTuningJson(LabState& state)
	{
		const std::filesystem::path path = ResolveTuningPath();
		if (path.empty())
		{
			EntryOf(eChampion::IRELIA);
			state.status = "Canonical Practice resource root was not found.";
			return false;
		}

		std::ifstream file{ path };
		if (!file.is_open())
		{
			EntryOf(eChampion::IRELIA);
			state.status = "Tuning JSON is missing; Irelia draft uses defaults.";
			return false;
		}

		try
		{
			json root{};
			file >> root;
			std::array<LabEntry, kChampionSlotCount> decoded{};
			std::string error{};
			if (!TryDecodeTuningJson(root, decoded, error))
				throw std::runtime_error(error);
			state.entries = decoded;
			EntryOf(eChampion::IRELIA);
			state.status = "JSON loaded into draft. Save and Apply are separate.";
			return true;
		}
		catch (const std::exception& exception)
		{
			EntryOf(eChampion::IRELIA);
			state.status = std::string{ "JSON load failed: " } + exception.what();
			return false;
		}
	}

	json BuildTuningJson(const LabState& state)
	{
		json root{};
		root["version"] = 1;
		root["champions"] = json::array();
		for (u16_t raw = 1u; raw < 255u; ++raw)
		{
			const eChampion champion = static_cast<eChampion>(raw);
			if (!FindChampionDef(champion))
				continue;
			const LabEntry& entry = state.entries[SlotOf(champion)];
			if (!entry.bSeeded)
				continue;
			root["champions"].push_back(
				{
					{ "champion", ChampionJsonKey(champion) },
					{ "targetAttackSpeed", entry.fTargetAttackSpeed },
					{ "animSpeedMul", entry.fAnimCorrectionScale },
				});
		}
		return root;
	}

	bool_t SaveTuningJson(LabState& state)
	{
		const std::filesystem::path path = ResolveTuningPath();
		if (path.empty())
		{
			state.status = "Save failed: canonical Practice resource root was not found.";
			return false;
		}

		std::error_code ec{};
		std::filesystem::create_directories(path.parent_path(), ec);
		if (ec)
		{
			state.status = std::string{ "Save failed: " } + ec.message();
			return false;
		}

		const json root = BuildTuningJson(state);
		std::filesystem::path tempPath = path;
		tempPath += L".tmp";
		std::filesystem::path backupPath = path;
		backupPath += L".bak";
		std::filesystem::remove(tempPath, ec);

		std::ofstream tempFile{ tempPath, std::ios::binary | std::ios::trunc };
		if (!tempFile.is_open())
		{
			state.status = "Save failed: temporary file could not be opened.";
			return false;
		}
		tempFile << root.dump(2) << '\n';
		tempFile.flush();
		const bool_t bWriteOk = tempFile.good();
		tempFile.close();
		if (!bWriteOk)
		{
			std::filesystem::remove(tempPath, ec);
			state.status = "Save failed while writing the temporary file.";
			return false;
		}

		try
		{
			std::ifstream verifyFile{ tempPath };
			json verifyRoot{};
			verifyFile >> verifyRoot;
			std::array<LabEntry, kChampionSlotCount> ignored{};
			std::string error{};
			if (!TryDecodeTuningJson(verifyRoot, ignored, error))
				throw std::runtime_error(error);
		}
		catch (const std::exception& exception)
		{
			std::filesystem::remove(tempPath, ec);
			state.status = std::string{ "Save verification failed: " } + exception.what();
			return false;
		}

		const bool_t bTargetExists = std::filesystem::exists(path, ec) && !ec;
		if (bTargetExists &&
			!CopyFileW(path.c_str(), backupPath.c_str(), FALSE))
		{
			std::filesystem::remove(tempPath, ec);
			state.status = "Save failed: backup copy could not be created.";
			return false;
		}
		if (!MoveFileExW(
			tempPath.c_str(),
			path.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			std::filesystem::remove(tempPath, ec);
			state.status = "Save failed: atomic replace failed.";
			return false;
		}

		state.status = std::string{ "JSON saved: " } + path.string();
		return true;
	}

	bool_t CanSendToServer(CScene_InGame* pScene)
	{
		if (!pScene || !pScene->IsNetworkAuthoritativeGameplay())
			return false;
		CClientNetwork* pNetwork = pScene->GetNetworkView();
		return pScene->GetCommandSerializer() && pNetwork && pNetwork->IsConnected();
	}

	u32_t SendPractice(
		CScene_InGame* pScene,
		ePracticeOperation operation,
		f32_t value,
		u8_t slot,
		NetEntityId targetNet,
		u32_t flags = 0u)
	{
		return pScene->GetCommandSerializer()->SendPracticeControl(
			*pScene->GetNetworkView(),
			operation,
			value,
			flags,
			slot,
			{},
			targetNet);
	}

	bool_t IsTunableEntity(const CWorld& world, EntityID entity)
	{
		return entity != NULL_ENTITY &&
			world.IsAlive(entity) &&
			world.HasComponent<ChampionComponent>(entity) &&
			world.HasComponent<StatComponent>(entity) &&
			!world.HasComponent<ViegoSoulComponent>(entity);
	}

	EntityID ResolveDefaultTarget(CScene_InGame* pScene)
	{
		if (IsTunableEntity(pScene->GetWorld(), pScene->GetPlayerEntity()))
			return pScene->GetPlayerEntity();

		EntityID first = NULL_ENTITY;
		CWorld& world = pScene->GetWorld();
		world.ForEach<ChampionComponent, StatComponent>(
			[&](EntityID entity, ChampionComponent&, StatComponent&)
			{
				if (first == NULL_ENTITY && IsTunableEntity(world, entity))
					first = entity;
			});
		return first;
	}

	NetEntityId ResolveTargetNetId(CScene_InGame* pScene, EntityID entity)
	{
		EntityIdMap* pMap = pScene ? pScene->GetEntityIdMap() : nullptr;
		return pMap ? pMap->ToNet(entity) : NULL_NET_ENTITY;
	}

	const char* TeamName(eTeam team)
	{
		switch (team)
		{
		case eTeam::Blue: return "Blue";
		case eTeam::Red: return "Red";
		default: return "Neutral";
		}
	}

	bool_t IsRosterBotNetId(NetEntityId netId)
	{
		if (netId == NULL_NET_ENTITY)
			return false;

		const MatchContext& context =
			CGameSessionClient::Instance().GetLobbyContext();
		for (const GameRosterSlot& slot : context.Roster)
		{
			if (slot.bBot && slot.netId == netId)
				return true;
		}
		return false;
	}

	void RefreshPendingState(CScene_InGame* pScene)
	{
		if (!pScene || g_Lab.uPendingEntityHandle == 0u)
			return;

		CWorld& world = pScene->GetWorld();
		const EntityHandle handle = EntityHandle::FromU64(g_Lab.uPendingEntityHandle);
		const EntityID entity = world.ResolveEntity(handle);
		if (!IsTunableEntity(world, entity))
		{
			if (g_Lab.ePendingControlKind == eControlRequestKind::None)
			{
				g_Lab.status =
					"Apply target disappeared before snapshot confirmation.";
			}
			g_Lab.uPendingEntityHandle = 0u;
			g_Lab.uPendingCommandSequence = 0u;
			g_Lab.fPendingAttackSpeed = 0.f;
			g_Lab.fPendingAnimCorrectionScale = 1.f;
			g_Lab.ePendingChampion = eChampion::END;
			return;
		}

		const CSnapshotApplier* pSnapshotApplier =
			pScene->GetSnapshotApplier();
		const bool_t bCommandAcked =
			pSnapshotApplier &&
			pSnapshotApplier->GetLastAckedCommandSequence() >=
				g_Lab.uPendingCommandSequence;
		const f32_t observed = world.GetComponent<StatComponent>(entity).attackSpeed;
		if (bCommandAcked &&
			std::isfinite(observed) &&
			std::fabs(observed - g_Lab.fPendingAttackSpeed) <= kObservedTolerance)
		{
			const eChampion observedChampion = static_cast<eChampion>(
				world.GetComponent<ChampionComponent>(entity).id.value);
			if (observedChampion != g_Lab.ePendingChampion)
				return;

			g_AppliedByEntity[handle.ToU64()] = AppliedTuning{
				g_Lab.ePendingChampion,
				g_Lab.fPendingAttackSpeed,
				g_Lab.fPendingAnimCorrectionScale,
			};
			char message[192]{};
			std::snprintf(
				message,
				sizeof(message),
				"Applied: seq=%u requested=%.3f observed=%.3f. Future attacks are ready to verify.",
				g_Lab.uPendingCommandSequence,
				g_Lab.fPendingAttackSpeed,
				observed);
			g_Lab.status = message;
			g_Lab.uPendingEntityHandle = 0u;
			g_Lab.uPendingCommandSequence = 0u;
			g_Lab.fPendingAttackSpeed = 0.f;
			g_Lab.fPendingAnimCorrectionScale = 1.f;
			g_Lab.ePendingChampion = eChampion::END;
		}
	}

	void ClearPendingControlRequest()
	{
		g_Lab.ePendingControlKind = eControlRequestKind::None;
		g_Lab.uPendingControlSequence = 0u;
		g_Lab.uPendingSourceNet = NULL_NET_ENTITY;
		g_Lab.uPendingTargetNet = NULL_NET_ENTITY;
		g_Lab.bPendingControlAckedWithoutConfirmation = false;
	}

	void RefreshPendingControlState(CScene_InGame* pScene)
	{
		if (!pScene ||
			g_Lab.ePendingControlKind == eControlRequestKind::None)
		{
			return;
		}

		const CSnapshotApplier* pSnapshotApplier =
			pScene->GetSnapshotApplier();
		if (!pSnapshotApplier ||
			g_Lab.uPendingControlSequence == 0u)
		{
			return;
		}

		const bool_t bCommandAcked =
			pSnapshotApplier->GetLastAckedCommandSequence() >=
			g_Lab.uPendingControlSequence;
		if (!bCommandAcked)
			return;

		const EntityID player = pScene->GetPlayerEntity();
		if (!IsTunableEntity(pScene->GetWorld(), player))
		{
			g_Lab.bPendingControlAckedWithoutConfirmation = true;
			g_Lab.status =
				"Control request was acknowledged, but no authoritative player entity was rebound.";
			return;
		}

		CWorld& world = pScene->GetWorld();
		const NetEntityId playerNet = ResolveTargetNetId(pScene, player);
		const NetEntityId helloLocalNet =
			pSnapshotApplier->GetLastHelloNetId();
		const NetEntityId snapshotLocalNet =
			pSnapshotApplier->GetLastSnapshotNetId();
		const eChampion playerChampion = static_cast<eChampion>(
			world.GetComponent<ChampionComponent>(player).id.value);
		EntityIdMap* pEntityMap = pScene->GetEntityIdMap();
		const bool_t bOldEntityGone =
			pEntityMap &&
			g_Lab.uPendingSourceNet != NULL_NET_ENTITY &&
			pEntityMap->FromNet(g_Lab.uPendingSourceNet) == NULL_ENTITY;
		const GoldComponent* pGold =
			world.TryGetComponent<GoldComponent>(player);
		const StatComponent& stat = world.GetComponent<StatComponent>(player);
		const bool_t bCanonicalBaseline =
			pGold && stat.level == 6u && pGold->amount == 10000u;
		const bool_t bHelloAndSnapshotBound =
			playerNet != NULL_NET_ENTITY &&
			helloLocalNet == playerNet &&
			snapshotLocalNet == playerNet;
		const bool_t bTakeConfirmed =
			g_Lab.ePendingControlKind ==
				eControlRequestKind::TakeRosterChampion &&
			bHelloAndSnapshotBound &&
			playerNet == g_Lab.uPendingTargetNet;
		const bool_t bReplaceConfirmed =
			g_Lab.ePendingControlKind == eControlRequestKind::FreshReplace &&
			bHelloAndSnapshotBound &&
			playerNet != g_Lab.uPendingSourceNet &&
			playerChampion == g_Lab.eRequestedChampion &&
			bOldEntityGone &&
			bCanonicalBaseline;
		if (!bTakeConfirmed && !bReplaceConfirmed)
		{
			g_Lab.bPendingControlAckedWithoutConfirmation = true;
			g_Lab.status =
				"Control request was acknowledged, but Hello/Snapshot invariants did not confirm it. Inspect the host trace, then dismiss.";
			return;
		}

		g_Lab.selectedEntity = player;
		ClearPendingControlRequest();
		g_Lab.status = bTakeConfirmed
			? "Controlled: authoritative Hello/Snapshot rebound camera, HUD, and input."
			: "Fresh baseline ready: level 6, gold 10000. Apply attack speed when ready.";
	}
}

namespace UI
{
	AttackSpeedPlaybackScales ResolveAttackSpeedPlaybackScales(
		const CWorld& world,
		EntityID entity)
	{
		AttackSpeedPlaybackScales scales{};
		if (!world.IsAlive(entity))
			return scales;

		const StatComponent* pStat = world.TryGetComponent<StatComponent>(entity);
		const ChampionComponent* pChampion =
			world.TryGetComponent<ChampionComponent>(entity);
		if (!pStat || !pChampion)
			return scales;

		const eChampion champion = static_cast<eChampion>(pChampion->id.value);
		const f32_t base = ResolveCanonicalAttackSpeedBase(*pStat, champion);
		if (std::isfinite(pStat->attackSpeed) && pStat->attackSpeed > 0.001f)
		{
			scales.fAttackSpeedScale = std::clamp(
				pStat->attackSpeed / base,
				0.2f,
				4.f);
		}

		const EntityHandle handle = world.GetEntityHandle(entity);
		const auto applied = g_AppliedByEntity.find(handle.ToU64());
		if (applied == g_AppliedByEntity.end() ||
			applied->second.eChampionId != champion ||
			std::fabs(pStat->attackSpeed - applied->second.fTargetAttackSpeed) >
				kObservedTolerance)
		{
			return scales;
		}

		scales.fAnimCorrectionScale = std::clamp(
			applied->second.fAnimCorrectionScale,
			kAnimCorrectionMin,
			kAnimCorrectionMax);
		return scales;
	}

	void CAttackSpeedLab::Open()
	{
		LoadTuningJson(g_Lab);
	}

	void CAttackSpeedLab::ResetRuntime()
	{
		g_Lab = LabState{};
		g_AppliedByEntity.clear();
	}

	void CAttackSpeedLab::OnEntityRemoved(
		const CWorld& world,
		EntityID entity)
	{
		if (entity == NULL_ENTITY || !world.IsAlive(entity))
			return;

		const u64_t handle = world.GetEntityHandle(entity).ToU64();
		g_AppliedByEntity.erase(handle);
		if (g_Lab.selectedEntity == entity)
			g_Lab.selectedEntity = NULL_ENTITY;
		if (g_Lab.uPendingEntityHandle == handle)
		{
			g_Lab.uPendingEntityHandle = 0u;
			g_Lab.uPendingCommandSequence = 0u;
			g_Lab.fPendingAttackSpeed = 0.f;
			g_Lab.fPendingAnimCorrectionScale = 1.f;
			g_Lab.ePendingChampion = eChampion::END;
			if (g_Lab.ePendingControlKind == eControlRequestKind::None)
			{
				g_Lab.status =
					"Previous attack-speed target was removed before confirmation.";
			}
		}
	}

	void CAttackSpeedLab::Render(CScene_InGame* pScene)
	{
		if (!pScene)
			return;

		CWorld& world = pScene->GetWorld();
		if (!IsTunableEntity(world, g_Lab.selectedEntity))
			g_Lab.selectedEntity = ResolveDefaultTarget(pScene);
		RefreshPendingState(pScene);
		RefreshPendingControlState(pScene);

		ImGui::SetNextWindowPos(ImVec2(80.f, 120.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(660.f, 760.f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Attack Speed Lab ('8')"))
		{
			ImGui::End();
			return;
		}

		ImGui::TextUnformatted("Draft -> Save JSON -> Apply -> wait for Observed/Applied.");
		ImGui::TextDisabled(
			"Target is final server attack speed. Animation uses replicated AS/base AS.");
		ImGui::TextColored(
			ImVec4(1.f, 0.75f, 0.2f, 1.f),
			"Keep '7' Model Animation Preview disabled while validating attacks.");

		if (ImGui::BeginTable(
			"##attackSpeedTargets",
			5,
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_ScrollY,
			ImVec2(0.f, 170.f)))
		{
			ImGui::TableSetupColumn("Champion");
			ImGui::TableSetupColumn("Team");
			ImGui::TableSetupColumn("Tag");
			ImGui::TableSetupColumn("NetId");
			ImGui::TableSetupColumn("Observed AS");
			ImGui::TableHeadersRow();

			world.ForEach<ChampionComponent, StatComponent>(
				[&](EntityID entity, ChampionComponent& champion, StatComponent& stat)
				{
					if (!IsTunableEntity(world, entity))
						return;
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::PushID(static_cast<int>(entity));
					if (ImGui::Selectable(
						GetChampionDisplayName(
							static_cast<eChampion>(champion.id.value)),
						g_Lab.selectedEntity == entity,
						ImGuiSelectableFlags_SpanAllColumns))
					{
						g_Lab.selectedEntity = entity;
					}
					ImGui::PopID();
					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(TeamName(champion.team));
					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(
						entity == pScene->GetPlayerEntity() ? "Player" : "Bot");
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%u", ResolveTargetNetId(pScene, entity));
					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%.3f", stat.attackSpeed);
				});
			ImGui::EndTable();
		}

		const EntityID playerEntity = pScene->GetPlayerEntity();
		const NetEntityId playerNet = ResolveTargetNetId(pScene, playerEntity);
		const NetEntityId selectedNet = ResolveTargetNetId(
			pScene,
			g_Lab.selectedEntity);
		const CSnapshotApplier* pSnapshotApplier =
			pScene->GetSnapshotApplier();
		const bool_t bHasAuthoritativeSnapshot =
			pSnapshotApplier &&
			pSnapshotApplier->GetLastAppliedSnapshotTick() != 0u &&
			pSnapshotApplier->GetLastSnapshotNetId() != NULL_NET_ENTITY;
		const bool_t bSimulationPaused =
			pSnapshotApplier &&
			pSnapshotApplier->GetTimelineState().simPaused;
		const bool_t bControlPending =
			g_Lab.ePendingControlKind != eControlRequestKind::None;
		const bool_t bSelectedRosterBot = IsRosterBotNetId(selectedNet);
		const bool_t bCanTakeControl =
			CanSendToServer(pScene) &&
			selectedNet != NULL_NET_ENTITY &&
			selectedNet != playerNet &&
			bSelectedRosterBot &&
			!bControlPending;

		ImGui::Separator();
		ImGui::TextUnformatted("Player Control");
		ImGui::TextDisabled(
			"Take Control preserves live roster state. Fresh Replace creates a comparable baseline.");
		ImGui::BeginDisabled(!bCanTakeControl);
		if (ImGui::Button("Take Control of Selected", ImVec2(220.f, 0.f)))
		{
			SendPractice(
				pScene,
				ePracticeOperation::SetEnabled,
				1.f,
				0u,
				NULL_NET_ENTITY);
			const u32_t sequence = SendPractice(
				pScene,
				ePracticeOperation::TakeControlRosterChampion,
				0.f,
				0u,
				selectedNet);
			if (sequence != 0u)
			{
				g_Lab.ePendingControlKind =
					eControlRequestKind::TakeRosterChampion;
				g_Lab.uPendingControlSequence = sequence;
				g_Lab.uPendingSourceNet = playerNet;
				g_Lab.uPendingTargetNet = selectedNet;
				g_Lab.bPendingControlAckedWithoutConfirmation = false;
				g_Lab.status =
					"Pending control transfer: waiting for command ACK plus authoritative Hello/Snapshot.";
			}
			else
			{
				g_Lab.status = "Take Control send failed.";
			}
		}
		ImGui::EndDisabled();
		if (selectedNet != NULL_NET_ENTITY &&
			selectedNet != playerNet &&
			!bSelectedRosterBot)
		{
			ImGui::TextDisabled(
				"Take Control is limited to a bot in the authoritative 10-slot roster.");
		}

		ImGui::BeginDisabled(bControlPending);
		const char* pRequestedName =
			GetChampionDisplayName(g_Lab.eRequestedChampion);
		if (ImGui::BeginCombo("All Registered Champions", pRequestedName))
		{
			for (u16_t raw = 1u; raw < 255u; ++raw)
			{
				const eChampion candidate = static_cast<eChampion>(raw);
				if (!FindChampionDef(candidate))
					continue;
				const bool_t bSelected = candidate == g_Lab.eRequestedChampion;
				if (ImGui::Selectable(
					GetChampionDisplayName(candidate),
					bSelected))
				{
					g_Lab.eRequestedChampion = candidate;
				}
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();

		const bool_t bCanFreshReplace =
			CanSendToServer(pScene) &&
			bHasAuthoritativeSnapshot &&
			!bSimulationPaused &&
			playerNet != NULL_NET_ENTITY &&
			FindChampionDef(g_Lab.eRequestedChampion) &&
			!bControlPending;
		ImGui::BeginDisabled(!bCanFreshReplace);
		if (ImGui::Button("Fresh Baseline Replace My Slot", ImVec2(260.f, 0.f)))
		{
			SendPractice(
				pScene,
				ePracticeOperation::SetEnabled,
				1.f,
				0u,
				NULL_NET_ENTITY);
			const u32_t sequence = SendPractice(
				pScene,
				ePracticeOperation::ReplaceControlledChampion,
				0.f,
				0u,
				NULL_NET_ENTITY,
				static_cast<u32_t>(g_Lab.eRequestedChampion));
			if (sequence != 0u)
			{
				g_Lab.ePendingControlKind = eControlRequestKind::FreshReplace;
				g_Lab.uPendingControlSequence = sequence;
				g_Lab.uPendingSourceNet = playerNet;
				g_Lab.uPendingTargetNet = NULL_NET_ENTITY;
				g_Lab.bPendingControlAckedWithoutConfirmation = false;
				g_Lab.status =
					"Pending fresh baseline: old NetId must disappear, then Hello/Snapshot must confirm the new level-6, 10000-gold player.";
			}
			else
			{
				g_Lab.status = "Fresh Replace send failed.";
			}
		}
		ImGui::EndDisabled();
		ImGui::TextDisabled(
			"Duplicate champions are intentionally allowed for repeatable baseline comparison.");
		if (bSimulationPaused)
		{
			ImGui::TextColored(
				ImVec4(1.f, 0.75f, 0.2f, 1.f),
				"Resume simulation before Fresh Replace; Take Control remains available.");
		}
		if (bControlPending)
		{
			if (!CanSendToServer(pScene))
			{
				ImGui::TextColored(
					ImVec4(1.f, 0.45f, 0.25f, 1.f),
					"Connection is not active; this request may never be acknowledged.");
			}
			if (ImGui::Button("Stop Waiting (UI Only)", ImVec2(260.f, 0.f)))
			{
				ClearPendingControlRequest();
				g_Lab.status =
					"Stopped waiting locally. Any command already accepted by the server remains authoritative.";
			}
		}

		if (!IsTunableEntity(world, g_Lab.selectedEntity))
		{
			ImGui::TextUnformatted("No replicated champion target is available.");
			ImGui::End();
			return;
		}

		const ChampionComponent& championComponent =
			world.GetComponent<ChampionComponent>(g_Lab.selectedEntity);
		const StatComponent& stat =
			world.GetComponent<StatComponent>(g_Lab.selectedEntity);
		const eChampion champion =
			static_cast<eChampion>(championComponent.id.value);
		LabEntry& draft = EntryOf(champion);
		const f32_t base = ResolveCanonicalAttackSpeedBase(stat, champion);
		const f32_t draftScale = std::clamp(
			draft.fTargetAttackSpeed / base,
			0.2f,
			4.f);

		ImGui::Separator();
		ImGui::Text(
			"Selected: %s / entity %u / net %u",
			GetChampionDisplayName(champion),
			g_Lab.selectedEntity,
			ResolveTargetNetId(pScene, g_Lab.selectedEntity));
		ImGui::Text(
			"Observed AS %.3f | Canonical base %.3f | Observed scale %.3fx",
			stat.attackSpeed,
			base,
			ResolveAttackSpeedPlaybackScales(
				world,
				g_Lab.selectedEntity).fAttackSpeedScale);

		ImGui::SliderFloat(
			"Target Attack Speed",
			&draft.fTargetAttackSpeed,
			kAttackSpeedMin,
			kAttackSpeedMax,
			"%.3f");
		ImGui::SliderFloat(
			"Animation Correction",
			&draft.fAnimCorrectionScale,
			kAnimCorrectionMin,
			kAnimCorrectionMax,
			"%.3fx");
		ImGui::TextDisabled(
			"Cooldown %.3fs | gameplay scale %.3fx | visual scale %.3fx",
			1.f / draft.fTargetAttackSpeed,
			draftScale,
			draftScale * draft.fAnimCorrectionScale);

		const NetEntityId targetNet = ResolveTargetNetId(
			pScene,
			g_Lab.selectedEntity);
		const bool_t bCanApply =
			CanSendToServer(pScene) &&
			targetNet != NULL_NET_ENTITY &&
			g_Lab.ePendingControlKind == eControlRequestKind::None;
		ImGui::BeginDisabled(!bCanApply);
		if (ImGui::Button("Apply", ImVec2(120.f, 0.f)))
		{
			SendPractice(
				pScene,
				ePracticeOperation::SetEnabled,
				1.f,
				0u,
				NULL_NET_ENTITY);
			const u32_t sequence = SendPractice(
				pScene,
				ePracticeOperation::ApplyChampionStatOverride,
				draft.fTargetAttackSpeed,
				static_cast<u8_t>(eChampionStatOverrideId::EffectiveAttackSpeed),
				targetNet);
			if (sequence != 0u)
			{
				const EntityHandle handle = world.GetEntityHandle(g_Lab.selectedEntity);
				g_Lab.uPendingEntityHandle = handle.ToU64();
				g_Lab.uPendingCommandSequence = sequence;
				g_Lab.fPendingAttackSpeed = draft.fTargetAttackSpeed;
				g_Lab.fPendingAnimCorrectionScale =
					draft.fAnimCorrectionScale;
				g_Lab.ePendingChampion = champion;
				g_Lab.status =
					"Pending: wait for the replicated Observed AS before attacking.";
			}
			else
			{
				g_Lab.status = "Apply send failed.";
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Selected", ImVec2(150.f, 0.f)))
		{
			const u32_t sequence = SendPractice(
				pScene,
				ePracticeOperation::ClearChampionStatOverrides,
				0.f,
				0u,
				targetNet);
			const EntityHandle handle = world.GetEntityHandle(g_Lab.selectedEntity);
			g_AppliedByEntity.erase(handle.ToU64());
			g_Lab.uPendingEntityHandle = 0u;
			g_Lab.uPendingCommandSequence = 0u;
			g_Lab.fPendingAttackSpeed = 0.f;
			g_Lab.fPendingAnimCorrectionScale = 1.f;
			g_Lab.ePendingChampion = eChampion::END;
			g_Lab.status = sequence != 0u
				? "Clear sent; wait for Observed AS to return to canonical."
				: "Clear send failed.";
		}
		ImGui::EndDisabled();

		if (bControlPending)
		{
			ImGui::TextDisabled(
				"Apply is disabled until the player-control request is confirmed or dismissed.");
		}
		else if (!bCanApply)
		{
			ImGui::TextDisabled(
				"Apply requires the connected authoritative host and a replicated NetId.");
		}

		if (ImGui::Button("Save JSON", ImVec2(120.f, 0.f)))
			SaveTuningJson(g_Lab);
		ImGui::SameLine();
		if (ImGui::Button("Reload JSON", ImVec2(120.f, 0.f)))
			LoadTuningJson(g_Lab);

		if (!g_Lab.status.empty())
		{
			ImGui::Spacing();
			ImGui::TextWrapped("%s", g_Lab.status.c_str());
		}
		ImGui::End();
	}
}
