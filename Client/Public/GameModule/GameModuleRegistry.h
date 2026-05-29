#pragma once
#include "Defines.h"
#include "GameModule/IGameModule.h"

#include <memory>
#include <vector>

class CGameModuleRegistry final
{
public:
	static CGameModuleRegistry& Instance();

	void Register(std::unique_ptr<IGameModule> pModule);
	void RegisterDefaults();

	IGameModule* Find(eGameProduct product) const;
	const std::vector<std::unique_ptr<IGameModule>>& GetModules() const { return m_Modules; }

	bool_t Activate(eGameProduct product, const GameLaunchConfig& config);
	void ShutdownActiveModule();

	IGameModule* GetActiveModule() const { return m_pActiveModule; }
	eGameProduct GetActiveProduct() const;

private:
	CGameModuleRegistry() = default;
	~CGameModuleRegistry() = default;
	CGameModuleRegistry(const CGameModuleRegistry&) = delete;
	CGameModuleRegistry& operator=(const CGameModuleRegistry&) = delete;
	
	bool_t IsRegistered(eGameProduct product) const;

	std::vector<std::unique_ptr<IGameModule>> m_Modules{};
	IGameModule* m_pActiveModule = nullptr;

};
