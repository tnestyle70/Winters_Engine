#include "WintersPCH.h"
#include "ECS/Systems/EntityBlueprintRegistry.h"
#include "ECS/World.h"

//Spawn이 어디에있음??

HRESULT CEntityBlueprintRegistry::Initialize(uint32_t iNumLevels)
{
	if (iNumLevels == 0)
		return E_FAIL;

	m_iNumScenes = iNumLevels;
	m_pBlueprints = std::make_unique<std::unordered_map<std::wstring,
		CEntityBlueprint>[]>(iNumLevels);
	return S_OK;
}

HRESULT CEntityBlueprintRegistry::Add_Blueprint(uint32_t iSceneID,
	const std::wstring& strKey, CEntityBlueprint blueprint)
{
	if (iSceneID >= m_iNumScenes)
		return E_FAIL;
	//키에 해당하는 맵 찾아서 넣어주기
	auto& map = m_pBlueprints[iSceneID];
	//이미 해당 키의 Scene이 존재할 경우 return
	if (map.find(strKey) != map.end())
		return E_FAIL;

	map.emplace(strKey, std::move(blueprint));
	
	return S_OK;
}

EntityID CEntityBlueprintRegistry::Clone_Entity(uint32_t iSceneID, const std::wstring& strKey, CWorld& world)
{
	return Clone_Entity(iSceneID, strKey, world, nullptr);
}

EntityID CEntityBlueprintRegistry::Clone_Entity(uint32_t iSceneID, const std::wstring& strKey,
	CWorld& world, const void* pArg)
{
	if (iSceneID >= m_iNumScenes)
		return NULL_ENTITY;

	auto& map = m_pBlueprints[iSceneID];
	auto it = map.find(strKey);
	if (it == map.end())
		return NULL_ENTITY;
	return it->second.Spawn(world, pArg);
}

void CEntityBlueprintRegistry::Clear_Scene(uint32_t iSceneID)
{
	if (iSceneID >= m_iNumScenes)
		return;

	m_pBlueprints[iSceneID].clear();
}

std::unique_ptr<CEntityBlueprintRegistry> CEntityBlueprintRegistry::Create(uint32_t iNumScenes)
{
	auto pInst = std::unique_ptr<CEntityBlueprintRegistry>(
		new CEntityBlueprintRegistry());
	if (FAILED(pInst->Initialize(iNumScenes)))
		return nullptr;

	return pInst;
}
