#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/Systems/EntityBlueprint.h"

#include <memory>
#include <unordered_map>
#include <string>

class CEntityBlueprintRegistry
{
public:
	~CEntityBlueprintRegistry() = default;

	//Scene의 개수만큼 map array 확보
	HRESULT Initialize(uint32_t iNumLevels);

	//Add Prototype과 대응
	HRESULT Add_Blueprint(uint32_t iSceneID, const std::wstring& strKey,
		CEntityBlueprint blueprint);

	//Clone Prototype과 대응 (pArg=nullptr)
	EntityID Clone_Entity(uint32_t iSceneID, const std::wstring& strKey,
		CWorld& world);

	//수업 Clone(void* pArg) 관례 — per-instance 초기화
	EntityID Clone_Entity(uint32_t iSceneID, const std::wstring& strKey,
		CWorld& world, const void* pArg);

	//Clear Resource에 대응
	void Clear_Scene(uint32_t iSceneID);

	static std::unique_ptr<CEntityBlueprintRegistry> Create(uint32_t iNumScenes);

private:
	CEntityBlueprintRegistry() = default;

	uint32_t m_iNumScenes = 0;
	std::unique_ptr<std::unordered_map<std::wstring, CEntityBlueprint>[]> m_pBlueprints;
};