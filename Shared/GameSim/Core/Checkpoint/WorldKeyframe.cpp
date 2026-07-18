#include "Shared/GameSim/Core/Checkpoint/WorldKeyframe.h"
#include "Shared/GameSim/Core/Checkpoint/KeyframeComponentRegistry.h"

// Engine 컴포넌트는 Phase 7F 어댑터 경유 (Check-SharedBoundary.ps1 규칙).
#include "Shared/GameSim/Core/Ecs/CoreComponents.h"
#include "Shared/GameSim/Core/Ecs/NavigationThrottleComponent.h"
#include "Shared/GameSim/Core/Ecs/SpatialAgentComponent.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/Ecs/VisionComponents.h"
#include "Shared/GameSim/Core/Ecs/VisionSystem.h"

#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/SkillChargeStateComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/AreaAuraComponent.h"
#include "Shared/GameSim/Components/AsheSimComponent.h"
#include "Shared/GameSim/Components/AttackChaseComponent.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionAssistCredit.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/ItemRuntimeComponent.h"
#include "Shared/GameSim/Components/IreliaSimComponent.h"
#include "Shared/GameSim/Components/JaxSimComponent.h"
#include "Shared/GameSim/Components/JungleAIComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Components/KalistaPassiveDashComponent.h"
#include "Shared/GameSim/Components/KalistaRendComponent.h"
#include "Shared/GameSim/Components/KalistaSentinelComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/ShieldComponent.h"
#include "Shared/GameSim/Components/MasterYiComponent.h"
#include "Shared/GameSim/Components/MatchScore.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/PoseStateComponent.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/SylasSimComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Components/WaypointPatrolComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"
#include "Shared/GameSim/Components/ZedSimComponent.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <limits>
#include <unordered_set>
#include <utility>

namespace SimCheckpoint
{
namespace
{
	constexpr u64_t kKeyframeMagic = 0x31464B57ull; // 'WKF1'
	constexpr u64_t kKeyframeVersion = 5ull;

	bool_t ValidateEntityManagerState(
		const std::vector<CEntityManager::EntitySlot>& slots,
		EntityID freeHead,
		u32_t aliveCount)
	{
		if (slots.empty())
			return freeHead == NULL_ENTITY && aliveCount == 0u;
		if (slots[NULL_ENTITY].generation != NULL_ENTITY_GENERATION ||
			slots[NULL_ENTITY].nextFree != NULL_ENTITY)
		{
			return false;
		}
		if (freeHead >= slots.size())
			return false;

		std::vector<u8_t> freeSeen(slots.size(), 0u);
		EntityID cursor = freeHead;
		while (cursor != NULL_ENTITY)
		{
			if (cursor >= slots.size() || freeSeen[cursor] != 0u)
				return false;
			const auto& slot = slots[cursor];
			if (slot.generation == NULL_ENTITY_GENERATION ||
				(slot.generation & 1u) != 0u)
			{
				return false;
			}
			freeSeen[cursor] = 1u;
			cursor = slot.nextFree;
		}

		u32_t countedAlive = 0u;
		for (EntityID entity = 1u; entity < slots.size(); ++entity)
		{
			const auto& slot = slots[entity];
			const bool_t bAlive = (slot.generation & 1u) != 0u;
			if (bAlive)
			{
				if (slot.nextFree != NULL_ENTITY || freeSeen[entity] != 0u)
					return false;
				++countedAlive;
			}
			else if (slot.generation == NULL_ENTITY_GENERATION ||
				freeSeen[entity] == 0u)
			{
				return false;
			}
		}
		return countedAlive == aliveCount;
	}

	bool_t ValidateEntityBindings(
		const std::vector<CEntityManager::EntitySlot>& slots,
		const std::vector<std::pair<u32_t, u32_t>>& bindings,
		NetEntityId nextNetId)
	{
		if (nextNetId == NULL_NET_ENTITY)
			return false;

		std::unordered_set<EntityID> seenEntities;
		NetEntityId previousNetId = NULL_NET_ENTITY;
		for (const auto& [rawNetId, rawEntity] : bindings)
		{
			const NetEntityId netId = static_cast<NetEntityId>(rawNetId);
			const EntityID entity = static_cast<EntityID>(rawEntity);
			if (netId == NULL_NET_ENTITY ||
				netId <= previousNetId ||
				netId >= nextNetId ||
				entity == NULL_ENTITY ||
				entity >= slots.size() ||
				(slots[entity].generation & 1u) == 0u ||
				!seenEntities.emplace(entity).second)
			{
				return false;
			}
			previousNetId = netId;
		}
		return true;
	}

	// TransformComponent는 m_vecChildren(std::vector) 때문에 memcpy 불가 —
	// 서버 sim은 계층을 쓰지 않으므로(SetParent 호출 0건) POD 미러로 저장하고,
	// 계층이 발견되면 트립와이어로 하드 실패한다.
	struct TransformKeyframePod
	{
		Vec3 localPosition{};
		Vec3 localRotation{};
		Vec3 localScale{};
		Mat4 localMatrix{};
		Mat4 worldMatrix{};
		EntityID parent = NULL_ENTITY;
		u8_t bLocalDirty = 0;
		u8_t bWorldDirty = 0;
		u8_t pad[2]{};
	};
	static_assert(std::is_trivially_copyable_v<TransformKeyframePod>);

	bool_t SaveTransformStore(const CWorld& world, std::vector<u8_t>& out)
	{
		const CComponentStore<TransformComponent>* pStore =
			world.Checkpoint_TryGetStore<TransformComponent>();
		if (!pStore)
		{
			Detail::WriteVector(out, std::vector<uint32_t>{});
			Detail::WriteVector(out, std::vector<EntityID>{});
			Detail::WriteVector(out, std::vector<TransformKeyframePod>{});
			return true;
		}

		const std::vector<TransformComponent>& data = pStore->RawData();
		std::vector<TransformKeyframePod> pods;
		pods.reserve(data.size());
		for (size_t i = 0; i < data.size(); ++i)
		{
			const TransformComponent& t = data[i];
			if (t.m_Parent != NULL_ENTITY || !t.m_vecChildren.empty())
			{
				std::cerr << "[Keyframe] TransformComponent hierarchy is not keyframe-supported (entity "
					<< pStore->RawDense()[i] << ")\n";
				return false;
			}
			TransformKeyframePod pod{};
			pod.localPosition = t.m_LocalPosition;
			pod.localRotation = t.m_LocalRotation;
			pod.localScale = t.m_LocalScale;
			pod.localMatrix = t.m_LocalMatrix;
			pod.worldMatrix = t.m_WorldMatrix;
			pod.parent = t.m_Parent;
			pod.bLocalDirty = t.m_bLocalDirty ? 1u : 0u;
			pod.bWorldDirty = t.m_bWorldDirty ? 1u : 0u;
			pods.push_back(pod);
		}

		Detail::WriteVector(out, pStore->RawSparse());
		Detail::WriteVector(out, pStore->RawDense());
		Detail::WriteVector(out, pods);
		return true;
	}

	bool_t LoadTransformStore(CWorld& world, const u8_t* pPayload, size_t size)
	{
		const u8_t* p = pPayload;
		const u8_t* pEnd = pPayload + size;
		std::vector<uint32_t> sparse;
		std::vector<EntityID> dense;
		std::vector<TransformKeyframePod> pods;
		if (!Detail::ReadVector(p, pEnd, sparse) ||
			!Detail::ReadVector(p, pEnd, dense) ||
			!Detail::ReadVector(p, pEnd, pods) ||
			p != pEnd ||
			dense.size() != pods.size() ||
			!Detail::ValidateComponentStoreTopology(
				world,
				sparse,
				dense))
		{
			return false;
		}

		std::vector<TransformComponent> data;
		data.resize(pods.size());
		for (size_t i = 0; i < pods.size(); ++i)
		{
			TransformComponent& t = data[i];
			const TransformKeyframePod& pod = pods[i];
			if (pod.parent != NULL_ENTITY ||
				pod.bLocalDirty > 1u ||
				pod.bWorldDirty > 1u)
			{
				return false;
			}
			t.m_LocalPosition = pod.localPosition;
			t.m_LocalRotation = pod.localRotation;
			t.m_LocalScale = pod.localScale;
			t.m_LocalMatrix = pod.localMatrix;
			t.m_WorldMatrix = pod.worldMatrix;
			t.m_Parent = pod.parent;
			t.m_bLocalDirty = pod.bLocalDirty != 0u;
			t.m_bWorldDirty = pod.bWorldDirty != 0u;
		}
		world.Checkpoint_GetOrCreateStore<TransformComponent>().RestoreRaw(
			std::move(sparse), std::move(dense), std::move(data));
		return true;
	}

	bool_t SaveChampionAIResearchTransient(
		const CWorld&,
		std::vector<u8_t>&)
	{
		// Decision traces and influence samples are re-derived after rewind.
		// Deliberately write an empty payload instead of keyframing debug evidence.
		return true;
	}

	bool_t LoadChampionAIResearchTransient(
		CWorld&,
		const u8_t*,
		size_t size)
	{
		return size == 0u;
	}

	void RegisterAllKeyframeComponents()
	{
		auto& reg = KeyframeComponentRegistry::Get();

		reg.RegisterCustom(std::type_index(typeid(TransformComponent)),
			"TransformComponent", &SaveTransformStore, &LoadTransformStore);

		// Engine (어댑터 경유)
		reg.Register<HealthComponent>("HealthComponent");
		reg.Register<VelocityComponent>("VelocityComponent");
		reg.Register<ColliderComponent>("ColliderComponent");
		reg.Register<SpatialAgentComponent>("SpatialAgentComponent");
		reg.Register<VisionConeComponent>("VisionConeComponent");
		reg.Register<VisionSensorComponent>("VisionSensorComponent");
		reg.Register<VisionSourceComponent>("VisionSourceComponent");
		reg.Register<VisibilityComponent>("VisibilityComponent");
		reg.Register<NavRepathThrottleComponent>("NavRepathThrottleComponent");

		// GameplayComponents.h 집합
		reg.Register<ChampionComponent>("ChampionComponent");
		reg.Register<DisarmComponent>("DisarmComponent");
		reg.Register<ExperienceComponent>("ExperienceComponent");
		reg.Register<ForcedMotionComponent>("ForcedMotionComponent");
		reg.Register<GameplayStateComponent>("GameplayStateComponent");
		reg.Register<PositionDiscontinuityComponent>(
			"PositionDiscontinuityComponent");
		reg.Register<InhibitorTag>("InhibitorTag");
		reg.Register<JungleComponent>("JungleComponent");
		reg.Register<JungleMonsterTag>("JungleMonsterTag");
		reg.Register<MinionComponent>("MinionComponent");
		reg.Register<MinionStateComponent>("MinionStateComponent");
		reg.Register<NexusTag>("NexusTag");
		reg.Register<PracticeDummyTag>("PracticeDummyTag");
		reg.Register<PracticePlayerComponent>("PracticePlayerComponent");
		reg.Register<PracticeChampionStatOverrideComponent>("PracticeChampionStatOverrideComponent");
		reg.Register<PracticeItemStatOverrideComponent>("PracticeItemStatOverrideComponent");
		reg.Register<PracticeSkillEffectOverrideComponent>("PracticeSkillEffectOverrideComponent");
		reg.Register<PracticeSpawnedTag>("PracticeSpawnedTag");
		reg.Register<RivenStateComponent>("RivenStateComponent");
		reg.Register<SkillStateComponent>("SkillStateComponent");
		reg.Register<SkillChargeStateComponent>("SkillChargeStateComponent");
		reg.Register<StatusEffectComponent>("StatusEffectComponent");
		reg.Register<StructureComponent>("StructureComponent");
		reg.Register<StructureProjectileComponent>("StructureProjectileComponent");
		reg.Register<TargetableTag>("TargetableTag");
		reg.Register<TowerAggroNotifyComponent>("TowerAggroNotifyComponent");
		reg.Register<TurretAIComponent>("TurretAIComponent");
		reg.Register<TurretComponent>("TurretComponent");
		reg.Register<YasuoStateComponent>("YasuoStateComponent");

		// 개별 헤더
		reg.Register<ActionStateComponent>("ActionStateComponent");
		reg.Register<AnnieSimComponent>("AnnieSimComponent");
		reg.Register<AnnieTibbersComponent>("AnnieTibbersComponent");
		reg.Register<AreaAuraComponent>("AreaAuraComponent");
		reg.Register<AsheSimComponent>("AsheSimComponent");
		reg.Register<AttackChaseComponent>("AttackChaseComponent");
		reg.Register<BuffComponent>("BuffComponent");
		reg.Register<ChampionAIComponent>("ChampionAIComponent");
		reg.RegisterCustom(
			std::type_index(typeid(ChampionAIResearchDebugComponent)),
			"Transient.ChampionAIResearchDebugComponent",
			&SaveChampionAIResearchTransient,
			&LoadChampionAIResearchTransient);
		reg.Register<ChampionAssistCreditComponent>("ChampionAssistCreditComponent");
		reg.Register<ChampionDefinitionComponent>("ChampionDefinitionComponent");
		reg.Register<ChampionScoreComponent>("ChampionScoreComponent");
		reg.Register<SummonerSpellStateComponent>("SummonerSpellStateComponent");
		reg.Register<CombatActionComponent>("CombatActionComponent");
		reg.Register<DamageRequestComponent>("DamageRequestComponent");
		reg.Register<FioraSimComponent>("FioraSimComponent");
		reg.Register<FormOverrideComponent>("FormOverrideComponent");
		reg.Register<GoldComponent>("GoldComponent");
		reg.Register<InventoryComponent>("InventoryComponent");
		reg.Register<ItemRuntimeComponent>("ItemRuntimeComponent");
		reg.Register<IreliaSimComponent>("IreliaSimComponent");
		reg.Register<IreliaMarkComponent>("IreliaMarkComponent");
		reg.Register<JaxSimComponent>("JaxSimComponent");
		reg.Register<JungleAIComponent>("JungleAIComponent");
		reg.Register<KalistaFateCallCarriedComponent>("KalistaFateCallCarriedComponent");
		reg.Register<KalistaOathswornByComponent>("KalistaOathswornByComponent");
		reg.Register<KalistaOathswornComponent>("KalistaOathswornComponent");
		reg.Register<KalistaFateCallComponent>("KalistaFateCallComponent");
		reg.Register<KalistaSentinelComponent>("KalistaSentinelComponent");
		reg.Register<KalistaPassiveDashComponent>("KalistaPassiveDashComponent");
		reg.Register<KalistaRendStackComponent>("KalistaRendStackComponent");
		reg.Register<KindredHealthFloorComponent>("KindredHealthFloorComponent");
		reg.Register<KindredSimComponent>("KindredSimComponent");
		reg.Register<LeeSinDashComponent>("LeeSinDashComponent");
		reg.Register<LeeSinQMarkComponent>("LeeSinQMarkComponent");
		reg.Register<LeeSinSimComponent>("LeeSinSimComponent");
		reg.Register<LeeSinWardOwnerComponent>("LeeSinWardOwnerComponent");
		reg.Register<MasterYiSimComponent>("MasterYiSimComponent");
		reg.Register<MatchScoreComponent>("MatchScoreComponent");
		reg.Register<MoveTargetComponent>("MoveTargetComponent");
		reg.Register<NetEntityIdComponent>("NetEntityIdComponent");
		reg.Register<PoseStateComponent>("PoseStateComponent");
		reg.Register<RecallComponent>("RecallComponent");
		reg.Register<ReplicatedEventComponent>("ReplicatedEventComponent");
		reg.Register<RespawnComponent>("RespawnComponent");
		reg.Register<ShieldComponent>("ShieldComponent");
		reg.Register<RuneLoadoutComponent>("RuneLoadoutComponent");
		reg.Register<RuneRuntimeComponent>("RuneRuntimeComponent");
		reg.Register<SkillLoadoutComponent>("SkillLoadoutComponent");
		reg.Register<SkillProjectileComponent>("SkillProjectileComponent");
		reg.Register<SkillRankComponent>("SkillRankComponent");
		reg.Register<SpellbookOverrideComponent>("SpellbookOverrideComponent");
		reg.Register<StatComponent>("StatComponent");
		reg.Register<SylasDashComponent>("SylasDashComponent");
		reg.Register<SylasSimComponent>("SylasSimComponent");
		reg.Register<ViegoSimComponent>("ViegoSimComponent");
		reg.Register<ViegoSoulComponent>("ViegoSoulComponent");
		reg.Register<WaypointPatrolComponent>("WaypointPatrolComponent");
		reg.Register<YoneSimComponent>("YoneSimComponent");
		reg.Register<ZedDeathMarkComponent>("ZedDeathMarkComponent");
		reg.Register<ZedSimComponent>("ZedSimComponent");
		reg.Register<ZedVanishComponent>("ZedVanishComponent");
	}

	void EnsureKeyframeRegistryInitialized()
	{
		static const bool_t s_bInitialized = []()
		{
			RegisterAllKeyframeComponents();
			return true;
		}();
		(void)s_bInitialized;
	}

	bool_t PreflightWorldKeyframeBlob(const std::vector<u8_t>& bytes,
		const KeyframeComponentRegistry& reg)
	{
		if (bytes.size() < sizeof(u64_t) * 4u)
		{
			std::cerr << "[Keyframe] restore failed - bad header\n";
			return false;
		}

		const u8_t* p = bytes.data();
		const u8_t* pEnd = bytes.data() + bytes.size();
		u64_t magic = 0, version = 0, tickIndex = 0, rngState = 0;
		if (!Detail::ReadU64(p, pEnd, magic) || magic != kKeyframeMagic ||
			!Detail::ReadU64(p, pEnd, version) || version != kKeyframeVersion ||
			!Detail::ReadU64(p, pEnd, tickIndex) ||
			!Detail::ReadU64(p, pEnd, rngState))
		{
			std::cerr << "[Keyframe] restore failed - bad header\n";
			return false;
		}

		std::vector<CEntityManager::EntitySlot> slots;
		u64_t freeHead = 0, aliveCount = 0;
		if (!Detail::ReadVector(p, pEnd, slots) ||
			!Detail::ReadU64(p, pEnd, freeHead) ||
			!Detail::ReadU64(p, pEnd, aliveCount) ||
			freeHead > (std::numeric_limits<EntityID>::max)() ||
			aliveCount > (std::numeric_limits<uint32_t>::max)())
		{
			std::cerr << "[Keyframe] restore failed - bad entity manager section\n";
			return false;
		}

		const size_t maxAliveCount = slots.empty() ? 0u : slots.size() - 1u;
		if (aliveCount > maxAliveCount ||
			(freeHead != NULL_ENTITY && freeHead >= slots.size()))
		{
			std::cerr << "[Keyframe] restore failed - invalid entity manager state\n";
			return false;
		}

		u64_t nextNetId = 0;
		std::vector<std::pair<u32_t, u32_t>> bindings;
		if (!Detail::ReadU64(p, pEnd, nextNetId) ||
			!Detail::ReadVector(p, pEnd, bindings) ||
			nextNetId > (std::numeric_limits<NetEntityId>::max)())
		{
			std::cerr << "[Keyframe] restore failed - bad entity id map section\n";
			return false;
		}

		u64_t storeCount = 0;
		if (!Detail::ReadU64(p, pEnd, storeCount))
		{
			std::cerr << "[Keyframe] restore failed - bad store count\n";
			return false;
		}

		u64_t registeredStoreCount = 0;
		reg.ForEachOps([&](const KeyframeStoreOps&) { ++registeredStoreCount; });
		if (storeCount != registeredStoreCount ||
			storeCount > static_cast<u64_t>((std::numeric_limits<size_t>::max)()))
		{
			std::cerr << "[Keyframe] restore failed - incomplete store set\n";
			return false;
		}

		std::unordered_set<u64_t> seenStoreHashes;
		seenStoreHashes.reserve(static_cast<size_t>(storeCount));
		for (u64_t i = 0; i < storeCount; ++i)
		{
			u64_t nameHash = 0, payloadSize = 0;
			if (!Detail::ReadU64(p, pEnd, nameHash) ||
				!Detail::ReadU64(p, pEnd, payloadSize) ||
				payloadSize > static_cast<u64_t>((std::numeric_limits<size_t>::max)()))
			{
				std::cerr << "[Keyframe] restore failed - bad store record\n";
				return false;
			}

			const size_t payloadByteCount = static_cast<size_t>(payloadSize);
			if (static_cast<size_t>(pEnd - p) < payloadByteCount)
			{
				std::cerr << "[Keyframe] restore failed - truncated store payload\n";
				return false;
			}
			if (!reg.FindByHash(nameHash))
			{
				std::cerr << "[Keyframe] restore failed - unknown store hash "
					<< nameHash << "\n";
				return false;
			}
			if (!seenStoreHashes.emplace(nameHash).second)
			{
				std::cerr << "[Keyframe] restore failed - duplicate store hash "
					<< nameHash << "\n";
				return false;
			}
			p += payloadByteCount;
		}

		if (p != pEnd)
		{
			std::cerr << "[Keyframe] restore failed - trailing bytes\n";
			return false;
		}
		return true;
	}
}

bool_t SaveWorldKeyframe(const CWorld& world, const DeterministicRng& rng,
	const EntityIdMap& entityMap, u64_t tickIndex, std::vector<u8_t>& outBytes)
{
	EnsureKeyframeRegistryInitialized();
	const KeyframeComponentRegistry& reg = KeyframeComponentRegistry::Get();

	// 완전성 기계 검사: 월드의 모든 스토어가 등록돼 있어야 저장을 허용한다.
	bool_t bComplete = true;
	world.ForEachStoreBase([&](const std::type_index& ti, const IComponentStoreBase&)
	{
		if (!reg.Find(ti))
		{
			std::cerr << "[Keyframe] unregistered component store: " << ti.name() << "\n";
			bComplete = false;
		}
	});
	if (!bComplete)
		return false;

	outBytes.clear();
	Detail::WriteU64(outBytes, kKeyframeMagic);
	Detail::WriteU64(outBytes, kKeyframeVersion);
	Detail::WriteU64(outBytes, tickIndex);
	Detail::WriteU64(outBytes, rng.GetState());

	const CEntityManager& mgr = world.GetEntityManager();
	Detail::WriteVector(outBytes, mgr.RawSlots());
	Detail::WriteU64(outBytes, mgr.RawFreeHead());
	Detail::WriteU64(outBytes, mgr.RawAliveCount());

	std::vector<std::pair<u32_t, u32_t>> bindings;
	entityMap.ForEachBinding([&](NetEntityId netId, EntityID entity)
	{
		bindings.emplace_back(static_cast<u32_t>(netId), static_cast<u32_t>(entity));
	});
	std::sort(bindings.begin(), bindings.end());
	Detail::WriteU64(outBytes, entityMap.GetNextNetId());
	Detail::WriteVector(outBytes, bindings);

	std::vector<const KeyframeStoreOps*> sortedOps;
	reg.ForEachOps([&](const KeyframeStoreOps& ops) { sortedOps.push_back(&ops); });
	std::sort(sortedOps.begin(), sortedOps.end(),
		[](const KeyframeStoreOps* a, const KeyframeStoreOps* b)
		{ return a->stableName < b->stableName; });

	Detail::WriteU64(outBytes, static_cast<u64_t>(sortedOps.size()));
	std::vector<u8_t> payload;
	for (const KeyframeStoreOps* pOps : sortedOps)
	{
		payload.clear();
		if (!pOps->save(world, payload))
		{
			std::cerr << "[Keyframe] save failed for store: " << pOps->stableName << "\n";
			return false;
		}
		Detail::WriteU64(outBytes, pOps->nameHash);
		Detail::WriteU64(outBytes, static_cast<u64_t>(payload.size()));
		outBytes.insert(outBytes.end(), payload.begin(), payload.end());
	}
	return true;
}

static bool_t RestoreWorldKeyframeStaged(CWorld& world, DeterministicRng& rng,
	EntityIdMap& entityMap, u64_t& outTickIndex, const std::vector<u8_t>& bytes)
{
	EnsureKeyframeRegistryInitialized();
	const KeyframeComponentRegistry& reg = KeyframeComponentRegistry::Get();

	// 복원 대상 월드에 미등록 스토어가 있으면 그 상태가 과거로 새어 들어간다 — 차단.
	bool_t bComplete = true;
	world.ForEachStoreBase([&](const std::type_index& ti, const IComponentStoreBase&)
	{
		if (!reg.Find(ti))
		{
			std::cerr << "[Keyframe] restore blocked - unregistered store: " << ti.name() << "\n";
			bComplete = false;
		}
	});
	if (!bComplete)
		return false;

	const u8_t* p = bytes.data();
	const u8_t* pEnd = bytes.data() + bytes.size();

	u64_t magic = 0, version = 0, tickIndex = 0, rngState = 0;
	if (!Detail::ReadU64(p, pEnd, magic) || magic != kKeyframeMagic ||
		!Detail::ReadU64(p, pEnd, version) || version != kKeyframeVersion ||
		!Detail::ReadU64(p, pEnd, tickIndex) ||
		!Detail::ReadU64(p, pEnd, rngState))
	{
		std::cerr << "[Keyframe] restore failed - bad header\n";
		return false;
	}

	std::vector<CEntityManager::EntitySlot> slots;
	u64_t freeHead = 0, aliveCount = 0;
	if (!Detail::ReadVector(p, pEnd, slots) ||
		!Detail::ReadU64(p, pEnd, freeHead) ||
		!Detail::ReadU64(p, pEnd, aliveCount))
	{
		std::cerr << "[Keyframe] restore failed - bad entity manager section\n";
		return false;
	}

	u64_t nextNetId = 0;
	std::vector<std::pair<u32_t, u32_t>> bindings;
	if (!Detail::ReadU64(p, pEnd, nextNetId) ||
		!Detail::ReadVector(p, pEnd, bindings))
	{
		std::cerr << "[Keyframe] restore failed - bad entity id map section\n";
		return false;
	}
	if (freeHead > (std::numeric_limits<EntityID>::max)() ||
		aliveCount > (std::numeric_limits<u32_t>::max)() ||
		nextNetId > (std::numeric_limits<NetEntityId>::max)() ||
		!ValidateEntityManagerState(
			slots,
			static_cast<EntityID>(freeHead),
			static_cast<u32_t>(aliveCount)) ||
		!ValidateEntityBindings(
			slots,
			bindings,
			static_cast<NetEntityId>(nextNetId)))
	{
		std::cerr << "[Keyframe] restore failed - invalid entity topology or bindings\n";
		return false;
	}

	u64_t storeCount = 0;
	if (!Detail::ReadU64(p, pEnd, storeCount))
	{
		std::cerr << "[Keyframe] restore failed - bad store count\n";
		return false;
	}

	// 헤더 검증이 끝난 시점부터 실제 복원 수행.
	world.GetEntityManager().RestoreRaw(std::move(slots),
		static_cast<EntityID>(freeHead), static_cast<uint32_t>(aliveCount));

	std::vector<std::pair<NetEntityId, EntityID>> mapBindings;
	mapBindings.reserve(bindings.size());
	for (const auto& [netId, entity] : bindings)
		mapBindings.emplace_back(static_cast<NetEntityId>(netId), static_cast<EntityID>(entity));
	entityMap.RestoreState(mapBindings, static_cast<NetEntityId>(nextNetId));

	for (u64_t i = 0; i < storeCount; ++i)
	{
		u64_t nameHash = 0, payloadSize = 0;
		if (!Detail::ReadU64(p, pEnd, nameHash) ||
			!Detail::ReadU64(p, pEnd, payloadSize) ||
			static_cast<size_t>(pEnd - p) < payloadSize)
		{
			std::cerr << "[Keyframe] restore failed - bad store record\n";
			return false;
		}
		const KeyframeStoreOps* pOps = reg.FindByHash(nameHash);
		if (!pOps)
		{
			std::cerr << "[Keyframe] restore failed - unknown store hash " << nameHash << "\n";
			return false;
		}
		if (!pOps->load(world, p, static_cast<size_t>(payloadSize)))
		{
			std::cerr << "[Keyframe] restore failed - load error in store: "
				<< pOps->stableName << "\n";
			return false;
		}
		p += payloadSize;
	}
	if (p != pEnd)
	{
		std::cerr << "[Keyframe] restore failed - trailing bytes\n";
		return false;
	}

	rng.SetState(rngState);
	outTickIndex = tickIndex;
	return true;
}

bool_t RestoreWorldKeyframe(CWorld& world, DeterministicRng& rng,
	EntityIdMap& entityMap, u64_t& outTickIndex, const std::vector<u8_t>& bytes)
{
	EnsureKeyframeRegistryInitialized();
	const KeyframeComponentRegistry& reg = KeyframeComponentRegistry::Get();

	bool_t bComplete = true;
	world.ForEachStoreBase([&](const std::type_index& ti, const IComponentStoreBase&)
	{
		if (!reg.Find(ti))
		{
			std::cerr << "[Keyframe] restore blocked - unregistered store: "
				<< ti.name() << "\n";
			bComplete = false;
		}
	});
	if (!bComplete)
		return false;

	try
	{
		// Structural validation sees the entire blob before a codec is allowed to
		// run. Codec validation then mutates only the staged state.
		if (!PreflightWorldKeyframeBlob(bytes, reg))
			return false;

		CWorld stagedWorld;
		DeterministicRng stagedRng(1ull);
		EntityIdMap stagedEntityMap;
		u64_t stagedTickIndex = 0;
		if (!RestoreWorldKeyframeStaged(
			stagedWorld, stagedRng, stagedEntityMap, stagedTickIndex, bytes))
		{
			return false;
		}

		// Commit is a set of noexcept swaps plus scalar assignments. The spatial
		// index is intentionally retained on the live world and rebuilt by the
		// rewind caller because it is a derived cache, not authoritative state.
		world.Checkpoint_SwapState(stagedWorld);
		entityMap.SwapState(stagedEntityMap);
		rng.SetState(stagedRng.GetState());
		outTickIndex = stagedTickIndex;
		return true;
	}
	catch (const std::exception& e)
	{
		std::cerr << "[Keyframe] restore failed - exception while staging: "
			<< e.what() << "\n";
		return false;
	}
	catch (...)
	{
		std::cerr << "[Keyframe] restore failed - unknown exception while staging\n";
		return false;
	}
}
}
