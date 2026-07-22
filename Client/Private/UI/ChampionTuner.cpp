#include "Network/Client/ClientNetwork.h"
#include "Network/Client/SnapshotApplier.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
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
#include "Shared/GameSim/Definitions/SkillAtomData.h"
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
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
	using json = nlohmann::json;
	using ordered_json = nlohmann::ordered_json;

	constexpr const char* kDefaultOverridePath =
		"Resource/Config/Practice/practice_balance_overrides.json";
	constexpr const wchar_t* kChampionBalanceDataPath =
		L"Data\\Gameplay\\ChampionGameData\\champions.json";
	constexpr const wchar_t* kSkillEffectBalanceDataPath =
		L"Data\\LoL\\ServerPrivate\\Gameplay\\SkillEffectGameplayDefs.json";
	constexpr const wchar_t* kSpawnObjectBalanceDataPath =
		L"Data\\LoL\\ServerPrivate\\Gameplay\\SpawnObjectGameplayDefs.json";
	constexpr const wchar_t* kEconomyBalanceDataPath =
		L"Data\\LoL\\ServerPrivate\\Gameplay\\EconomyGameplayDefs.json";
	constexpr f32_t kMinionAttackRangeMin = 0.1f;
	constexpr f32_t kMinionAttackRangeMax = 100.f;
	constexpr f32_t kAdRatioAuthoringMax = 10.f;
	constexpr size_t kMaxPracticeOverrides = 32u;

	struct ParamOption
	{
		const char* pName = nullptr;
		eSkillEffectParamId id = eSkillEffectParamId::None;
	};

	constexpr std::array<ParamOption, 69> kParamOptions =
	{
		ParamOption{ "BaseDamage", eSkillEffectParamId::BaseDamage },
		ParamOption{ "DamagePerRank", eSkillEffectParamId::DamagePerRank },
		ParamOption{ "Range", eSkillEffectParamId::Range },
		ParamOption{ "Speed", eSkillEffectParamId::Speed },
		ParamOption{ "MoveSpeedMul", eSkillEffectParamId::MoveSpeedMul },
		ParamOption{ "StunDurationSec", eSkillEffectParamId::StunDurationSec },
		ParamOption{ "SlowDurationSec", eSkillEffectParamId::SlowDurationSec },
		ParamOption{ "AirborneDurationSec", eSkillEffectParamId::AirborneDurationSec },
		ParamOption{ "MarkDurationSec", eSkillEffectParamId::MarkDurationSec },
		ParamOption{ "StackWindowSec", eSkillEffectParamId::StackWindowSec },
		ParamOption{ "Gap", eSkillEffectParamId::Gap },
		ParamOption{ "DashDistance", eSkillEffectParamId::DashDistance },
		ParamOption{ "DashDurationSec", eSkillEffectParamId::DashDurationSec },
		ParamOption{ "TargetDashDurationSec", eSkillEffectParamId::TargetDashDurationSec },
		ParamOption{ "HalfAngleCos", eSkillEffectParamId::HalfAngleCos },
		ParamOption{ "Radius", eSkillEffectParamId::Radius },
		ParamOption{ "ShieldDurationSec", eSkillEffectParamId::ShieldDurationSec },
		ParamOption{ "ShieldBaseAmount", eSkillEffectParamId::ShieldBaseAmount },
		ParamOption{ "ShieldAmountPerRank", eSkillEffectParamId::ShieldAmountPerRank },
		ParamOption{ "ShieldArmorPerRank", eSkillEffectParamId::ShieldArmorPerRank },
		ParamOption{ "DashDelaySec", eSkillEffectParamId::DashDelaySec },
		ParamOption{ "EffectDurationSec", eSkillEffectParamId::EffectDurationSec },
		ParamOption{ "TickIntervalSec", eSkillEffectParamId::TickIntervalSec },
		ParamOption{ "RefreshDurationSec", eSkillEffectParamId::RefreshDurationSec },
		ParamOption{ "VanishDurationSec", eSkillEffectParamId::VanishDurationSec },
		ParamOption{ "MissingHealthDamageRatio", eSkillEffectParamId::MissingHealthDamageRatio },
		ParamOption{ "TargetHealthThresholdRatio", eSkillEffectParamId::TargetHealthThresholdRatio },
		ParamOption{ "AcquireRange", eSkillEffectParamId::AcquireRange },
		ParamOption{ "LifetimeSec", eSkillEffectParamId::LifetimeSec },
		ParamOption{ "RespawnSec", eSkillEffectParamId::RespawnSec },
		ParamOption{ "SideDotThreshold", eSkillEffectParamId::SideDotThreshold },
		ParamOption{ "TargetMaxHpRatio", eSkillEffectParamId::TargetMaxHpRatio },
		ParamOption{ "ChallengeDurationSec", eSkillEffectParamId::ChallengeDurationSec },
		ParamOption{ "HealDurationSec", eSkillEffectParamId::HealDurationSec },
		ParamOption{ "HealRadius", eSkillEffectParamId::HealRadius },
		ParamOption{ "HealIntervalSec", eSkillEffectParamId::HealIntervalSec },
		ParamOption{ "HealAmount", eSkillEffectParamId::HealAmount },
		ParamOption{ "MinHealthAmount", eSkillEffectParamId::MinHealthAmount },
		ParamOption{ "HealBaseAmount", eSkillEffectParamId::HealBaseAmount },
		ParamOption{ "HealAmountPerRank", eSkillEffectParamId::HealAmountPerRank },
		ParamOption{ "RectLength", eSkillEffectParamId::RectLength },
		ParamOption{ "RectWidth", eSkillEffectParamId::RectWidth },
		ParamOption{ "HalfWidth", eSkillEffectParamId::HalfWidth },
		ParamOption{ "DisarmDurationSec", eSkillEffectParamId::DisarmDurationSec },
		ParamOption{ "TornadoSpeed", eSkillEffectParamId::TornadoSpeed },
		ParamOption{ "TornadoDurationSec", eSkillEffectParamId::TornadoDurationSec },
		ParamOption{ "TornadoRadius", eSkillEffectParamId::TornadoRadius },
		ParamOption{ "TornadoDamage", eSkillEffectParamId::TornadoDamage },
		ParamOption{ "DashAreaRadius", eSkillEffectParamId::DashAreaRadius },
		ParamOption{ "DashAreaDamage", eSkillEffectParamId::DashAreaDamage },
		ParamOption{ "BonusAd", eSkillEffectParamId::BonusAd },
		ParamOption{ "BonusAttackSpeed", eSkillEffectParamId::BonusAttackSpeed },
		ParamOption{ "TotalAdRatio", eSkillEffectParamId::TotalAdRatio },
		ParamOption{ "BonusAdRatio", eSkillEffectParamId::BonusAdRatio },
		ParamOption{ "ApRatio", eSkillEffectParamId::ApRatio },
		ParamOption{ "NonEpicBaseDamage", eSkillEffectParamId::NonEpicBaseDamage },
		ParamOption{ "NonEpicDamagePerRank", eSkillEffectParamId::NonEpicDamagePerRank },
		ParamOption{ "CooldownRefundSec", eSkillEffectParamId::CooldownRefundSec },
		ParamOption{ "ManaRestoreFlat", eSkillEffectParamId::ManaRestoreFlat },
		ParamOption{ "CastTimeSec", eSkillEffectParamId::CastTimeSec },
		ParamOption{ "ManaCostPerRank", eSkillEffectParamId::ManaCostPerRank },
		ParamOption{ "CooldownReductionPerRank", eSkillEffectParamId::CooldownReductionPerRank },
		ParamOption{ "MaxStacks", eSkillEffectParamId::MaxStacks },
		ParamOption{ "RectLengthPerRank", eSkillEffectParamId::RectLengthPerRank },
		ParamOption{ "FormationDelaySec", eSkillEffectParamId::FormationDelaySec },
		ParamOption{ "DamagePerSpear", eSkillEffectParamId::DamagePerSpear },
		ParamOption{ "CooldownSecOverride", eSkillEffectParamId::CooldownSecOverride },
		ParamOption{ "DamageFlatOverride", eSkillEffectParamId::DamageFlatOverride },
		ParamOption{ "HealDamageRatio", eSkillEffectParamId::HealDamageRatio },
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

	constexpr std::array<ItemFieldOption, 19> kItemFieldOptions =
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
		ItemFieldOption{ "CritDamageBonus", eItemStatOverrideField::CritDamageBonus },
		ItemFieldOption{ "PercentMoveSpeed", eItemStatOverrideField::PercentMoveSpeed },
		ItemFieldOption{ "ArmorPenPercent", eItemStatOverrideField::ArmorPenPercent },
		ItemFieldOption{ "BonusArmorPenPercent", eItemStatOverrideField::BonusArmorPenPercent },
		ItemFieldOption{ "MagicPenPercent", eItemStatOverrideField::MagicPenPercent },
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

	struct BalanceDataDraft
	{
		std::filesystem::path championPath{};
		std::filesystem::path skillEffectPath{};
		std::filesystem::path spawnObjectPath{};
		std::filesystem::path economyPath{};
		std::string championSource{};
		std::string skillEffectSource{};
		std::string spawnObjectSource{};
		std::string economySource{};
		ordered_json champions{};
		ordered_json skillEffects{};
		ordered_json spawnObjects{};
		ordered_json economy{};
		int championIndex = 0;
		int skillSlot = 1;
		int minionRole = 0;
		int jungleSubKind = 0;
		bool_t bLoaded = false;
		bool_t bChampionDirty = false;
		bool_t bSkillEffectDirty = false;
		bool_t bSpawnObjectDirty = false;
		bool_t bEconomyDirty = false;
		u32_t pendingHotLoadSequence = 0u;
		u64_t expectedToolRevision = 0u;
	};

	struct PracticeToolState
	{
		PracticeToolState()
		{
			strncpy_s(path, kDefaultOverridePath, _TRUNCATE);
			strncpy_s(profileName, "Designer Scratch", _TRUNCATE);
		}

		char path[512]{};
		char profileName[128]{};
		std::filesystem::path resolvedPath{};
		std::vector<OverrideRow> overrides{};
		std::string status = "Ready.";
		u32_t lastSequence = 0u;
		bool_t bLoadedOnce = false;
		BalanceDataDraft balanceData{};

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

		// -1 = 미설정(팩 값 유지). 단일값이 전 랭크에 적용된다.
		f32_t skillCooldownDraft[4] = { -1.f, -1.f, -1.f, -1.f };
		f32_t skillDamageDraft[4] = { -1.f, -1.f, -1.f, -1.f };
		f32_t minionAttackDamageDraft[4] = { 40.f, 60.f, 40.f, 100.f };
		bool_t minionAttackDamageEnabled[4] = { false, false, false, false };
		f32_t jungleMaxHpDraft = 1000.f;
		f32_t jungleAttackDamageDraft = 50.f;
		f32_t structureTurretHpDraft = 3000.f;
		f32_t structureInhibitorHpDraft = 3000.f;
		f32_t structureNexusHpDraft = 5000.f;
		f32_t structureTurretDamageDraft = 150.f;
		int balanceCategory = static_cast<int>(
			UI::eBalanceTunerCategory::Champions);
		f32_t respawnSecondsByLevel[18] = {
			10.f, 10.f, 12.f, 12.f, 14.f, 16.f, 20.f, 25.f, 28.f,
			32.5f, 35.f, 37.5f, 40.f, 42.5f, 45.f, 47.5f, 50.f, 52.5f
		};
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

	f32_t ResolveItemFieldBaseline(
		const ClientData::ShopItemPresentationDefinition& item,
		int fieldIndex)
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
		case eItemStatOverrideField::CritDamageBonus: return item.stats.critDamageBonus;
		case eItemStatOverrideField::AbilityHaste: return item.stats.abilityHaste;
		case eItemStatOverrideField::PercentMoveSpeed: return item.stats.percentMoveSpeed;
		case eItemStatOverrideField::ArmorPenPercent: return item.stats.armorPenPercent;
		case eItemStatOverrideField::BonusArmorPenPercent: return item.stats.bonusArmorPenPercent;
		case eItemStatOverrideField::MagicPenPercent: return item.stats.magicPenPercent;
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

			f32_t parsedMinionDamage[4] = { 40.f, 60.f, 40.f, 100.f };
			bool_t parsedMinionEnabled[4] = { false, false, false, false };
			if (root.contains("minionAttackDamage") &&
				root["minionAttackDamage"].is_array())
			{
				for (const json& source : root["minionAttackDamage"])
				{
					if (!source.is_object() ||
						!source.contains("role") || !source["role"].is_number_integer() ||
						!source.contains("value") || !source["value"].is_number())
					{
						throw std::runtime_error(
							"each minionAttackDamage row requires role and numeric value");
					}
					const int role = source["role"].get<int>();
					const f32_t value = source["value"].get<f32_t>();
					if (role < 0 || role >= 4 || !std::isfinite(value) || value < 0.f)
						throw std::runtime_error("minionAttackDamage row is invalid");
					parsedMinionDamage[role] = value;
					parsedMinionEnabled[role] = true;
				}
			}

			const std::string profile =
				root.value("profileName", std::string{ "Designer Scratch" });
			strncpy_s(state.profileName, profile.c_str(), _TRUNCATE);
			state.overrides = std::move(parsed);
			state.statOverrides = std::move(parsedStats);
			state.itemOverrides = std::move(parsedItems);
			for (int role = 0; role < 4; ++role)
			{
				state.minionAttackDamageDraft[role] = parsedMinionDamage[role];
				state.minionAttackDamageEnabled[role] = parsedMinionEnabled[role];
			}
			for (f32_t& draft : state.skillDamageDraft)
				draft = -1.f;
			for (const OverrideRow& row : state.overrides)
			{
				if (row.slot >= 1u && row.slot <= 4u &&
					kParamOptions[row.paramIndex].id ==
						eSkillEffectParamId::DamageFlatOverride)
				{
					state.skillDamageDraft[row.slot - 1u] = row.value;
				}
			}
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
		root["minionAttackDamage"] = json::array();
		for (int role = 0; role < 4; ++role)
		{
			if (!state.minionAttackDamageEnabled[role])
				continue;
			const f32_t value = state.minionAttackDamageDraft[role];
			if (!std::isfinite(value) || value < 0.f)
			{
				state.status = "Save failed: a minion attack damage value is invalid.";
				return false;
			}
			root["minionAttackDamage"].push_back(
				{
					{ "role", role },
					{ "value", value },
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

	void SyncEssentialSkillRows(PracticeToolState& state)
	{
		const int flatDamageIndex = FindParamIndex("DamageFlatOverride");
		state.overrides.erase(
			std::remove_if(
				state.overrides.begin(),
				state.overrides.end(),
				[&](const OverrideRow& row)
				{
					return row.slot >= 1u && row.slot <= 4u &&
						row.paramIndex == flatDamageIndex;
				}),
			state.overrides.end());

		for (int slotIndex = 0; slotIndex < 4; ++slotIndex)
		{
			if (state.skillDamageDraft[slotIndex] < 0.f)
				continue;
			state.overrides.push_back(
				{ static_cast<u8_t>(slotIndex + 1),
				  flatDamageIndex,
				  state.skillDamageDraft[slotIndex] });
		}
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

	struct HotLoadAvailability
	{
		bool_t bAvailable = false;
		const char* pMessage = nullptr;
	};

	HotLoadAvailability ResolveHotLoadAvailability(CScene_InGame* pScene)
	{
#if !defined(_DEBUG)
		(void)pScene;
		return {
			false,
			"Hot Load unavailable: Release Client. Run Debug Client + Debug Server as room host."
		};
#else
		if (!pScene || !pScene->IsNetworkAuthoritativeGameplay())
		{
			return {
				false,
				"Hot Load unavailable: this scene is not server-authoritative."
			};
		}
		if (!pScene->GetCommandSerializer())
		{
			return {
				false,
				"Hot Load unavailable: the practice command sender is not ready."
			};
		}
		CClientNetwork* pNetwork = pScene->GetNetworkView();
		if (!pNetwork)
		{
			return {
				false,
				"Hot Load unavailable: the authoritative network session is missing."
			};
		}
		if (!pNetwork->IsConnected())
		{
			return {
				false,
				"Hot Load unavailable: the authoritative network session is disconnected."
			};
		}
		if (!pScene->GetSnapshotApplier())
		{
			return {
				false,
				"Hot Load unavailable: the server acknowledgement tracker is not ready."
			};
		}
		return {
			true,
			"Hot Load ready. The Debug server accepts this command from the room host only."
		};
#endif
	}

	u32_t SendPracticeCommand(
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
			return 0u;
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
			return 0u;
		}

		state.lastSequence = sequence;
		state.status = "Command sent. Confirm the result through the observed server snapshot.";
		return sequence;
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
		ImGui::TextWrapped(
			"Effect overrides are temporary scalar/variant parameters. Ranked damage formulas "
			"are canonical in ServerPrivate/Gameplay/SkillEffectGameplayDefs.json and are "
			"applied through the server definition reload path.");
		ImGui::TextWrapped(
			"Practice JSON is a session draft. Release truth changes only after canonical JSON "
			"validation, codegen, SimLab, and build succeed.");
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
				"Definition reload requested: champions / skill effects / spells / economy / items / spawns";
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("JSON 저장 -> 이 버튼 -> 즉시 반영 (Debug 서버)");
	}

	std::string ToLowerAscii(std::string value)
	{
		for (char& ch : value)
		{
			if (ch >= 'A' && ch <= 'Z')
				ch = static_cast<char>(ch - 'A' + 'a');
		}
		return value;
	}

	u32_t BalanceFnv1a32(const std::string& value)
	{
		u32_t hash = 2166136261u;
		for (const char ch : value)
		{
			hash ^= static_cast<u8_t>(ch);
			hash *= 16777619u;
		}
		return hash;
	}

	bool_t ReadTextFile(
		const std::filesystem::path& path,
		std::string& outText)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input.is_open())
			return false;
		outText.assign(
			std::istreambuf_iterator<char>(input),
			std::istreambuf_iterator<char>());
		return input.good() || input.eof();
	}

	bool_t ResolveBalanceDataPath(
		const wchar_t* pRelative,
		std::filesystem::path& outPath,
		std::string& outError)
	{
		wchar_t resolved[MAX_PATH]{};
		if (!WintersResolveContentPath(pRelative, resolved, MAX_PATH))
		{
			outError = "Workspace Data file not found.";
			return false;
		}
		outPath = resolved;
		return true;
	}

	bool_t ReadOrderedJson(
		const std::filesystem::path& path,
		std::string& outSource,
		ordered_json& outDocument,
		std::string& outError)
	{
		if (!ReadTextFile(path, outSource))
		{
			outError = "Read failed: " + path.filename().string();
			return false;
		}
		outDocument = ordered_json::parse(outSource, nullptr, false);
		if (outDocument.is_discarded())
		{
			outError = "JSON parse failed: " + path.filename().string();
			return false;
		}
		return true;
	}

	const char* SkillSlotToken(int slot)
	{
		static const char* const kTokens[5] = {
			"basic_attack", "q", "w", "e", "r"
		};
		return slot >= 0 && slot < 5 ? kTokens[slot] : "";
	}

	int SkillRankCount(int slot)
	{
		if (slot == 0)
			return 1;
		return slot == 4 ? 3 : 5;
	}

	bool_t IsRankedDamageParam(
		const std::string& effectKey,
		const char* pParam)
	{
		return
			(effectKey == "skill.yasuo.q" &&
				(std::strcmp(pParam, "tornadoDamage") == 0 ||
				 std::strcmp(pParam, "dashAreaDamage") == 0)) ||
			(effectKey == "skill.leesin.q" &&
				std::strcmp(pParam, "baseDamage") == 0) ||
			(effectKey == "skill.kalista.e" &&
				std::strcmp(pParam, "damagePerSpear") == 0) ||
			(effectKey == "skill.ezreal.r" &&
				std::strcmp(pParam, "nonEpicBaseDamage") == 0);
	}

	const char* RankedDamageParamLabel(
		const std::string& effectKey,
		const char* pParam)
	{
		if (effectKey == "skill.yasuo.q")
		{
			if (std::strcmp(pParam, "tornadoDamage") == 0)
				return "Q3 Tornado Flat Damage";
			if (std::strcmp(pParam, "dashAreaDamage") == 0)
				return "EQ Flat Damage";
		}
		if (effectKey == "skill.leesin.q")
			return "Q2 Recast Flat Damage";
		if (effectKey == "skill.kalista.e")
			return "Damage Per Spear";
		if (effectKey == "skill.ezreal.r")
			return "Non-Epic Flat Damage";
		return pParam;
	}

	ordered_json* FindChampionEntry(BalanceDataDraft& draft)
	{
		if (!draft.champions.contains("champions") ||
			!draft.champions["champions"].is_array() ||
			draft.champions["champions"].empty())
		{
			return nullptr;
		}
		auto& champions = draft.champions["champions"];
		draft.championIndex = std::clamp(
			draft.championIndex, 0, static_cast<int>(champions.size()) - 1);
		return &champions[static_cast<size_t>(draft.championIndex)];
	}

	ordered_json* FindChampionSkill(ordered_json& champion, int slot)
	{
		if (!champion.contains("skills") || !champion["skills"].is_array())
			return nullptr;
		for (ordered_json& skill : champion["skills"])
		{
			if (skill.value("slot", -1) == slot)
				return &skill;
		}
		return nullptr;
	}

	ordered_json* FindSkillEffect(
		BalanceDataDraft& draft,
		const std::string& championName,
		int slot)
	{
		if (!draft.skillEffects.contains("skillEffects") ||
			!draft.skillEffects["skillEffects"].is_array())
		{
			return nullptr;
		}
		const std::string key =
			"skill." + ToLowerAscii(championName) + "." + SkillSlotToken(slot);
		for (ordered_json& effect : draft.skillEffects["skillEffects"])
		{
			if (effect.value("key", std::string{}) == key)
				return &effect;
		}
		return nullptr;
	}

	ordered_json* FindMinionEntry(BalanceDataDraft& draft, int role)
	{
		if (!draft.spawnObjects.contains("minions") ||
			!draft.spawnObjects["minions"].is_array())
		{
			return nullptr;
		}
		for (ordered_json& minion : draft.spawnObjects["minions"])
		{
			if (minion.value("roleType", -1) == role)
				return &minion;
		}
		return nullptr;
	}

	bool_t ValidateNumber(
		const ordered_json& node,
		const char* pField,
		f32_t minValue,
		f32_t maxValue,
		std::string& outError,
		const std::string& owner)
	{
		if (!node.contains(pField) || !node[pField].is_number())
		{
			outError = owner + "." + pField + " must be a number";
			return false;
		}
		const f32_t value = node[pField].get<f32_t>();
		if (!std::isfinite(value) || value < minValue || value > maxValue)
		{
			outError = owner + "." + pField + " is out of range";
			return false;
		}
		return true;
	}

	bool_t ValidateRankArray(
		const ordered_json& node,
		const char* pField,
		int rankCount,
		f32_t minValue,
		f32_t maxValue,
		std::string& outError,
		const std::string& owner)
	{
		if (!node.contains(pField) || !node[pField].is_array() ||
			static_cast<int>(node[pField].size()) != rankCount)
		{
			outError = owner + "." + pField + " rank count mismatch";
			return false;
		}
		for (const ordered_json& valueNode : node[pField])
		{
			if (!valueNode.is_number())
			{
				outError = owner + "." + pField + " contains a non-number";
				return false;
			}
			const f32_t value = valueNode.get<f32_t>();
			if (!std::isfinite(value) || value < minValue || value > maxValue)
			{
				outError = owner + "." + pField + " is out of range";
				return false;
			}
		}
		return true;
	}

	bool_t EnsureCooldownRanks(
		ordered_json& skill,
		int rankCount,
		std::string& outError,
		const std::string& owner)
	{
		if (skill.contains("cooldownSecByRank"))
		{
			return ValidateRankArray(
				skill, "cooldownSecByRank", rankCount, 0.f, 3600.f,
				outError, owner);
		}
		if (!ValidateNumber(
			skill, "cooldownSec", 0.f, 3600.f, outError, owner))
		{
			return false;
		}
		const f32_t value = skill["cooldownSec"].get<f32_t>();
		skill["cooldownSecByRank"] = ordered_json::array();
		for (int rank = 0; rank < rankCount; ++rank)
			skill["cooldownSecByRank"].push_back(value);
		return true;
	}

	bool_t ValidateBalanceDraft(BalanceDataDraft& draft, std::string& outError)
	{
		if (!draft.champions.contains("champions") ||
			!draft.champions["champions"].is_array() ||
			draft.champions["champions"].empty() ||
			!draft.skillEffects.contains("skillEffects") ||
			!draft.skillEffects["skillEffects"].is_array() ||
			!draft.spawnObjects.contains("minions") ||
			!draft.spawnObjects["minions"].is_array() ||
			!draft.economy.contains("jungle") ||
			!draft.economy["jungle"].is_object() ||
			!draft.economy.contains("objectives") ||
			!draft.economy["objectives"].is_object())
		{
			outError = "Balance JSON is missing champions, skillEffects, minions, jungle, or objectives.";
			return false;
		}

		std::unordered_map<std::string, ordered_json*> effectByKey;
		std::unordered_map<u32_t, std::string> hashOwners;
		for (ordered_json& effect : draft.skillEffects["skillEffects"])
		{
			const std::string key = effect.value("key", std::string{});
			if (key.empty() || effectByKey.contains(key))
			{
				outError = "Duplicate or empty skill effect key: " + key;
				return false;
			}
			effectByKey.emplace(key, &effect);
			const u32_t hash = BalanceFnv1a32(key);
			const auto [it, inserted] = hashOwners.emplace(hash, key);
			if (!inserted && it->second != key)
			{
				outError = "Definition hash collision: " + it->second + " / " + key;
				return false;
			}
		}

		static const char* const kStatFields[] = {
			"baseHp", "hpPerLevel", "baseMana", "manaPerLevel",
			"baseAd", "adPerLevel", "baseAp", "apPerLevel",
			"baseArmor", "armorPerLevel", "baseMr", "mrPerLevel",
			"baseAttackSpeed", "attackSpeedPerLevel", "attackSpeedRatio",
			"resourceRegenPerSec"
		};
		static const char* const kDamageFields[] = {
			"flatByRank", "totalAdRatioByRank", "bonusAdRatioByRank",
			"apRatioByRank", "targetMaxHpRatioByRank",
			"targetMissingHpRatioByRank"
		};

		std::unordered_set<std::string> championNames;
		for (ordered_json& champion : draft.champions["champions"])
		{
			const std::string name = champion.value("champion", std::string{});
			const std::string token = ToLowerAscii(name);
			if (token.empty() || !championNames.emplace(token).second)
			{
				outError = "Duplicate or empty champion name: " + name;
				return false;
			}
			const std::string championKey = "champion." + token;
			const u32_t championHash = BalanceFnv1a32(championKey);
			const auto [hashIt, hashInserted] =
				hashOwners.emplace(championHash, championKey);
			if (!hashInserted && hashIt->second != championKey)
			{
				outError = "Definition hash collision: " + hashIt->second +
					" / " + championKey;
				return false;
			}

			if (!champion.contains("stats") || !champion["stats"].is_object())
			{
				outError = name + ".stats missing";
				return false;
			}
			for (const char* pField : kStatFields)
			{
				if (!ValidateNumber(
					champion["stats"], pField, 0.f, 1000000.f,
					outError, name + ".stats"))
				{
					return false;
				}
			}

			if (!champion.contains("skills") || !champion["skills"].is_array())
			{
				outError = name + ".skills missing";
				return false;
			}
			bool_t slots[5]{};
			for (ordered_json& skill : champion["skills"])
			{
				const int slot = skill.value("slot", -1);
				if (slot < 0 || slot >= 5 || slots[slot])
				{
					outError = name + " has duplicate or invalid skill slot";
					return false;
				}
				slots[slot] = true;
				const int rankCount = slot == 0 ? 1 : SkillRankCount(slot);
				if (!EnsureCooldownRanks(
					skill, rankCount, outError,
					name + "." + SkillSlotToken(slot)))
				{
					return false;
				}
				if (!ValidateNumber(
					skill, "rangeMax", 0.f, 500.f, outError,
					name + "." + SkillSlotToken(slot)))
				{
					return false;
				}
				// Keep legacy scalar readers in sync while the rank table remains the
				// canonical editor representation.
				if (skill.contains("cooldownSec"))
				{
					skill["cooldownSec"] = skill["cooldownSecByRank"][0];
				}

				const std::string effectKey =
					"skill." + token + "." + SkillSlotToken(slot);
				const auto effectIt = effectByKey.find(effectKey);
				if (effectIt == effectByKey.end())
				{
					outError = "Missing skill effect: " + effectKey;
					return false;
				}
				ordered_json& effect = *effectIt->second;
				if (!effect.contains("damage") || !effect["damage"].is_object())
				{
					outError = effectKey + ".damage missing";
					return false;
				}
				for (const char* pField : kDamageFields)
				{
					if (!ValidateRankArray(
						effect["damage"], pField, rankCount,
						-1000000.f, 1000000.f, outError,
						effectKey + ".damage"))
					{
						return false;
					}
				}
				if (effect.contains("params") && effect["params"].is_object())
				{
					ordered_json& params = effect["params"];
					static const char* const kRankedDamageParams[] = {
						"baseDamage", "damagePerSpear", "tornadoDamage",
						"dashAreaDamage", "nonEpicBaseDamage"
					};
					for (const char* pParam : kRankedDamageParams)
					{
						if (IsRankedDamageParam(effectKey, pParam) &&
							!ValidateRankArray(
								params, pParam, rankCount, 0.f, 1000000.f,
								outError, effectKey + ".params"))
						{
							return false;
						}
					}
					struct ParamRange
					{
						const char* pField;
						f32_t maxValue;
					};
					static const ParamRange kPresentMechanics[] = {
						{ "radius", 100.f },
						{ "formationDelaySec", 10.f },
						{ "healDamageRatio", 5.f },
					};
					for (const ParamRange& range : kPresentMechanics)
					{
						if (params.contains(range.pField) &&
							!ValidateNumber(
								params, range.pField, 0.f, range.maxValue,
								outError, effectKey + ".params"))
						{
							return false;
						}
					}
				}
			}
			for (bool_t bPresent : slots)
			{
				if (!bPresent)
				{
					outError = name + " must contain skill slots 0..4 exactly once";
					return false;
				}
			}
		}

		bool_t laneRoles[4]{};
		for (ordered_json& minion : draft.spawnObjects["minions"])
		{
			const int role = minion.value("roleType", -1);
			if (role < 0)
			{
				outError = "minions[].roleType invalid";
				return false;
			}
			if (role < 4)
			{
				if (laneRoles[role])
				{
					outError = "Duplicate lane minion role";
					return false;
				}
				laneRoles[role] = true;
				if (!ValidateNumber(
					minion, "maxHp", 1.f, 1000000.f, outError,
					"minions[" + std::to_string(role) + "]") ||
					!ValidateNumber(
						minion, "attackDamage", 0.f, 1000000.f, outError,
						"minions[" + std::to_string(role) + "]") ||
					!ValidateNumber(
						minion, "attackRange",
						kMinionAttackRangeMin, kMinionAttackRangeMax, outError,
						"minions[" + std::to_string(role) + "]"))
				{
					return false;
				}
			}
		}
		for (bool_t bPresent : laneRoles)
		{
			if (!bPresent)
			{
				outError = "Lane minion roles 0..3 must exist";
				return false;
			}
		}

		if (!draft.spawnObjects.contains("structure") ||
			!draft.spawnObjects["structure"].is_object() ||
			!draft.spawnObjects["structure"].contains("turretAI") ||
			!draft.spawnObjects["structure"]["turretAI"].is_object())
		{
			outError = "structure.turretAI missing";
			return false;
		}
		if (!ValidateNumber(
			draft.spawnObjects["structure"], "turretMaxHp", 1.f, 1000000.f,
			outError, "structure") ||
			!ValidateNumber(
				draft.spawnObjects["structure"]["turretAI"],
				"attackDamage", 0.f, 1000000.f, outError,
				"structure.turretAI") ||
			!ValidateNumber(
				draft.spawnObjects["structure"]["turretAI"],
				"nexusAttackDamage", 0.f, 1000000.f, outError,
				"structure.turretAI"))
		{
			return false;
		}

		if (!draft.spawnObjects.contains("jungleCamps") ||
			!draft.spawnObjects["jungleCamps"].is_array())
		{
			outError = "jungleCamps missing";
			return false;
		}
		for (ordered_json& camp : draft.spawnObjects["jungleCamps"])
		{
			const int subKind = camp.value("subKind", -1);
			if (subKind < 0 || subKind > 10 ||
				!ValidateNumber(camp, "maxHp", 1.f, 1000000.f, outError, "jungleCamp") ||
				!ValidateNumber(camp, "attackDamage", 0.f, 1000000.f, outError, "jungleCamp") ||
				!ValidateNumber(camp, "baseArmor", 0.f, 1000000.f, outError, "jungleCamp") ||
				!ValidateNumber(camp, "baseMr", 0.f, 1000000.f, outError, "jungleCamp"))
			{
				return false;
			}
		}

		ordered_json& jungleRewards = draft.economy["jungle"];
		static const char* const kJungleRewardFields[] = {
			"smallCampGold", "smallCampXP", "epicGold", "epicXP", "baronGold", "baronXP"
		};
		for (const char* pField : kJungleRewardFields)
		{
			if (!ValidateNumber(jungleRewards, pField, 0.f, 1000000.f, outError, "jungle"))
				return false;
		}
		ordered_json& objectives = draft.economy["objectives"];
		static const char* const kObjectiveFields[] = {
			"teamGoldPerChampion", "buffDurationSec", "baronRecallDurationMultiplier",
			"baronAuraRadius", "baronMinionHpMultiplier",
			"baronMinionAttackDamageMultiplier", "baronMinionScaleMultiplier",
			"elderAttackDamageMultiplier", "elderBurnDurationSec",
			"elderBurnTickIntervalSec", "elderBurnTargetMaxHpRatioPerTick",
			"elderExecuteThresholdRatio", "blueManaRegenPerSec",
			"redHealthRegenPerSec", "redBurnDurationSec",
			"redBurnTickIntervalSec", "redBurnDamagePerTick"
		};
		for (const char* pField : kObjectiveFields)
		{
			if (!ValidateNumber(objectives, pField, 0.f, 1000000.f, outError, "objectives"))
				return false;
		}
		if (!ValidateNumber(
			objectives, "elderBurnTickIntervalSec", 0.001f, 1000000.f,
			outError, "objectives") ||
			!ValidateNumber(
				objectives, "redBurnTickIntervalSec", 0.001f, 1000000.f,
				outError, "objectives") ||
			!ValidateNumber(
				objectives, "elderBurnTargetMaxHpRatioPerTick", 0.f, 1.f,
				outError, "objectives") ||
			!ValidateNumber(
				objectives, "elderExecuteThresholdRatio", 0.f, 1.f,
				outError, "objectives"))
		{
			return false;
		}
		if (!objectives.contains("teamLevelGrant") ||
			!objectives["teamLevelGrant"].is_number_integer())
		{
			outError = "objectives.teamLevelGrant must be an integer";
			return false;
		}
		const int levelGrant = objectives["teamLevelGrant"].get<int>();
		if (levelGrant < 0 || levelGrant > 18)
		{
			outError = "objectives.teamLevelGrant must be in [0, 18]";
			return false;
		}
		return true;
	}

	bool_t LoadBalanceData(PracticeToolState& state)
	{
		BalanceDataDraft loaded{};
		loaded.championIndex = state.balanceData.championIndex;
		loaded.skillSlot = state.balanceData.skillSlot;
		loaded.minionRole = state.balanceData.minionRole;
		loaded.jungleSubKind = state.balanceData.jungleSubKind;
		std::string error;
		if (!ResolveBalanceDataPath(
			kChampionBalanceDataPath, loaded.championPath, error) ||
			!ResolveBalanceDataPath(
				kSkillEffectBalanceDataPath, loaded.skillEffectPath, error) ||
			!ResolveBalanceDataPath(
				kSpawnObjectBalanceDataPath, loaded.spawnObjectPath, error) ||
			!ResolveBalanceDataPath(
				kEconomyBalanceDataPath, loaded.economyPath, error) ||
			!ReadOrderedJson(
				loaded.championPath, loaded.championSource,
				loaded.champions, error) ||
			!ReadOrderedJson(
				loaded.skillEffectPath, loaded.skillEffectSource,
				loaded.skillEffects, error) ||
			!ReadOrderedJson(
				loaded.spawnObjectPath, loaded.spawnObjectSource,
				loaded.spawnObjects, error) ||
			!ReadOrderedJson(
				loaded.economyPath, loaded.economySource,
				loaded.economy, error) ||
			!ValidateBalanceDraft(loaded, error))
		{
			state.status = "Balance draft load failed: " + error;
			state.balanceData.bLoaded = false;
			return false;
		}
		loaded.bLoaded = true;
		state.balanceData = std::move(loaded);
		state.status = "Balance Data JSON loaded.";
		return true;
	}

	struct PendingBalanceWrite
	{
		std::filesystem::path path{};
		std::filesystem::path tempPath{};
		std::filesystem::path backupPath{};
		std::string* pSource = nullptr;
		ordered_json* pDocument = nullptr;
		int indent = 2;
		std::string serialized{};
	};

	void CleanupPendingWrites(std::vector<PendingBalanceWrite>& writes)
	{
		for (PendingBalanceWrite& write : writes)
		{
			std::error_code ec;
			std::filesystem::remove(write.tempPath, ec);
			std::filesystem::remove(write.backupPath, ec);
		}
	}

	bool_t SaveBalanceData(PracticeToolState& state)
	{
		BalanceDataDraft& draft = state.balanceData;
		std::string error;
		if (!draft.bLoaded || !ValidateBalanceDraft(draft, error))
		{
			state.status = "Save blocked: " + error;
			return false;
		}

		std::vector<PendingBalanceWrite> writes;
		if (draft.bChampionDirty)
			writes.push_back({ draft.championPath, {}, {}, &draft.championSource, &draft.champions, 4 });
		if (draft.bSkillEffectDirty)
			writes.push_back({ draft.skillEffectPath, {}, {}, &draft.skillEffectSource, &draft.skillEffects, 2 });
		if (draft.bSpawnObjectDirty)
			writes.push_back({ draft.spawnObjectPath, {}, {}, &draft.spawnObjectSource, &draft.spawnObjects, 2 });
		if (draft.bEconomyDirty)
			writes.push_back({ draft.economyPath, {}, {}, &draft.economySource, &draft.economy, 2 });

		for (PendingBalanceWrite& write : writes)
		{
			std::string diskSource;
			if (!ReadTextFile(write.path, diskSource) || diskSource != *write.pSource)
			{
				state.status = "Save blocked: " + write.path.filename().string() +
					" changed outside F4. Use Reload JSON.";
				return false;
			}
			write.serialized = write.pDocument->dump(write.indent) + "\n";
			const ordered_json parsed =
				ordered_json::parse(write.serialized, nullptr, false);
			if (parsed.is_discarded())
			{
				state.status = "Save blocked: serialized JSON preflight failed.";
				return false;
			}
			write.tempPath = write.path;
			write.tempPath += L".balance.tmp";
			write.backupPath = write.path;
			write.backupPath += L".balance.bak";
			std::ofstream output(write.tempPath, std::ios::binary | std::ios::trunc);
			if (!output.is_open())
			{
				CleanupPendingWrites(writes);
				state.status = "Save failed: cannot write " +
					write.path.filename().string();
				return false;
			}
			output.write(
				write.serialized.data(),
				static_cast<std::streamsize>(write.serialized.size()));
			output.close();
			if (!output)
			{
				CleanupPendingWrites(writes);
				state.status = "Save failed: temp write incomplete.";
				return false;
			}
		}

		for (PendingBalanceWrite& write : writes)
		{
			std::error_code ec;
			std::filesystem::copy_file(
				write.path, write.backupPath,
				std::filesystem::copy_options::overwrite_existing, ec);
			if (ec)
			{
				CleanupPendingWrites(writes);
				state.status = "Save failed: backup creation failed.";
				return false;
			}
		}

		bool_t bReplacedAll = true;
		for (PendingBalanceWrite& write : writes)
		{
			if (!MoveFileExW(
				write.tempPath.c_str(), write.path.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				bReplacedAll = false;
				break;
			}
		}
		if (!bReplacedAll)
		{
			for (PendingBalanceWrite& write : writes)
			{
				std::error_code ec;
				if (std::filesystem::exists(write.backupPath, ec))
				{
					std::filesystem::copy_file(
						write.backupPath, write.path,
						std::filesystem::copy_options::overwrite_existing, ec);
				}
			}
			CleanupPendingWrites(writes);
			state.status = "Save failed: JSON transaction rolled back.";
			return false;
		}

		for (PendingBalanceWrite& write : writes)
			*write.pSource = write.serialized;
		CleanupPendingWrites(writes);
		draft.bChampionDirty = false;
		draft.bSkillEffectDirty = false;
		draft.bSpawnObjectDirty = false;
		draft.bEconomyDirty = false;
		state.status = writes.empty()
			? "No file changes; requesting server hot load."
			: "Data JSON saved; requesting server hot load.";
		return true;
	}
}

namespace UI
{
	void CChampionTuner::Open(eBalanceTunerCategory category)
	{
		g_PracticeTool.balanceCategory = static_cast<int>(category);
	}

	void CChampionTuner::Render(CScene_InGame* pScene)
	{
		if (!pScene)
			return;

		PracticeToolState& state = g_PracticeTool;
		if (!state.bLoadedOnce)
		{
			state.bLoadedOnce = true;
			LoadBalanceData(state);
		}

		const CSnapshotApplier* pSnapshot = pScene->GetSnapshotApplier();
		if (state.balanceData.pendingHotLoadSequence != 0u && pSnapshot &&
			pSnapshot->GetLastAckedCommandSequence() >=
				state.balanceData.pendingHotLoadSequence)
		{
			const u64_t toolRevision = pSnapshot->GetTimelineState().toolRevision;
			if (toolRevision >= state.balanceData.expectedToolRevision)
				state.status = "Hot load applied by the authoritative server.";
			else
			{
				state.status =
					"Hot load rejected by the server. It requires a Debug server "
					"and room host; check the server data error log.";
			}
			state.balanceData.pendingHotLoadSequence = 0u;
		}

		const HotLoadAvailability hotLoadAvailability =
			ResolveHotLoadAvailability(pScene);
		const bool_t bCanHotLoad = hotLoadAvailability.bAvailable;

		ImGui::SetNextWindowPos(ImVec2(500.f, 70.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(1180.f, 650.f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Balance"))
		{
			ImGui::End();
			return;
		}

		BalanceDataDraft& draft = state.balanceData;
		if (!draft.bLoaded)
		{
			ImGui::TextColored(
				ImVec4(1.f, 0.45f, 0.25f, 1.f), "%s", state.status.c_str());
			if (ImGui::Button("Reload JSON"))
				LoadBalanceData(state);
			ImGui::End();
			return;
		}

		ordered_json* pChampion = FindChampionEntry(draft);
		if (!pChampion)
		{
			ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Champion data is unavailable.");
			ImGui::End();
			return;
		}

		auto& champions = draft.champions["champions"];
		const auto DrawChampionSelector = [&]()
		{
			pChampion = FindChampionEntry(draft);
			const std::string selectedName =
				pChampion->value("champion", std::string("Unknown"));
			ImGui::SetNextItemWidth(260.f);
			if (ImGui::BeginCombo("Champion", selectedName.c_str()))
			{
				for (size_t index = 0u; index < champions.size(); ++index)
				{
					const std::string name =
						champions[index].value("champion", std::string("Unknown"));
					const bool_t bSelected =
						draft.championIndex == static_cast<int>(index);
					if (ImGui::Selectable(name.c_str(), bSelected))
						draft.championIndex = static_cast<int>(index);
					if (bSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			pChampion = FindChampionEntry(draft);
		};

		const auto EditFloat = [](
			ordered_json& owner,
			const char* pField,
			const char* pLabel,
			bool_t& bDirty,
			const char* pFormat = "%.3f")
		{
			f32_t value = owner.value(pField, 0.f);
			ImGui::PushID(pField);
			bool_t changed = false;
			if (pLabel && pLabel[0] == '#' && pLabel[1] == '#')
			{
				ImGui::SetNextItemWidth(-1.f);
				changed = ImGui::InputFloat(
					pLabel, &value, 0.f, 0.f, pFormat);
			}
			else if (ImGui::BeginTable(
				"##EditFloatRow", 2,
				ImGuiTableFlags_SizingStretchProp |
					ImGuiTableFlags_NoSavedSettings))
			{
				ImGui::TableSetupColumn(
					"Label", ImGuiTableColumnFlags_WidthFixed, 190.f);
				ImGui::TableSetupColumn(
					"Value", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(pLabel);
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-1.f);
				changed = ImGui::InputFloat(
					"##Value", &value, 0.f, 0.f, pFormat);
				ImGui::EndTable();
			}
			if (changed && std::isfinite(value))
			{
				owner[pField] = value;
				bDirty = true;
			}
			ImGui::PopID();
		};

		const auto EditDragFloat = [](
			ordered_json& owner,
			const char* pField,
			const char* pLabel,
			f32_t dragSpeed,
			f32_t minValue,
			f32_t maxValue,
			const char* pFormat,
			bool_t& bDirty)
		{
			f32_t value = owner.value(pField, 0.f);
			ImGui::PushID(pField);
			bool_t changed = false;
			if (pLabel && pLabel[0] == '#' && pLabel[1] == '#')
			{
				ImGui::SetNextItemWidth(-1.f);
				changed = ImGui::DragFloat(
					pLabel,
					&value,
					dragSpeed,
					minValue,
					maxValue,
					pFormat,
					ImGuiSliderFlags_AlwaysClamp);
			}
			else if (ImGui::BeginTable(
				"##EditDragFloatRow", 2,
				ImGuiTableFlags_SizingStretchProp |
					ImGuiTableFlags_NoSavedSettings))
			{
				ImGui::TableSetupColumn(
					"Label", ImGuiTableColumnFlags_WidthFixed, 190.f);
				ImGui::TableSetupColumn(
					"Value", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(pLabel);
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-1.f);
				changed = ImGui::DragFloat(
					"##Value",
					&value,
					dragSpeed,
					minValue,
					maxValue,
					pFormat,
					ImGuiSliderFlags_AlwaysClamp);
				ImGui::EndTable();
			}
			if (changed && std::isfinite(value))
			{
				owner[pField] = value;
				bDirty = true;
			}
			ImGui::PopID();
		};

		if (ImGui::BeginTabBar("BalanceDataTabs"))
		{
			if (ImGui::BeginTabItem("Champions"))
			{
				state.balanceCategory = static_cast<int>(eBalanceTunerCategory::Champions);
				DrawChampionSelector();
				ordered_json& stats = (*pChampion)["stats"];
				struct StatRow
				{
					const char* pLabel;
					const char* pBase;
					const char* pGrowth;
				};
				static const StatRow kRows[] = {
					{ "Health", "baseHp", "hpPerLevel" },
					{ "Mana / Energy", "baseMana", "manaPerLevel" },
					{ "Attack Damage", "baseAd", "adPerLevel" },
					{ "Ability Power", "baseAp", "apPerLevel" },
					{ "Armor", "baseArmor", "armorPerLevel" },
					{ "Magic Resist", "baseMr", "mrPerLevel" },
					{ "Attack Speed", "baseAttackSpeed", "attackSpeedPerLevel" },
				};
				if (ImGui::BeginTable(
					"ChampionStats", 3,
					ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("Stat", ImGuiTableColumnFlags_WidthFixed, 180.f);
					ImGui::TableSetupColumn("Base");
					ImGui::TableSetupColumn("Per Level");
					ImGui::TableHeadersRow();
					for (const StatRow& row : kRows)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(row.pLabel);
						ImGui::TableSetColumnIndex(1);
						EditFloat(stats, row.pBase, "##Base", draft.bChampionDirty);
						ImGui::TableSetColumnIndex(2);
						EditFloat(stats, row.pGrowth, "##Growth", draft.bChampionDirty);
					}
					ImGui::EndTable();
				}
				ImGui::SeparatorText("Attack / Resource");
				EditFloat(
					stats, "attackSpeedRatio", "Attack Speed Ratio",
					draft.bChampionDirty);
				EditFloat(
					stats, "resourceRegenPerSec", "Resource Regen / Sec",
					draft.bChampionDirty);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Skills"))
			{
				state.balanceCategory = static_cast<int>(eBalanceTunerCategory::Skills);
				DrawChampionSelector();
				static const char* const kSkillNames[5] = {
					"Passive / Basic Attack", "Q", "W", "E", "R"
				};
				int skillIndex = std::clamp(draft.skillSlot, 0, 4);
				ImGui::SetNextItemWidth(180.f);
				if (ImGui::Combo("Skill", &skillIndex, kSkillNames, 5))
					draft.skillSlot = skillIndex;
				ImGui::TextDisabled("Double-click to type an exact value.");
				const int rankCount = SkillRankCount(draft.skillSlot);
				ordered_json* pSkill = FindChampionSkill(*pChampion, draft.skillSlot);
				const std::string championName =
					pChampion->value("champion", std::string{});
				ordered_json* pEffect =
					FindSkillEffect(draft, championName, draft.skillSlot);
				if (!pSkill || !pEffect)
				{
					ImGui::TextColored(
						ImVec4(1.f, 0.3f, 0.3f, 1.f), "Skill JSON mapping is missing.");
				}
				else
				{
					std::string validationError;
					EnsureCooldownRanks(
						*pSkill, rankCount, validationError,
						championName + "." + SkillSlotToken(draft.skillSlot));
					ordered_json& damage = (*pEffect)["damage"];
					const std::string effectKey =
						pEffect->value("key", std::string{});
					if (!pEffect->contains("params") || !(*pEffect)["params"].is_object())
						(*pEffect)["params"] = ordered_json::object();
					ordered_json& params = (*pEffect)["params"];
					static const std::unordered_set<std::string> kDamageExecutionMissing = {
						"skill.riven.q", "skill.riven.w",
						"skill.masteryi.q", "skill.masteryi.e"
					};
					const bool_t bDamageExecutionMissing =
						kDamageExecutionMissing.contains(effectKey);
					const bool_t bNonDamagingSkill = effectKey == "skill.masteryi.w";
					const bool_t bDisableDamageRows =
						bDamageExecutionMissing || bNonDamagingSkill;
					if (bDamageExecutionMissing)
					{
						ImGui::TextColored(
							ImVec4(1.f, 0.45f, 0.2f, 1.f),
							"Server damage execution: NOT_IMPLEMENTED");
					}
					else if (bNonDamagingSkill)
					{
						ImGui::TextDisabled("Non-damaging skill: damage rows are read-only.");
					}
					const auto DrawRankRow = [&](const char* pLabel,
						ordered_json& owner, const char* pField, f32_t dragSpeed,
						f32_t minValue, f32_t maxValue, const char* pFormat, bool_t bDisabled,
						bool_t& bDirty)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(pLabel);
						for (int rank = 0; rank < 5; ++rank)
						{
							ImGui::TableSetColumnIndex(rank + 1);
							if (rank >= rankCount)
							{
								ImGui::TextDisabled("-");
								continue;
							}
							if (bDisabled)
							{
								ImGui::TextDisabled("Unavailable");
								continue;
							}
							f32_t value = owner[pField][rank].get<f32_t>();
							ImGui::PushID(pField);
							ImGui::PushID(rank);
							ImGui::SetNextItemWidth(-1.f);
							if (ImGui::DragFloat(
								"##Value",
								&value,
								dragSpeed,
								minValue,
								maxValue,
								pFormat,
								ImGuiSliderFlags_AlwaysClamp) && std::isfinite(value))
							{
								owner[pField][rank] = value;
								bDirty = true;
							}
							ImGui::PopID();
							ImGui::PopID();
						}
					};

					EditDragFloat(
						*pSkill, "rangeMax", "Skill Range (m)",
						0.1f, 0.f, 500.f, "%.1f m", draft.bChampionDirty);
					const char* pFlatDamageLabel = "Flat Damage";
					if (effectKey == "skill.yasuo.q")
						pFlatDamageLabel = "Q1/Q2 Flat Damage";
					else if (effectKey == "skill.leesin.q")
						pFlatDamageLabel = "Q1 Flat Damage";
					else if (effectKey == "skill.kalista.e")
						pFlatDamageLabel = "Rend Base Damage";
					else if (effectKey == "skill.ezreal.r")
						pFlatDamageLabel = "Champion / Epic Flat Damage";

					if (ImGui::BeginTable(
						"SkillBalance", 6,
						ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
							ImGuiTableFlags_ScrollX))
					{
						ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 190.f);
						for (int rank = 1; rank <= 5; ++rank)
						{
							const std::string label = "Rank " + std::to_string(rank);
							ImGui::TableSetupColumn(label.c_str(), ImGuiTableColumnFlags_WidthFixed, 155.f);
						}
						ImGui::TableHeadersRow();
						DrawRankRow(
							"Cooldown (sec)", *pSkill, "cooldownSecByRank",
							0.1f, 0.f, 300.f, "%.1f s", false, draft.bChampionDirty);
						DrawRankRow(
							pFlatDamageLabel, damage, "flatByRank",
							1.f, 0.f, 2000.f, "%.0f", bDisableDamageRows,
							draft.bSkillEffectDirty);
						static const char* const kRankedDamageParams[] = {
							"baseDamage", "damagePerSpear", "tornadoDamage",
							"dashAreaDamage", "nonEpicBaseDamage"
						};
						for (const char* pParam : kRankedDamageParams)
						{
							if (!IsRankedDamageParam(effectKey, pParam))
								continue;
							DrawRankRow(
								RankedDamageParamLabel(effectKey, pParam),
								params, pParam, 1.f, 0.f, 2000.f, "%.0f",
								bDisableDamageRows, draft.bSkillEffectDirty);
						}
						DrawRankRow(
							"Total AD Ratio", damage, "totalAdRatioByRank",
							0.01f, 0.f, kAdRatioAuthoringMax, "%.2f", bDisableDamageRows,
							draft.bSkillEffectDirty);
						DrawRankRow(
							"Bonus AD Ratio", damage, "bonusAdRatioByRank",
							0.01f, 0.f, kAdRatioAuthoringMax, "%.2f", bDisableDamageRows,
							draft.bSkillEffectDirty);
						DrawRankRow(
							"AP Ratio", damage, "apRatioByRank",
							0.01f, 0.f, 5.f, "%.2f", bDisableDamageRows,
							draft.bSkillEffectDirty);
						DrawRankRow(
							"Target Max HP Ratio", damage, "targetMaxHpRatioByRank",
							0.01f, 0.f, 5.f, "%.2f", bDisableDamageRows,
							draft.bSkillEffectDirty);
						DrawRankRow(
							"Missing HP Ratio", damage, "targetMissingHpRatioByRank",
							0.01f, 0.f, 5.f, "%.2f", bDisableDamageRows,
							draft.bSkillEffectDirty);
						ImGui::EndTable();
					}
					ImGui::TextDisabled(
						"Raw = Flat + Total AD Ratio x final Total AD + Bonus AD Ratio x Bonus AD.");
					ImGui::TextDisabled(
						"Armor / Magic Resist is applied after raw damage. Ratio 1.0 = 100%%.");

					struct ConditionalDamageEditor
					{
						const char* pField;
						const char* pLabel;
						f32_t maxValue;
					};
					std::vector<ConditionalDamageEditor> conditionalEditors;
					if (effectKey == "skill.fiora.basic_attack")
					{
						conditionalEditors.push_back({
							"targetMaxHpRatio", "Vital Target Max HP Ratio", 5.f });
					}
					else if (effectKey == "skill.sylas.basic_attack")
					{
						conditionalEditors.push_back({
							"baseDamage", "Petricite Burst Flat Damage", 2000.f });
						conditionalEditors.push_back({
							"apRatio", "Petricite Burst AP Ratio", 5.f });
					}
					else if (effectKey == "skill.zed.basic_attack")
					{
						conditionalEditors.push_back({
							"missingHealthDamageRatio",
							"Contempt Missing HP Ratio", 5.f });
						conditionalEditors.push_back({
							"targetHealthThresholdRatio",
							"Target Health Threshold Ratio", 1.f });
					}
					if (!conditionalEditors.empty())
					{
						ImGui::SeparatorText("Conditional Damage Mechanics");
						for (const ConditionalDamageEditor& editor : conditionalEditors)
						{
							if (!params.contains(editor.pField) ||
								!params[editor.pField].is_number())
							{
								continue;
							}
							const bool_t bFlat = std::strcmp(editor.pField, "baseDamage") == 0;
							EditDragFloat(
								params, editor.pField, editor.pLabel,
								bFlat ? 1.f : 0.01f, 0.f, editor.maxValue,
								bFlat ? "%.1f" : "%.2f", draft.bSkillEffectDirty);
						}
					}

					struct MechanicParamEditor
					{
						const char* pField;
						const char* pLabel;
						f32_t dragSpeed;
						f32_t maxValue;
						const char* pFormat;
					};
					static const MechanicParamEditor kMechanicEditors[] = {
						{ "radius", "Effect Radius / Half Width (m)", 0.05f, 100.f, "%.2f m" },
						{ "formationDelaySec", "Delay (sec)", 0.05f, 10.f, "%.2f s" },
						{ "healDamageRatio", "Heal / Damage Ratio", 0.01f, 5.f, "%.2f" },
					};
					bool_t bHasMechanicParam = false;
					for (const MechanicParamEditor& editor : kMechanicEditors)
						bHasMechanicParam = bHasMechanicParam || params.contains(editor.pField);
					if (bHasMechanicParam)
					{
						ImGui::SeparatorText("Effect Mechanics");
						for (const MechanicParamEditor& editor : kMechanicEditors)
						{
							if (!params.contains(editor.pField) ||
								!params[editor.pField].is_number())
							{
								continue;
							}
							EditDragFloat(
								params, editor.pField, editor.pLabel,
								editor.dragSpeed, 0.f, editor.maxValue,
								editor.pFormat, draft.bSkillEffectDirty);
						}
					}
				}
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Minions"))
			{
				state.balanceCategory = static_cast<int>(eBalanceTunerCategory::Minions);
				static const char* const kRoles[4] = { "Melee", "Ranged", "Siege", "Super" };
				ImGui::SetNextItemWidth(220.f);
				ImGui::Combo("Role", &draft.minionRole, kRoles, 4);
				if (ordered_json* pMinion = FindMinionEntry(draft, draft.minionRole))
				{
					EditFloat(
						*pMinion, "maxHp", "Max Health",
						draft.bSpawnObjectDirty, "%.2f");
					EditFloat(
						*pMinion, "attackDamage", "Attack Damage",
						draft.bSpawnObjectDirty, "%.2f");
					EditDragFloat(
						*pMinion, "attackRange", "Attack Range",
						0.1f, kMinionAttackRangeMin, kMinionAttackRangeMax,
						"%.1f", draft.bSpawnObjectDirty);
				}
				ImGui::TextDisabled("Lane roles 0..3 only; summon role 4 is preserved.");
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Towers"))
			{
				state.balanceCategory = static_cast<int>(eBalanceTunerCategory::Towers);
				ordered_json& structure = draft.spawnObjects["structure"];
				ordered_json& turretAI = structure["turretAI"];
				EditFloat(
					structure, "turretMaxHp", "Turret Max Health",
					draft.bSpawnObjectDirty, "%.2f");
				EditFloat(
					turretAI, "attackDamage", "Turret Attack Damage",
					draft.bSpawnObjectDirty, "%.2f");
				EditFloat(
					turretAI, "nexusAttackDamage", "Nexus Turret Attack Damage",
					draft.bSpawnObjectDirty, "%.2f");
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Objectives"))
			{
				state.balanceCategory = static_cast<int>(eBalanceTunerCategory::Objectives);
				static const char* const kCampNames[] = {
					"Baron", "Elder Dragon", "Blue Buff", "Red Buff", "Krug",
					"Gromp", "Wolf", "Razorbeak", "Razorbeak Mini", "Wolf Mini", "Krug Mini"
				};
				ImGui::SetNextItemWidth(240.f);
				ImGui::Combo("Jungle Monster", &draft.jungleSubKind, kCampNames, 11);
				ordered_json* pCamp = nullptr;
				for (ordered_json& camp : draft.spawnObjects["jungleCamps"])
				{
					if (camp.value("subKind", -1) == draft.jungleSubKind)
					{
						pCamp = &camp;
						break;
					}
				}
				if (pCamp)
				{
					ImGui::SeparatorText("Monster Combat");
					EditFloat(*pCamp, "maxHp", "Max Health", draft.bSpawnObjectDirty, "%.0f");
					EditFloat(*pCamp, "attackDamage", "Attack Damage", draft.bSpawnObjectDirty, "%.1f");
					EditFloat(*pCamp, "baseArmor", "Armor", draft.bSpawnObjectDirty, "%.1f");
					EditFloat(*pCamp, "baseMr", "Magic Resist", draft.bSpawnObjectDirty, "%.1f");
					EditDragFloat(
						*pCamp, "respawnDelaySec", "Respawn Time (sec)",
						0.5f, 1.f, 600.f, "%.1f s", draft.bSpawnObjectDirty);
				}

				ImGui::BeginDisabled(!bCanHotLoad);
				if (ImGui::Button("Refill Health"))
				{
					SendPracticeCommand(pScene, state,
						ePracticeOperation::RefillJungleHealth, 0.f,
						static_cast<u32_t>(draft.jungleSubKind + 1));
				}
				ImGui::SameLine();
				if (ImGui::Button("Reset Monster"))
				{
					SendPracticeCommand(pScene, state,
						ePracticeOperation::ResetJungleMonster, 0.f,
						static_cast<u32_t>(draft.jungleSubKind + 1));
				}
				ImGui::SameLine();
				if (ImGui::Button("Clear All Objective Buffs"))
				{
					SendPracticeCommand(pScene, state,
						ePracticeOperation::ClearObjectiveBuffs);
				}
				ImGui::EndDisabled();
				ImGui::TextDisabled(
					"Refill preserves combat state. Reset returns the selected camp to its anchor."
				);

				ordered_json& rewards = draft.economy["jungle"];
				ImGui::SeparatorText("Regular Jungle Rewards");
				EditFloat(rewards, "smallCampGold", "Gold / Kill", draft.bEconomyDirty, "%.0f");
				EditFloat(rewards, "smallCampXP", "XP / Kill", draft.bEconomyDirty, "%.0f");

				ordered_json& objectives = draft.economy["objectives"];
				ImGui::SeparatorText("Shared Objective Reward / Duration");
				EditFloat(objectives, "teamGoldPerChampion", "Gold / Team Champion", draft.bEconomyDirty, "%.0f");
				int levelGrant = objectives.value("teamLevelGrant", 3);
				ImGui::SetNextItemWidth(-1.f);
				if (ImGui::InputInt("Levels / Team Champion", &levelGrant))
				{
					objectives["teamLevelGrant"] = std::clamp(levelGrant, 0, 18);
					draft.bEconomyDirty = true;
				}
				EditFloat(objectives, "buffDurationSec", "Buff Duration (sec)", draft.bEconomyDirty, "%.1f");

				ImGui::SeparatorText("Baron");
				EditFloat(objectives, "baronRecallDurationMultiplier", "Recall Duration Multiplier", draft.bEconomyDirty, "%.2f");
				EditFloat(objectives, "baronAuraRadius", "Minion Aura Radius", draft.bEconomyDirty, "%.1f");
				EditFloat(objectives, "baronMinionHpMultiplier", "Minion Health Multiplier", draft.bEconomyDirty, "%.2f");
				EditFloat(objectives, "baronMinionAttackDamageMultiplier", "Minion AD Multiplier", draft.bEconomyDirty, "%.2f");
				EditFloat(objectives, "baronMinionScaleMultiplier", "Minion Visual Scale", draft.bEconomyDirty, "%.2f");

				ImGui::SeparatorText("Elder Dragon");
				EditFloat(objectives, "elderAttackDamageMultiplier", "Total AD Multiplier", draft.bEconomyDirty, "%.2f");
				EditFloat(objectives, "elderBurnDurationSec", "Burn Duration (sec)", draft.bEconomyDirty, "%.2f");
				EditFloat(objectives, "elderBurnTickIntervalSec", "Burn Tick Interval (sec)", draft.bEconomyDirty, "%.2f");
				EditFloat(objectives, "elderBurnTargetMaxHpRatioPerTick", "Burn Target Max HP / Tick", draft.bEconomyDirty, "%.3f");
				EditFloat(objectives, "elderExecuteThresholdRatio", "Execute Health Ratio", draft.bEconomyDirty, "%.3f");

				ImGui::SeparatorText("Blue / Red");
				EditFloat(objectives, "blueManaRegenPerSec", "Blue Mana / Sec", draft.bEconomyDirty, "%.1f");
				EditFloat(objectives, "redHealthRegenPerSec", "Red Health / Sec", draft.bEconomyDirty, "%.1f");
				EditFloat(objectives, "redBurnDurationSec", "Red Burn Duration (sec)", draft.bEconomyDirty, "%.2f");
				EditFloat(objectives, "redBurnTickIntervalSec", "Red Burn Tick Interval (sec)", draft.bEconomyDirty, "%.2f");
				EditFloat(objectives, "redBurnDamagePerTick", "Red Burn Damage / Tick", draft.bEconomyDirty, "%.1f");
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::Separator();
		const bool_t bHotLoadPending = draft.pendingHotLoadSequence != 0u;
		ImGui::BeginDisabled(!bCanHotLoad || !draft.bLoaded || bHotLoadPending);
		if (ImGui::Button("Save & Hot Load", ImVec2(160.f, 0.f)) &&
			SaveBalanceData(state))
		{
			const u64_t beforeRevision = pSnapshot
				? pSnapshot->GetTimelineState().toolRevision
				: 0u;
			const u32_t enableSequence = SendPracticeCommand(
				pScene, state, ePracticeOperation::SetEnabled, 1.f);
			const u32_t reloadSequence = SendPracticeCommand(
				pScene, state, ePracticeOperation::ReloadGameplayDefinitions);
			if (enableSequence != 0u && reloadSequence != 0u)
			{
				draft.pendingHotLoadSequence = reloadSequence;
				draft.expectedToolRevision = beforeRevision + 2u;
				state.status = "Saved. Waiting for authoritative hot-load confirmation.";
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip(
				"Validate and save the four JSON files, then ask the authoritative "
				"Debug server to reload them. The room host is required.");
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(bHotLoadPending);
		if (ImGui::Button("Reload JSON", ImVec2(130.f, 0.f)))
		{
			if (draft.bChampionDirty || draft.bSkillEffectDirty ||
				draft.bSpawnObjectDirty || draft.bEconomyDirty)
			{
				ImGui::OpenPopup("Discard unsaved F4 edits?");
			}
			else
			{
				LoadBalanceData(state);
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip(
				"Reread the four JSON files. This discards unsaved F4 edits and "
				"does not change server values by itself.");
		}
		ImGui::EndDisabled();

		if (ImGui::BeginPopupModal(
			"Discard unsaved F4 edits?",
			nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted(
				"Reload JSON discards unsaved F4 values and rereads the four data files.");
			if (ImGui::Button("Discard & Reload"))
			{
				LoadBalanceData(state);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		if (bCanHotLoad)
			ImGui::TextDisabled("%s", hotLoadAvailability.pMessage);
		else
			ImGui::TextColored(
				ImVec4(1.f, 0.6f, 0.2f, 1.f),
				"%s",
				hotLoadAvailability.pMessage);
		ImGui::TextDisabled("Release persistence requires data cook + build.");
		ImGui::TextWrapped("%s", state.status.c_str());
		ImGui::End();
	}

#if 0 // Superseded current-champion override surface.
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
		const bool_t bCanSend = CanSendPracticeCommand(pScene);
		const ChampionStatsDef baselineDef =
			BuildDefaultChampionStatsDef(observed.championId);

		const auto StatValue = [&](int statIndex)
		{
			const StatOverrideRow* pRow = FindStatOverrideRow(state, statIndex);
			return pRow ? pRow->value : ResolveStatBaseline(baselineDef, statIndex);
		};
		const auto ApplyEssentialStats = [&]()
		{
			SendPracticeCommand(
				pScene, state, ePracticeOperation::ClearChampionStatOverrides);
			for (const StatOverrideRow& row : state.statOverrides)
			{
				if (row.statIndex < 0 || row.statIndex >= 14)
					continue;
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::ApplyChampionStatOverride,
					row.value,
					0u,
					static_cast<u8_t>(kStatOptions[row.statIndex].id));
			}
		};
		const auto ApplyEssentialSkills = [&]()
		{
			SendPracticeCommand(
				pScene, state, ePracticeOperation::ClearSkillEffectOverrides);
			for (int slotIndex = 0; slotIndex < 4; ++slotIndex)
			{
				if (state.skillDamageDraft[slotIndex] < 0.f)
					continue;
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::ApplySkillEffectOverride,
					state.skillDamageDraft[slotIndex],
					static_cast<u32_t>(eSkillEffectParamId::DamageFlatOverride),
					static_cast<u8_t>(slotIndex + 1));
			}
		};
		const auto ApplyEssentialMinions = [&]()
		{
			SendPracticeCommand(
				pScene, state, ePracticeOperation::ClearMinionStatOverrides);
			for (int role = 0; role < 4; ++role)
			{
				if (!state.minionAttackDamageEnabled[role])
					continue;
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::ApplyMinionStatOverride,
					state.minionAttackDamageDraft[role],
					static_cast<u32_t>(role),
					static_cast<u8_t>(eMinionStatOverrideId::AttackDamage));
			}
		};

		ImGui::SetNextWindowPos(ImVec2(560.f, 100.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(640.f, 520.f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Balance"))
		{
			ImGui::End();
			return;
		}

		if (!bCanSend)
		{
			ImGui::TextColored(
				ImVec4(1.f, 0.55f, 0.25f, 1.f),
				"Connect to a server-authoritative Debug match to apply values.");
		}
		else if (observed.bValid)
		{
			ImGui::TextDisabled(
				"Current champion | Level %u",
				static_cast<u32_t>(observed.level));
		}

		if (ImGui::BeginTabBar("BalanceEssentials"))
		{
			if (ImGui::BeginTabItem("Champion Damage"))
			{
				state.balanceCategory = static_cast<int>(
					eBalanceTunerCategory::ChampionDamage);
				ImGui::TextDisabled(
					"Base AD changes basic attacks. Skill values replace only the flat part.");
				if (ImGui::BeginTable(
					"ChampionDamageTable",
					3,
					ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("Damage", ImGuiTableColumnFlags_WidthFixed, 150.f);
					ImGui::TableSetupColumn("Custom", ImGuiTableColumnFlags_WidthFixed, 80.f);
					ImGui::TableSetupColumn("Value");
					ImGui::TableHeadersRow();

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted("Base AD");
					ImGui::TableSetColumnIndex(1);
					ImGui::TextDisabled("auto");
					ImGui::TableSetColumnIndex(2);
					f32_t baseAd = StatValue(4);
					ImGui::SetNextItemWidth(-1.f);
					if (ImGui::InputFloat("##BaseAD", &baseAd, 0.f, 0.f, "%.2f") &&
						std::isfinite(baseAd) && baseAd >= 0.f)
					{
						UpsertStatOverrideRow(
							state, 4, baseAd, ResolveStatBaseline(baselineDef, 4));
					}

					static const char* const kSkillLabels[4] = {
						"Q Flat Damage", "W Flat Damage", "E Flat Damage", "R Flat Damage"
					};
					for (int slotIndex = 0; slotIndex < 4; ++slotIndex)
					{
						ImGui::PushID(slotIndex + 21000);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(kSkillLabels[slotIndex]);
						ImGui::TableSetColumnIndex(1);
						bool enabled = state.skillDamageDraft[slotIndex] >= 0.f;
						if (ImGui::Checkbox("##Custom", &enabled))
							state.skillDamageDraft[slotIndex] = enabled ? 0.f : -1.f;
						ImGui::TableSetColumnIndex(2);
						if (enabled)
						{
							ImGui::SetNextItemWidth(-1.f);
							ImGui::InputFloat(
								"##Value",
								&state.skillDamageDraft[slotIndex],
								0.f,
								0.f,
								"%.2f");
						}
						else
						{
							ImGui::TextDisabled("Pack value");
						}
						ImGui::PopID();
					}
					ImGui::EndTable();
				}
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Growth"))
			{
				state.balanceCategory = static_cast<int>(eBalanceTunerCategory::Growth);
				struct GrowthRow
				{
					const char* pLabel;
					int baseIndex;
					int growthIndex;
				};
				static const GrowthRow kGrowthRows[7] = {
					{ "Health", 0, 1 },
					{ "Mana", 2, 3 },
					{ "Attack Damage", 4, 5 },
					{ "Ability Power", 6, 7 },
					{ "Armor", 8, 9 },
					{ "Magic Resist", 10, 11 },
					{ "Attack Speed", 12, 13 },
				};

				if (ImGui::BeginTable(
					"GrowthTable",
					3,
					ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("Stat", ImGuiTableColumnFlags_WidthFixed, 150.f);
					ImGui::TableSetupColumn("Base");
					ImGui::TableSetupColumn("Per Level");
					ImGui::TableHeadersRow();
					for (int rowIndex = 0; rowIndex < 7; ++rowIndex)
					{
						const GrowthRow& row = kGrowthRows[rowIndex];
						ImGui::PushID(rowIndex + 22000);
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(row.pLabel);
						const int indices[2] = { row.baseIndex, row.growthIndex };
						for (int column = 0; column < 2; ++column)
						{
							const int statIndex = indices[column];
							f32_t value = StatValue(statIndex);
							ImGui::TableSetColumnIndex(column + 1);
							ImGui::SetNextItemWidth(-1.f);
							ImGui::PushID(column);
							if (ImGui::InputFloat("##Value", &value, 0.f, 0.f, "%.3f") &&
								std::isfinite(value) && value >= 0.f)
							{
								UpsertStatOverrideRow(
									state,
									statIndex,
									value,
									ResolveStatBaseline(baselineDef, statIndex));
							}
							ImGui::PopID();
						}
						ImGui::PopID();
					}
					ImGui::EndTable();
				}
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Minions"))
			{
				state.balanceCategory = static_cast<int>(eBalanceTunerCategory::Minions);
				static const char* const kRoleNames[4] = {
					"Melee", "Ranged", "Siege", "Super"
				};
				ImGui::SetNextItemWidth(220.f);
				ImGui::Combo("Role", &state.minionRole, kRoleNames, 4);
				bool enabled = state.minionAttackDamageEnabled[state.minionRole];
				if (ImGui::Checkbox("Use custom attack damage", &enabled))
					state.minionAttackDamageEnabled[state.minionRole] = enabled;
				if (enabled)
				{
					ImGui::SetNextItemWidth(220.f);
					if (ImGui::InputFloat(
						"Attack Damage",
						&state.minionAttackDamageDraft[state.minionRole],
						0.f,
						0.f,
						"%.2f"))
					{
						state.minionAttackDamageEnabled[state.minionRole] = true;
					}
				}
				else
				{
					ImGui::TextDisabled("Active pack value + normal time growth");
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::Separator();
		ImGui::BeginDisabled(!bCanSend || !observed.bValid);
		if (ImGui::Button("Save & Apply", ImVec2(150.f, 0.f)))
		{
			SyncEssentialSkillRows(state);
			if (SaveOverrides(state))
			{
				SendPracticeCommand(pScene, state, ePracticeOperation::SetEnabled, 1.f);
				ApplyEssentialSkills();
				ApplyEssentialStats();
				ApplyEssentialMinions();
				state.status = "Saved and applied to the authoritative server.";
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Restore This Category", ImVec2(190.f, 0.f)))
		{
			const eBalanceTunerCategory category =
				static_cast<eBalanceTunerCategory>(state.balanceCategory);
			if (category == eBalanceTunerCategory::ChampionDamage)
			{
				for (f32_t& draft : state.skillDamageDraft)
					draft = -1.f;
				if (StatOverrideRow* pBaseAd = FindStatOverrideRow(state, 4))
				{
					state.statOverrides.erase(
						state.statOverrides.begin() +
						(pBaseAd - state.statOverrides.data()));
				}
				ApplyEssentialSkills();
				ApplyEssentialStats();
			}
			else if (category == eBalanceTunerCategory::Growth)
			{
				state.statOverrides.erase(
					std::remove_if(
						state.statOverrides.begin(),
						state.statOverrides.end(),
						[](const StatOverrideRow& row)
						{
							return row.statIndex >= 0 && row.statIndex < 14;
						}),
					state.statOverrides.end());
				ApplyEssentialStats();
			}
			else
			{
				for (bool_t& roleEnabled : state.minionAttackDamageEnabled)
					roleEnabled = false;
				ApplyEssentialMinions();
			}
			SyncEssentialSkillRows(state);
			SaveOverrides(state);
			state.status = "Category restored to active pack values.";
		}
		ImGui::EndDisabled();
		ImGui::TextDisabled("%s", state.status.c_str());
		ImGui::End();
	}
#endif

#if 0 // Legacy all-in-one practice surface retained as backend reference only.

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
		ImGui::TextDisabled(
			"F4 categories cover skills, champion stats, items, units/structures, respawn, and runtime tools. "
			"F5('8') remains the focused Attack Speed Lab; F9 is AI Debug.");
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

		static const char* const kCategoryLabels[] = {
			"Skills", "Champion", "Items", "Units & Structures",
			"Respawn", "Runtime", "All"
		};
		ImGui::SetNextItemWidth(220.f);
		ImGui::Combo(
			"Category",
			&state.balanceCategory,
			kCategoryLabels,
			IM_ARRAYSIZE(kCategoryLabels));
		const eBalanceTunerCategory category =
			static_cast<eBalanceTunerCategory>(state.balanceCategory);
		const bool_t bShowAll = category == eBalanceTunerCategory::All;
		const bool_t bShowSkills = bShowAll || category == eBalanceTunerCategory::Skills;
		const bool_t bShowChampion = bShowAll || category == eBalanceTunerCategory::Champion;
		const bool_t bShowItems = bShowAll || category == eBalanceTunerCategory::Items;
		const bool_t bShowUnits =
			bShowAll || category == eBalanceTunerCategory::UnitsAndStructures;
		const bool_t bShowRespawn = bShowAll || category == eBalanceTunerCategory::Respawn;
		const bool_t bShowRuntime = bShowAll || category == eBalanceTunerCategory::Runtime;

		if (bShowRuntime && ImGui::CollapsingHeader(
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

		if (bShowRuntime && ImGui::CollapsingHeader("Spawn Champion (Practice)"))
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

		if (bShowRuntime && ImGui::CollapsingHeader(
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

		if (bShowRuntime && ImGui::CollapsingHeader(
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

		if (bShowChampion && ImGui::CollapsingHeader("Champion Stats (Live)"))
		{
			ImGui::TextDisabled(
				"Full stat sheet. Baseline = champions.json table; edited fields become server overrides.");
			ImGui::TextDisabled(
				"Target = 'Target NetId' above (0 = self). Sheet baseline shows your champion's values.");

			const ChampionStatsDef baselineDef =
				BuildDefaultChampionStatsDef(observed.championId);
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

		if (bShowItems && ImGui::CollapsingHeader("Item Balance (Live)"))
		{
			ImGui::TextDisabled(
				"Full field sheet per item: every stat listed, +0 fields included. Price gates BuyItem.");

			const ClientData::ShopItemPresentationDefinition* pSheetItem =
				ClientData::FindShopItemPresentationDefinition(
					static_cast<u16_t>(state.sheetItemId));
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
					if (!view.bRegistered)
						continue;
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

		if (bShowItems && ImGui::CollapsingHeader("Shop Layout"))
		{
			ImGui::TextDisabled(
				"Every PNG in Resource/Texture/UI/Items is listed. Purchases stay server-validated by ItemDef.");
			ImGui::TextDisabled(
				"Edit Resource/UI/Lua/itemshop_catalog.lua for exact x/y/order/section placement.");

			static char shopFilter[96]{};
			ImGui::SetNextItemWidth(260.f);
			ImGui::InputText("Filter##ShopCatalog", shopFilter, sizeof(shopFilter));
			if (ImGui::Button("Rescan Item PNG Folder"))
			{
				Client::ReloadLoLShopEditorCatalog();
				Client::ReapplyLoLShopItems(*CGameInstance::Get());
				state.status = "Item PNG folder rescanned and Lua shop reloaded.";
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload Lua Layout"))
			{
				CGameInstance::Get()->UI_Reload_Lua();
				state.status = "itemshop_catalog.lua reloaded.";
			}

			const u32_t entryCount = Client::GetLoLShopEditorEntryCount();
			ImGui::Text("Catalog: %u PNG assets", entryCount);
			ImGui::BeginChild("ShopCatalogEntries", ImVec2(0.f, 360.f), true);
			for (u32_t index = 0u; index < entryCount; ++index)
			{
				const Client::LoLShopEditorEntryView view = Client::GetLoLShopEditorEntry(index);
				const char* assetKey = view.pAssetKey ? view.pAssetKey : "";
				const char* displayName = view.pDisplayName ? view.pDisplayName : assetKey;
				if (shopFilter[0] != '\0' &&
					std::strstr(assetKey, shopFilter) == nullptr &&
					std::strstr(displayName, shopFilter) == nullptr)
				{
					continue;
				}
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
				ImGui::Text("%u  [%s]  %s  (%s, %u gold)",
					static_cast<u32_t>(view.iItemId),
					view.bRegistered ? "server" : "resource",
					displayName,
					view.pSection ? view.pSection : "resource",
					static_cast<u32_t>(view.iPrice));
				if (view.bRegistered)
				{
					ImGui::SameLine();
					if (ImGui::SmallButton("Balance"))
						state.sheetItemId = view.iItemId;
				}
				ImGui::PopID();
			}
			ImGui::EndChild();

			if (ImGui::Button("Apply Shop Layout"))
			{
				Client::ReapplyLoLShopItems(*CGameInstance::Get());
				state.status = "Shop layout re-registered to the in-game shop UI.";
			}
		}

		if (bShowUnits && ImGui::CollapsingHeader(
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

		if (bShowSkills && ImGui::CollapsingHeader(
			"Skill Effect Overrides",
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			RenderOverrideTable(pScene, state);
		}

		if (bShowSkills && ImGui::CollapsingHeader("Skill Balance (Live)"))
		{
			ImGui::TextDisabled(
				"Per-slot cooldown / flat-damage override. -1 = keep pack value. "
				"Single value applies to every rank.");
			ImGui::TextDisabled(
				"Target = 'Target NetId' above (0 = self). Rank tables stay in JSON + Reload Definitions.");

			static const char* const kCooldownLabels[4] =
			{ "Q CooldownSec", "W CooldownSec", "E CooldownSec", "R CooldownSec" };
			static const char* const kDamageLabels[4] =
			{ "Q FlatDamage", "W FlatDamage", "E FlatDamage", "R FlatDamage" };
			for (int slotIndex = 0; slotIndex < 4; ++slotIndex)
			{
				ImGui::PushID(slotIndex + 9500);
				ImGui::SetNextItemWidth(120.f);
				ImGui::InputFloat(
					kCooldownLabels[slotIndex],
					&state.skillCooldownDraft[slotIndex], 0.f, 0.f, "%.2f");
				ImGui::SameLine(0.f, 24.f);
				ImGui::SetNextItemWidth(120.f);
				ImGui::InputFloat(
					kDamageLabels[slotIndex],
					&state.skillDamageDraft[slotIndex], 0.f, 0.f, "%.1f");
				ImGui::PopID();
			}

			const NetEntityId skillTarget =
				state.overrideTargetNetId > 0
				? static_cast<NetEntityId>(state.overrideTargetNetId)
				: NULL_NET_ENTITY;
			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Apply Skill Balance"))
			{
				for (int slotIndex = 0; slotIndex < 4; ++slotIndex)
				{
					const u8_t skillSlot = static_cast<u8_t>(slotIndex + 1);
					if (state.skillCooldownDraft[slotIndex] >= 0.f)
					{
						SendPracticeCommand(
							pScene, state, ePracticeOperation::ApplySkillEffectOverride,
							state.skillCooldownDraft[slotIndex],
							static_cast<u32_t>(eSkillEffectParamId::CooldownSecOverride),
							skillSlot, {}, skillTarget);
					}
					if (state.skillDamageDraft[slotIndex] >= 0.f)
					{
						SendPracticeCommand(
							pScene, state, ePracticeOperation::ApplySkillEffectOverride,
							state.skillDamageDraft[slotIndex],
							static_cast<u32_t>(eSkillEffectParamId::DamageFlatOverride),
							skillSlot, {}, skillTarget);
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear ALL Skill Overrides"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ClearSkillEffectOverrides,
					0.f, 0u, 0u, {}, skillTarget);
				for (int slotIndex = 0; slotIndex < 4; ++slotIndex)
				{
					state.skillCooldownDraft[slotIndex] = -1.f;
					state.skillDamageDraft[slotIndex] = -1.f;
				}
			}
			ImGui::EndDisabled();
			ImGui::TextDisabled(
				"Clear removes every skill-effect override on the target, "
				"including rows from the table above.");
		}

		if (bShowRespawn && ImGui::CollapsingHeader(
			"Respawn By Level",
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextDisabled(
				"Base seconds by champion level. Standard game-time scaling is applied on death.");
			for (int levelIndex = 0; levelIndex < 18; ++levelIndex)
			{
				ImGui::PushID(levelIndex + 9700);
				char label[32]{};
				sprintf_s(label, "Level %d", levelIndex + 1);
				ImGui::SetNextItemWidth(120.f);
				ImGui::InputFloat(
					label,
					&state.respawnSecondsByLevel[levelIndex],
					0.f,
					0.f,
					"%.2f sec");
				ImGui::PopID();
			}

			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Apply All Respawn Levels"))
			{
				for (int levelIndex = 0; levelIndex < 18; ++levelIndex)
				{
					SendPracticeCommand(
						pScene,
						state,
						ePracticeOperation::ApplyRespawnTimeOverride,
						state.respawnSecondsByLevel[levelIndex],
						0u,
						static_cast<u8_t>(levelIndex + 1));
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Restore Respawn Pack Values"))
			{
				constexpr f32_t kPackRespawnSeconds[18] = {
					10.f, 10.f, 12.f, 12.f, 14.f, 16.f, 20.f, 25.f, 28.f,
					32.5f, 35.f, 37.5f, 40.f, 42.5f, 45.f, 47.5f, 50.f, 52.5f
				};
				for (int levelIndex = 0; levelIndex < 18; ++levelIndex)
					state.respawnSecondsByLevel[levelIndex] = kPackRespawnSeconds[levelIndex];
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ClearRespawnTimeOverrides);
			}
			ImGui::EndDisabled();
		}

		if (bShowUnits && ImGui::CollapsingHeader("Jungle Balance (Live)"))
		{
			ImGui::TextDisabled(
				"All jungle monsters at once. MaxHp also updates dead camps' cached max.");
			ImGui::SetNextItemWidth(140.f);
			ImGui::InputFloat("Jungle MaxHp", &state.jungleMaxHpDraft, 0.f, 0.f, "%.0f");
			ImGui::SameLine();
			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Apply##JungleHp"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ApplyJungleStatOverride,
					state.jungleMaxHpDraft, 0u,
					static_cast<u8_t>(eJungleStatOverrideId::MaxHp));
			}
			ImGui::EndDisabled();

			ImGui::SetNextItemWidth(140.f);
			ImGui::InputFloat(
				"Jungle AttackDamage", &state.jungleAttackDamageDraft, 0.f, 0.f, "%.1f");
			ImGui::SameLine();
			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Apply##JungleAd"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ApplyJungleStatOverride,
					state.jungleAttackDamageDraft, 0u,
					static_cast<u8_t>(eJungleStatOverrideId::AttackDamage));
			}
			ImGui::EndDisabled();

			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Restore Jungle Pack Values"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ClearJungleStatOverrides);
			}
			ImGui::EndDisabled();
		}

		if (bShowUnits && ImGui::CollapsingHeader("Structure Balance (Live)"))
		{
			ImGui::TextDisabled(
				"Server-authoritative structure HP and turret damage overrides.");

			struct StructureFieldRow
			{
				const char* pLabel;
				f32_t* pDraft;
				eStructureStatOverrideId id;
			};
			const StructureFieldRow structureRows[4] =
			{
				{ "Turret MaxHp", &state.structureTurretHpDraft,
					eStructureStatOverrideId::TurretMaxHp },
				{ "Inhibitor MaxHp", &state.structureInhibitorHpDraft,
					eStructureStatOverrideId::InhibitorMaxHp },
				{ "Nexus MaxHp", &state.structureNexusHpDraft,
					eStructureStatOverrideId::NexusMaxHp },
				{ "Turret AttackDamage", &state.structureTurretDamageDraft,
					eStructureStatOverrideId::TurretAttackDamage },
			};
			for (int rowIndex = 0; rowIndex < 4; ++rowIndex)
			{
				const StructureFieldRow& row = structureRows[rowIndex];
				ImGui::PushID(rowIndex + 9600);
				ImGui::SetNextItemWidth(140.f);
				ImGui::InputFloat(row.pLabel, row.pDraft, 0.f, 0.f, "%.0f");
				ImGui::SameLine();
				ImGui::BeginDisabled(!bCanSend);
				if (ImGui::Button("Apply"))
				{
					SendPracticeCommand(
						pScene, state, ePracticeOperation::ApplyStructureStatOverride,
						*row.pDraft, 0u, static_cast<u8_t>(row.id));
				}
				ImGui::EndDisabled();
				ImGui::PopID();
			}

			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Restore Structure Pack Values"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::ClearStructureStatOverrides);
			}
			ImGui::EndDisabled();
		}

		ImGui::Separator();
		ImGui::TextWrapped("Status: %s", state.status.c_str());
		ImGui::End();
	}
#endif
}
