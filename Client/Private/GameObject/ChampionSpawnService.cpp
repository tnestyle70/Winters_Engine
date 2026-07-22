#include "GameObject/ChampionSpawnService.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

#include <Windows.h>
#include <cstdio>
#include <memory>
#include <string>

#include "ECS/World.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/MeshGroupVisibilityComponent.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "GameObject/Champion/Annie/Annie_Components.h"
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Ezreal/Ezreal_Components.h"
#include "GameObject/Champion/Fiora/Fiora_Components.h"
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Yone/Yone_Components.h"
#include "GameObject/Champion/Yone/Yone_MeshGroups.h"
#include "GameObject/SkillDef.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"

namespace
{
	// Report missing animation keys against the assembled runtime model.
	void ValidateChampionAnimKeys(const ChampionDef& def, ModelRenderer& renderer)
	{
#if defined(_DEBUG)
		const char* pPrefix = def.animPrefix ? def.animPrefix : "";
		auto FullName = [&](const char* pKey)
		{
			std::string full(pKey);
			if (pPrefix[0] != '\0' && full.find(pPrefix) == std::string::npos)
				full.insert(0, pPrefix);
			return full;
		};
		auto LogMiss = [&](const char* pLabel, const char* pKey)
		{
			if (!pKey)
				return;
			const std::string full = FullName(pKey);
			if (renderer.HasAnimationByName(full))
				return;
			char msg[224]{};
			sprintf_s(msg, "[AnimKeyMiss] champion=%u %s=%s\n",
				static_cast<u32_t>(def.id), pLabel, full.c_str());
			OutputDebugStringA(msg);
		};

		LogMiss("idle", def.idleAnimKey);
		LogMiss("run", def.runAnimKey);
		LogMiss("attack", def.basicAttackKey);
		for (u8_t slot = 0; slot < 5u; ++slot)
		{
			const SkillDef* pSkill = CSkillRegistry::Instance().Find(def.id, slot);
			if (!pSkill)
				continue;
			char label[16]{};
			sprintf_s(label, "slot%u", static_cast<u32_t>(slot));
			LogMiss(label, pSkill->animKey);
			if (pSkill->stage2AnimKey)
			{
				char label2[16]{};
				sprintf_s(label2, "slot%u_s2", static_cast<u32_t>(slot));
				LogMiss(label2, pSkill->stage2AnimKey);
			}
		}
#else
		(void)def;
		(void)renderer;
#endif
	}

	const ChampionDef* FindSpawnChampionDef(eChampion id)
	{
		const ChampionCatalogEntry* pEntry = CChampionCatalog::Instance().Find(id);
		if (pEntry && pEntry->pDef)
			return pEntry->pDef;

		const ChampionDef* pDef = CChampionRegistry::Instance().Find(id);
		if (pDef)
			return pDef;

		return FindChampionDef(id);
	}

	void AttachVisionAgent(
		CWorld& world,
		EntityID entity,
		eSpatialKind kind,
		eTeam team,
		f32_t radius,
		f32_t sightRange)
	{
		SpatialAgentComponent spatial{};
		spatial.kind = kind;
		spatial.team = static_cast<u8_t>(team);
		spatial.radius = radius;
		if (!world.HasComponent<SpatialAgentComponent>(entity))
			world.AddComponent<SpatialAgentComponent>(entity, spatial);

		VisionSourceComponent vision{};
		vision.sightRange = sightRange;
		if (!world.HasComponent<VisionSourceComponent>(entity))
			world.AddComponent<VisionSourceComponent>(entity, vision);

		if (!world.HasComponent<VisibilityComponent>(entity))
			world.AddComponent<VisibilityComponent>(entity);
	}

	void AttachGameplayCollider(CWorld& world, EntityID entity, f32_t radius, f32_t halfHeight)
	{
		ColliderComponent collider{};
		collider.vHalfExtents = { radius, halfHeight, radius };
		collider.vOffset = { 0.f, halfHeight, 0.f };
		collider.bIsTrigger = false;
		if (!world.HasComponent<ColliderComponent>(entity))
			world.AddComponent<ColliderComponent>(entity, collider);
	}

	void AttachChampionStateComponents(CWorld& world, EntityID entity, eChampion id)
	{
		if (id == eChampion::YASUO)
			world.AddComponent<YasuoStateComponent>(entity);
		else if (id == eChampion::RIVEN)
			world.AddComponent<RivenStateComponent>(entity);
		else if (id == eChampion::EZREAL)
			world.AddComponent<EzrealStateComponent>(entity);
		else if (id == eChampion::FIORA)
			world.AddComponent<FioraStateComponent>(entity);
		else if (id == eChampion::JAX)
			world.AddComponent<JaxStateComponent>(entity);
		else if (id == eChampion::ANNIE)
			world.AddComponent<AnnieStateComponent>(entity);
		else if (id == eChampion::ASHE)
			world.AddComponent<AsheStateComponent>(entity);
		else if (id == eChampion::YONE)
		{
			world.AddComponent<YoneStateComponent>(entity);

			MeshGroupVisibilityComponent visibility{};
			visibility.mask = Yone::MeshGroups::MaskBaseDefault();
			visibility.bEnabled = true;
			world.AddComponent<MeshGroupVisibilityComponent>(entity, visibility);
		}
	}
}

ChampionSpawnResult CChampionSpawnService::Spawn(
	ChampionSpawnContext& context,
	const ChampionSpawnRequest& request)
{
	ChampionSpawnResult result{};

	const ChampionDef* pDef = FindSpawnChampionDef(request.champion);
	if (!pDef || !pDef->fbxPath)
	{
		static u32_t s_spawnNoDefLogCount = 0;
		if (s_spawnNoDefLogCount < 8)
		{
			char msg[128]{};
			sprintf_s(msg, "[ChampionSpawn] FAILED champion=%u reason=no-def\n",
				static_cast<u32_t>(request.champion));
			OutputDebugStringA(msg);
			++s_spawnNoDefLogCount;
		}
		return result;
	}

	std::unique_ptr<ModelRenderer> pRenderer = std::make_unique<ModelRenderer>();
	if (!pRenderer->Initialize(pDef->fbxPath, pDef->shaderPath))
	{
		static u32_t s_spawnInitFailLogCount = 0;
		if (s_spawnInitFailLogCount < 8)
		{
			char msg[512]{};
			sprintf_s(msg, "[ChampionSpawn] FAILED champion=%u reason=renderer-init fbx=%s\n",
				static_cast<u32_t>(request.champion),
				pDef->fbxPath);
			OutputDebugStringA(msg);
			++s_spawnInitFailLogCount;
		}
		return result;
	}

	if (pDef->defaultTexturePath)
		pRenderer->LoadTextureForAllMeshes(pDef->defaultTexturePath);

	for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
	{
		if (pDef->texturePath[i])
			pRenderer->LoadMeshTexture(i, pDef->texturePath[i]);
	}

	if (pDef->animPrefix && pDef->idleAnimKey)
	{
		const std::string IdleFull = std::string(pDef->animPrefix) + pDef->idleAnimKey;
		pRenderer->PlayAnimationByName(IdleFull, true);
	}

	ValidateChampionAnimKeys(*pDef, *pRenderer);

	const EntityID entity = context.world.CreateEntity();
	const Vec3 spawnPosition = request.bUseCatalogSpawnPosition
		? pDef->spawnPosition
		: request.position;
	const ChampionStatsDef statsDef =
		BuildDefaultChampionStatsDef(request.champion);
	constexpr u8_t kSpawnChampionLevel = 6;
	StatComponent stat = CStatSystem::BuildBaseStats(statsDef, kSpawnChampionLevel);

	TransformComponent transform{};
	transform.SetPosition(spawnPosition);
	transform.SetScale(pDef->spawnScale);
	context.world.AddComponent<TransformComponent>(entity, transform);

	RenderComponent render{};
	render.pRenderer = pRenderer.get();
	render.bVisible = true;
	render.bAnimated = true;
	render.bSceneManaged = false;
	context.world.AddComponent<RenderComponent>(entity, render);

	ChampionComponent champion{};
	champion.id = request.champion;
	champion.team = request.team;
	champion.hp = stat.hpMax;
	champion.maxHp = stat.hpMax;
	champion.mana = stat.manaMax;
	champion.maxMana = stat.manaMax;
	champion.moveSpeed = stat.moveSpeed;
	champion.level = stat.level;
	context.world.AddComponent<ChampionComponent>(entity, champion);

	context.world.AddComponent<StatComponent>(entity, stat);

	SetPoseState(context.world, entity, ePoseStateId::Idle, 0, true);

	HealthComponent health{};
	health.fCurrent = champion.hp;
	health.fMaximum = champion.maxHp;
	health.bIsDead = false;
	context.world.AddComponent<HealthComponent>(entity, health);

	context.world.AddComponent<ServerIdComponent>(entity);
	context.world.AddComponent<TargetableTag>(entity);

	NavAgentComponent agent{};
	agent.fSpeed = stat.moveSpeed;
	agent.fArriveRadius = statsDef.navArriveRadius;
	agent.bHasGoal = false;
	agent.bPathDirty = false;
	context.world.AddComponent<NavAgentComponent>(entity, agent);

	context.world.AddComponent<VelocityComponent>(entity);
	context.world.AddComponent<SkillStateComponent>(entity);

	CExperienceSystem::InitializeChampionExperience(context.world, entity, champion.level);

	SkillRankComponent skillRank{};
	CSkillRankSystem::SyncPointsForLevel(skillRank, champion.level);
	context.world.AddComponent<SkillRankComponent>(entity, skillRank);

	AttachVisionAgent(
		context.world,
		entity,
		eSpatialKind::Character,
		request.team,
		statsDef.spatialRadius,
		statsDef.sightRange);
	AttachGameplayCollider(context.world, entity, statsDef.spatialRadius, 1.5f);
	AttachChampionStateComponents(context.world, entity, request.champion);

	context.networkChampionPrevPos[entity] = transform.GetPosition();
	context.networkChampionMoveGraceSec[entity] = 0.f;
	context.networkChampionMoving[entity] = false;
	context.renderers[entity] = std::move(pRenderer);

	result.entity = entity;
	result.pRenderer = context.renderers[entity].get();
	result.pDef = pDef;

	return result;
}

bool_t CChampionSpawnService::AttachVisual(ChampionSpawnContext& context, EntityID entity, eChampion champion)
{
	const ChampionDef* pDef = FindSpawnChampionDef(champion);
	if (!pDef || !pDef->fbxPath)
		return false;

	std::unique_ptr<ModelRenderer> pRenderer = std::make_unique<ModelRenderer>();
	if (!pRenderer->Initialize(pDef->fbxPath, pDef->shaderPath))
		return false;

	if (pDef->defaultTexturePath)
		pRenderer->LoadTexture(pDef->defaultTexturePath);

	for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
	{
		if (pDef->texturePath[i])
			pRenderer->LoadMeshTexture(i, pDef->texturePath[i]);
	}

	if (pDef->animPrefix && pDef->idleAnimKey)
		pRenderer->PlayAnimationByName(
			std::string(pDef->animPrefix) + pDef->idleAnimKey, true);

	ValidateChampionAnimKeys(*pDef, *pRenderer);

	if (champion == eChampion::YONE)
	{
		auto& visibility = context.world.HasComponent<MeshGroupVisibilityComponent>(entity)
			? context.world.GetComponent<MeshGroupVisibilityComponent>(entity)
			: context.world.AddComponent<MeshGroupVisibilityComponent>(
				entity,
				MeshGroupVisibilityComponent{});
		visibility.mask = Yone::MeshGroups::MaskBaseDefault();
		visibility.bEnabled = true;
	}
	else if (context.world.HasComponent<MeshGroupVisibilityComponent>(entity))
	{
		context.world.RemoveComponent<MeshGroupVisibilityComponent>(entity);
	}

	if (context.world.HasComponent<TransformComponent>(entity))
		context.world.GetComponent<TransformComponent>(entity).SetScale(pDef->spawnScale);

	RenderComponent render{};
	render.pRenderer = pRenderer.get();
	render.bVisible = true;
	render.bAnimated = true;

	if (context.world.HasComponent<RenderComponent>(entity))
		context.world.GetComponent<RenderComponent>(entity) = render;
	else
		context.world.AddComponent<RenderComponent>(entity, render);

	context.renderers[entity] = std::move(pRenderer);

	return true;
}
