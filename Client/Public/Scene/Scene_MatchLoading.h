#pragma once
#include "Defines.h"
#include "GameObject/ChampionDef.h"
#include "IScene.h"
#include "Resource/Texture.h"
#include "Scene/Loader.h"
#include "UI/ImageScenePresenter.h"
#include <functional>
#include <memory>
#include <vector>

class CScene_MatchLoading final : public IScene
{
public:
    ~CScene_MatchLoading() = default;

    using LoadCallback = std::function<std::unique_ptr<IScene>()>;

    bool OnEnter() override;
    void OnExit()  override;
    void OnUpdate(f32_t dt) override;
    void OnLateUpdate(f32_t /*dt*/) override {}
    void OnRender() override;
    void OnImGui() override;

    static std::unique_ptr<CScene_MatchLoading> Create(LoadCallback onLoaded,
        f32_t fDuration = 3.f);

private:
    CScene_MatchLoading() = default;

    struct MatchLoadingCard
    {
        u32_t iSlotId = 0;
        eChampion champion = eChampion::END;
        std::unique_ptr<Engine::CTexture> pTexture{};
    };

    void BuildChampionCards();
    void ShutdownMatchLoadingTextures();
    void RenderTeamCards(u32_t iBeginSlot, u32_t iEndSlot,
        f32_t fTop, const Vec4& vFrameColor);

    LoadCallback m_onLoaded{};
    std::unique_ptr<Client::CLoader> m_pLoader{};
    f32_t m_fElapsed = 0.f;
    f32_t m_fDuration = 3.f;
    bool_t m_bTransitioning = false;
    CImageScenePresenter m_ImageUI{};
    std::vector<MatchLoadingCard> m_ChampionCards{};
    bool_t m_bChampionCardsBuilt = false;
};
