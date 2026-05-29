Session - LOL CustomMode와 ChampionSelect를 서버 동기화 BanPick 플로우로 분리한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
enum class eRoomPhase : u8_t
{
    Lobby,
    Loading,
    InGame,
};
```

아래로 교체:

```cpp
enum class eRoomPhase : u8_t
{
    SeatSelect,
    ChampionSelect,
    Loading,
    InGame,
};
```

기존 코드:

```cpp
bool TryStartGame(u32_t sessionId);
```

아래로 교체:

```cpp
bool TryAdvanceToChampionSelect(u32_t sessionId);
bool TryStartGame(u32_t sessionId);
```

기존 코드:

```cpp
eRoomPhase m_roomPhase = eRoomPhase::Lobby;
```

아래로 교체:

```cpp
eRoomPhase m_roomPhase = eRoomPhase::SeatSelect;
```

1-2. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
if (m_roomPhase != eRoomPhase::Lobby)
{
    if (m_roomPhase == eRoomPhase::Loading &&
        command->kind() == Shared::Schema::LobbyCommandKind::SetReady)
```

아래로 교체:

```cpp
const bool_t bLobbyEditablePhase =
    m_roomPhase == eRoomPhase::SeatSelect ||
    m_roomPhase == eRoomPhase::ChampionSelect;

if (!bLobbyEditablePhase)
{
    if (m_roomPhase == eRoomPhase::Loading &&
        command->kind() == Shared::Schema::LobbyCommandKind::SetReady)
```

기존 코드:

```cpp
case Shared::Schema::LobbyCommandKind::JoinSlot:
    bChanged = TryJoinSlot(sessionId, command->slotId());
    break;
case Shared::Schema::LobbyCommandKind::LeaveSlot:
    bChanged = TryLeaveSlot(sessionId);
    break;
case Shared::Schema::LobbyCommandKind::PickChampion:
    bChanged = TryPickChampion(sessionId, static_cast<eChampion>(command->championId()));
    break;
case Shared::Schema::LobbyCommandKind::SetBotChampion:
    bChanged = TrySetBotChampion(
        sessionId,
        command->slotId(),
        static_cast<eChampion>(command->championId()));
    break;
case Shared::Schema::LobbyCommandKind::StartGame:
    if (TryStartGame(sessionId))
        return;
    break;
```

아래로 교체:

```cpp
case Shared::Schema::LobbyCommandKind::JoinSlot:
    if (m_roomPhase != eRoomPhase::SeatSelect)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            command->kind(),
            command->slotId(),
            static_cast<eChampion>(command->championId()),
            "slot changes are only allowed in seat select"));
        break;
    }
    bChanged = TryJoinSlot(sessionId, command->slotId());
    break;
case Shared::Schema::LobbyCommandKind::LeaveSlot:
    if (m_roomPhase != eRoomPhase::SeatSelect)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            command->kind(),
            command->slotId(),
            static_cast<eChampion>(command->championId()),
            "slot changes are only allowed in seat select"));
        break;
    }
    bChanged = TryLeaveSlot(sessionId);
    break;
case Shared::Schema::LobbyCommandKind::PickChampion:
    if (m_roomPhase != eRoomPhase::ChampionSelect)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            command->kind(),
            command->slotId(),
            static_cast<eChampion>(command->championId()),
            "player champion pick is only allowed in champion select"));
        break;
    }
    bChanged = TryPickChampion(sessionId, static_cast<eChampion>(command->championId()));
    break;
case Shared::Schema::LobbyCommandKind::SetBotChampion:
    bChanged = TrySetBotChampion(
        sessionId,
        command->slotId(),
        static_cast<eChampion>(command->championId()));
    break;
case Shared::Schema::LobbyCommandKind::StartGame:
    if (m_roomPhase == eRoomPhase::SeatSelect)
    {
        if (TryAdvanceToChampionSelect(sessionId))
            return;
    }
    else if (m_roomPhase == eRoomPhase::ChampionSelect)
    {
        if (TryStartGame(sessionId))
            return;
    }
    else
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            command->kind(),
            command->slotId(),
            static_cast<eChampion>(command->championId()),
            "room is not ready to advance"));
    }
    break;
```

기존 코드:

```cpp
if (m_roomPhase == eRoomPhase::Lobby)
{
    OnLobbyJoin(sessionId);
    ++m_lobbyRevision;
    SendHelloToSessionLocked(sessionId, NULL_NET_ENTITY, eChampion::END, 0);
    BroadcastLobbyStateLocked();
```

아래로 교체:

```cpp
if (m_roomPhase == eRoomPhase::SeatSelect)
{
    OnLobbyJoin(sessionId);
    ++m_lobbyRevision;
    SendHelloToSessionLocked(sessionId, NULL_NET_ENTITY, eChampion::END, 0);
    BroadcastLobbyStateLocked();
```

기존 코드:

```cpp
if (m_roomPhase == eRoomPhase::Lobby)
    BroadcastLobbyStateLocked();
```

아래로 교체:

```cpp
if (m_roomPhase == eRoomPhase::SeatSelect ||
    m_roomPhase == eRoomPhase::ChampionSelect)
{
    BroadcastLobbyStateLocked();
}
```

`bool CGameRoom::TryStartGame(u32_t sessionId)` 바로 위에 아래로 추가:

```cpp
bool CGameRoom::TryAdvanceToChampionSelect(u32_t sessionId)
{
    if (sessionId != m_hostSessionId && !m_bAllPlayersCanEditBots)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StartGame,
            kInvalidGameRosterSlot,
            eChampion::END,
            "only host can advance to champion select"));
        return false;
    }

    bool_t bHasHuman = false;
    for (const LobbySlotState& slot : m_lobbySlots)
    {
        if (slot.bHuman)
        {
            bHasHuman = true;
            break;
        }
    }

    if (!bHasHuman)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::StartGame,
            kInvalidGameRosterSlot,
            eChampion::END,
            "no human player"));
        return false;
    }

    m_roomPhase = eRoomPhase::ChampionSelect;
    ++m_lobbyRevision;

    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::StartGame,
        kInvalidGameRosterSlot,
        eChampion::END,
        "champion select started"));

    BroadcastLobbyStateLocked();
    return true;
}
```

`bool CGameRoom::TrySetBotChampion(u32_t sessionId, u8_t slotId, eChampion champion)` 안에서 기존 코드:

```cpp
if (slot.bHuman)
{
    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "reject",
        sessionId,
        Shared::Schema::LobbyCommandKind::SetBotChampion,
        slotId,
        champion,
        "slot is occupied by a player"));
    return false;
}
```

아래로 추가:

```cpp
if (m_roomPhase == eRoomPhase::ChampionSelect)
{
    if (!slot.bBot)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotChampion,
            slotId,
            champion,
            "only existing bot champion can be changed in champion select"));
        return false;
    }

    if (champion == eChampion::END || champion == eChampion::NONE)
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            Shared::Schema::LobbyCommandKind::SetBotChampion,
            slotId,
            champion,
            "bot cannot be removed in champion select"));
        return false;
    }
}
```

기존 코드:

```cpp
const auto phase = (m_roomPhase == eRoomPhase::InGame)
    ? Shared::Schema::LobbyPhase::InGame
    : ((m_roomPhase == eRoomPhase::Loading)
        ? Shared::Schema::LobbyPhase::Starting
        : Shared::Schema::LobbyPhase::ChampionSelect);
```

아래로 교체:

```cpp
Shared::Schema::LobbyPhase phase = Shared::Schema::LobbyPhase::None;
switch (m_roomPhase)
{
case eRoomPhase::SeatSelect:
    phase = Shared::Schema::LobbyPhase::SeatSelect;
    break;
case eRoomPhase::ChampionSelect:
    phase = Shared::Schema::LobbyPhase::ChampionSelect;
    break;
case eRoomPhase::Loading:
    phase = Shared::Schema::LobbyPhase::Starting;
    break;
case eRoomPhase::InGame:
    phase = Shared::Schema::LobbyPhase::InGame;
    break;
}
```

1-3. C:/Users/user/Desktop/Winters/Client/Public/Defines.h

기존 코드:

```cpp
MainMenu,
BanPick,
Shop,
```

아래로 교체:

```cpp
MainMenu,
CustomMode,
BanPick,
Shop,
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/GameModule/LOL/LOLGameModule.cpp

기존 코드:

```cpp
#include "Scene/Scene_BanPick.h"
#include "Scene/Scene_Loading.h"
```

아래로 교체:

```cpp
#include "Scene/Scene_CustomMode.h"
#include "Scene/Scene_Loading.h"
```

기존 코드:

```cpp
return CScene_Loading::Create(
    eSceneID::BanPick,
    []() -> std::unique_ptr<IScene>
    {
        return CScene_BanPick::Create();
    });
```

아래로 교체:

```cpp
return CScene_Loading::Create(
    eSceneID::CustomMode,
    []() -> std::unique_ptr<IScene>
    {
        return CScene_CustomMode::Create();
    });
```

1-5. C:/Users/user/Desktop/Winters/Client/Public/UI/ImageScenePresenter.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

#include <memory>

class CUIRenderer;

namespace Engine
{
    class CTexture;
}

struct ImageSourceRect
{
    f32_t fLeft = 0.f;
    f32_t fTop = 0.f;
    f32_t fRight = 0.f;
    f32_t fBottom = 0.f;
};

class CImageScenePresenter final
{
public:
    bool Initialize(const wchar_t* pTexturePath, u32_t sourceWidth, u32_t sourceHeight);
    void Shutdown();
    void Render();
    bool WasSourceRectClicked(const ImageSourceRect& rect) const;
    bool ScreenToSource(f32_t screenX, f32_t screenY, f32_t& outX, f32_t& outY) const;

private:
    void UpdateLayout() const;

    std::unique_ptr<CUIRenderer> m_pRenderer{};
    std::unique_ptr<Engine::CTexture> m_pTexture{};
    u32_t m_iSourceWidth = 0;
    u32_t m_iSourceHeight = 0;
    mutable f32_t m_fDrawX = 0.f;
    mutable f32_t m_fDrawY = 0.f;
    mutable f32_t m_fDrawW = 0.f;
    mutable f32_t m_fDrawH = 0.f;
};
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/UI/ImageScenePresenter.cpp

새 파일:

```cpp
#include "UI/ImageScenePresenter.h"

#include "Core/CInput.h"
#include "Framework/CEngineApp.h"
#include "Renderer/UIRenderer.h"
#include "Resource/Texture.h"

#include <algorithm>
#include <string>

bool CImageScenePresenter::Initialize(const wchar_t* pTexturePath, u32_t sourceWidth, u32_t sourceHeight)
{
    Shutdown();

    if (!pTexturePath || sourceWidth == 0 || sourceHeight == 0)
        return false;

    IRHIDevice& device = CEngineApp::Get().GetDevice();
    m_pRenderer = CUIRenderer::Create(&device);
    m_pTexture = Engine::CTexture::Create(
        &device,
        std::wstring(pTexturePath),
        Engine::eTexSamplerMode::Clamp);

    m_iSourceWidth = sourceWidth;
    m_iSourceHeight = sourceHeight;
    return m_pRenderer && m_pRenderer->IsReady() && m_pTexture;
}
```

아래로 추가:

```cpp
void CImageScenePresenter::Shutdown()
{
    m_pTexture.reset();
    m_pRenderer.reset();
    m_iSourceWidth = 0;
    m_iSourceHeight = 0;
}

void CImageScenePresenter::Render()
{
    if (!m_pRenderer || !m_pTexture || m_iSourceWidth == 0 || m_iSourceHeight == 0)
        return;

    auto& window = CEngineApp::Get().GetWindow();
    const u32_t width = static_cast<u32_t>(window.GetWidth());
    const u32_t height = static_cast<u32_t>(window.GetHeight());

    UpdateLayout();

    m_pRenderer->Begin(width, height);
    m_pRenderer->DrawImage(
        m_pTexture->GetNativeSRV(),
        m_fDrawX,
        m_fDrawY,
        m_fDrawW,
        m_fDrawH,
        Vec4(0.f, 0.f, 1.f, 1.f),
        Vec4(1.f, 1.f, 1.f, 1.f));
    m_pRenderer->End();
}
```

아래로 추가:

```cpp
bool CImageScenePresenter::WasSourceRectClicked(const ImageSourceRect& rect) const
{
    if (!CInput::Get().IsLButtonPressed())
        return false;

    f32_t sourceX = 0.f;
    f32_t sourceY = 0.f;
    if (!ScreenToSource(
        static_cast<f32_t>(CInput::Get().GetMouseX()),
        static_cast<f32_t>(CInput::Get().GetMouseY()),
        sourceX,
        sourceY))
    {
        return false;
    }

    return sourceX >= rect.fLeft &&
        sourceX <= rect.fRight &&
        sourceY >= rect.fTop &&
        sourceY <= rect.fBottom;
}

bool CImageScenePresenter::ScreenToSource(
    f32_t screenX,
    f32_t screenY,
    f32_t& outX,
    f32_t& outY) const
{
    if (m_iSourceWidth == 0 || m_iSourceHeight == 0)
        return false;

    UpdateLayout();

    if (screenX < m_fDrawX || screenX > m_fDrawX + m_fDrawW ||
        screenY < m_fDrawY || screenY > m_fDrawY + m_fDrawH)
    {
        return false;
    }

    outX = (screenX - m_fDrawX) * static_cast<f32_t>(m_iSourceWidth) / m_fDrawW;
    outY = (screenY - m_fDrawY) * static_cast<f32_t>(m_iSourceHeight) / m_fDrawH;
    return true;
}

void CImageScenePresenter::UpdateLayout() const
{
    auto& window = CEngineApp::Get().GetWindow();
    const f32_t screenW = static_cast<f32_t>(window.GetWidth());
    const f32_t screenH = static_cast<f32_t>(window.GetHeight());
    const f32_t scale = std::min(
        screenW / static_cast<f32_t>(m_iSourceWidth),
        screenH / static_cast<f32_t>(m_iSourceHeight));

    m_fDrawW = static_cast<f32_t>(m_iSourceWidth) * scale;
    m_fDrawH = static_cast<f32_t>(m_iSourceHeight) * scale;
    m_fDrawX = (screenW - m_fDrawW) * 0.5f;
    m_fDrawY = (screenH - m_fDrawH) * 0.5f;
}
```

1-7. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_CustomMode.h

새 파일:

```cpp
#pragma once

#include "IScene.h"
#include "GameContext.h"
#include "UI/ImageScenePresenter.h"

#include <memory>

class CScene_CustomMode final : public IScene
{
private:
    CScene_CustomMode() = default;

public:
    ~CScene_CustomMode() override = default;

    bool OnEnter() override;
    void OnExit() override;
    void OnUpdate(f32_t dt) override;
    void OnLateUpdate(f32_t dt) override;
    void OnRender() override;
    void OnImGui() override;

    static std::unique_ptr<CScene_CustomMode> Create();

private:
    void HandleInput();
    void HandleServerInput();
    void HandleLocalInput();
    void ChangeToChampionSelectScene();
    void StartMatchLoadingScene();

    u8_t ResolveClickedSlot(f32_t sourceX, f32_t sourceY) const;
    bool_t IsGamePlayClicked() const;
    bool_t IsAddBlueBotClicked() const;
    bool_t IsAddRedBotClicked() const;
    bool_t AddBotToFirstEmptySlot(u32_t beginSlot, u32_t endSlot);

    CImageScenePresenter m_ImageUI{};
    bool_t m_bServerLobbyActive = false;
    bool_t m_bSceneTransitionStarted = false;
    u8_t m_SelectedSlotId = 0;
};
```

1-8. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_CustomMode.cpp

새 파일:

```cpp
#include "Scene/Scene_CustomMode.h"

#include "Core/CInput.h"
#include "GameInstance.h"
#include "GameObject/ChampionDef.h"
#include "GamePlay/ChampionCatalog.h"
#include "Network/Client/GameSessionClient.h"
#include "Scene/Scene_BanPick.h"
#include "Scene/Scene_InGame.h"
#include "Scene/Scene_MatchLoading.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/LobbyTypes_generated.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace
{
    static constexpr ImageSourceRect kBlueBotAddRect{ 450.f, 324.f, 636.f, 364.f };
    static constexpr ImageSourceRect kRedBotAddRect{ 1055.f, 266.f, 1241.f, 306.f };
    static constexpr ImageSourceRect kGamePlayRect{ 560.f, 796.f, 760.f, 837.f };
}
```

아래로 추가:

```cpp
std::unique_ptr<CScene_CustomMode> CScene_CustomMode::Create()
{
    return std::unique_ptr<CScene_CustomMode>(new CScene_CustomMode());
}

bool CScene_CustomMode::OnEnter()
{
    m_bSceneTransitionStarted = false;
    m_SelectedSlotId = 0;
    m_ImageUI.Initialize(
        L"Client/Bin/Resource/Texture/UI/CustomMode1.png",
        1539,
        861);

    m_bServerLobbyActive = CGameSessionClient::Instance().Connect("127.0.0.1", 9000);
    if (!m_bServerLobbyActive)
    {
        GameContext& context = CGameInstance::Get()->Get_GameContext();
        InitializeLocalCustomRoom(context);
        m_SelectedSlotId = context.MySlotId;
    }
    return true;
}
```

확인 필요:
- `InitializeLocalCustomRoom`, `AddBotToSlot`, `GetDefaultBotChampion` 등 현재 `Scene_BanPick.cpp` anonymous namespace helper는 새 `Scene_CustomMode.cpp`에서도 필요하다.
- 구현 때는 helper를 `Client/Public/Scene/LobbyRosterHelpers.h`, `Client/Private/Scene/LobbyRosterHelpers.cpp`로 빼서 `CustomMode`와 `BanPick`가 같이 쓰게 한다.

아래로 추가:

```cpp
void CScene_CustomMode::OnExit()
{
    m_ImageUI.Shutdown();
}

void CScene_CustomMode::OnUpdate(f32_t /*dt*/)
{
    if (m_bSceneTransitionStarted)
        return;

    if (m_bServerLobbyActive)
    {
        CGameSessionClient& session = CGameSessionClient::Instance();
        session.Pump();

        if (session.HasLobbyState())
            session.CopyLobbyToGameContext(CGameInstance::Get()->Get_GameContext());

        if (session.GetLobbyPhase() == static_cast<u8_t>(Shared::Schema::LobbyPhase::ChampionSelect))
        {
            ChangeToChampionSelectScene();
            return;
        }

        if (session.IsServerLoading())
        {
            session.CopyLobbyToGameContext(CGameInstance::Get()->Get_GameContext());
            session.ClearServerLoading();
            StartMatchLoadingScene();
            return;
        }
    }

    HandleInput();
}
```

아래로 추가:

```cpp
void CScene_CustomMode::OnLateUpdate(f32_t /*dt*/)
{}

void CScene_CustomMode::OnRender()
{
    m_ImageUI.Render();
}

void CScene_CustomMode::OnImGui()
{}
```

아래로 추가:

```cpp
void CScene_CustomMode::HandleInput()
{
    if (m_bServerLobbyActive)
        HandleServerInput();
    else
        HandleLocalInput();
}

void CScene_CustomMode::HandleServerInput()
{
    CGameSessionClient& session = CGameSessionClient::Instance();
    if (!session.HasLobbyState())
        return;

    if (IsAddBlueBotClicked())
    {
        AddBotToFirstEmptySlot(0, 5);
        return;
    }

    if (IsAddRedBotClicked())
    {
        AddBotToFirstEmptySlot(5, 10);
        return;
    }

    if (IsGamePlayClicked())
    {
        session.SendLobbyCommand(Shared::Schema::LobbyCommandKind::StartGame, 0);
        return;
    }

    f32_t sourceX = 0.f;
    f32_t sourceY = 0.f;
    if (CInput::Get().IsLButtonPressed() &&
        m_ImageUI.ScreenToSource(
            static_cast<f32_t>(CInput::Get().GetMouseX()),
            static_cast<f32_t>(CInput::Get().GetMouseY()),
            sourceX,
            sourceY))
    {
        const u8_t slotId = ResolveClickedSlot(sourceX, sourceY);
        if (slotId < kGameRosterSlotCount)
        {
            m_SelectedSlotId = slotId;
            session.SendLobbyCommand(Shared::Schema::LobbyCommandKind::JoinSlot, slotId);
        }
    }
}
```

아래로 추가:

```cpp
void CScene_CustomMode::HandleLocalInput()
{
    GameContext& context = CGameInstance::Get()->Get_GameContext();

    if (IsAddBlueBotClicked())
    {
        AddBotToFirstEmptySlot(0, 5);
        return;
    }

    if (IsAddRedBotClicked())
    {
        AddBotToFirstEmptySlot(5, 10);
        return;
    }

    if (IsGamePlayClicked())
    {
        ChangeToChampionSelectScene();
        return;
    }

    f32_t sourceX = 0.f;
    f32_t sourceY = 0.f;
    if (CInput::Get().IsLButtonPressed() &&
        m_ImageUI.ScreenToSource(
            static_cast<f32_t>(CInput::Get().GetMouseX()),
            static_cast<f32_t>(CInput::Get().GetMouseY()),
            sourceX,
            sourceY))
    {
        const u8_t slotId = ResolveClickedSlot(sourceX, sourceY);
        if (slotId < kGameRosterSlotCount)
        {
            JoinLocalPlayerSlot(context, slotId);
            m_SelectedSlotId = slotId;
        }
    }
}
```

아래로 추가:

```cpp
void CScene_CustomMode::ChangeToChampionSelectScene()
{
    if (m_bSceneTransitionStarted)
        return;

    m_bSceneTransitionStarted = true;
    CGameInstance::Get()->Change_Scene(
        static_cast<u32_t>(eSceneID::BanPick),
        CScene_BanPick::Create());
}
```

확인 필요:
- `ResolveClickedSlot`의 slot rect는 `CustomMode1.png` 원본 좌표를 실제 F5 클릭으로 보정한다.
- 봇 챔피언 변경 UI는 선택된 봇 슬롯 + 챔피언 grid 클릭으로 `SetBotChampion(slot, champion)`을 보내도록 같은 파일에 추가한다.

1-9. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_BanPick.h

기존 코드:

```cpp
void UpdateServerSmokeAutomation(f32_t dt);
void StartMatchLoadingScene();
```

아래로 교체:

```cpp
void UpdateServerSmokeAutomation(f32_t dt);
void HandleChampionSelectInput();
void HandleServerChampionSelectInput();
void HandleLocalChampionSelectInput();
void StartMatchLoadingScene();

eChampion ResolveClickedChampion(f32_t sourceX, f32_t sourceY) const;
bool_t IsReadyButtonClicked() const;
bool_t IsLocalPlayerChampionPicked() const;
```

기존 멤버 아래로 추가:

```cpp
CImageScenePresenter m_ImageUI{};
eChampion m_SelectedChampion = eChampion::END;
```

아래로 추가:

```cpp
#include "UI/ImageScenePresenter.h"
```

1-10. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_BanPick.cpp

삭제할 코드:

```cpp
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")
```

`CScene_BanPick::OnEnter()` 안에 아래로 추가:

```cpp
m_ImageUI.Initialize(
    L"Client/Bin/Resource/Texture/UI/IreliaSelect1.png",
    1555,
    861);
m_SelectedChampion = eChampion::END;
```

기존 코드:

```cpp
void CScene_BanPick::OnExit()
{}
```

아래로 교체:

```cpp
void CScene_BanPick::OnExit()
{
    m_ImageUI.Shutdown();
}
```

`CScene_BanPick::OnUpdate(f32_t dt)`에서 서버 lobby pump 뒤 아래로 추가:

```cpp
if (session.GetLobbyPhase() == static_cast<u8_t>(Shared::Schema::LobbyPhase::SeatSelect))
{
    CGameInstance::Get()->Change_Scene(
        static_cast<u32_t>(eSceneID::CustomMode),
        CScene_CustomMode::Create());
    return;
}

HandleChampionSelectInput();
```

기존 코드:

```cpp
void CScene_BanPick::OnRender()
{}
```

아래로 교체:

```cpp
void CScene_BanPick::OnRender()
{
    m_ImageUI.Render();
    RenderChampionGridAndRosterOverlay();
}
```

확인 필요:
- `RenderChampionGridAndRosterOverlay`는 새 helper로 만들거나 `Scene_BanPick.cpp` private 함수로 둔다.
- 이름은 PascalCase를 사용한다.
- 실제 구현에서는 `CChampionCatalog::Instance().GetSelectableChampions()`를 순회해서 모든 selectable champion을 grid로 렌더한다.
- champion portrait는 `Client/Bin/Resource/Texture/Character/<Champion>/<lower-name>loadscreen*.png`를 우선 사용하고, 없으면 `ChampionDef::defaultTexturePath` 또는 텍스트 fallback을 사용한다.

기존 코드:

```cpp
void CScene_BanPick::OnImGui()
{
    ...
}
```

아래로 교체:

```cpp
void CScene_BanPick::OnImGui()
{}
```

아래로 추가:

```cpp
void CScene_BanPick::HandleChampionSelectInput()
{
    if (m_bServerLobbyActive)
        HandleServerChampionSelectInput();
    else
        HandleLocalChampionSelectInput();
}
```

아래로 추가:

```cpp
void CScene_BanPick::HandleServerChampionSelectInput()
{
    CGameSessionClient& session = CGameSessionClient::Instance();
    if (!session.HasLobbyState())
        return;

    f32_t sourceX = 0.f;
    f32_t sourceY = 0.f;
    if (CInput::Get().IsLButtonPressed() &&
        m_ImageUI.ScreenToSource(
            static_cast<f32_t>(CInput::Get().GetMouseX()),
            static_cast<f32_t>(CInput::Get().GetMouseY()),
            sourceX,
            sourceY))
    {
        const eChampion champion = ResolveClickedChampion(sourceX, sourceY);
        if (champion != eChampion::END)
        {
            m_SelectedChampion = champion;
            session.SendLobbyCommand(
                Shared::Schema::LobbyCommandKind::PickChampion,
                session.GetLobbyContext().MySlotId,
                champion);
            return;
        }
    }

    if (IsReadyButtonClicked())
    {
        session.SendLobbyCommand(
            Shared::Schema::LobbyCommandKind::StartGame,
            0);
    }
}
```

아래로 추가:

```cpp
void CScene_BanPick::HandleLocalChampionSelectInput()
{
    GameContext& context = CGameInstance::Get()->Get_GameContext();

    f32_t sourceX = 0.f;
    f32_t sourceY = 0.f;
    if (CInput::Get().IsLButtonPressed() &&
        m_ImageUI.ScreenToSource(
            static_cast<f32_t>(CInput::Get().GetMouseX()),
            static_cast<f32_t>(CInput::Get().GetMouseY()),
            sourceX,
            sourceY))
    {
        const eChampion champion = ResolveClickedChampion(sourceX, sourceY);
        if (champion != eChampion::END)
        {
            m_SelectedChampion = champion;
            AssignChampionToSlot(context, context.MySlotId, champion);
            return;
        }
    }

    if (IsReadyButtonClicked())
    {
        char reason[160]{};
        if (!ValidateRosterForStart(context, reason, sizeof(reason)))
            return;

        FinalizeRosterForStart(context);
        StartMatchLoadingScene();
    }
}
```

확인 필요:
- server branch에서는 ready button이 `StartGame`을 보내지만, 서버가 현재 `ChampionSelect` phase일 때만 loading으로 전환한다.
- player champion pick은 반드시 `LobbyCommand::PickChampion`으로만 처리한다.
- bot champion 변경은 선택된 봇 슬롯에 대해 `LobbyCommand::SetBotChampion`으로만 처리한다.

1-11. C:/Users/user/Desktop/Winters/Client/Public/Scene/LobbyRosterHelpers.h

새 파일:

```cpp
#pragma once

#include "GameContext.h"
#include "GameObject/ChampionDef.h"
#include "WintersTypes.h"

void InitializeLocalCustomRoom(GameContext& context);
void JoinLocalPlayerSlot(GameContext& context, u32_t slotId);
void AddBotToSlot(GameContext& context, u32_t slotId, eChampion champion);
void AssignChampionToSlot(GameContext& context, u32_t slotId, eChampion champion);
void ClearSlot(GameContext& context, u32_t slotId);
eChampion GetDefaultBotChampion(u32_t slotId);
bool_t ValidateRosterForStart(const GameContext& context, char* pReason, size_t reasonBytes);
void FinalizeRosterForStart(GameContext& context);
bool_t IsSlotEmpty(const GameRosterSlot& slot);
bool_t IsSlotOccupied(const GameRosterSlot& slot);
```

1-12. C:/Users/user/Desktop/Winters/Client/Private/Scene/LobbyRosterHelpers.cpp

새 파일:

```cpp
#include "Scene/LobbyRosterHelpers.h"
```

아래로 추가:

```cpp
// Scene_BanPick.cpp anonymous namespace에 있는 local roster helper들을 이 파일로 이동한다.
// 이동 대상:
// - IsRosterChampionSupported
// - IsSlotEmpty
// - IsSlotOccupied
// - GetTeamFromSlotId
// - FindLocalHumanSlot
// - ClearSlot
// - JoinLocalPlayerSlot
// - GetDefaultBotChampion
// - AddBotToSlot
// - AssignChampionToSlot
// - FillEmptySlotsWithBots
// - ClearBotSlots
// - InitializeLocalCustomRoom
// - ValidateRosterForStart
// - FinalizeRosterForStart
// - CountRoster
```

확인 필요:
- public header에는 실제로 두 scene에서 필요한 최소 함수만 노출한다.
- `using namespace`는 public header에 넣지 않는다.

1-13. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_Login.cpp

기존 ImGui login UI는 삭제하고 `Login1.png` 렌더 + 로그인 화살표 클릭으로 기존 `RequestOfflineLogin()`을 호출한다.

기존 코드:

```cpp
void CScene_Login::OnRender()
{}
```

아래로 교체:

```cpp
void CScene_Login::OnRender()
{
    m_ImageUI.Render();
}
```

기존 코드:

```cpp
void CScene_Login::OnImGui()
{
    ...
}
```

아래로 교체:

```cpp
void CScene_Login::OnImGui()
{}
```

1-14. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_MainMenu.cpp

기존 ImGui main menu UI는 삭제하고 `MainMenu1.png` 렌더 + GamePlay 버튼 클릭으로 기존 `LaunchSelectedProduct()` 흐름을 탄다.

기존 코드:

```cpp
void CScene_MainMenu::RequestPlay()
{
    m_ePanel = ePanel::Lobby;
    m_strStatus = "Lobby ready";
}
```

아래로 교체:

```cpp
void CScene_MainMenu::RequestPlay()
{
    m_strStatus = "Launching selected product...";
    m_bPlayRequested = true;
}
```

기존 코드:

```cpp
void CScene_MainMenu::OnImGui()
{
    ...
}
```

아래로 교체:

```cpp
void CScene_MainMenu::OnImGui()
{}
```

1-15. C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj

기존 코드:

```xml
<ClCompile Include="..\Private\Scene\Scene_BanPick.cpp" />
```

아래로 추가:

```xml
<ClCompile Include="..\Private\Scene\Scene_CustomMode.cpp" />
<ClCompile Include="..\Private\Scene\LobbyRosterHelpers.cpp" />
<ClCompile Include="..\Private\UI\ImageScenePresenter.cpp" />
```

기존 코드:

```xml
<ClInclude Include="..\Public\Scene\Scene_BanPick.h" />
```

아래로 추가:

```xml
<ClInclude Include="..\Public\Scene\Scene_CustomMode.h" />
<ClInclude Include="..\Public\Scene\LobbyRosterHelpers.h" />
<ClInclude Include="..\Public\UI\ImageScenePresenter.h" />
```

1-16. C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj.filters

기존 Scene filter 항목에 아래로 추가:

```xml
<ClInclude Include="..\Public\Scene\Scene_CustomMode.h">
  <Filter>01. Scene\00. LOL\BanPick</Filter>
</ClInclude>
<ClInclude Include="..\Public\Scene\LobbyRosterHelpers.h">
  <Filter>01. Scene\00. LOL\BanPick</Filter>
</ClInclude>
<ClCompile Include="..\Private\Scene\Scene_CustomMode.cpp">
  <Filter>01. Scene\00. LOL\BanPick</Filter>
</ClCompile>
<ClCompile Include="..\Private\Scene\LobbyRosterHelpers.cpp">
  <Filter>01. Scene\00. LOL\BanPick</Filter>
</ClCompile>
```

기존 UI filter 항목에 아래로 추가:

```xml
<ClInclude Include="..\Public\UI\ImageScenePresenter.h">
  <Filter>05. UI</Filter>
</ClInclude>
<ClCompile Include="..\Private\UI\ImageScenePresenter.cpp">
  <Filter>05. UI</Filter>
</ClCompile>
```

2. 검증

미검증:
- 아직 코드 반영 전 계획서 단계다.

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug-DX12 /p:Platform=x64`

수동 확인:
- 클라이언트 2개 이상 실행 후 같은 서버 방에 접속한다.
- MainMenu에서 GamePlay를 눌렀을 때 `CustomMode1.png` 방 화면으로 들어간다.
- 각 클라이언트가 Blue/Red 슬롯을 클릭하면 `JoinSlot`이 서버로 가고 모든 클라 roster가 같이 갱신된다.
- 빈 슬롯에 봇 추가/제거/챔피언 변경을 하면 `SetBotChampion`이 서버로 가고 모든 클라 roster가 같이 갱신된다.
- CustomMode에서 GamePlay 버튼을 누르면 서버 `LobbyPhase::SeatSelect -> ChampionSelect`로 바뀌고 모든 클라가 BanPick/ChampionSelect 화면으로 넘어간다.
- ChampionSelect에서 각 플레이어가 서로 다른 챔피언을 선택하면 `PickChampion`이 서버로 가고 모든 클라에 선택 상태가 표시된다.
- ChampionSelect에서 시작 버튼을 누르면 서버가 roster champion 검증 후 `LobbyPhase::Starting`으로 전환하고 모든 클라가 기존 MatchLoading/InGame 플로우로 들어간다.

확인 필요:
- `CustomMode1.png`, `IreliaSelect1.png`의 실제 버튼/슬롯 좌표는 F5로 클릭 보정한다.
- `IreliaSelect1.png` 파일명은 임시 배경 이름일 뿐, 구현 개념은 `ChampionSelect`이며 이렐리아 하드코딩을 금지한다.
- `Shared/Schemas/LobbyTypes.fbs`에는 이미 `SeatSelect`와 `ChampionSelect`가 있으므로 새 enum 추가는 하지 않는다.
- `LobbyCommandKind::StartGame`은 phase에 따라 의미가 갈린다: SeatSelect에서는 ChampionSelect 진입, ChampionSelect에서는 loading 시작.
- Engine public header를 바꾸지 않으므로 `EngineSDK/inc` 직접 수정은 하지 않는다.
