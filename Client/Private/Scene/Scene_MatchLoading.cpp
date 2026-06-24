#include "Scene/Scene_MatchLoading.h"

#include "GameInstance.h"
#include "Network/Client/GameSessionClient.h"
#include "Scene/LobbyRosterHelpers.h"

#include <Windows.h>

#include <array>
#include <string>
#include <utility>

namespace
{
	constexpr f32_t kChampionCardW = 154.f;
	constexpr f32_t kChampionCardH = 280.f;
	constexpr f32_t kChampionCardGap = 20.f;
	constexpr f32_t kRedTeamTop = 38.f;
	constexpr f32_t kBlueTeamTop = 402.f;

	const Vec4 kRedFrameColor = Vec4(0.78f, 0.22f, 0.18f, 0.95f);
	const Vec4 kBlueFrameColor = Vec4(0.16f, 0.46f, 0.92f, 0.95f);
	const Vec4 kCardShadowColor = Vec4(0.f, 0.f, 0.f, 0.48f);

	bool_t HasRosterEntries(const GameContext& context)
	{
		for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
		{
			if (IsSlotOccupied(context.Roster[i]))
				return true;
		}
		return false;
	}
}

bool CScene_MatchLoading::OnEnter()
{
	m_fElapsed = 0.f;
	m_bTransitioning = false;
	BuildChampionCards();
	m_pLoader = Client::CLoader::Create(eSceneID::InGame, m_onLoaded);
	return true;
}

void CScene_MatchLoading::OnExit()
{
	m_pLoader.reset();
	ShutdownMatchLoadingTextures();
}

void CScene_MatchLoading::OnUpdate(f32_t dt)
{
	if (m_bTransitioning)
		return;

	GameContext& context = CGameInstance::Get()->Get_GameContext();
	CGameSessionClient& session = CGameSessionClient::Instance();
	const bool_t bNetworkLoading = context.bUseNetworkRoster && session.IsConnected();

	m_fElapsed += dt;

	if (m_pLoader && !m_pLoader->IsFinished())
		m_pLoader->TickMainThreadLoad();

	if (bNetworkLoading)
	{
		session.Pump();

		if (session.HasLobbyState())
		{
			session.CopyLobbyToGameContext(context);
			if (!m_bChampionCardsBuilt && HasRosterEntries(context))
				BuildChampionCards();
		}
	}

	if (m_fElapsed < m_fDuration)
		return;

	if (m_pLoader && !m_pLoader->IsFinished())
		return;

	if (!m_pLoader && !m_onLoaded)
		return;

	m_bTransitioning = true;
	auto pNext = m_pLoader ? m_pLoader->Build_NextScene() : m_onLoaded();
	m_pLoader.reset();

	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::InGame),
		std::move(pNext));
}

void CScene_MatchLoading::OnRender()
{
	if (!m_ImageUI.Begin())
		return;

	m_ImageUI.DrawBackground();
	RenderTeamCards(5u, kGameRosterSlotCount, kRedTeamTop, kRedFrameColor);
	RenderTeamCards(0u, 5u, kBlueTeamTop, kBlueFrameColor);
	m_ImageUI.End();
}

void CScene_MatchLoading::OnImGui()
{
}

std::unique_ptr<CScene_MatchLoading> CScene_MatchLoading::Create(
	LoadCallback onLoaded,
	f32_t fDuration)
{
	auto pInst = std::unique_ptr<CScene_MatchLoading>(new CScene_MatchLoading());
	pInst->m_onLoaded = std::move(onLoaded);
	pInst->m_fDuration = fDuration;
	return pInst;
}

void CScene_MatchLoading::BuildChampionCards()
{
	ShutdownMatchLoadingTextures();

	m_ImageUI.Initialize(
		L"Client/Bin/Resource/Texture/UI/MatchLoadingBackground.png",
		g_iWinSizeX,
		g_iWinSizeY);

	EnsureLobbyChampionCatalogReady();

	IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
	if (!pDevice)
		return;

	const GameContext& context = CGameInstance::Get()->Get_GameContext();
	if (!HasRosterEntries(context))
		return;

	m_bChampionCardsBuilt = true;

	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		const GameRosterSlot& slot = context.Roster[i];
		if (!IsSlotOccupied(slot) || !IsRosterChampionSupported(slot.champion))
			continue;

		const wchar_t* pPath = GetRosterChampionLoadscreenPath(slot.champion);
		if (!pPath)
			continue;

		MatchLoadingCard card{};
		card.iSlotId = i;
		card.champion = slot.champion;
		card.pTexture = Engine::CTexture::Create(
			pDevice,
			std::wstring(pPath),
			Engine::eTexSamplerMode::Clamp,
			Engine::eTexColorSpace::IgnoreSRGB);

		if (card.pTexture)
			m_ChampionCards.emplace_back(std::move(card));
	}
}

void CScene_MatchLoading::ShutdownMatchLoadingTextures()
{
	m_ChampionCards.clear();
	m_ImageUI.Shutdown();
	m_bChampionCardsBuilt = false;
}

void CScene_MatchLoading::RenderTeamCards(
	u32_t iBeginSlot,
	u32_t iEndSlot,
	f32_t fTop,
	const Vec4& vFrameColor)
{
	std::array<const MatchLoadingCard*, 5> cards{};
	u32_t iCount = 0;

	for (const MatchLoadingCard& card : m_ChampionCards)
	{
		if (card.iSlotId >= iBeginSlot &&
			card.iSlotId < iEndSlot &&
			iCount < static_cast<u32_t>(cards.size()))
		{
			cards[iCount++] = &card;
		}
	}

	if (iCount == 0)
		return;

	const f32_t fTotalW =
		static_cast<f32_t>(iCount) * kChampionCardW +
		static_cast<f32_t>(iCount - 1u) * kChampionCardGap;
	const f32_t fLeft = (static_cast<f32_t>(g_iWinSizeX) - fTotalW) * 0.5f;

	for (u32_t i = 0; i < iCount; ++i)
	{
		const f32_t fX = fLeft + static_cast<f32_t>(i) * (kChampionCardW + kChampionCardGap);
		const ImageSourceRect shadowRect{
			fX + 6.f,
			fTop + 8.f,
			fX + kChampionCardW + 6.f,
			fTop + kChampionCardH + 8.f
		};
		const ImageSourceRect cardRect{
			fX,
			fTop,
			fX + kChampionCardW,
			fTop + kChampionCardH
		};

		m_ImageUI.DrawSourceRect(shadowRect, kCardShadowColor);
		m_ImageUI.DrawSourceImage(cards[i]->pTexture.get(), cardRect, Vec4(1.f, 1.f, 1.f, 1.f));
		m_ImageUI.DrawSourceRectOutline(cardRect, vFrameColor, 2.f);
	}
}
