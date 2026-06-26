#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

#include <functional>
#include <vector>

class CWorld;

//Entity Template - CWorld + EntityID를 받아 컴포넌트를 추가하는 Funtion
//Composition으로  I/O 없는 복제 구현 
class WINTERS_ENGINE CEntityBlueprint
{
public:
	using Installer = std::function<void(CWorld&, EntityID)>;
	using ArgsInstaller = std::function<void(CWorld&, EntityID, const void*)>;
	
	CEntityBlueprint() = default;

	//Entity는 Transform + Actor + SkillState를 갖는다.
	CEntityBlueprint& Add(Installer fn)
	{
		m_vecInstallers.push_back(std::move(fn));
		return *this;
	}
	CEntityBlueprint& AddArgs(ArgsInstaller fn)
	{
		m_vecArgsInstallers.push_back(std::move(fn));
		return *this;
	}
	//world 안에 새 Entity 생성 후 모든 Installer 실행 (pArg=nullptr)
	EntityID Spawn(CWorld& world) const { return Spawn(world, nullptr); }
	//수업 Clone(void* pArg) 관례 — per-instance 인자 전달
	EntityID Spawn(CWorld& world, const void* pArg) const;
private: 
	std::vector<Installer> m_vecInstallers{};
	std::vector<ArgsInstaller> m_vecArgsInstallers{};
};
