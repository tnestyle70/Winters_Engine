#include "Network/Client/ClientNetwork.h"
#include "UI/ChampionTuner.h"

#include "Core/CInput.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameInstance.h"
#include "GamePlay/LoLUIContentRegistry.h"
#include "Network/Client/CommandSerializer.h"
#include "Scene/Scene_InGame.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/ManaComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
#include "Shared/GameSim/Definitions/SkillAtomData.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "WintersPaths.h"

#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#include <imgui.h>
#pragma pop_macro("new")

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace
{
	using json = nlohmann::json;

	constexpr const char* kDefaultOverridePath =
		"Resource/Config/Practice/practice_balance_overrides.json";
	constexpr size_t kMaxPracticeOverrides = 32u;

	struct ParamOption
	{
		const char* pName = nullptr;
		eSkillEffectParamId id = eSkillEffectParamId::None;
	};

	constexpr std::array<ParamOption, 14> kParamOptions =
	{
		ParamOption{ "BaseDamage", eSkillEffectParamId::BaseDamage },
		ParamOption{ "DamagePerRank", eSkillEffectParamId::DamagePerRank },
		ParamOption{ "Range", eSkillEffectParamId::Range },
		ParamOption{ "Speed", eSkillEffectParamId::Speed },
		ParamOption{ "MoveSpeedMul", eSkillEffectParamId::MoveSpeedMul },
		ParamOption{ "StunDurationSec", eSkillEffectParamId::StunDurationSec },
		ParamOption{ "SlowDurationSec", eSkillEffectParamId::SlowDurationSec },
		ParamOption{ "AirborneDurationSec", eSkillEffectParamId::AirborneDurationSec },
		ParamOption{ "DashDistance", eSkillEffectParamId::DashDistance },
		ParamOption{ "DashDurationSec", eSkillEffectParamId::DashDurationSec },
		ParamOption{ "Radius", eSkillEffectParamId::Radius },
		ParamOption{ "EffectDurationSec", eSkillEffectParamId::EffectDurationSec },
		ParamOption{ "BonusAd", eSkillEffectParamId::BonusAd },
		ParamOption{ "BonusAttackSpeed", eSkillEffectParamId::BonusAttackSpeed },
	};

	struct StatOption
	{
		const char* pName = nullptr;
		eChampionStatOverrideId id = eChampionStatOverrideId::None;
	};

	constexpr std::array<StatOption, 16> kStatOptions =
	{
		StatOption{ "BaseHp", eChampionStatOverrideId::BaseHp },
		StatOption{ "HpPerLevel", eChampionStatOverrideId::HpPerLevel },
		StatOption{ "BaseMana", eChampionStatOverrideId::BaseMana },
		StatOption{ "ManaPerLevel", eChampionStatOverrideId::ManaPerLevel },
		StatOption{ "BaseAd", eChampionStatOverrideId::BaseAd },
		StatOption{ "AdPerLevel", eChampionStatOverrideId::AdPerLevel },
		StatOption{ "BaseAp", eChampionStatOverrideId::BaseAp },
		StatOption{ "ApPerLevel", eChampionStatOverrideId::ApPerLevel },
		StatOption{ "BaseArmor", eChampionStatOverrideId::BaseArmor },
		StatOption{ "ArmorPerLevel", eChampionStatOverrideId::ArmorPerLevel },
		StatOption{ "BaseMr", eChampionStatOverrideId::BaseMr },
		StatOption{ "MrPerLevel", eChampionStatOverrideId::MrPerLevel },
		StatOption{ "BaseAttackSpeed", eChampionStatOverrideId::BaseAttackSpeed },
		StatOption{ "AttackSpeedPerLevel", eChampionStatOverrideId::AttackSpeedPerLevel },
		StatOption{ "BaseMoveSpeed", eChampionStatOverrideId::BaseMoveSpeed },
		StatOption{ "BaseAttackRange", eChampionStatOverrideId::BaseAttackRange },
	};

	struct ItemFieldOption
	{
		const char* pName = nullptr;
		eItemStatOverrideField id = eItemStatOverrideField::None;
	};

	constexpr std::array<ItemFieldOption, 14> kItemFieldOptions =
	{
		ItemFieldOption{ "Price", eItemStatOverrideField::Price },
		ItemFieldOption{ "FlatAd", eItemStatOverrideField::FlatAd },
		ItemFieldOption{ "FlatAp", eItemStatOverrideField::FlatAp },
		ItemFieldOption{ "FlatHealth", eItemStatOverrideField::FlatHealth },
		ItemFieldOption{ "FlatMana", eItemStatOverrideField::FlatMana },
		ItemFieldOption{ "FlatArmor", eItemStatOverrideField::FlatArmor },
		ItemFieldOption{ "FlatMr", eItemStatOverrideField::FlatMr },
		ItemFieldOption{ "BonusAttackSpeed", eItemStatOverrideField::BonusAttackSpeed },
		ItemFieldOption{ "CritChance", eItemStatOverrideField::CritChance },
		ItemFieldOption{ "AbilityHaste", eItemStatOverrideField::AbilityHaste },
		ItemFieldOption{ "FlatMoveSpeed", eItemStatOverrideField::FlatMoveSpeed },
		ItemFieldOption{ "LifeSteal", eItemStatOverrideField::LifeSteal },
		ItemFieldOption{ "FlatMagicPen", eItemStatOverrideField::FlatMagicPen },
		ItemFieldOption{ "Lethality", eItemStatOverrideField::Lethality },
	};

	struct StatOverrideRow
	{
		int statIndex = 4; // BaseAd
		f32_t value = 0.f;
	};

	struct ItemOverrideRow
	{
		int itemId = 1055;
		int fieldIndex = 1; // FlatAd
		f32_t value = 0.f;
	};

	constexpr std::array<const char*, 5> kSkillSlotLabels =
	{
		"Basic Attack", "Q", "W", "E", "R"
	};

	constexpr std::array<const char*, 5> kSkillSlotJsonNames =
	{
		"BA", "Q", "W", "E", "R"
	};

	struct OverrideRow
	{
		u8_t slot = 1u;
		int paramIndex = 0;
		f32_t value = 0.f;
	};

	struct ObservedPlayer
	{
		bool_t bValid = false;
		f32_t hp = 0.f;
		f32_t maxHp = 0.f;
		f32_t mana = 0.f;
		f32_t maxMana = 0.f;
		u32_t gold = 0u;
		u8_t level = 1u;
		f32_t cooldowns[4]{};
		Vec3 position{};
		eChampion championId = static_cast<eChampion>(0);
	};

	struct PracticeToolState
	{
		PracticeToolState()
		{
			strncpy_s(path, kDefaultOverridePath, _TRUNCATE);
			strncpy_s(profileName, "Designer Scratch", _TRUNCATE);
			overrides.push_back({ 1u, 0, 100.f });
			overrides.push_back({ 1u, 1, 25.f });
		}

		char path[512]{};
		char profileName[128]{};
		std::filesystem::path resolvedPath{};
		std::vector<OverrideRow> overrides{};
		std::string status = "Ready.";
		u32_t lastSequence = 0u;
		bool_t bLoadedOnce = false;

		bool_t bInfiniteHealth = false;
		bool_t bInfiniteMana = false;
		bool_t bNoCooldowns = false;
		bool_t bInfiniteGold = false;

		f32_t teleportPosition[3]{};
		f32_t spawnPosition[3]{};
		bool_t bSeededPositions = false;
		bool_t bTeleportArmed = false;

		std::vector<StatOverrideRow> statOverrides{};
		std::vector<ItemOverrideRow> itemOverrides{};
		int goldTarget = 500;
		bool_t bSeededGoldTarget = false;
		int sheetItemId = 1055;
		int minionTeam = 1;
		int minionRole = 0;
		int minionLane = 1;

		bool_t bSimPauseRequested = false;
		int simStepTicks = 1;
		f32_t simTimeScale = 1.f;

		int overrideTargetNetId = 0;
		int spawnChampionId = 1;
		int spawnChampionTeam = 1;
		int spawnChampionBrain = 1;
	};

	PracticeToolState g_PracticeTool{};

	f32_t ResolveStatBaseline(const ChampionStatsDef& def, int statIndex)
	{
		switch (kStatOptions[static_cast<size_t>(statIndex)].id)
		{
		case eChampionStatOverrideId::BaseHp: return def.baseHp;
		case eChampionStatOverrideId::HpPerLevel: return def.hpPerLevel;
		case eChampionStatOverrideId::BaseMana: return def.baseMana;
		case eChampionStatOverrideId::ManaPerLevel: return def.manaPerLevel;
		case eChampionStatOverrideId::BaseAd: return def.baseAd;
		case eChampionStatOverrideId::AdPerLevel: return def.adPerLevel;
		case eChampionStatOverrideId::BaseAp: return def.baseAp;
		case eChampionStatOverrideId::ApPerLevel: return def.apPerLevel;
		case eChampionStatOverrideId::BaseArmor: return def.baseArmor;
		case eChampionStatOverrideId::ArmorPerLevel: return def.armorPerLevel;
		case eChampionStatOverrideId::BaseMr: return def.baseMr;
		case eChampionStatOverrideId::MrPerLevel: return def.mrPerLevel;
		case eChampionStatOverrideId::BaseAttackSpeed: return def.baseAttackSpeed;
		case eChampionStatOverrideId::AttackSpeedPerLevel: return def.attackSpeedPerLevel;
		case eChampionStatOverrideId::BaseMoveSpeed: return def.baseMoveSpeed;
		case eChampionStatOverrideId::BaseAttackRange: return def.baseAttackRange;
		default: return 0.f;
		}
	}

	f32_t ResolveItemFieldBaseline(const ItemDef& item, int fieldIndex)
	{
		switch (kItemFieldOptions[static_cast<size_t>(fieldIndex)].id)
		{
		case eItemStatOverrideField::Price: return static_cast<f32_t>(item.price);
		case eItemStatOverrideField::FlatAd: return item.stats.flatAd;
		case eItemStatOverrideField::FlatAp: return item.stats.flatAp;
		case eItemStatOverrideField::FlatHealth: return item.stats.flatHealth;
		case eItemStatOverrideField::FlatMana: return item.stats.flatMana;
		case eItemStatOverrideField::FlatArmor: return item.stats.flatArmor;
		case eItemStatOverrideField::FlatMr: return item.stats.flatMr;
		case eItemStatOverrideField::BonusAttackSpeed: return item.stats.bonusAttackSpeed;
		case eItemStatOverrideField::CritChance: return item.stats.critChance;
		case eItemStatOverrideField::AbilityHaste: return item.stats.abilityHaste;
		case eItemStatOverrideField::FlatMoveSpeed: return item.stats.flatMoveSpeed;
		case eItemStatOverrideField::LifeSteal: return item.stats.lifeSteal;
		case eItemStatOverrideField::FlatMagicPen: return item.stats.flatMagicPen;
		case eItemStatOverrideField::Lethality: return item.stats.lethality;
		default: return 0.f;
		}
	}

	StatOverrideRow* FindStatOverrideRow(PracticeToolState& state, int statIndex)
	{
		for (StatOverrideRow& row : state.statOverrides)
		{
			if (row.statIndex == statIndex)
				return &row;
		}
		return nullptr;
	}

	// 시트 편집: 베이스라인과 같아지면 오버라이드 행을 제거해 "변경분만" 남긴다.
	void UpsertStatOverrideRow(PracticeToolState& state, int statIndex, f32_t value, f32_t baseline)
	{
		StatOverrideRow* pRow = FindStatOverrideRow(state, statIndex);
		if (std::fabs(value - baseline) <= 0.0001f)
		{
			if (pRow)
			{
				state.statOverrides.erase(
					state.statOverrides.begin() + (pRow - state.statOverrides.data()));
			}
			return;
		}
		if (pRow)
		{
			pRow->value = value;
			return;
		}
		if (state.statOverrides.size() < kMaxPracticeOverrides)
			state.statOverrides.push_back({ statIndex, value });
	}

	ItemOverrideRow* FindItemOverrideRow(PracticeToolState& state, int itemId, int fieldIndex)
	{
		for (ItemOverrideRow& row : state.itemOverrides)
		{
			if (row.itemId == itemId && row.fieldIndex == fieldIndex)
				return &row;
		}
		return nullptr;
	}

	void UpsertItemOverrideRow(
		PracticeToolState& state, int itemId, int fieldIndex, f32_t value, f32_t baseline)
	{
		ItemOverrideRow* pRow = FindItemOverrideRow(state, itemId, fieldIndex);
		if (std::fabs(value - baseline) <= 0.0001f)
		{
			if (pRow)
			{
				state.itemOverrides.erase(
					state.itemOverrides.begin() + (pRow - state.itemOverrides.data()));
			}
			return;
		}
		if (pRow)
		{
			pRow->value = value;
			return;
		}
		if (state.itemOverrides.size() < kMaxPracticeOverrides)
			state.itemOverrides.push_back({ itemId, fieldIndex, value });
	}

	int FindParamIndex(const std::string& name)
	{
		for (size_t index = 0u; index < kParamOptions.size(); ++index)
		{
			if (name == kParamOptions[index].pName)
				return static_cast<int>(index);
		}
		return -1;
	}

	int FindSlotIndex(const std::string& name)
	{
		for (size_t index = 0u; index < kSkillSlotJsonNames.size(); ++index)
		{
			if (name == kSkillSlotJsonNames[index])
				return static_cast<int>(index);
		}
		return -1;
	}

	int FindStatIndex(const std::string& name)
	{
		for (size_t index = 0u; index < kStatOptions.size(); ++index)
		{
			if (name == kStatOptions[index].pName)
				return static_cast<int>(index);
		}
		return -1;
	}

	int FindItemFieldIndex(const std::string& name)
	{
		for (size_t index = 0u; index < kItemFieldOptions.size(); ++index)
		{
			if (name == kItemFieldOptions[index].pName)
				return static_cast<int>(index);
		}
		return -1;
	}

	std::filesystem::path ResolveExistingPath(const char* pPath)
	{
		if (!pPath || !pPath[0])
			return {};

		std::error_code ec{};
		const std::filesystem::path directPath{ pPath };
		if (std::filesystem::is_regular_file(directPath, ec))
			return std::filesystem::absolute(directPath, ec);

		wchar_t resolved[1024]{};
		const std::wstring widePath = directPath.wstring();
		if (WintersResolveContentPath(
			widePath.c_str(),
			resolved,
			static_cast<u32_t>(std::size(resolved))))
		{
			return std::filesystem::path{ resolved };
		}

		return {};
	}

	std::filesystem::path ResolveWritePath(PracticeToolState& state)
	{
		if (!state.resolvedPath.empty())
			return state.resolvedPath;

		if (const std::filesystem::path existing = ResolveExistingPath(state.path);
			!existing.empty())
		{
			return existing;
		}

		const std::filesystem::path requested{ state.path };
		if (requested.is_absolute())
			return requested;

		const std::filesystem::path defaultResolved =
			ResolveExistingPath(kDefaultOverridePath);
		if (!defaultResolved.empty())
		{
			const std::filesystem::path resourceRoot =
				defaultResolved.parent_path().parent_path().parent_path();
			std::filesystem::path resourceRelative = requested;
			auto component = requested.begin();
			if (component != requested.end() && component->string() == "Resource")
			{
				resourceRelative.clear();
				for (++component; component != requested.end(); ++component)
					resourceRelative /= *component;
			}
			return resourceRoot / resourceRelative;
		}

		return requested;
	}

	bool_t LoadOverrides(PracticeToolState& state)
	{
		const std::filesystem::path resolved = ResolveExistingPath(state.path);
		if (resolved.empty())
		{
			state.status = "Load failed: JSON file was not found in the runtime Resource tree.";
			return false;
		}

		std::ifstream file{ resolved };
		if (!file.is_open())
		{
			state.status = "Load failed: could not open JSON file.";
			return false;
		}

		try
		{
			json root{};
			file >> root;

			if (!root.is_object() || root.value("version", 0) != 1)
				throw std::runtime_error("version must be 1");
			if (root.value("scope", std::string{}) != "current-player")
				throw std::runtime_error("scope must be current-player");
			if (!root.contains("overrides") || !root["overrides"].is_array())
				throw std::runtime_error("overrides must be an array");
			if (root["overrides"].size() > kMaxPracticeOverrides)
				throw std::runtime_error("override count exceeds 32");

			std::vector<OverrideRow> parsed{};
			parsed.reserve(root["overrides"].size());
			for (const json& source : root["overrides"])
			{
				if (!source.is_object() ||
					!source.contains("slot") || !source["slot"].is_string() ||
					!source.contains("param") || !source["param"].is_string() ||
					!source.contains("value") || !source["value"].is_number())
				{
					throw std::runtime_error("each override requires slot, param, and numeric value");
				}

				const int slot = FindSlotIndex(source["slot"].get<std::string>());
				const int paramIndex = FindParamIndex(source["param"].get<std::string>());
				const f32_t value = source["value"].get<f32_t>();
				if (slot < 0)
					throw std::runtime_error("unknown skill slot");
				if (paramIndex < 0)
					throw std::runtime_error("unknown or disallowed parameter");
				if (!std::isfinite(value))
					throw std::runtime_error("override value must be finite");

				parsed.push_back(
					{ static_cast<u8_t>(slot), paramIndex, value });
			}

			// championStats / itemStats 배열은 선택 항목 (구버전 JSON 호환).
			std::vector<StatOverrideRow> parsedStats{};
			if (root.contains("championStats") && root["championStats"].is_array())
			{
				if (root["championStats"].size() > kMaxPracticeOverrides)
					throw std::runtime_error("championStats count exceeds 32");
				for (const json& source : root["championStats"])
				{
					if (!source.is_object() ||
						!source.contains("stat") || !source["stat"].is_string() ||
						!source.contains("value") || !source["value"].is_number())
					{
						throw std::runtime_error("each championStat requires stat and numeric value");
					}
					const int statIndex = FindStatIndex(source["stat"].get<std::string>());
					const f32_t value = source["value"].get<f32_t>();
					if (statIndex < 0)
						throw std::runtime_error("unknown champion stat");
					if (!std::isfinite(value))
						throw std::runtime_error("champion stat value must be finite");
					parsedStats.push_back({ statIndex, value });
				}
			}

			std::vector<ItemOverrideRow> parsedItems{};
			if (root.contains("itemStats") && root["itemStats"].is_array())
			{
				if (root["itemStats"].size() > kMaxPracticeOverrides)
					throw std::runtime_error("itemStats count exceeds 32");
				for (const json& source : root["itemStats"])
				{
					if (!source.is_object() ||
						!source.contains("itemId") || !source["itemId"].is_number_integer() ||
						!source.contains("field") || !source["field"].is_string() ||
						!source.contains("value") || !source["value"].is_number())
					{
						throw std::runtime_error("each itemStat requires itemId, field, and numeric value");
					}
					const int fieldIndex = FindItemFieldIndex(source["field"].get<std::string>());
					const int itemId = source["itemId"].get<int>();
					const f32_t value = source["value"].get<f32_t>();
					if (fieldIndex < 0)
						throw std::runtime_error("unknown item field");
					if (itemId <= 0 || itemId > 65535)
						throw std::runtime_error("itemId out of range");
					if (!std::isfinite(value))
						throw std::runtime_error("item stat value must be finite");
					parsedItems.push_back({ itemId, fieldIndex, value });
				}
			}

			const std::string profile =
				root.value("profileName", std::string{ "Designer Scratch" });
			strncpy_s(state.profileName, profile.c_str(), _TRUNCATE);
			state.overrides = std::move(parsed);
			state.statOverrides = std::move(parsedStats);
			state.itemOverrides = std::move(parsedItems);
			state.resolvedPath = resolved;
			state.status = "Loaded JSON. Values are local until Apply is sent to the server.";
			return true;
		}
		catch (const std::exception& exception)
		{
			state.status = std::string{ "Load failed: " } + exception.what();
			return false;
		}
	}

	bool_t SaveOverrides(PracticeToolState& state)
	{
		for (const OverrideRow& row : state.overrides)
		{
			if (row.slot >= kSkillSlotJsonNames.size() ||
				row.paramIndex < 0 ||
				row.paramIndex >= static_cast<int>(kParamOptions.size()) ||
				!std::isfinite(row.value))
			{
				state.status = "Save failed: an override row is invalid.";
				return false;
			}
		}

		const std::filesystem::path resolved = ResolveWritePath(state);
		if (resolved.empty())
		{
			state.status = "Save failed: target path is empty.";
			return false;
		}

		std::error_code ec{};
		if (!resolved.parent_path().empty())
			std::filesystem::create_directories(resolved.parent_path(), ec);
		if (ec)
		{
			state.status = "Save failed: could not create the target directory.";
			return false;
		}

		json root{};
		root["version"] = 1;
		root["profileName"] = state.profileName;
		root["scope"] = "current-player";
		root["overrides"] = json::array();
		for (const OverrideRow& row : state.overrides)
		{
			root["overrides"].push_back(
				{
					{ "slot", kSkillSlotJsonNames[row.slot] },
					{ "param", kParamOptions[row.paramIndex].pName },
					{ "value", row.value },
				});
		}
		root["championStats"] = json::array();
		for (const StatOverrideRow& row : state.statOverrides)
		{
			if (row.statIndex < 0 ||
				row.statIndex >= static_cast<int>(kStatOptions.size()) ||
				!std::isfinite(row.value))
			{
				state.status = "Save failed: a champion stat row is invalid.";
				return false;
			}
			root["championStats"].push_back(
				{
					{ "stat", kStatOptions[row.statIndex].pName },
					{ "value", row.value },
				});
		}
		root["itemStats"] = json::array();
		for (const ItemOverrideRow& row : state.itemOverrides)
		{
			if (row.fieldIndex < 0 ||
				row.fieldIndex >= static_cast<int>(kItemFieldOptions.size()) ||
				row.itemId <= 0 || row.itemId > 65535 ||
				!std::isfinite(row.value))
			{
				state.status = "Save failed: an item stat row is invalid.";
				return false;
			}
			root["itemStats"].push_back(
				{
					{ "itemId", row.itemId },
					{ "field", kItemFieldOptions[row.fieldIndex].pName },
					{ "value", row.value },
				});
		}

		std::ofstream file{ resolved, std::ios::trunc };
		if (!file.is_open())
		{
			state.status = "Save failed: could not open the target file.";
			return false;
		}

		file << root.dump(2) << '\n';
		if (!file.good())
		{
			state.status = "Save failed while writing JSON.";
			return false;
		}

		state.resolvedPath = resolved;
		state.status = "Saved JSON. Use Apply to update the authoritative server overlay.";
		return true;
	}

	bool_t CanSendPracticeCommand(CScene_InGame* pScene)
	{
		if (!pScene || !pScene->IsNetworkAuthoritativeGameplay())
			return false;

		CClientNetwork* pNetwork = pScene->GetNetworkView();
		return pScene->GetCommandSerializer() &&
			pNetwork &&
			pNetwork->IsConnected();
	}

	void SendPracticeCommand(
		CScene_InGame* pScene,
		PracticeToolState& state,
		ePracticeOperation operation,
		f32_t value = 0.f,
		u32_t flags = 0u,
		u8_t slot = 0u,
		const Vec3& position = {},
		NetEntityId targetNet = NULL_NET_ENTITY)
	{
		if (!CanSendPracticeCommand(pScene))
		{
			state.status = "Command not sent: connect to a server-authoritative Debug match.";
			return;
		}

		const u32_t sequence = pScene->GetCommandSerializer()->SendPracticeControl(
			*pScene->GetNetworkView(),
			operation,
			value,
			flags,
			slot,
			position,
			targetNet);
		if (sequence == 0u)
		{
			state.status = "Command not sent: client-side payload validation failed.";
			return;
		}

		state.lastSequence = sequence;
		state.status = "Command sent. Confirm the result through the observed server snapshot.";
	}

	ObservedPlayer ReadObservedPlayer(const CScene_InGame* pScene)
	{
		ObservedPlayer observed{};
		if (!pScene)
			return observed;

		const EntityID player = pScene->GetPlayerEntity();
		const CWorld& world = pScene->GetWorld();
		if (player == NULL_ENTITY || !world.IsAlive(player))
			return observed;

		observed.bValid = true;
		if (const ChampionComponent* pChampion =
			world.TryGetComponent<ChampionComponent>(player))
		{
			observed.championId = pChampion->id;
			observed.hp = pChampion->hp;
			observed.maxHp = pChampion->maxHp;
			observed.mana = pChampion->mana;
			observed.maxMana = pChampion->maxMana;
			observed.level = pChampion->level;
			for (size_t index = 0u; index < std::size(observed.cooldowns); ++index)
				observed.cooldowns[index] = pChampion->cooldowns[index];
		}

		if (const HealthComponent* pHealth =
			world.TryGetComponent<HealthComponent>(player))
		{
			observed.hp = pHealth->fCurrent;
			observed.maxHp = pHealth->fMaximum;
		}
		if (const ManaComponent* pMana =
			world.TryGetComponent<ManaComponent>(player))
		{
			observed.mana = pMana->fCurrent;
			observed.maxMana = pMana->fMaximum;
		}
		if (const GoldComponent* pGold =
			world.TryGetComponent<GoldComponent>(player))
		{
			observed.gold = pGold->amount;
		}
		if (const StatComponent* pStat =
			world.TryGetComponent<StatComponent>(player))
		{
			observed.level = pStat->level;
		}
		if (const SkillStateComponent* pSkillState =
			world.TryGetComponent<SkillStateComponent>(player))
		{
			for (size_t index = 0u; index < std::size(observed.cooldowns); ++index)
				observed.cooldowns[index] =
					pSkillState->slots[index + 1u].cooldownRemaining;
		}
		if (const TransformComponent* pTransform =
			world.TryGetComponent<TransformComponent>(player))
		{
			observed.position = pTransform->GetPosition();
		}

		return observed;
	}

	u32_t BuildOptionFlags(const PracticeToolState& state)
	{
		u32_t flags = 0u;
		if (state.bInfiniteHealth)
			flags |= kPracticeInfiniteHealthFlag;
		if (state.bInfiniteMana)
			flags |= kPracticeInfiniteManaFlag;
		if (state.bNoCooldowns)
			flags |= kPracticeNoCooldownFlag;
		if (state.bInfiniteGold)
			flags |= kPracticeInfiniteGoldFlag;
		return flags;
	}

	void RenderObservedSnapshot(const ObservedPlayer& observed)
	{
		if (!observed.bValid)
		{
			ImGui::TextDisabled("Observed snapshot: local player is not available yet.");
			return;
		}

		ImGui::Text(
			"HP %.1f / %.1f | Mana %.1f / %.1f | Gold %u | Level %u",
			observed.hp,
			observed.maxHp,
			observed.mana,
			observed.maxMana,
			observed.gold,
			static_cast<u32_t>(observed.level));
		ImGui::Text(
			"Cooldown Q %.2f | W %.2f | E %.2f | R %.2f",
			observed.cooldowns[0],
			observed.cooldowns[1],
			observed.cooldowns[2],
			observed.cooldowns[3]);
		ImGui::Text(
			"Position (%.2f, %.2f, %.2f)",
			observed.position.x,
			observed.position.y,
			observed.position.z);
	}

	void RenderOverrideTable(CScene_InGame* pScene, PracticeToolState& state)
	{
		if (ImGui::InputText("JSON Path", state.path, std::size(state.path)))
			state.resolvedPath.clear();
		ImGui::InputText("Profile", state.profileName, std::size(state.profileName));
		if (ImGui::Button("Load JSON"))
		{
			state.resolvedPath.clear();
			LoadOverrides(state);
		}
		ImGui::SameLine();
		if (ImGui::Button("Save JSON"))
			SaveOverrides(state);

		int deleteIndex = -1;
		if (ImGui::BeginTable(
			"PracticeOverrideTable",
			4,
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_ScrollY,
			ImVec2(0.f, 220.f)))
		{
			ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 105.f);
			ImGui::TableSetupColumn("Parameter");
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 125.f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30.f);
			ImGui::TableHeadersRow();

			for (size_t index = 0u; index < state.overrides.size(); ++index)
			{
				OverrideRow& row = state.overrides[index];
				ImGui::PushID(static_cast<int>(index));
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				int slot = static_cast<int>(row.slot);
				if (ImGui::Combo(
					"##Slot",
					&slot,
					kSkillSlotLabels.data(),
					static_cast<int>(kSkillSlotLabels.size())))
				{
					row.slot = static_cast<u8_t>(slot);
				}

				ImGui::TableSetColumnIndex(1);
				if (ImGui::BeginCombo(
					"##Parameter",
					kParamOptions[row.paramIndex].pName))
				{
					for (size_t paramIndex = 0u;
						paramIndex < kParamOptions.size();
						++paramIndex)
					{
						const bool_t bSelected =
							row.paramIndex == static_cast<int>(paramIndex);
						if (ImGui::Selectable(
							kParamOptions[paramIndex].pName,
							bSelected))
						{
							row.paramIndex = static_cast<int>(paramIndex);
						}
						if (bSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-1.f);
				ImGui::InputFloat("##Value", &row.value, 0.f, 0.f, "%.3f");

				ImGui::TableSetColumnIndex(3);
				if (ImGui::SmallButton("X"))
					deleteIndex = static_cast<int>(index);

				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		if (deleteIndex >= 0)
		{
			state.overrides.erase(state.overrides.begin() + deleteIndex);
		}

		ImGui::BeginDisabled(state.overrides.size() >= kMaxPracticeOverrides);
		if (ImGui::Button("Add Override"))
			state.overrides.push_back({ 1u, 0, 0.f });
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled(
			"%u / %u",
			static_cast<u32_t>(state.overrides.size()),
			static_cast<u32_t>(kMaxPracticeOverrides));

		ImGui::SetNextItemWidth(140.f);
		ImGui::InputInt("Target NetId (0 = self)", &state.overrideTargetNetId);
		if (state.overrideTargetNetId < 0)
			state.overrideTargetNetId = 0;
		ImGui::SameLine();
		ImGui::TextDisabled("F9 AI Debug 패널의 봇 netId로 적 챔피언 스킬도 튜닝 가능");

		const bool_t bCanSend = CanSendPracticeCommand(pScene);
		ImGui::BeginDisabled(!bCanSend || state.overrides.empty());
		if (ImGui::Button("Apply Overrides To Server"))
		{
			bool_t bValid = true;
			for (const OverrideRow& row : state.overrides)
			{
				if (row.slot >= kSkillSlotLabels.size() ||
					row.paramIndex < 0 ||
					row.paramIndex >= static_cast<int>(kParamOptions.size()) ||
					!std::isfinite(row.value))
				{
					bValid = false;
					break;
				}
			}

			if (!bValid)
			{
				state.status = "Apply failed: an override row is invalid.";
			}
			else
			{
				const NetEntityId overrideTarget =
					state.overrideTargetNetId > 0
					? static_cast<NetEntityId>(state.overrideTargetNetId)
					: NULL_NET_ENTITY;
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::ClearSkillEffectOverrides,
					0.f, 0u, 0u, {}, overrideTarget);
				for (const OverrideRow& row : state.overrides)
				{
					SendPracticeCommand(
						pScene,
						state,
						ePracticeOperation::ApplySkillEffectOverride,
						row.value,
						static_cast<u32_t>(kParamOptions[row.paramIndex].id),
						row.slot,
						{},
						overrideTarget);
				}
				state.status = "Override commands sent. Confirm values through gameplay and snapshots.";
			}
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!bCanSend);
		if (ImGui::Button("Clear Server Overrides"))
		{
			SendPracticeCommand(
				pScene,
				state,
				ePracticeOperation::ClearSkillEffectOverrides,
				0.f, 0u, 0u, {},
				state.overrideTargetNetId > 0
				? static_cast<NetEntityId>(state.overrideTargetNetId)
				: NULL_NET_ENTITY);
		}
		ImGui::EndDisabled();

		ImGui::BeginDisabled(!bCanSend);
		if (ImGui::Button("Reload Definitions From JSON (Server)"))
		{
			SendPracticeCommand(
				pScene,
				state,
				ePracticeOperation::ReloadGameplayDefinitions,
				0.f, 0u, 0u, {}, NULL_NET_ENTITY);
			state.status =
				"Definition reload requested: champions.json / SkillEffectGameplayDefs.json / SummonerSpellGameplayDefs.json";
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("JSON 저장 -> 이 버튼 -> 즉시 반영 (Debug 서버)");
	}
}

namespace UI
{
	void CChampionTuner::Render(CScene_InGame* pScene)
	{
		if (!pScene)
			return;

		PracticeToolState& state = g_PracticeTool;
		if (!state.bLoadedOnce)
		{
			state.bLoadedOnce = true;
			LoadOverrides(state);
		}

		const ObservedPlayer observed = ReadObservedPlayer(pScene);
		if (observed.bValid && !state.bSeededGoldTarget)
		{
			state.goldTarget = static_cast<int>(observed.gold);
			state.bSeededGoldTarget = true;
		}
		if (observed.bValid && !state.bSeededPositions)
		{
			state.teleportPosition[0] = observed.position.x;
			state.teleportPosition[1] = observed.position.y;
			state.teleportPosition[2] = observed.position.z;
			state.spawnPosition[0] = observed.position.x;
			state.spawnPosition[1] = observed.position.y;
			state.spawnPosition[2] = observed.position.z;
			state.bSeededPositions = true;
		}

		CClientNetwork* pNetwork = pScene->GetNetworkView();
		const bool_t bConnected = pNetwork && pNetwork->IsConnected();
		const bool_t bAuthoritative = pScene->IsNetworkAuthoritativeGameplay();
		const bool_t bCanSend = CanSendPracticeCommand(pScene);

		ImGui::SetNextWindowPos(ImVec2(540.f, 80.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(760.f, 820.f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Practice Tool / Balance Lab"))
		{
			ImGui::End();
			return;
		}

		ImGui::Text(
			"Connection: %s | Authority: %s | Last sequence: %u",
			bConnected ? "Connected" : "Disconnected",
			bAuthoritative ? "Server" : "Local/Replay",
			state.lastSequence);
		ImGui::TextWrapped(
			"Commands are accepted only by the Debug room host. This panel never mutates "
			"client ECS state; every result must return through server snapshots.");
		if (!bCanSend)
		{
			ImGui::TextColored(
				ImVec4(1.f, 0.55f, 0.25f, 1.f),
				"Practice commands are currently disabled.");
		}
		ImGui::Separator();

		RenderObservedSnapshot(observed);
		ImGui::Separator();

		ImGui::BeginDisabled(!bCanSend);
		if (ImGui::Button("Enable Practice Session"))
		{
			SendPracticeCommand(
				pScene, state, ePracticeOperation::SetEnabled, 1.f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Disable Practice Session"))
		{
			SendPracticeCommand(
				pScene, state, ePracticeOperation::SetEnabled, 0.f);
		}
		ImGui::EndDisabled();

		if (ImGui::CollapsingHeader(
			"Simulation Time",
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Pause"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::SetSimulationPaused, 1.f);
				state.bSimPauseRequested = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Resume"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::SetSimulationPaused, 0.f);
				state.bSimPauseRequested = false;
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(
				state.bSimPauseRequested ? "(pause requested)" : "(running)");

			ImGui::SetNextItemWidth(110.f);
			ImGui::InputInt("Ticks", &state.simStepTicks);
			if (state.simStepTicks < 1)
				state.simStepTicks = 1;
			if (state.simStepTicks > 300)
				state.simStepTicks = 300;
			ImGui::SameLine();
			if (ImGui::Button("Step"))
			{
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::StepSimulationTicks,
					static_cast<f32_t>(state.simStepTicks));
			}
			ImGui::SameLine();
			if (ImGui::Button("Step 1"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::StepSimulationTicks, 1.f);
			}
			ImGui::SameLine();
			if (ImGui::Button("Step 30 (1s)"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::StepSimulationTicks, 30.f);
			}

			ImGui::SetNextItemWidth(220.f);
			ImGui::SliderFloat(
				"Time Scale", &state.simTimeScale, 0.1f, 8.f, "%.2fx",
				ImGuiSliderFlags_Logarithmic);
			ImGui::SameLine();
			if (ImGui::Button("Apply Scale"))
			{
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::SetSimulationTimeScale,
					state.simTimeScale);
			}
			ImGui::SameLine();
			if (ImGui::Button("1.0x"))
			{
				state.simTimeScale = 1.f;
				SendPracticeCommand(
					pScene, state, ePracticeOperation::SetSimulationTimeScale, 1.f);
			}
			ImGui::Separator();
			ImGui::TextUnformatted("Chrono Break");
			ImGui::SameLine();
			if (ImGui::Button("Rewind 5s"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::RewindSimulationSeconds, 5.f);
				state.bSimPauseRequested = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Rewind 10s"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::RewindSimulationSeconds, 10.f);
				state.bSimPauseRequested = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Rewind 30s"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::RewindSimulationSeconds, 30.f);
				state.bSimPauseRequested = true;
			}
			ImGui::EndDisabled();
			ImGui::TextWrapped(
				"Pause freezes server ticks (world + bots). Gameplay input during "
				"pause is void (still acked); Practice/AI-debug commands keep working. "
				"Step runs N ticks then re-freezes. Time Scale changes wall-clock "
				"pacing only - sim dt stays fixed, determinism preserved. "
				"Rewind restores a 1s-interval keyframe (90s window) and LANDS PAUSED - "
				"use Step/Resume to continue; bots re-decide from the restored state.");
		}

		if (ImGui::CollapsingHeader("Spawn Champion (Practice)"))
		{
			static const char* const kChampionNames[] = {
				"Irelia", "Yasuo", "Kalista", "Sylas", "Viego", "Annie", "Ashe",
				"Fiora", "Garen", "Riven", "Zed", "Ezreal", "Yone", "Jax",
				"MasterYi", "Kindred", "LeeSin" };
			int championIndex = state.spawnChampionId - 1;
			if (championIndex < 0 || championIndex >= IM_ARRAYSIZE(kChampionNames))
				championIndex = 0;
			ImGui::SetNextItemWidth(160.f);
			if (ImGui::Combo("Champion", &championIndex, kChampionNames,
				IM_ARRAYSIZE(kChampionNames)))
			{
				state.spawnChampionId = championIndex + 1;
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.f);
			ImGui::Combo("Team", &state.spawnChampionTeam, "Blue\0Red\0");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(110.f);
			ImGui::Combo("Brain", &state.spawnChampionBrain, "Dummy\0AI Bot\0");

			ImGui::InputFloat3("Spawn Pos##champ", state.spawnPosition, "%.2f");
			ImGui::SameLine();
			ImGui::BeginDisabled(!observed.bValid);
			if (ImGui::Button("Use Current##champspawn"))
			{
				state.spawnPosition[0] = observed.position.x;
				state.spawnPosition[1] = observed.position.y;
				state.spawnPosition[2] = observed.position.z;
			}
			ImGui::EndDisabled();

			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Spawn Champion"))
			{
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::SpawnChampion,
					static_cast<f32_t>(state.spawnChampionTeam),
					static_cast<u32_t>(state.spawnChampionId),
					static_cast<u8_t>(state.spawnChampionBrain),
					Vec3{
						state.spawnPosition[0],
						state.spawnPosition[1],
						state.spawnPosition[2] });
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Practice Spawns##champ"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ClearPracticeSpawns);
			}
			ImGui::EndDisabled();
			ImGui::TextWrapped(
				"Dummy = stands still (target practice), AI Bot = plays. "
				"Sylas dummy is reserved by the smoke roster.");
		}

		if (ImGui::CollapsingHeader(
			"Player Options",
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Infinite Health", &state.bInfiniteHealth);
			ImGui::SameLine();
			ImGui::Checkbox("Infinite Mana", &state.bInfiniteMana);
			ImGui::Checkbox("No Cooldowns", &state.bNoCooldowns);
			ImGui::SameLine();
			ImGui::Checkbox("Infinite Gold", &state.bInfiniteGold);

			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Apply Options"))
			{
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::SetOptions,
					0.f,
					BuildOptionFlags(state));
			}
			ImGui::SameLine();
			if (ImGui::Button("Restore HP + Mana"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::RestoreHealthMana);
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset Cooldowns"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ResetCooldowns);
			}

			if (ImGui::Button("+10,000 Gold"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::AddGold, 10000.f);
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(120.f);
			ImGui::InputInt("##GoldTarget", &state.goldTarget);
			if (state.goldTarget < 0)
				state.goldTarget = 0;
			ImGui::SameLine();
			ImGui::BeginDisabled(!bCanSend || !observed.bValid);
			if (ImGui::Button("Set Gold"))
			{
				// 서버 AddGold op는 양수만 허용 — 감소 목표는 거절 사유를 그대로 보여준다.
				const long long delta =
					static_cast<long long>(state.goldTarget) -
					static_cast<long long>(observed.gold);
				if (delta > 0)
				{
					SendPracticeCommand(
						pScene, state, ePracticeOperation::AddGold,
						static_cast<f32_t>(delta));
				}
				else
				{
					state.status =
						"Set Gold: target is not above current gold (server AddGold is add-only).";
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!observed.bValid || observed.level >= 18u);
			if (ImGui::Button("Level Up"))
			{
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::SetLevel,
					static_cast<f32_t>(observed.level + 1u));
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Level 18"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::SetLevel, 18.f);
			}
			ImGui::EndDisabled();
		}

		if (ImGui::CollapsingHeader(
			"Teleport",
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::InputFloat3("Destination", state.teleportPosition, "%.2f");
			ImGui::BeginDisabled(!observed.bValid);
			if (ImGui::Button("Use Current Position##Teleport"))
			{
				state.teleportPosition[0] = observed.position.x;
				state.teleportPosition[1] = observed.position.y;
				state.teleportPosition[2] = observed.position.z;
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Teleport##Execute"))
			{
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::Teleport,
					0.f,
					0u,
					0u,
					Vec3{
						state.teleportPosition[0],
						state.teleportPosition[1],
						state.teleportPosition[2] });
			}
			ImGui::SameLine();
			if (ImGui::Button(state.bTeleportArmed
				? "Click Map... (ESC Cancel)##TeleportArm"
				: "Teleport (Click Map)##TeleportArm"))
			{
				state.bTeleportArmed = !state.bTeleportArmed;
			}
			ImGui::EndDisabled();

			if (state.bTeleportArmed && bCanSend)
			{
				auto& in = CInput::Get();
				if (in.IsKeyPressed(0x1B /* VK_ESCAPE */))
				{
					state.bTeleportArmed = false;
				}
				else if (!ImGui::GetIO().WantCaptureMouse && in.IsLButtonPressed())
				{
					// 지면 클릭 지점으로 즉시 텔레포트. 원거리는 미니맵 클릭(카메라 점프) 후 지면 클릭.
					const Vec3 clickPos = pScene->ResolveMouseMapSurfacePos();

					state.teleportPosition[0] = clickPos.x;
					state.teleportPosition[1] = clickPos.y;
					state.teleportPosition[2] = clickPos.z;
					SendPracticeCommand(
						pScene, state, ePracticeOperation::Teleport,
						0.f, 0u, 0u, clickPos);
					state.bTeleportArmed = false;
				}
			}
		}

		if (ImGui::CollapsingHeader("Champion Stats (Live)"))
		{
			ImGui::TextDisabled(
				"Full stat sheet. Baseline = champions.json table; edited fields become server overrides.");
			ImGui::TextDisabled(
				"Target = 'Target NetId' above (0 = self). Sheet baseline shows your champion's values.");

			const ChampionStatsDef baselineDef =
				ChampionGameDataDB::ResolveStats(observed.championId);
			for (int statIndex = 0; statIndex < static_cast<int>(kStatOptions.size()); ++statIndex)
			{
				const f32_t baseline = ResolveStatBaseline(baselineDef, statIndex);
				const StatOverrideRow* pRow = FindStatOverrideRow(state, statIndex);
				f32_t value = pRow ? pRow->value : baseline;

				ImGui::PushID(statIndex + 9000);
				ImGui::SetNextItemWidth(150.f);
				if (ImGui::InputFloat(
					kStatOptions[statIndex].pName, &value, 0.f, 0.f, "%.3f"))
				{
					if (std::isfinite(value) && value >= 0.f)
						UpsertStatOverrideRow(state, statIndex, value, baseline);
				}
				if (pRow)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("(override, base %.3f)", baseline);
				}
				ImGui::PopID();
			}
			ImGui::TextDisabled(
				"changed fields: %u / %u",
				static_cast<u32_t>(state.statOverrides.size()),
				static_cast<u32_t>(kMaxPracticeOverrides));

			ImGui::BeginDisabled(!bCanSend || state.statOverrides.empty());
			if (ImGui::Button("Apply Stats To Server"))
			{
				const NetEntityId statTarget =
					state.overrideTargetNetId > 0
					? static_cast<NetEntityId>(state.overrideTargetNetId)
					: NULL_NET_ENTITY;
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ClearChampionStatOverrides,
					0.f, 0u, 0u, {}, statTarget);
				for (const StatOverrideRow& row : state.statOverrides)
				{
					SendPracticeCommand(
						pScene, state, ePracticeOperation::ApplyChampionStatOverride,
						row.value, 0u,
						static_cast<u8_t>(kStatOptions[row.statIndex].id),
						{}, statTarget);
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Clear Server Stats"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ClearChampionStatOverrides,
					0.f, 0u, 0u, {},
					state.overrideTargetNetId > 0
					? static_cast<NetEntityId>(state.overrideTargetNetId)
					: NULL_NET_ENTITY);
			}
			ImGui::EndDisabled();
		}

		if (ImGui::CollapsingHeader("Item Balance (Live)"))
		{
			ImGui::TextDisabled(
				"Full field sheet per item: every stat listed, +0 fields included. Price gates BuyItem.");

			const ItemDef* pSheetItem =
				CItemRegistry::Instance().Find(static_cast<u16_t>(state.sheetItemId));
			char itemLabel[96]{};
			std::snprintf(itemLabel, sizeof(itemLabel), "%d %s",
				state.sheetItemId,
				pSheetItem && pSheetItem->displayName ? pSheetItem->displayName : "?");
			ImGui::SetNextItemWidth(260.f);
			if (ImGui::BeginCombo("Item##Sheet", itemLabel))
			{
				const u32_t entryCount = Client::GetLoLShopEditorEntryCount();
				for (u32_t entry = 0u; entry < entryCount; ++entry)
				{
					const Client::LoLShopEditorEntryView view =
						Client::GetLoLShopEditorEntry(entry);
					char optionLabel[96]{};
					std::snprintf(optionLabel, sizeof(optionLabel), "%u %s",
						static_cast<u32_t>(view.iItemId),
						view.pDisplayName ? view.pDisplayName : "?");
					if (ImGui::Selectable(optionLabel, state.sheetItemId == view.iItemId))
						state.sheetItemId = view.iItemId;
				}
				ImGui::EndCombo();
			}

			if (pSheetItem)
			{
				for (int fieldIndex = 0;
					fieldIndex < static_cast<int>(kItemFieldOptions.size());
					++fieldIndex)
				{
					const f32_t baseline = ResolveItemFieldBaseline(*pSheetItem, fieldIndex);
					const ItemOverrideRow* pRow =
						FindItemOverrideRow(state, state.sheetItemId, fieldIndex);
					f32_t value = pRow ? pRow->value : baseline;

					ImGui::PushID(fieldIndex + 10000);
					ImGui::SetNextItemWidth(150.f);
					if (ImGui::InputFloat(
						kItemFieldOptions[fieldIndex].pName, &value, 0.f, 0.f, "%.3f"))
					{
						if (std::isfinite(value) && value >= 0.f)
						{
							UpsertItemOverrideRow(
								state, state.sheetItemId, fieldIndex, value, baseline);
						}
					}
					if (pRow)
					{
						ImGui::SameLine();
						ImGui::TextDisabled("(override, base %.3f)", baseline);
					}
					ImGui::PopID();
				}
			}
			ImGui::TextDisabled(
				"changed fields (all items): %u / %u",
				static_cast<u32_t>(state.itemOverrides.size()),
				static_cast<u32_t>(kMaxPracticeOverrides));

			ImGui::BeginDisabled(!bCanSend || state.itemOverrides.empty());
			if (ImGui::Button("Apply Items To Server"))
			{
				const NetEntityId itemTarget =
					state.overrideTargetNetId > 0
					? static_cast<NetEntityId>(state.overrideTargetNetId)
					: NULL_NET_ENTITY;
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ClearItemStatOverrides,
					0.f, 0u, 0u, {}, itemTarget);
				for (const ItemOverrideRow& row : state.itemOverrides)
				{
					const u32_t packedFlags =
						(static_cast<u32_t>(row.itemId) << 8) |
						static_cast<u32_t>(kItemFieldOptions[row.fieldIndex].id);
					SendPracticeCommand(
						pScene, state, ePracticeOperation::ApplyItemStatOverride,
						row.value, packedFlags, 0u, {}, itemTarget);
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Clear Server Items"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ClearItemStatOverrides,
					0.f, 0u, 0u, {},
					state.overrideTargetNetId > 0
					? static_cast<NetEntityId>(state.overrideTargetNetId)
					: NULL_NET_ENTITY);
			}
			ImGui::EndDisabled();
		}

		if (ImGui::CollapsingHeader("Shop Layout"))
		{
			ImGui::TextDisabled(
				"Client-side shop arrangement. Purchases stay server-validated by item id.");

			const u32_t entryCount = Client::GetLoLShopEditorEntryCount();
			for (u32_t index = 0u; index < entryCount; ++index)
			{
				const Client::LoLShopEditorEntryView view = Client::GetLoLShopEditorEntry(index);
				ImGui::PushID(static_cast<int>(index) + 12000);
				if (ImGui::ArrowButton("##ShopUp", ImGuiDir_Up))
					Client::MoveLoLShopEditorEntry(index, true);
				ImGui::SameLine();
				if (ImGui::ArrowButton("##ShopDown", ImGuiDir_Down))
					Client::MoveLoLShopEditorEntry(index, false);
				ImGui::SameLine();
				bool bEnabled = static_cast<bool>(view.bEnabled);
				if (ImGui::Checkbox("##ShopEnabled", &bEnabled))
					Client::SetLoLShopEditorEntryEnabled(index, bEnabled);
				ImGui::SameLine();
				ImGui::Text("%u  %s",
					static_cast<u32_t>(view.iItemId),
					view.pDisplayName ? view.pDisplayName : "?");
				ImGui::PopID();
			}

			if (ImGui::Button("Apply Shop Layout"))
			{
				Client::ReapplyLoLShopItems(*CGameInstance::Get());
				state.status = "Shop layout re-registered to the in-game shop UI.";
			}
		}

		if (ImGui::CollapsingHeader(
			"Minion Spawn",
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			constexpr std::array<const char*, 2> teams = { "Blue", "Red" };
			constexpr std::array<const char*, 4> roles =
				{ "Melee", "Ranged", "Siege", "Super" };
			constexpr std::array<const char*, 3> lanes = { "Top", "Mid", "Bot" };

			ImGui::Combo(
				"Team", &state.minionTeam, teams.data(), static_cast<int>(teams.size()));
			ImGui::SameLine();
			ImGui::Combo(
				"Role", &state.minionRole, roles.data(), static_cast<int>(roles.size()));
			ImGui::SameLine();
			ImGui::Combo(
				"Lane", &state.minionLane, lanes.data(), static_cast<int>(lanes.size()));
			ImGui::InputFloat3("Spawn Position", state.spawnPosition, "%.2f");
			ImGui::BeginDisabled(!observed.bValid);
			if (ImGui::Button("Use Current Position##Spawn"))
			{
				state.spawnPosition[0] = observed.position.x;
				state.spawnPosition[1] = observed.position.y;
				state.spawnPosition[2] = observed.position.z;
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Spawn Minion"))
			{
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::SpawnMinion,
					static_cast<f32_t>(state.minionTeam),
					static_cast<u32_t>(state.minionLane),
					static_cast<u8_t>(state.minionRole),
					Vec3{
						state.spawnPosition[0],
						state.spawnPosition[1],
						state.spawnPosition[2] });
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Practice Spawns"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ClearPracticeSpawns);
			}
			ImGui::EndDisabled();
		}

		if (ImGui::CollapsingHeader(
			"Skill Effect Overrides",
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			RenderOverrideTable(pScene, state);
		}

		ImGui::Separator();
		ImGui::TextWrapped("Status: %s", state.status.c_str());
		ImGui::End();
	}
}
