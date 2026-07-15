#pragma once
#include "IScene.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "UI/ImageScenePresenter.h"

#include <memory>
#include <string>
#include <vector>

// 챔피언 RP 상점 씬 — MainMenu 상점 버튼으로 진입.
// 상품/가격/소유권의 권위는 Services storefront 스냅샷(ClientShellDataStore)이다.
class CScene_Shop final : public IScene
{
public:
	static std::unique_ptr<CScene_Shop> Create();
	~CScene_Shop() override = default;

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t dt) override;
	void OnLateUpdate(f32_t dt) override;
	void OnRender() override;
	void OnImGui() override;

private:
	CScene_Shop() = default;
	CScene_Shop(const CScene_Shop&) = delete;
	CScene_Shop& operator=(const CScene_Shop&) = delete;

	struct ShopCell
	{
		std::string strContentKey{};
		eChampion champion = eChampion::END;
		ImageSourceRect rect{};
		std::unique_ptr<Engine::CTexture> pTexture{};
	};

	void RebuildCellsFromStorefront();
	void RequestPurchaseByCell(const ShopCell& cell);
	void ChangeToMainMenu();

	CImageScenePresenter m_ImageUI{};
	std::vector<ShopCell> m_Cells{};
	std::string m_strStatus{};
	u32_t m_uBuiltStorefrontRevision = 0;
	bool_t m_bBackRequested = false;
};
