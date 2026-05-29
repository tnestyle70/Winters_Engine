#pragma once

#include "Defines.h"
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameContext.h"
#include "GameObject/ChampionDef.h"
#include "Renderer/ModelRenderer.h"

#include <memory>
#include <unordered_map>

class CWorld;

struct ChampionSpawnRequest
{
	eChampion champion = eChampion::END;
	eTeam team = eTeam::TEAM_END;
	Vec3 position{};
	bool_t bUseCatalogSpawnPosition = true;
};

struct ChampionSpawnResult
{
	EntityID entity = NULL_ENTITY;
	ModelRenderer* pRenderer = nullptr;
	const ChampionDef* pDef = nullptr;
};

struct ChampionSpawnContext
{
	CWorld& world;
	std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>>& renderers;
	std::unordered_map<EntityID, Vec3>& networkChampionPrevPos;
	std::unordered_map<EntityID, f32_t>& networkChampionMoveGraceSec;
	std::unordered_map<EntityID, bool_t>& networkChampionMoving;
};

class CChampionSpawnService final
{
public:
	static ChampionSpawnResult Spawn(
		ChampionSpawnContext& context,
		const ChampionSpawnRequest& request);

	static bool_t AttachVisual(
		ChampionSpawnContext& context,
		EntityID entity,
		eChampion champion);
private:
	CChampionSpawnService() = delete;
};
