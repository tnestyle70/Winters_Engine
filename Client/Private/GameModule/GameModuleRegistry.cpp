#include "GameModule/GameModuleRegistry.h"

#include "GameModule/LOL/LOLGameModule.h"

#include <Windows.h>
#include <utility>

namespace
{
	class CPlaceholderGameModule final : public IGameModule
	{
	public:
		explicit CPlaceholderGameModule(eGameProduct product)
			: m_eProduct(product)
		{
		}

		~CPlaceholderGameModule() override = default;

		eGameProduct GetProductID() const override { return m_eProduct; }
		const char* GetDisplayName() const override { return GetGameProductName(m_eProduct); }
		bool_t IsAvailable() const override { return false; }

		bool_t InitializeClient(const GameLaunchConfig& /*config*/) override { return false; }
		void ShutdownClient() override {}

		eSceneID GetInitialSceneID() const override { return eSceneID::End; }
		std::unique_ptr<IScene> CreateInitialScene() override { return nullptr; }

	private:
		eGameProduct m_eProduct = eGameProduct::None;
	};
}

CGameModuleRegistry& CGameModuleRegistry::Instance()
{
	static CGameModuleRegistry s_Instance;
	return s_Instance;
}

void CGameModuleRegistry::Register(std::unique_ptr<IGameModule> pModule)
{
	if (!pModule)
		return;

	const eGameProduct eProduct = pModule->GetProductID();
	if (IsRegistered(eProduct))
		return;

	m_Modules.emplace_back(std::move(pModule));
}

void CGameModuleRegistry::RegisterDefaults()
{
	Register(CLOLGameModule::Create());
	Register(std::unique_ptr<IGameModule>(new CPlaceholderGameModule(eGameProduct::EldenRing)));
	Register(std::unique_ptr<IGameModule>(new CPlaceholderGameModule(eGameProduct::ClassServant)));
}

IGameModule* CGameModuleRegistry::Find(eGameProduct product) const
{
	for (const auto& pModule : m_Modules)
	{
		if (pModule && pModule->GetProductID() == product)
			return pModule.get();
	}

	return nullptr;
}

bool_t CGameModuleRegistry::Activate(eGameProduct product, const GameLaunchConfig& config)
{
	IGameModule* pModule = Find(product);
	if (!pModule)
	{
		OutputDebugStringA("[GameModuleRegistry] Module not found.\n");
		return false;
	}

	if (!pModule->IsAvailable())
	{
		OutputDebugStringA("[GameModuleRegistry] Module is not available.\n");
		return false;
	}

	if (m_pActiveModule && m_pActiveModule != pModule)
		m_pActiveModule->ShutdownClient();

	if (!pModule->InitializeClient(config))
		return false;

	m_pActiveModule = pModule;
	return true;
}

void CGameModuleRegistry::ShutdownActiveModule()
{
	if (!m_pActiveModule)
		return;

	m_pActiveModule->ShutdownClient();
	m_pActiveModule = nullptr;
}

eGameProduct CGameModuleRegistry::GetActiveProduct() const
{
	return m_pActiveModule ? m_pActiveModule->GetProductID() : eGameProduct::None;
}

bool_t CGameModuleRegistry::IsRegistered(eGameProduct product) const
{
	return Find(product) != nullptr;
}
