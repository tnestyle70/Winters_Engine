Session - S030 잔여분을 반영한다: Scene_InGame 게임종료/저장/메인메뉴 배선, AI Debug 수동 trace 저장, 인게임 기어 설정창(아틀라스 631,269,32,32 기어 + 4,90,341,311 패널), 야스오 Q=요네 이펙트 이식·회오리 반투명 화이트·피격 패시브 장막, Q stage 분기 서버 권위화.

Session - 선행 반영분(빌드 PASS): 서버 넥서스 파괴 감지+GameEnd 이벤트+리플레이 발행, 세션 전원 이탈 시 리플레이 발행, EventApplier GameEnd latch(`ConsumeGameEndEvent`), `AiTraceExport`/`LocalMatchRecord` 모듈, `Scene_InGame.h`의 S030 멤버/메서드 선언 5종(구현은 본 계획서), `Scene_Shop`/`Scene_MyInfo` 씬. Bot AI는 GameCommand 생산자이며 본 계획서의 어떤 변경도 게임플레이 truth를 직접 변이하지 않는다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
#include "Manager/Navigation/Pathfinder.h"
```

아래에 추가:

```cpp
// S030: 게임 종료 산출물 저장 + 메인 메뉴 복귀
#include "ClientShell/ClientShellSession.h"
#include "Replay/LocalMatchRecord.h"
#include "Scene/Scene_MainMenu.h"
#include "Scene/Scene_MyInfo.h"
#include "UI/AiTraceExport.h"
```

기존 코드:

```cpp
void CScene_InGame::OnUpdate(f32_t dt)
{
    WINTERS_PROFILE_SCOPE("Scene_InGame::OnUpdate");
```

아래에 추가:

```cpp
    PollGameEndAndSettings();
    if (m_bReturnToMainMenuRequested)
    {
        m_bReturnToMainMenuRequested = false;
        ChangeToMainMenuScene();
        return;
    }
    if (m_bExitReplayToMyInfoRequested)
    {
        m_bExitReplayToMyInfoRequested = false;
        ChangeToMyInfoScene();
        return;
    }
```

기존 코드:

```cpp
void CScene_InGame::OnLateUpdate(f32_t /*dt*/)
{
}
```

아래에 추가:

```cpp
void CScene_InGame::PollGameEndAndSettings()
{
    if (m_pEventApplier && !m_bGameEndActive)
    {
        u8_t winningTeam = 0u;
        if (m_pEventApplier->ConsumeGameEndEvent(winningTeam))
        {
            m_bGameEndActive = true;
            m_bLocalVictory = static_cast<u8_t>(m_PlayerTeam) == winningTeam;
            // 정상 종료(넥서스 파괴) 저장 — 서버는 리플레이를 발행하고,
            // 클라는 로컬 전적 + AI trace JSONL을 저장한다 (S030 저장 보증 1/3).
            SaveEndOfMatchArtifacts(m_bLocalVictory ? "victory" : "defeat");
        }
    }

    if (CGameInstance::Get()->UI_ConsumeMainMenuRequest())
        m_bReturnToMainMenuRequested = true;
}

void CScene_InGame::SaveEndOfMatchArtifacts(const char* pResultLabel)
{
    // 리플레이 재생은 관전이므로 전적/trace를 남기지 않는다.
    if (m_bEndOfMatchArtifactsSaved || m_bReplayPlaybackMode)
        return;
    m_bEndOfMatchArtifactsSaved = true;

    Winters::LocalMatchRecord record{};
    record.strUser = CClientShellSession::Instance().GetDisplayName();
    record.strResult = pResultLabel ? pResultLabel : "unknown";
    record.uEndTick = m_pSnapshotApplier
        ? m_pSnapshotApplier->GetLastAppliedServerTick()
        : 0ull;
    Winters::AppendLocalMatchRecord(record);

    std::string strTracePath;
    Winters::ExportAiDecisionTraceJsonl(m_World, strTracePath);
}

void CScene_InGame::ChangeToMainMenuScene()
{
    if (m_bUsingSharedNetwork)
        CGameSessionClient::Instance().Disconnect();

    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::MainMenu),
        CScene_MainMenu::Create());
}

void CScene_InGame::ChangeToMyInfoScene()
{
    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::MyInfo),
        CScene_MyInfo::Create());
}
```

### 1-2. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

`void CScene_InGame::OnExit()` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
void CScene_InGame::OnExit()
{
    Viego::Fx::StopAllSoulIdle(m_World);
```

아래에 추가:

```cpp
    // ESC 강제 종료/메인 메뉴 이탈 포함 — 종료 산출물이 없으면 aborted로 저장
    // (S030 저장 보증 3/3; 정상 종료 시에는 이미 저장되어 no-op).
    SaveEndOfMatchArtifacts("aborted");
```

### 1-3. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp

기존 코드:

```cpp
    if (m_bReplayPlaybackMode || m_bShowReplayControl || m_bReplayStopRequested)
    {
        WINTERS_PROFILE_SCOPE("UI::Replay");
        DrawReplayControlPanel();
    }
```

아래에 추가:

```cpp
    DrawGameEndOverlay();
```

`DrawReplayControlPanel()`의 재생 분기에서 아래 기존 코드를:

```cpp
        ImGui::TextUnformatted(m_strReplayStatus.empty() ? "Replay playback" : m_strReplayStatus.c_str());
        ImGui::End();
        return;
```

아래로 교체:

```cpp
        ImGui::TextUnformatted(m_strReplayStatus.empty() ? "Replay playback" : m_strReplayStatus.c_str());
        if (ImGui::Button("나의 정보로 돌아가기"))
            m_bExitReplayToMyInfoRequested = true;
        ImGui::End();
        return;
```

`DrawReplayControlPanel()`의 라이브 분기에서 아래 기존 코드를:

```cpp
    if (ImGui::Button("Stop"))
        SendStopReplayRequest();

    ImGui::TextUnformatted(m_strReplayStatus.empty() ? "Recording on server" : m_strReplayStatus.c_str());
    ImGui::End();
}
```

아래로 교체:

```cpp
    if (ImGui::Button("Stop & Save Replay"))
        SendStopReplayRequest();

    ImGui::TextUnformatted(m_strReplayStatus.empty() ? "Recording on server" : m_strReplayStatus.c_str());
    ImGui::End();
}

void CScene_InGame::DrawGameEndOverlay()
{
    if (!m_bGameEndActive)
        return;

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 windowSize(380.f, 180.f);
    ImGui::SetNextWindowPos(
        ImVec2((io.DisplaySize.x - windowSize.x) * 0.5f,
            (io.DisplaySize.y - windowSize.y) * 0.35f),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("Game End", nullptr, flags))
    {
        ImGui::SetWindowFontScale(1.6f);
        ImGui::TextUnformatted(m_bLocalVictory ? "승리!" : "패배");
        ImGui::SetWindowFontScale(1.f);
        ImGui::Separator();
        ImGui::TextWrapped("넥서스가 파괴되어 게임이 종료되었습니다. 리플레이와 AI trace가 저장되었습니다.");
        ImGui::Spacing();
        if (ImGui::Button("메인 메뉴로", ImVec2(160.f, 0.f)))
            m_bReturnToMainMenuRequested = true;
    }
    ImGui::End();
}
```

### 1-4. C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
```

아래에 추가:

```cpp
#include "UI/AiTraceExport.h"
```

`UI::CAIDebugPanel::Render` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
	ImGui::TextUnformatted("Champion AI");
	ImGui::SameLine();
	ImGui::TextDisabled("(F9 AI panel, F10 legacy debug)");
	ImGui::Separator();
```

아래에 추가:

```cpp
	{
		static std::string s_strTraceExportStatus;
		if (ImGui::Button("Save Trace JSONL"))
		{
			std::string strPath;
			s_strTraceExportStatus = Winters::ExportAiDecisionTraceJsonl(world, strPath)
				? "Saved: " + strPath
				: "No trace rows to save";
		}
		if (!s_strTraceExportStatus.empty())
		{
			ImGui::SameLine();
			ImGui::TextUnformatted(s_strTraceExportStatus.c_str());
		}
		ImGui::Separator();
	}
```

### 1-5. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

기존 코드:

```cpp
    void ToggleInGameShop();
```

아래에 추가:

```cpp
    // S030 설정창: 우상단 기어 → 패널 → "메인 메뉴로 돌아가기" 요청 latch.
    // Client(Scene_InGame)가 폴링으로 1회 소비한다.
    bool_t ConsumeMainMenuRequest();
```

기존 코드:

```cpp
    void    DrawMatchContextHUD(ImDrawList* pDraw);
```

아래에 추가:

```cpp
    void    DrawSettingsOverlay(ImDrawList* pDraw);
```

기존 코드:

```cpp
    bool_t m_bInGameShopOpen = false;
    bool_t m_bStatusPanelOpen = false;
```

아래에 추가:

```cpp
    bool_t m_bSettingsPanelOpen = false;
    bool_t m_bMainMenuRequested = false;
```

### 1-6. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

`Render_Overlay` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    ImDrawList* pFG = ImGui::GetForegroundDrawList();
    {
        WINTERS_PROFILE_SCOPE("UI::MatchContextHUD");
        DrawMatchContextHUD(pFG);
    }
```

아래에 추가:

```cpp
    {
        WINTERS_PROFILE_SCOPE("UI::SettingsOverlay");
        DrawSettingsOverlay(pFG);
    }
```

기존 코드:

```cpp
void* CUI_Manager::FindOrLoadKillFeedPortrait(u8_t iActorContentId)
```

이 함수 정의 바로 위에 추가:

```cpp
bool_t CUI_Manager::ConsumeMainMenuRequest()
{
    if (!m_bMainMenuRequested)
        return false;
    m_bMainMenuRequested = false;
    return true;
}

void CUI_Manager::DrawSettingsOverlay(ImDrawList* pDraw)
{
    if (!pDraw || !m_bShowMatchContextHUD)
        return;

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const f32_t screenW = (m_iWinSizeX > 0) ? static_cast<f32_t>(m_iWinSizeX) : displaySize.x;
    const f32_t screenH = (m_iWinSizeY > 0) ? static_cast<f32_t>(m_iWinSizeY) : displaySize.y;
    if (screenW <= 0.f || screenH <= 0.f)
        return;

    ImGuiIO& IO = ImGui::GetIO();
    auto PointInRect = [](const ImVec2& p, const ImVec2& mn, const ImVec2& mx)
    {
        return p.x >= mn.x && p.x <= mx.x && p.y >= mn.y && p.y <= mx.y;
    };

    // 우상단 기어 버튼 (match context HUD 아래, clarity_hudatlas 631,269,32,32)
    const ImVec2 gearMin(screenW - 52.f, 44.f);
    const ImVec2 gearMax(gearMin.x + 32.f, gearMin.y + 32.f);
    const bool_t bGearHovered = PointInRect(IO.MousePos, gearMin, gearMax);
    if (!UI_DrawManifestSprite(pDraw, m_HudAtlasManifest, "settings.gear",
        gearMin, gearMax, bGearHovered ? 1.f : 0.85f))
    {
        pDraw->AddRectFilled(gearMin, gearMax, IM_COL32(20, 46, 52, 220), 4.f);
    }
    if (bGearHovered && IO.MouseClicked[0])
        m_bSettingsPanelOpen = !m_bSettingsPanelOpen;

    if (!m_bSettingsPanelOpen)
        return;

    // 배경 딤 + 중앙 설정 패널 (clarity_hudatlas 4,90,341,311 골드/틸 프레임)
    pDraw->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(screenW, screenH), IM_COL32(0, 0, 0, 120));
    const f32_t panelW = 341.f;
    const f32_t panelH = 311.f;
    const ImVec2 panelMin((screenW - panelW) * 0.5f, (screenH - panelH) * 0.5f);
    const ImVec2 panelMax(panelMin.x + panelW, panelMin.y + panelH);
    pDraw->AddRectFilled(panelMin, panelMax, IM_COL32(10, 22, 30, 235));
    UI_DrawManifestSprite(pDraw, m_HudAtlasManifest, "settings.panel", panelMin, panelMax, 1.f);

    ImFont* pFont = FindUIFont("hud");
    if (pFont)
    {
        UI_DrawOutlinedText(pDraw, pFont, 20.f,
            ImVec2(panelMin.x + 24.f, panelMin.y + 20.f),
            IM_COL32(238, 224, 177, 255), "설정");
    }

    // 메인 메뉴 버튼
    const ImVec2 mainMenuMin(panelMin.x + 60.f, panelMin.y + 120.f);
    const ImVec2 mainMenuMax(panelMax.x - 60.f, mainMenuMin.y + 40.f);
    const bool_t bMainMenuHovered = PointInRect(IO.MousePos, mainMenuMin, mainMenuMax);
    pDraw->AddRectFilled(mainMenuMin, mainMenuMax,
        bMainMenuHovered ? IM_COL32(26, 66, 74, 240) : IM_COL32(14, 40, 48, 240), 3.f);
    pDraw->AddRect(mainMenuMin, mainMenuMax, IM_COL32(174, 133, 46, 255), 3.f, 0, 1.5f);
    if (pFont)
    {
        UI_DrawOutlinedText(pDraw, pFont, 17.f,
            ImVec2(mainMenuMin.x + 46.f, mainMenuMin.y + 10.f),
            IM_COL32(238, 224, 177, 255), "메인 메뉴로 돌아가기");
    }
    if (bMainMenuHovered && IO.MouseClicked[0])
    {
        m_bMainMenuRequested = true;
        m_bSettingsPanelOpen = false;
    }

    // 닫기 버튼
    const ImVec2 closeMin(panelMin.x + 60.f, mainMenuMax.y + 16.f);
    const ImVec2 closeMax(panelMax.x - 60.f, closeMin.y + 40.f);
    const bool_t bCloseHovered = PointInRect(IO.MousePos, closeMin, closeMax);
    pDraw->AddRectFilled(closeMin, closeMax,
        bCloseHovered ? IM_COL32(26, 66, 74, 240) : IM_COL32(14, 40, 48, 240), 3.f);
    pDraw->AddRect(closeMin, closeMax, IM_COL32(90, 120, 126, 255), 3.f, 0, 1.5f);
    if (pFont)
    {
        UI_DrawOutlinedText(pDraw, pFont, 17.f,
            ImVec2(closeMin.x + 96.f, closeMin.y + 10.f),
            IM_COL32(210, 214, 214, 255), "닫기");
    }
    if (bCloseHovered && IO.MouseClicked[0])
        m_bSettingsPanelOpen = false;
}
```

### 1-7. C:/Users/user/Desktop/Winters/Engine/Include/GameInstance.h

기존 코드:

```cpp
    void UI_Toggle_InGameShop();
```

아래에 추가:

```cpp
    bool_t UI_ConsumeMainMenuRequest();
```

### 1-8. C:/Users/user/Desktop/Winters/Engine/Private/GameInstance.cpp

기존 코드:

```cpp
void CGameInstance::UI_Toggle_InGameShop()
{
	if (m_pUI_Manager)
		m_pUI_Manager->ToggleInGameShop();
}
```

아래에 추가:

```cpp
bool_t CGameInstance::UI_ConsumeMainMenuRequest()
{
	return m_pUI_Manager ? m_pUI_Manager->ConsumeMainMenuRequest() : false;
}
```

### 1-9. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/hud_atlas_manifest.json

기존 코드:

```json
        "gamecontext.time.icon": {
            "texture": "hud",
            "x": 897,
            "y": 448,
            "w": 20,
            "h": 18
        }
```

아래로 교체 (마지막 항목 뒤 콤마 주의):

```json
        "gamecontext.time.icon": {
            "texture": "hud",
            "x": 897,
            "y": 448,
            "w": 20,
            "h": 18
        },
        "settings.gear": {
            "texture": "hud",
            "x": 631,
            "y": 269,
            "w": 32,
            "h": 32
        },
        "settings.panel": {
            "texture": "hud",
            "x": 4,
            "y": 90,
            "w": 341,
            "h": 311
        }
```

### 1-10. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Yasuo/q_slash.wfx

파일 전체를 아래 내용으로 교체 (요네 `Yone.Q.MortalSteel` 이미터 세트를 야스오 cue명으로 이식 — 사용자 확인: 두 이펙트 동일):

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Yasuo.Q.Slash",
  "emitters": [
    {
      "name": "q_stab_width_ground",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_stab_width.png",
      "lifetime": 0.34,
      "fade_in": 0.01,
      "fade_out": 0.22,
      "width": 7.38,
      "height": 1.10,
      "yaw": 1.5708,
      "color": [0.55, 1.00, 1.28, 0.66],
      "attach_offset": [0.0, 0.06, 2.20],
      "billboard": false,
      "blockable_by_wind_wall": true
    },
    {
      "name": "q_sword_core",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yone/particles/yone_q_sword.png",
      "lifetime": 0.30,
      "fade_in": 0.01,
      "fade_out": 0.18,
      "width": 6.76,
      "height": 2.26,
      "yaw": 1.5708,
      "color": [0.72, 1.05, 1.32, 0.90],
      "attach_offset": [0.0, 1.08, 2.00],
      "billboard": false,
      "blockable_by_wind_wall": true
    },
    {
      "name": "q_sword_sub_red",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yone/particles/yone_q_sword_sub.png",
      "lifetime": 0.28,
      "start_delay": 0.02,
      "fade_in": 0.01,
      "fade_out": 0.18,
      "width": 5.20,
      "height": 5.20,
      "yaw": 1.5708,
      "color": [1.00, 0.14, 0.28, 0.68],
      "attach_offset": [0.0, 1.02, 1.90],
      "billboard": false,
      "blockable_by_wind_wall": true
    },
    {
      "name": "q_air_cylinder",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_air_streaks.png",
      "model": "Client/Bin/Resource/Texture/Character/Yone/particles/fbx/yone_base_q_cylinder.fbx",
      "lifetime": 0.42,
      "fade_in": 0.02,
      "fade_out": 0.26,
      "scale": [0.018, 0.018, 0.018],
      "rotation": [0.0, 3.1416, 0.0],
      "color": [0.65, 0.95, 1.15, 0.52],
      "attach_offset": [0.0, 0.86, 2.35],
      "billboard": true,
      "blockable_by_wind_wall": true
    },
    {
      "name": "q_hit_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_q_wind_hit_flash.png",
      "lifetime": 0.22,
      "start_delay": 0.08,
      "fade_in": 0.01,
      "fade_out": 0.16,
      "width": 5.33,
      "height": 5.29,
      "color": [0.55, 1.10, 1.35, 0.88],
      "attach_offset": [0.88, 1.05, 1.94],
      "billboard": true,
      "blockable_by_wind_wall": true
    }
  ]
}
```

### 1-11. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Yasuo/q_tornado.wfx

`q_tornado_blade_mesh` 이미터에서 아래 기존 코드를:

```json
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Yasuo/particles/fbx/yasuo_base_q_tornado_blade_cas.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Yasuo/particles/color_yasuo_base_e_tonado_blend.png",
      "lifetime": 1.50,
      "fade_in": 0.03,
      "fade_out": 0.34,
      "scale": [0.024, 0.024, 0.024],
      "world_yaw_spin_speed": 12.566,
      "color": [0.78, 0.97, 0.95, 0.95],
```

아래로 교체 (회오리 메쉬 = 반투명 하얀색):

```json
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Yasuo/particles/fbx/yasuo_base_q_tornado_blade_cas.fbx",
      "texture": "Client/Bin/Resource/Texture/Character/Yasuo/particles/color_yasuo_base_e_tonado_blend.png",
      "lifetime": 1.50,
      "fade_in": 0.03,
      "fade_out": 0.34,
      "scale": [0.024, 0.024, 0.024],
      "world_yaw_spin_speed": 12.566,
      "color": [1.00, 1.00, 1.00, 0.50],
```

`q_tornado_ground_shape` 이미터 블록(닫는 `},` 포함) 바로 아래에 추가 (회오리 하단 파티클):

```json
    {
      "name": "q_tornado_base_dust",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yasuo/particles/yasuo_q_tornado_buff.png",
      "lifetime": 1.50,
      "fade_in": 0.04,
      "fade_out": 0.40,
      "width": 2.60,
      "height": 2.60,
      "color": [0.85, 0.97, 0.96, 0.55],
      "attach_offset": [0.0, 0.10, 0.0],
      "uv_scroll": [0.0, -0.85],
      "billboard": false,
      "blockable_by_wind_wall": true
    },
```

### 1-12. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Yasuo/passive_shield.wfx

새 파일 (리신 `w1_cast.wfx` 보호막 구조 참조 — 장막 fbx `haga_sphere_geo.fbx` + 패시브 장막 png, 반투명):

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Yasuo.Passive.Shield",
  "emitters": [
    {
      "name": "shield_sphere_mesh",
      "render_type": "MeshParticle",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yasuo/particles/color_yasuo_passive_shield.png",
      "model": "Client/Bin/Resource/Texture/Character/Yasuo/particles/fbx/haga_sphere_geo.fbx",
      "lifetime": 1.10,
      "fade_in": 0.05,
      "fade_out": 0.45,
      "scale": [0.016, 0.016, 0.016],
      "world_yaw_spin_speed": 1.256,
      "color": [0.75, 0.95, 1.00, 0.35],
      "attach_offset": [0.0, 1.05, 0.0],
      "blockable_by_wind_wall": false
    },
    {
      "name": "shield_glow_billboard",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Yasuo/particles/color_yasuo_passive_shield.png",
      "lifetime": 1.10,
      "start_delay": 0.02,
      "fade_in": 0.04,
      "fade_out": 0.45,
      "width": 2.45,
      "height": 2.45,
      "color": [0.62, 0.94, 1.00, 0.48],
      "attach_offset": [0.0, 1.05, 0.0],
      "billboard": true,
      "blockable_by_wind_wall": false
    }
  ]
}
```

### 1-13. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Yasuo/YasuoFxPresets.h

기존 코드:

```cpp
    EntityID SpawnQBuildUp(CWorld& world, EntityID owner, f32_t fLifetime);
```

아래에 추가:

```cpp
    // 피격 시 패시브 장막 (리신 W 보호막 스타일 반투명 버블, 시전자 attach)
    EntityID SpawnPassiveShield(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, f32_t fLifetime);
```

### 1-14. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp

기존 코드:

```cpp
    constexpr const char* kCueQTornado = "Yasuo.Q.Tornado";
```

아래에 추가:

```cpp
    constexpr const char* kCuePassiveShield = "Yasuo.Passive.Shield";
```

`SpawnQBuildUp` 함수 정의(닫는 `}` 포함) 바로 아래에 추가:

```cpp
EntityID YasuoFx::SpawnPassiveShield(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID owner, f32_t fLifetime)
{
    FxCueContext cue{};
    cue.attachTo = owner;
    cue.vWorldPos = ResolveEntityWorldPos(world, owner);
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    cue.pFxMeshRenderer = pRenderer;
    return CFxCuePlayer::Play(world, kCuePassiveShield, cue);
}
```

### 1-15. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

include 블록에서 기존 코드:

```cpp
#include "Network/Client/EventApplier.h"
```

아래에 추가 (경로/이름은 파일 상단 include 스타일에 맞춤):

```cpp
#include "GameObject/Champion/Yasuo/YasuoFxPresets.h"
```

`CEventApplier::ApplyDamage` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    Vec3 damageTextPos = pos;
    damageTextPos.y += 2.1f;
    CGameInstance::Get()->UI_Push_DamageNumber(
        damageTextPos,
        ev->amount(),
        ev->type(),
        ev->bWasCrit(),
        ev->bKilled());
```

아래에 추가:

```cpp
    // 야스오 피격 패시브 장막: 서버가 복제한 shield 잔량이 있는 피격에만 표시 (S030 Goal 3a).
    if (world.HasComponent<ChampionComponent>(target))
    {
        const auto& champion = world.GetComponent<ChampionComponent>(target);
        if (champion.shield > 0.f &&
            static_cast<eChampion>(champion.id) == eChampion::YASUO)
        {
            YasuoFx::SpawnPassiveShield(world, m_pFxMeshRenderer, target, 1.1f);
        }
    }
```

### 1-16. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Yasuo/Yasuo_Skills.cpp

Q 로직 검토 결과: 클라 `OnCastAccepted_Q`는 시전 시 로컬 `ys.qStackCount`/`ys.bEActive`로 분기·증가하지만, 서버(`YasuoGameSim.cpp`)는 `RegisterQHit`(적중 시 스택, 상한 2, 데이터 파라미터 윈도우)과 `ResolveQVariantStage`(E활성=4, 스택2=3, 스택1=2)로 판정한다. 시전-시-스택 vs 적중-시-스택 divergence로 클라 회오리 표현과 서버 판정이 어긋날 수 있다. `SkillHookContext.skillStage`(서버 stage 전달)가 이미 존재하므로 표현 분기를 서버 stage 우선으로 교체한다.

`OnCastAccepted_Q` 안에서 아래 기존 코드를:

```cpp
        if (ys.bEActive)
        {
            YasuoFx::SpawnEQRing(world, ctx.casterEntity, origin,
                0.6f, tuning.eqRadius);
```

아래로 교체:

```cpp
        // 서버 권위 stage 우선(4=EQ 원형, 3=회오리 Q3, 1~2=직선 Q).
        // stage가 전달되지 않는 로컬 예측 경로에서만 로컬 상태로 보조 판정한다.
        const u8_t effectiveStage = ctx.skillStage >= 2u
            ? ctx.skillStage
            : (ys.bEActive ? 4u : (ys.qStackCount >= 2 ? 3u : 1u));

        if (effectiveStage == 4u)
        {
            YasuoFx::SpawnEQRing(world, ctx.casterEntity, origin,
                0.6f, tuning.eqRadius);
```

이어지는 기존 코드를:

```cpp
        else if (ys.qStackCount >= 2)
        {
            YasuoFx::SpawnQTornado(world, ctx.pFxMeshRenderer,
```

아래로 교체:

```cpp
        else if (effectiveStage == 3u)
        {
            YasuoFx::SpawnQTornado(world, ctx.pFxMeshRenderer,
```

## 2. 검증

미검증:

- 본 계획서 전체가 빌드 미검증 (선행 반영분은 GameSim/Server/Client Debug x64 PASS 상태)
- 인게임 전 시나리오 미검증

검증 명령:

```text
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine\Include\Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
& ... GameSim.vcxproj / Server.vcxproj / Client.vcxproj / Tools\SimLab\SimLab.vcxproj 동일 플래그 순차
.\Tools\Bin\Debug\SimLab.exe 1800 42   → PASS full SimLab + 키프레임 골든(blob=81027) + 해시 DB0DC85E451999AD / 57A9B2394575042A 유지
git diff --check
```

백엔드 기동 (Goal 1 인게임 검증용 — 현재 세션에서 이미 기동/검증됨):

```powershell
Set-Location C:\Users\user\Desktop\Winters\Services
docker compose up -d postgres redis
go run ./cmd/auth    # 별도 터미널, 8081
go run ./cmd/profile # 8084
go run ./cmd/shop    # 8086
```

인게임 게이트:

1. 아이디 로그인(미가입 404 → "회원 가입을 하세요" → 가입 → RP 1000) → MainMenu 상점 → 챔피언 구매(50RP 차감, 보유 오버레이) → 보유 챔피언 클릭 시 "이미 구매한 챔피언입니다" → EXE 재시작 후 같은 아이디 로그인 → RP/소유권 복원.
2. MainMenu 우상단 초상화 → 나의 정보(프로필/전적/리플레이) → 리플레이 재생 → "나의 정보로 돌아가기" 복귀.
3. 인게임 우상단 기어 → 설정 패널 → "메인 메뉴로 돌아가기" → MainMenu 복귀 + 서버 리플레이 발행(`Replay/*.wrpl`).
4. 넥서스 파괴 → 승리/패배 오버레이 + `Replay/*.wrpl` + `Replay/AITrace/*.jsonl` + `Replay/LocalMatchHistory.jsonl` 생성.
5. ESC 강제 종료 → 서버 콘솔 OnSessionLeave 후 `Replay/*.wrpl` 발행 + 클라 OnExit aborted 기록.
6. F9 AI Debug → "Save Trace JSONL" → 저장 경로 표시.
7. 야스오: Q 시전 = 요네 찌르기 이펙트, Q3/EQ 회오리 = 반투명 화이트 메쉬 + 하단 파티클, 피격(실드 잔량) 시 반투명 보호막 버블.

확인 필요:

- `ChampionComponent.id`(GameplayContentId)가 `eChampion` 정수값과 동일한지 (1-15의 캐스트 전제). 다르면 콘텐츠 id → eChampion 변환 헬퍼 사용.
- 피격 장막 과다 트리거 시 최근 스폰 tick 기준 스로틀(예: 0.6초) 추가 여부.
- `q_air_cylinder`의 `yone_base_q_cylinder.fbx`가 런타임에서 로드되는지 (요네 Q가 인게임에서 정상 표시되면 동일 경로로 OK; 미로드 시 해당 이미터 제거).
- `haga_sphere_geo.fbx` 모델 경로 해석: 계획서는 authoring 경로(`Character/Yasuo/particles/fbx/`) 기준 — 실 Bin 위치는 `Texture/FX/Yasuo/fbx/`이므로 로더 해석 실패 시 preload 항목(`LoLVisualDefinitions.generated.cpp`의 `MakeFxMeshPreloadVisual_FX_YASUO_*` 패턴) 추가.
- 로컬 예측 경로에서 `ctx.skillStage`가 1로 고정 전달되는지 (1-16의 fallback 전제).
- 기어 클릭이 게임 월드 클릭으로도 전달되는 기존 HUD 버튼 동작과 동일한지 (기존 상점 버튼과 같은 수준이면 수용).

후속 동기화:

- Engine public header(`UI_Manager.h`, `GameInstance.h`) 변경 후 Engine 빌드 시 `UpdateLib.bat` post-build로 EngineSDK/inc 자동 동기화 확인.

수동 확인:

- `Scene_InGame.h`의 S030 멤버/메서드 선언 5종( `PollGameEndAndSettings`/`SaveEndOfMatchArtifacts`/`DrawGameEndOverlay`/`ChangeToMainMenuScene`/`ChangeToMyInfoScene` )은 이미 반영되어 있음 — 본 계획서는 구현부만 추가한다.
- `AiTraceExport.cpp/.h`, `LocalMatchRecord.cpp/.h`는 이미 `Client.vcxproj`/`.filters`에 등록되어 있음.
