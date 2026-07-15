Session - 숫자 8 Attack Speed Lab에서 JSON 초안을 저장하고 선택한 이렐리아를 시작으로 현재 등록된 모든 챔피언의 최종 공격속도 0.8~2.5를 서버 권위 주기·윈드업·클라이언트 애니메이션에 함께 적용한다. 시작 골드 10000/레벨 6은 기존 canonical spawn 정책을 검증하며, 5:5 조작 캐릭터 전환은 이 단계가 통과한 뒤로 미룬다. 이 세션의 ceiling 산출물은 0.8/2.5 비교 캡처와 JSON 재사용 가능한 튜닝 루프이며 전체 작업 예산의 30% 이내로 제한한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Client/Public/UI/AttackSpeedLab.h

파일 전체를 아래로 교체:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Definitions/SkillTypes.h"
#include "WintersTypes.h"

class CScene_InGame;
class CWorld;

namespace UI
{
	struct AttackSpeedPlaybackScales
	{
		f32_t fAttackSpeedScale = 1.f;
		f32_t fAnimCorrectionScale = 1.f;
	};

	constexpr bool_t IsAttackSpeedPlaybackAction(
		u16_t actionId,
		u8_t sourceSlot)
	{
		const eActionStateId id = static_cast<eActionStateId>(actionId);
		if (id == eActionStateId::BasicAttack)
			return true;
		if (sourceSlot != static_cast<u8_t>(eSkillSlot::BasicAttack))
			return false;
		return id == eActionStateId::SkillQ ||
			id == eActionStateId::SkillW ||
			id == eActionStateId::SkillE ||
			id == eActionStateId::SkillR;
	}

	static_assert(IsAttackSpeedPlaybackAction(
		static_cast<u16_t>(eActionStateId::SkillW),
		static_cast<u8_t>(eSkillSlot::BasicAttack)));
	static_assert(!IsAttackSpeedPlaybackAction(
		static_cast<u16_t>(eActionStateId::SkillW),
		static_cast<u8_t>(eSkillSlot::W)));

	AttackSpeedPlaybackScales ResolveAttackSpeedPlaybackScales(
		const CWorld& world,
		EntityID entity);

	class CAttackSpeedLab
	{
	public:
		static void Open();
		static void Render(CScene_InGame* pScene);
		static void ResetRuntime();
	};
}
```

### 1-2. C:/Users/user/Desktop/Winters/Client/Private/UI/AttackSpeedLab.cpp

파일 전체를 아래로 교체:

```cpp
#include "UI/AttackSpeedLab.h"

#include "ECS/World.h"
#include "GameObject/ChampionDef.h"
#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"
#include "Scene/Scene_InGame.h"
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

	struct LabState
	{
		EntityID selectedEntity = NULL_ENTITY;
		u64_t uPendingEntityHandle = 0u;
		u32_t uPendingCommandSequence = 0u;
		f32_t fPendingAttackSpeed = 0.f;
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
		NetEntityId targetNet)
	{
		return pScene->GetCommandSerializer()->SendPracticeControl(
			*pScene->GetNetworkView(),
			operation,
			value,
			0u,
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

	void RefreshPendingState(CScene_InGame* pScene)
	{
		if (!pScene || g_Lab.uPendingEntityHandle == 0u)
			return;

		CWorld& world = pScene->GetWorld();
		const EntityHandle handle = EntityHandle::FromU64(g_Lab.uPendingEntityHandle);
		const EntityID entity = world.ResolveEntity(handle);
		if (!IsTunableEntity(world, entity))
		{
			g_Lab.status = "Apply target disappeared before snapshot confirmation.";
			g_Lab.uPendingEntityHandle = 0u;
			return;
		}

		const f32_t observed = world.GetComponent<StatComponent>(entity).attackSpeed;
		if (std::isfinite(observed) &&
			std::fabs(observed - g_Lab.fPendingAttackSpeed) <= kObservedTolerance)
		{
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
		}
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

		const eChampion champion = static_cast<eChampion>(pChampion->id);
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

	void CAttackSpeedLab::Render(CScene_InGame* pScene)
	{
		if (!pScene)
			return;

		CWorld& world = pScene->GetWorld();
		if (!IsTunableEntity(world, g_Lab.selectedEntity))
			g_Lab.selectedEntity = ResolveDefaultTarget(pScene);
		RefreshPendingState(pScene);

		ImGui::SetNextWindowPos(ImVec2(80.f, 120.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(620.f, 600.f), ImGuiCond_FirstUseEver);
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
			"Keep F5 Model Animation Preview disabled while validating attacks.");

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
						GetChampionDisplayName(champion.id),
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
		const eChampion champion = static_cast<eChampion>(championComponent.id);
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
			CanSendToServer(pScene) && targetNet != NULL_NET_ENTITY;
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
				g_AppliedByEntity[handle.ToU64()] = AppliedTuning{
					champion,
					draft.fTargetAttackSpeed,
					draft.fAnimCorrectionScale,
				};
				g_Lab.uPendingEntityHandle = handle.ToU64();
				g_Lab.uPendingCommandSequence = sequence;
				g_Lab.fPendingAttackSpeed = draft.fTargetAttackSpeed;
				g_Lab.status = "Pending: wait for the replicated Observed AS before attacking.";
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
			g_Lab.status = sequence != 0u
				? "Clear sent; wait for Observed AS to return to canonical."
				: "Clear send failed.";
		}
		ImGui::EndDisabled();

		if (!bCanApply)
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
```

### 1-3. C:/Users/user/Desktop/Winters/Client/Bin/Resource/Config/Practice/attack_speed_tuning.json

새 파일:

```json
{
  "version": 1,
  "champions": [
    {
      "champion": "IRELIA",
      "targetAttackSpeed": 0.8,
      "animSpeedMul": 1.0
    }
  ]
}
```

### 1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

`enum class eChampionStatOverrideId : uint8_t` 안의 아래 기존 코드:

```cpp
    BaseMoveSpeed = 15,
    BaseAttackRange = 16,
    Count = 17,
```

아래로 교체:

```cpp
    BaseMoveSpeed = 15,
    BaseAttackRange = 16,
    // Practice-only final value applied after level, item, rune, and buff modifiers.
    EffectiveAttackSpeed = 17,
    Count = 18,
```

같은 파일의 `ePracticeOperation` 마지막 부분 기존 코드:

```cpp
    ApplyChampionStatOverride = 17,
    ClearChampionStatOverrides = 18,
    ApplyItemStatOverride = 19,
    ClearItemStatOverrides = 20,
```

아래로 교체:

```cpp
    ApplyChampionStatOverride = 17,
    ClearChampionStatOverrides = 18,
    ApplyItemStatOverride = 19,
    ClearItemStatOverrides = 20,
    Count = 21,
```

### 1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/GameplayComponents.h

`PracticeChampionStatOverrideEntry` 바로 위 기존 주석:

```cpp
// Practice 전용 챔피언 베이스 스탯 오버레이. StatSystem::Recompute가 정의 팩 값 위에 적용한다.
```

아래로 교체:

```cpp
// Practice 전용 챔피언 스탯 오버레이.
// Base 계열은 정의 팩 위에, EffectiveAttackSpeed는 runtime modifier 계산 뒤 최종값에 적용한다.
```

### 1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Stat/StatSystem.cpp

`ApplyPracticeChampionStatOverrides` 함수 바로 아래에 추가:

```cpp
    bool_t TryResolvePracticeEffectiveAttackSpeed(
        const CWorld& world,
        EntityID entity,
        f32_t& outAttackSpeed)
    {
        const PracticeChampionStatOverrideComponent* pOverrides =
            world.TryGetComponent<PracticeChampionStatOverrideComponent>(entity);
        if (!pOverrides)
            return false;

        const u8_t count = (std::min)(
            pOverrides->count,
            PracticeChampionStatOverrideComponent::kMaxEntries);
        for (u8_t i = 0u; i < count; ++i)
        {
            const PracticeChampionStatOverrideEntry& entry = pOverrides->entries[i];
            if (static_cast<eChampionStatOverrideId>(entry.statId) ==
                eChampionStatOverrideId::EffectiveAttackSpeed)
            {
                outAttackSpeed = entry.value;
                return true;
            }
        }
        return false;
    }
```

`ApplyRuntimeModifiers` 안의 아래 기존 코드:

```cpp
    stat.attackSpeed = CCombatFormula::ResolveAttackSpeed(
        stat.baseAttackSpeed,
        stat.attackSpeedRatio,
        stat.attackSpeedGrowth,
        stat.bonusAttackSpeed,
        stat.level);

    stat.abilityHaste = std::max(0.f, stat.abilityHaste);
```

아래로 교체:

```cpp
    stat.attackSpeed = CCombatFormula::ResolveAttackSpeed(
        stat.baseAttackSpeed,
        stat.attackSpeedRatio,
        stat.attackSpeedGrowth,
        stat.bonusAttackSpeed,
        stat.level);

    f32_t fPracticeEffectiveAttackSpeed = 0.f;
    if (TryResolvePracticeEffectiveAttackSpeed(
        world,
        entity,
        fPracticeEffectiveAttackSpeed))
    {
        stat.attackSpeed = fPracticeEffectiveAttackSpeed;
    }

    stat.abilityHaste = std::max(0.f, stat.abilityHaste);
```

### 1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`ResolveBasicAttackAnimSpeedScale` 함수 전체를 아래로 교체:

```cpp
    f32_t ResolveBasicAttackAnimSpeedScale(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<StatComponent>(entity))
            return 1.f;

        const auto& stat = world.GetComponent<StatComponent>(entity);
        const f32_t baseAttackSpeed =
            std::isfinite(stat.baseAttackSpeed) && stat.baseAttackSpeed > 0.001f
            ? stat.baseAttackSpeed
            : stat.attackSpeedRatio;
        if (!std::isfinite(stat.attackSpeed) ||
            stat.attackSpeed <= 0.001f ||
            !std::isfinite(baseAttackSpeed) ||
            baseAttackSpeed <= 0.001f)
        {
            return 1.f;
        }

        return std::clamp(stat.attackSpeed / baseAttackSpeed, 0.2f, 4.f);
    }
```

기존 호출:

```cpp
    const f32_t attackSpeedScale =
        ResolveBasicAttackAnimSpeedScale(world, cmd.issuerEntity, attackActionDurationSec);
```

아래로 교체:

```cpp
    const f32_t attackSpeedScale =
        ResolveBasicAttackAnimSpeedScale(world, cmd.issuerEntity);
```

### 1-8. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

Debug practice 상수 아래에 추가:

```cpp
    constexpr f32_t kAttackSpeedLabMin = 0.8f;
    constexpr f32_t kAttackSpeedLabMax = 2.5f;
```

`SetEnabled`의 `if (!bEnable)` 안에서 skill-effect override 정리 loop 바로 아래에 추가:

```cpp
            const auto championStatOverrideEntities =
                DeterministicEntityIterator<PracticeChampionStatOverrideComponent>
                    ::CollectSorted(m_world);
            for (EntityID overrideEntity : championStatOverrideEntities)
            {
                if (!m_world.IsAlive(overrideEntity))
                    continue;
                m_world.RemoveComponent<PracticeChampionStatOverrideComponent>(
                    overrideEntity);
                if (m_world.HasComponent<StatComponent>(overrideEntity))
                    m_world.GetComponent<StatComponent>(overrideEntity).bDirty = true;
            }

            const auto itemStatOverrideEntities =
                DeterministicEntityIterator<PracticeItemStatOverrideComponent>
                    ::CollectSorted(m_world);
            for (EntityID overrideEntity : itemStatOverrideEntities)
            {
                if (!m_world.IsAlive(overrideEntity))
                    continue;
                m_world.RemoveComponent<PracticeItemStatOverrideComponent>(overrideEntity);
                if (m_world.HasComponent<StatComponent>(overrideEntity))
                    m_world.GetComponent<StatComponent>(overrideEntity).bDirty = true;
            }
```

`ApplyChampionStatOverride`에서 일반 범위 검사 바로 아래에 추가:

```cpp
        const eChampionStatOverrideId statId =
            static_cast<eChampionStatOverrideId>(cmd.slot);
        if (statId == eChampionStatOverrideId::EffectiveAttackSpeed &&
            (cmd.practiceValue < kAttackSpeedLabMin ||
                cmd.practiceValue > kAttackSpeedLabMax))
        {
            return Finish(false, "effective-attack-speed-out-of-range");
        }
```

### 1-9. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
    bool_t m_bShowModelAnimPanel = false;
    bool_t m_bShowReplayControl = false;
```

아래로 교체:

```cpp
    bool_t m_bShowModelAnimPanel = false;
    bool_t m_bShowAttackSpeedLab = false;
    bool_t m_bShowReplayControl = false;
```

### 1-10. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp

숫자 8 처리 기존 코드:

```cpp
    // '8' = 공격속도 랩 (Attack Speed Lab).
    static bool_t s_bShowAttackSpeedLab = false;
    if (!ImGui::GetIO().WantCaptureKeyboard && input.IsKeyPressed('8'))
        s_bShowAttackSpeedLab = !s_bShowAttackSpeedLab;
```

아래로 교체:

```cpp
    // Numeric 8 opens the focused attack-speed authoring lab and reloads its JSON draft.
    if (!ImGui::GetIO().WantCaptureKeyboard && input.IsKeyPressed('8'))
    {
        m_bShowAttackSpeedLab = !m_bShowAttackSpeedLab;
        if (m_bShowAttackSpeedLab)
            UI::CAttackSpeedLab::Open();
    }
```

기존 렌더 코드:

```cpp
    if (s_bShowAttackSpeedLab)
```

아래로 교체:

```cpp
    if (m_bShowAttackSpeedLab)
```

### 1-11. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

`#include "Scene/Scene_InGame.h"` 바로 아래에 추가:

```cpp
#include "UI/AttackSpeedLab.h"
```

`CScene_InGame::OnEnter()` 시작 바로 아래에 추가:

```cpp
    UI::CAttackSpeedLab::ResetRuntime();
```

### 1-12. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

삭제할 코드:

```cpp
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
```

`eChampion animationChampion = champion.id;` 바로 아래에 추가하고, 기존 `ReplicatedActionComponent` 분기 안에서 source slot을 갱신:

```cpp
    u8_t replicatedSourceSlot = SlotFromReplicatedAction(actionId);
```

```cpp
        replicatedSourceSlot = action.sourceSlot;
```

`if (!animName.empty())` 블록 시작에 추가:

```cpp
        const bool_t bBasicAttackPresentation =
            UI::IsAttackSpeedPlaybackAction(actionId, replicatedSourceSlot);
```

기본 공격 배속 블록 전체를 아래로 교체:

```cpp
            // BA 모션은 복제된 최종 공속 / canonical base 비율로 재생하고,
            // AttackSpeedLab('8' 키)의 per-entity 시각 보정값을 그 위에 곱한다.
            if (bBasicAttackPresentation)
            {
                const UI::AttackSpeedPlaybackScales scales =
                    UI::ResolveAttackSpeedPlaybackScales(world, entity);
                playSpeed *= scales.fAttackSpeedScale *
                    scales.fAnimCorrectionScale;
            }
```

### 1-13. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp

UI include 묶음에 추가:

```cpp
#include "UI/AttackSpeedLab.h"
```

`ResolveNetworkActionDurationSec` 함수 전체를 아래로 교체:

```cpp
    f32_t ResolveNetworkActionDurationSec(
        eChampion champion,
        const SkillDef* pDef,
        u16_t actionId,
        u8_t stage,
        const RenderComponent& render,
        const std::string& animName,
        f32_t fAttackSpeedScale,
        f32_t fAnimCorrectionScale)
    {
        if (!std::isfinite(fAttackSpeedScale) || fAttackSpeedScale <= 0.01f)
            fAttackSpeedScale = 1.f;
        if (!std::isfinite(fAnimCorrectionScale) || fAnimCorrectionScale <= 0.01f)
            fAnimCorrectionScale = 1.f;

        const f32_t lockDurationSec =
            ResolveNetworkActionLockDurationSec(champion, pDef, actionId, stage);
        const f32_t scaledLockDurationSec = lockDurationSec / fAttackSpeedScale;
        const bool_t bLoopAction =
            champion == eChampion::JAX &&
            static_cast<eActionStateId>(actionId) == eActionStateId::SkillE &&
            stage <= 1u;
        if (bLoopAction || !render.pRenderer || animName.empty())
            return scaledLockDurationSec;

        const f32_t animDurationSec =
            render.pRenderer->GetAnimationDurationSecondsByName(animName);
        if (!std::isfinite(animDurationSec) || animDurationSec <= 0.01f)
            return scaledLockDurationSec;

        const f32_t effectivePlaySpeed =
            ResolveNetworkActionPlaySpeed(champion, pDef, actionId, stage) *
            fAttackSpeedScale *
            fAnimCorrectionScale;
        const f32_t visualDurationSec = animDurationSec / effectivePlaySpeed;
        if (!std::isfinite(visualDurationSec) || visualDurationSec <= 0.01f)
            return scaledLockDurationSec;

        return (std::min)(scaledLockDurationSec, visualDurationSec);
    }
```

`actionState.actionRemainingSec`를 설정하는 기존 호출을 아래로 교체:

```cpp
                        UI::AttackSpeedPlaybackScales attackSpeedScales{};
                        const bool_t bBasicAttackPresentation =
                            UI::IsAttackSpeedPlaybackAction(
                                pAction->actionId,
                                pAction->sourceSlot);
                        if (bBasicAttackPresentation)
                        {
                            attackSpeedScales =
                                UI::ResolveAttackSpeedPlaybackScales(m_World, e);
                        }
                        actionState.actionRemainingSec =
                            ResolveNetworkActionDurationSec(
                                actionChampion,
                                pSkillDef,
                                pAction->actionId,
                                pAction->stage,
                                rc,
                                actionAnimName,
                                attackSpeedScales.fAttackSpeedScale,
                                attackSpeedScales.fAnimCorrectionScale);
```

### 1-14. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunPracticeDefinitionOverlayProbe()` 바로 아래, `RunAuthoredNavGridProbe()` 바로 위에 추가:

```cpp
    bool_t RunAttackSpeedLabMatrixProbe()
    {
        static constexpr f32_t kTargetAttackSpeeds[] =
        {
            0.8f, 1.0f, 1.5f, 2.0f, 2.5f,
        };

        const auto NearlyEqual = [](f32_t lhs, f32_t rhs)
        {
            return std::fabs(lhs - rhs) <= 0.001f;
        };
        const auto ResolveExpectedTicks = [](f32_t durationSec, f32_t speedScale)
        {
            const f32_t scaledSec = std::clamp(
                durationSec / speedScale,
                DeterministicTime::kFixedDt,
                5.f);
            const u64_t ticks = static_cast<u64_t>(std::ceil(
                static_cast<f64_t>(scaledSec) *
                static_cast<f64_t>(DeterministicTime::kTicksPerSecond)));
            return ticks > 0u ? ticks : 1u;
        };

        const ChampionBasicAttackTimingDefaults attackTiming =
            GetDefaultChampionBasicAttackTiming(eChampion::IRELIA);
        u32_t sequence = 1u;

        for (const f32_t targetAttackSpeed : kTargetAttackSpeeds)
        {
            CWorld world;
            DeterministicRng rng(20260714ull + sequence);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();

            const EntityID attacker = SpawnChampion(
                world,
                entityMap,
                eChampion::IRELIA,
                static_cast<u8_t>(eTeam::Blue),
                0u);
            const EntityID target = SpawnChampion(
                world,
                entityMap,
                eChampion::JAX,
                static_cast<u8_t>(eTeam::Red),
                5u);
            world.GetComponent<TransformComponent>(attacker).SetPosition(Vec3{});
            world.GetComponent<TransformComponent>(target).SetPosition(
                Vec3{ 1.f, 0.f, 0.f });

            BuffComponent attackSpeedBuff{};
            attackSpeedBuff.count = 1u;
            attackSpeedBuff.buffs[0].stackCount = 1u;
            attackSpeedBuff.buffs[0].bonusAttackSpeedPerStack = 0.5f;
            world.AddComponent<BuffComponent>(attacker, attackSpeedBuff);
            world.GetComponent<StatComponent>(attacker).bDirty = true;
            CStatSystem::Execute(
                world,
                ServerData::GetLoLGameplayDefinitionPack());
            const StatComponent canonicalStat =
                world.GetComponent<StatComponent>(attacker);

            PracticeChampionStatOverrideComponent overrides{};
            overrides.count = 1u;
            overrides.revision = sequence;
            overrides.entries[0].statId = static_cast<u8_t>(
                eChampionStatOverrideId::EffectiveAttackSpeed);
            overrides.entries[0].value = targetAttackSpeed;
            world.AddComponent<PracticeChampionStatOverrideComponent>(
                attacker,
                overrides);
            world.GetComponent<StatComponent>(attacker).bDirty = true;
            CStatSystem::Execute(
                world,
                ServerData::GetLoLGameplayDefinitionPack());

            const StatComponent& appliedStat =
                world.GetComponent<StatComponent>(attacker);
            if (!NearlyEqual(appliedStat.attackSpeed, targetAttackSpeed) ||
                !NearlyEqual(appliedStat.baseAttackSpeed, canonicalStat.baseAttackSpeed) ||
                !NearlyEqual(appliedStat.attackSpeedRatio, canonicalStat.attackSpeedRatio))
            {
                std::printf(
                    "[SimLab][AttackSpeedLab] FAIL: stat target=%.3f effective=%.3f base=%.3f canonicalBase=%.3f ratio=%.3f canonicalRatio=%.3f\n",
                    targetAttackSpeed,
                    appliedStat.attackSpeed,
                    appliedStat.baseAttackSpeed,
                    canonicalStat.baseAttackSpeed,
                    appliedStat.attackSpeedRatio,
                    canonicalStat.attackSpeedRatio);
                return false;
            }

            const f32_t speedBase = appliedStat.baseAttackSpeed > 0.001f
                ? appliedStat.baseAttackSpeed
                : appliedStat.attackSpeedRatio;
            const f32_t expectedSpeedScale = std::clamp(
                targetAttackSpeed / speedBase,
                0.2f,
                4.f);
            const u64_t expectedActionTicks = ResolveExpectedTicks(
                attackTiming.fActionDurationSec,
                expectedSpeedScale);
            const u64_t expectedWindupTicks = (std::min)(
                expectedActionTicks,
                ResolveExpectedTicks(
                    attackTiming.fWindupSec,
                    expectedSpeedScale));
            const f32_t expectedCooldown = std::clamp(
                1.f / targetAttackSpeed,
                0.333f,
                5.f);

            TickContext attackTick = MakeProbeTickContext(
                100ull + sequence,
                rng,
                entityMap,
                walkable);
            GameCommand attack{};
            attack.kind = eCommandKind::BasicAttack;
            attack.issuerEntity = attacker;
            attack.targetEntity = target;
            attack.direction = Vec3{ 1.f, 0.f, 0.f };
            attack.sequenceNum = sequence;
            attack.issuedAtTick = attackTick.tickIndex;
            const CommandExecutionResult result =
                executor->ExecuteCommand(world, attackTick, attack);
            if (result.state != eCommandExecutionState::Accepted ||
                !world.HasComponent<CombatActionComponent>(attacker))
            {
                std::printf(
                    "[SimLab][AttackSpeedLab] FAIL: attack rejected target=%.3f state=%u reason=%u\n",
                    targetAttackSpeed,
                    static_cast<u32_t>(result.state),
                    static_cast<u32_t>(result.reason));
                return false;
            }

            const SkillSlotRuntime& basicAttackSlot =
                world.GetComponent<SkillStateComponent>(attacker).slots[0];
            const CombatActionComponent& action =
                world.GetComponent<CombatActionComponent>(attacker);
            const u64_t actualActionTicks = action.uEndTick - action.uStartTick;
            const u64_t actualWindupTicks = action.uImpactTick - action.uStartTick;
            if (!NearlyEqual(basicAttackSlot.cooldownRemaining, expectedCooldown) ||
                !NearlyEqual(basicAttackSlot.cooldownDuration, expectedCooldown) ||
                action.eKind != eCombatActionKind::BasicAttack ||
                action.uStartTick != attackTick.tickIndex ||
                actualActionTicks != expectedActionTicks ||
                actualWindupTicks != expectedWindupTicks)
            {
                std::printf(
                    "[SimLab][AttackSpeedLab] FAIL: timing target=%.3f cooldown=%.3f/%.3f action=%llu/%llu windup=%llu/%llu\n",
                    targetAttackSpeed,
                    basicAttackSlot.cooldownDuration,
                    expectedCooldown,
                    static_cast<unsigned long long>(actualActionTicks),
                    static_cast<unsigned long long>(expectedActionTicks),
                    static_cast<unsigned long long>(actualWindupTicks),
                    static_cast<unsigned long long>(expectedWindupTicks));
                return false;
            }

            world.RemoveComponent<PracticeChampionStatOverrideComponent>(attacker);
            world.GetComponent<StatComponent>(attacker).bDirty = true;
            CStatSystem::Execute(
                world,
                ServerData::GetLoLGameplayDefinitionPack());
            const StatComponent& restoredStat =
                world.GetComponent<StatComponent>(attacker);
            if (!NearlyEqual(restoredStat.attackSpeed, canonicalStat.attackSpeed) ||
                !NearlyEqual(restoredStat.baseAttackSpeed, canonicalStat.baseAttackSpeed) ||
                !NearlyEqual(restoredStat.attackSpeedRatio, canonicalStat.attackSpeedRatio))
            {
                std::printf(
                    "[SimLab][AttackSpeedLab] FAIL: clear target=%.3f restored=%.3f canonical=%.3f\n",
                    targetAttackSpeed,
                    restoredStat.attackSpeed,
                    canonicalStat.attackSpeed);
                return false;
            }

            std::printf(
                "[SimLab][AttackSpeedLab] target=%.3f cooldown=%.3f actionTicks=%llu windupTicks=%llu\n",
                targetAttackSpeed,
                expectedCooldown,
                static_cast<unsigned long long>(actualActionTicks),
                static_cast<unsigned long long>(actualWindupTicks));
            ++sequence;
        }

        std::printf(
            "[SimLab][AttackSpeedLab] PASS: effective 0.8..2.5, authoritative cadence/action/windup, clear restore\n");
        return true;
    }
```

main의 practice probe 호출 바로 아래에 추가:

```cpp
    const bool_t bAttackSpeedLabMatrixProbePass =
        RunAttackSpeedLabMatrixProbe();
```

최종 `bPass` AND 체인의 `bPracticeDefinitionOverlayProbePass &&` 바로 아래에 추가:

```cpp
        bAttackSpeedLabMatrixProbePass &&
```

### 1-15. C:/Users/user/Desktop/Winters/.gitignore

기존 코드:

```gitignore
/Client/Bin/Resource/Config/Practice/*
!/Client/Bin/Resource/Config/Practice/practice_balance_overrides.json
```

아래로 교체:

```gitignore
/Client/Bin/Resource/Config/Practice/*
!/Client/Bin/Resource/Config/Practice/practice_balance_overrides.json
!/Client/Bin/Resource/Config/Practice/attack_speed_tuning.json
```

### 1-16. C:/Users/user/Desktop/Winters/Shared/Schemas/Command.fbs

`PracticeOperation` 마지막 기존 코드:

```fbs
    RewindSimulationSeconds = 15,
    SpawnChampion = 16
```

아래로 교체:

```fbs
    RewindSimulationSeconds = 15,
    SpawnChampion = 16,
    ApplyChampionStatOverride = 17,
    ClearChampionStatOverrides = 18,
    ApplyItemStatOverride = 19,
    ClearItemStatOverrides = 20
```

원본 스키마 반영 뒤 generated C++/Go를 수동 편집하지 않고 아래 명령으로 재생성:

```powershell
cmd /c .\Shared\Schemas\run_codegen.bat
```

### 1-17. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

표준 라이브러리 include 묶음 바로 아래, anonymous namespace 바로 위에 추가:

```cpp
static_assert(
    static_cast<u16_t>(ePracticeOperation::Count) ==
    static_cast<u16_t>(Shared::Schema::PracticeOperation::MAX) + 1u,
    "ePracticeOperation and Command.fbs PracticeOperation must stay append-only and aligned.");
```

### 1-18. C:/Users/user/Desktop/Winters/Server/Private/Game/CommandIngress.cpp

표준 라이브러리 include 묶음 바로 아래, `CCommandIngress::AcceptCommandBatch` 바로 위에 추가:

```cpp
static_assert(
    static_cast<u16_t>(ePracticeOperation::Count) ==
    static_cast<u16_t>(Shared::Schema::PracticeOperation::MAX) + 1u,
    "ePracticeOperation and Command.fbs PracticeOperation must stay append-only and aligned.");
```

### 1-19. C:/Users/user/Desktop/Winters/.claude/gotchas.md

FlatBuffers authoritative packet gotcha 바로 아래에 추가:

```markdown
- 2026-07-14 - [Practice wire enums] adding `ePracticeOperation` only to GameSim leaves C++ raw casts apparently working while FlatBuffers/Go contracts remain stale -> append the same numeric value in `Shared/Schemas/Command.fbs`, run schema codegen, and keep the runtime `Count` versus generated `MAX` compile-time checks aligned.
```

## 2. 검증

자동 검증 완료 (2026-07-14):
- ChampionGameData 및 LoL definition pack `--check`가 각각 PASS했다.
- `Command.fbs`와 generated C++/Go `PracticeOperation`이 0~20으로 일치하며, runtime `Count`/generated `MAX` compile-time guard를 포함한 Client/Server 빌드가 PASS했다.
- Shared boundary 검사와 GameSim/Server/Client/SimLab Debug x64 빌드가 모두 PASS했다.
- Jax W 강화 평타처럼 action id가 `SkillW`여도 `sourceSlot == BasicAttack`인 경로를 공격속도 재생 대상으로 판별하는 compile-time 회귀 조건과 두 클라이언트 재생 경로를 반영했다.
- `SimLab.exe 1800 42`에서 effective AS 0.8/1.0/1.5/2.0/2.5가 각각 공격 쿨다운 1.25/1.0/0.667/0.5/0.4초, action tick 12/10/7/5/4, windup tick 4/4/3/2/2로 검증되었고 Clear 뒤 canonical 값 복귀까지 PASS했다.
- 동일 seed 결정성 및 전체 SimLab 회귀가 PASS했다.
- 10 bot, 1800 tick, seed 42의 5:5 서버 소크가 Jax W 강화 경로를 포함해 PASS했다. 증거는 `.md/build/evidence/s024_bot_soak/debug_ticks_1800_seed_42_20260714_173949_532_782162c1`에 있다.
- `startGold: 10000`, `startLevel: 6`은 canonical JSON과 generated pack에 이미 일치하며 데이터 검사로 확인했다.
- 실제 조작 캐릭터 전환은 이번 단계 범위가 아니다.

네이티브 수동 확인 필요:
- 터미널 자동 검증은 네이티브 ImGui를 직접 클릭하거나 모션을 육안 판정하지 못한다. 아래 체크리스트로 숫자 8 패널, Save/재로드, Applied 전환, 0.8/2.5 모션 캡처를 확인한다.

검증 명령:

```powershell
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
cmd /c .\Shared\Schemas\run_codegen.bat
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py --check
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
& $msbuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
Tools/Bin/Debug/SimLab.exe 1800 42
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/RunGameRoomBotMatchSoak.ps1 -TickCount 1800 -Seed 42 -Runs 1 -Configuration Debug -SkipServerBuild
git diff --check
```

수동 확인 체크리스트:
- `Client/Include/Client.vcxproj`와 `.filters`의 `AttackSpeedLab.h/.cpp` 등록은 확인 완료했다.
- `Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json`의 `startGold: 10000`, `startLevel: 6`과 generated pack의 동일 값은 `--check`로 확인 완료했다.
- 서버는 Debug x64에서 `--net-transport=tcp --no-sylas-smoke`로 실행해 정상 5:5 roster를 유지하고 Client Debug x64로 접속한다.
- 숫자 8을 누를 때 canonical `Client/Bin/Resource/Config/Practice/attack_speed_tuning.json`이 즉시 draft로 로드되는지 확인한다.
- Irelia 행을 선택해 0.8, 1.0, 1.5, 2.0, 2.5 각각 Apply하고 `Pending`이 `Applied`로 바뀌며 Observed AS가 목표값과 0.005 이내인지 기다린 뒤 공격한다.
- 각 값에서 서버 공격 주기, 임팩트 시점, 액션 종료와 클라이언트 모션이 함께 빨라지고, Model Animation Preview를 끈 상태에서 idle 조기 복귀/고정 0.46초 잔류가 없는지 확인한다.
- Save JSON 뒤 패널을 닫고 숫자 8로 다시 열어 값이 재로드되는지, `.bak`이 기존 파일을 보존하는지, 잘못된 JSON은 기존 draft/runtime 적용을 훼손하지 않고 오류를 표시하는지 확인한다.
- Clear Selected와 Practice disable 후 Observed AS 및 애니메이션 보정이 canonical 값으로 돌아오는지 확인한다.
- HUD/서버 trace에서 시작 골드 10000, 레벨 6을 확인한다.
- ceiling 산출물로 Irelia 0.8과 2.5의 동일 카메라/동일 대상 10초 캡처, 패널 Observed 값, SimLab 행렬 로그를 함께 남긴다.
