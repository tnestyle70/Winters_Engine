#include "Scene/Scene_Shop.h"

#include "ClientShell/ClientShellBackendService.h"
#include "ClientShell/ClientShellDataStore.h"
#include "GameInstance.h"
#include "GamePlay/ChampionCatalog.h"
#include "Scene/LobbyRosterHelpers.h"
#include "Scene/Scene_MainMenu.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace
{
	constexpr ImageSourceRect kBackButtonRect{ 30.f, 24.f, 170.f, 64.f };

	constexpr u32_t kGridColumns = 6;
	constexpr f32_t kGridLeft = 205.f;
	constexpr f32_t kGridTop = 150.f;
	constexpr f32_t kCellPortraitSize = 120.f;
	constexpr f32_t kCellPriceBarHeight = 28.f;
	constexpr f32_t kCellPitchX = 150.f;
	constexpr f32_t kCellPitchY = 185.f;

	ImageSourceRect GetCellPriceBarRect(const ImageSourceRect& portraitRect)
	{
		return ImageSourceRect{
			portraitRect.fLeft,
			portraitRect.fBottom,
			portraitRect.fRight,
			portraitRect.fBottom + kCellPriceBarHeight
		};
	}
}

std::unique_ptr<CScene_Shop> CScene_Shop::Create()
{
	return std::unique_ptr<CScene_Shop>(new CScene_Shop());
}

bool CScene_Shop::OnEnter()
{
	EnsureLobbyChampionCatalogReady();
	m_ImageUI.Initialize(
		L"Texture/UI/MatchLoadingBackground.png",
		g_iWinSizeX,
		g_iWinSizeY);

	m_strStatus.clear();
	m_uBuiltStorefrontRevision = 0;
	m_bBackRequested = false;

	// 재진입 시 최신 잔액/소유권으로 재동기화한다.
	CClientShellBackendService::Instance().RequestStorefrontSync();
	RebuildCellsFromStorefront();
	return true;
}

void CScene_Shop::OnExit()
{
	m_Cells.clear();
	m_ImageUI.Shutdown();
	m_strStatus.clear();
}

void CScene_Shop::OnUpdate(f32_t /*dt*/)
{
	CClientShellBackendService& service = CClientShellBackendService::Instance();
	service.ProcessCallbacks();
	if (!service.GetStatus().empty())
		m_strStatus = service.GetStatus();

	const CClientShellDataStore& store = CClientShellDataStore::Instance();
	if (m_uBuiltStorefrontRevision != store.GetStorefrontRevision())
		RebuildCellsFromStorefront();

	if (m_ImageUI.WasSourceRectClicked(kBackButtonRect))
		m_bBackRequested = true;

	if (!m_bBackRequested && !service.IsPurchaseInFlight())
	{
		for (const ShopCell& cell : m_Cells)
		{
			if (!m_ImageUI.WasSourceRectClicked(cell.rect))
				continue;

			RequestPurchaseByCell(cell);
			break;
		}
	}

	if (m_bBackRequested)
	{
		m_bBackRequested = false;
		ChangeToMainMenu();
		return;
	}
}

void CScene_Shop::OnLateUpdate(f32_t /*dt*/)
{}

void CScene_Shop::OnRender()
{
	if (!m_ImageUI.Begin())
		return;

	m_ImageUI.DrawBackground();

	// 좌상단 뒤로가기 버튼
	m_ImageUI.DrawSourceRect(kBackButtonRect, Vec4(0.02f, 0.06f, 0.10f, 0.85f));
	m_ImageUI.DrawSourceRectOutline(kBackButtonRect, Vec4(0.68f, 0.52f, 0.18f, 0.95f), 1.5f);

	for (const ShopCell& cell : m_Cells)
	{
		const ShellStoreItem* pItem =
			CClientShellDataStore::Instance().FindStoreItemByContentKey(cell.strContentKey);
		const bool_t bOwned = pItem && pItem->bOwned;

		if (cell.pTexture)
			m_ImageUI.DrawSourceImage(cell.pTexture.get(), cell.rect, Vec4(1.f, 1.f, 1.f, 1.f));
		else
			m_ImageUI.DrawSourceRect(cell.rect, Vec4(0.05f, 0.05f, 0.08f, 0.9f));

		const ImageSourceRect priceRect = GetCellPriceBarRect(cell.rect);
		m_ImageUI.DrawSourceRect(priceRect, Vec4(0.01f, 0.04f, 0.08f, 0.88f));

		if (bOwned)
		{
			// 이미 구매한 챔피언 = 반투명 오버레이로 구분
			m_ImageUI.DrawSourceRect(cell.rect, Vec4(0.f, 0.f, 0.f, 0.55f));
			m_ImageUI.DrawSourceRectOutline(cell.rect, Vec4(0.30f, 0.80f, 0.45f, 0.95f), 2.f);
		}
		else
		{
			m_ImageUI.DrawSourceRectOutline(cell.rect, Vec4(0.68f, 0.52f, 0.18f, 0.85f), 1.5f);
		}
		m_ImageUI.DrawSourceRectOutline(priceRect, Vec4(0.68f, 0.52f, 0.18f, 0.65f), 1.f);
	}

	m_ImageUI.End();
}

void CScene_Shop::OnImGui()
{
	ImDrawList* pDraw = ImGui::GetForegroundDrawList();
	if (!pDraw)
		return;

	const CClientShellDataStore& store = CClientShellDataStore::Instance();
	const bool_t bSyncReady = store.IsInitialSyncReady();

	auto drawSourceText = [this, pDraw](f32_t fX, f32_t fY, ImU32 color, const char* pText)
	{
		ImageScreenRect screenRect{};
		if (m_ImageUI.SourceRectToScreen(ImageSourceRect{ fX, fY, fX + 1.f, fY + 1.f }, screenRect))
			pDraw->AddText(ImVec2(screenRect.fX, screenRect.fY), color, pText);
	};

	drawSourceText(46.f, 34.f, IM_COL32(235, 220, 180, 255), "← 뒤로가기");
	drawSourceText(560.f, 34.f, IM_COL32(240, 230, 200, 255), "챔피언 상점");

	char szBuffer[160]{};
	if (bSyncReady)
		sprintf_s(szBuffer, sizeof(szBuffer), "보유 RP: %d", store.GetProfile().iRP);
	else
		sprintf_s(szBuffer, sizeof(szBuffer), "상점 동기화 중...");
	drawSourceText(1050.f, 34.f, IM_COL32(120, 210, 255, 255), szBuffer);

	for (const ShopCell& cell : m_Cells)
	{
		const ShellStoreItem* pItem = store.FindStoreItemByContentKey(cell.strContentKey);
		if (!pItem)
			continue;

		const ImageSourceRect priceRect = GetCellPriceBarRect(cell.rect);
		sprintf_s(szBuffer, sizeof(szBuffer), "%s", pItem->strName.c_str());
		drawSourceText(cell.rect.fLeft + 4.f, cell.rect.fTop + 2.f, IM_COL32(255, 255, 255, 230), szBuffer);

		if (pItem->bOwned)
			sprintf_s(szBuffer, sizeof(szBuffer), "보유 중");
		else
			sprintf_s(szBuffer, sizeof(szBuffer), "%d RP", pItem->iPriceRP);
		drawSourceText(priceRect.fLeft + 6.f, priceRect.fTop + 5.f,
			pItem->bOwned ? IM_COL32(140, 235, 170, 255) : IM_COL32(255, 214, 112, 255), szBuffer);
	}

	if (!m_strStatus.empty())
		drawSourceText(30.f, 690.f, IM_COL32(255, 235, 160, 255), m_strStatus.c_str());
}

void CScene_Shop::RebuildCellsFromStorefront()
{
	m_Cells.clear();

	const std::vector<ShellStoreItem>& items = CClientShellDataStore::Instance().GetStoreItems();
	m_uBuiltStorefrontRevision = CClientShellDataStore::Instance().GetStorefrontRevision();

	IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();

	u32_t cellIndex = 0;
	for (const ShellStoreItem& item : items)
	{
		if (item.strItemType != "champion" || item.strContentKey.empty())
			continue;

		const ChampionCatalogEntry* pEntry =
			CChampionCatalog::Instance().FindByContentKey(item.strContentKey);
		if (!pEntry)
			continue;

		const u32_t col = cellIndex % kGridColumns;
		const u32_t row = cellIndex / kGridColumns;
		++cellIndex;

		ShopCell cell{};
		cell.strContentKey = item.strContentKey;
		cell.champion = pEntry->id;
		cell.rect = ImageSourceRect{
			kGridLeft + static_cast<f32_t>(col) * kCellPitchX,
			kGridTop + static_cast<f32_t>(row) * kCellPitchY,
			kGridLeft + static_cast<f32_t>(col) * kCellPitchX + kCellPortraitSize,
			kGridTop + static_cast<f32_t>(row) * kCellPitchY + kCellPortraitSize
		};

		if (pDevice && cell.champion != eChampion::END)
		{
			if (const wchar_t* pPath = GetRosterChampionPortraitPath(cell.champion))
			{
				cell.pTexture = Engine::CTexture::Create(
					pDevice,
					std::wstring(pPath),
					Engine::eTexSamplerMode::Clamp,
					Engine::eTexColorSpace::IgnoreSRGB);
			}
		}

		m_Cells.emplace_back(std::move(cell));
	}
}

void CScene_Shop::RequestPurchaseByCell(const ShopCell& cell)
{
	const ShellStoreItem* pItem =
		CClientShellDataStore::Instance().FindStoreItemByContentKey(cell.strContentKey);
	if (!pItem)
		return;

	if (pItem->bOwned)
	{
		m_strStatus = "이미 구매한 상품입니다.";
		return;
	}

	if (!CClientShellDataStore::Instance().IsInitialSyncReady())
	{
		m_strStatus = "상점 동기화 중입니다. 잠시 후 다시 시도하세요";
		return;
	}

	CClientShellBackendService::Instance().RequestPurchase(pItem->strItemID);
}

void CScene_Shop::ChangeToMainMenu()
{
	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::MainMenu),
		CScene_MainMenu::Create());
}
