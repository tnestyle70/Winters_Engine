// Scene_InGameImGui.cpp — CScene_InGame의 debug/replay UI 책임 TU.
// Stage 1 (mechanical split): Scene_InGame.cpp에서 OnImGui + replay UI 메서드를 verbatim 이동.
// 동작/시그니처/호출 순서 불변. 설계: .md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md
#define _CRT_SECURE_NO_WARNINGS

#include "Network/Client/GameSessionClient.h"
#include "Replay/ReplayPlayer.h"

#include "Scene/Scene_InGame.h"
#include "Scene/Scene_Editor.h"
#include "Core/CInput.h"
#include "ECS/Systems/VisionSystem.h"
#include "GameInstance.h"
#include "ProfilerAPI.h"
#include "Manager/Minion_Manager.h"

#include "UI/AIDebugPanel.h"
#include "UI/CombatDebugPanel.h"
#include "UI/MapTunerPanel.h"
#include "UI/MinimapPanel.h"
#include "UI/RenderDebug.h"
#include "UI/SkillTimingPanel.h"
#include "UI/ChampionTuner.h"
#include "UI/AttackSpeedLab.h"
#include "UI/StructureTunerPanel.h"
#include "UI/EffectTuner.h"
#include "UI/WfxEffectToolPanel.h"
#include "UI/ModelAnimPanel.h"
#include "Network/Client/NetworkEventTrace.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/LobbyTypes_generated.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

void CScene_InGame::OnImGui()
{
    auto& input = CInput::Get();

    if (input.IsKeyPressed('M'))
    {
        using namespace Engine;
        auto pEditor = unique_ptr<IScene>(new CScene_Editor());
        CGameInstance::Get()->Change_Scene((uint32_t)eSceneID::Editor, std::move(pEditor));
        return;
    }

    if (input.IsKeyPressed(VK_F9))
    {
        m_bShowAIDebug = !m_bShowAIDebug;
        m_bShowUITuner = m_bShowAIDebug;
    }
    if (input.IsKeyPressed(VK_F8))
        m_bShowUITuner = !m_bShowUITuner;
    if (input.IsKeyPressed(VK_F7))
        m_bShowWfxEffectTool = !m_bShowWfxEffectTool;
    // F4 = 구조물(포탑/억제기/넥서스) 체력·데미지 튜너.
    // 엔진 전역 프로파일러 JSON 캡처는 F12 로 이전 (CEngineApp.cpp).
    if (input.IsKeyPressed(VK_F4))
        m_bShowStructureTuner = !m_bShowStructureTuner;
    // F5 = 기본 공격 속도/애니 튜닝(Attack Speed Lab). '8' 은 기존 별칭으로 유지.
    if (input.IsKeyPressed(VK_F5))
    {
        m_bShowAttackSpeedLab = !m_bShowAttackSpeedLab;
        if (m_bShowAttackSpeedLab)
            UI::CAttackSpeedLab::Open();
    }
    if (input.IsKeyPressed(VK_F6))
        m_bShowReplayControl = !m_bShowReplayControl;
    if (input.IsKeyPressed(VK_F10))
        m_bShowLegacyInGameDebug = !m_bShowLegacyInGameDebug;
    // '9' = 연습 패널(Practice Tool / Balance Lab) 바로 열기. 텍스트 입력 중에는 무시.
    if (!ImGui::GetIO().WantCaptureKeyboard && input.IsKeyPressed('9'))
        m_bShowLegacyInGameDebug = !m_bShowLegacyInGameDebug;
    // Numeric 8 opens the focused attack-speed authoring lab and reloads its JSON draft.
    if (!ImGui::GetIO().WantCaptureKeyboard && input.IsKeyPressed('8'))
    {
        m_bShowAttackSpeedLab = !m_bShowAttackSpeedLab;
        if (m_bShowAttackSpeedLab)
            UI::CAttackSpeedLab::Open();
    }
    // '7' = 모델/애니메이션 랩 (F5 에서 이전 — F5 는 공속 튜닝 전용).
    if (!ImGui::GetIO().WantCaptureKeyboard && input.IsKeyPressed('7'))
        m_bShowModelAnimPanel = !m_bShowModelAnimPanel;
    if (input.IsKeyPressed(VK_F1))
    {
        m_bShowRenderDebug = !m_bShowRenderDebug;
        if (m_bShowRenderDebug)
        {
            m_bDbgShowColliders = true;
            m_bDbgShowChampions = true;
        }
    }

    if (m_bShowAIDebug)
    {
        WINTERS_PROFILE_SCOPE("UI::AIDebug");
        UI::CAIDebugPanel::Render(m_World, this);
    }

    if (m_bShowRenderDebug)
    {
        WINTERS_PROFILE_SCOPE("UI::RenderDebug");
        UI::CRenderDebugPanel::Render(this);
    }

    if (m_bShowUITuner)
    {
        WINTERS_PROFILE_SCOPE("UI::Tuner");
        CGameInstance::Get()->UI_OnImGui_Tuner();
        CGameInstance::Get()->UI_OnImGui_StatusPanelLayoutTuner();
        UI::MinimapProjection AppliedProjection{};
        if (UI::CMinimapPanel::DrawTunerImGui(
                m_pVisionSystem != nullptr,
                AppliedProjection) &&
            m_pVisionSystem)
        {
            Engine::CVisionSystem::FowProjection FowProjection{};
            FowProjection.vWorldAtUv00 = AppliedProjection.vWorldAtUv00;
            FowProjection.vWorldAtUv10 = AppliedProjection.vWorldAtUv10;
            FowProjection.vWorldAtUv01 = AppliedProjection.vWorldAtUv01;
            m_pVisionSystem->SetFowProjection(FowProjection);
        }
    }

    if (m_bShowWfxEffectTool)
    {
        WINTERS_PROFILE_SCOPE("UI::WfxEffectTool");
        UI::CWfxEffectToolPanel::Render(this);
    }

    if (m_bShowModelAnimPanel)
    {
        WINTERS_PROFILE_SCOPE("UI::ModelAnim");
        UI::CModelAnimPanel::Render(this);
    }

    if (m_bReplayPlaybackMode || m_bShowReplayControl || m_bReplayStopRequested)
    {
        WINTERS_PROFILE_SCOPE("UI::Replay");
        DrawReplayControlPanel();
    }

    if (m_bShowAttackSpeedLab)
    {
        WINTERS_PROFILE_SCOPE("UI::AttackSpeedLab");
        UI::CAttackSpeedLab::Render(this);
    }

    if (m_bShowStructureTuner)
    {
        WINTERS_PROFILE_SCOPE("UI::StructureTuner");
        UI::CStructureTunerPanel::Render(m_World, this);
    }

    DrawGameEndOverlay();

    if (!m_bShowLegacyInGameDebug)
        return;

    UI::CCombatDebugPanel::Render(m_World, this);
    UI::CMapTunerPanel::Render(this);
    UI::CChampionTuner::Render(this);
    UI::CEffectTuner::Render(this);
    UI::CRenderDebugPanel::Render(this);
    UI::CSkillTimingPanel::Render(this);
    CNetworkEventTrace::Instance().DrawImGui();

    ImGui::SetNextWindowSize(ImVec2(220.f, 120.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(180.f, 90.f), ImVec2(360.f, 220.f));
    if (ImGui::Begin("Camera"))
    {
        if (m_pCamera)
        {
            Vec3 eye = m_pCamera->GetEye();
            ImGui::Text("Eye: (%.1f, %.1f, %.1f)", eye.x, eye.y, eye.z);
        }
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        if (m_pCamera)
        {
            bool bFollow = m_pCamera->IsFollowMode();
            if (ImGui::Checkbox("Follow Mode (F2)", &bFollow))
                m_pCamera->SetFollowMode(bFollow);
        }
        ImGui::Checkbox("Log Frame Events", reinterpret_cast<bool*>(&m_bLogFrameEvents));
    }
    ImGui::End();

    if (m_pCamera)
        m_pCamera->OnImGui();
    CMinion_Manager::Get()->OnImGui_Tuner();
}

void CScene_InGame::UpdateReplayPlayback(f32_t dt)
{
    if (!m_pReplayPlayer || !m_pEntityIdMap || !m_pSnapshotApplier || !m_pEventApplier)
        return;

    if (m_pReplayPlayer->Update(
        dt,
        m_World,
        *m_pEntityIdMap,
        *m_pSnapshotApplier,
        *m_pEventApplier))
    {
        ProjectGameplayActorsToMapSurface();
    }
}

bool_t CScene_InGame::SendStopReplayRequest()
{
    if (m_bReplayPlaybackMode || m_bReplayStopRequested)
        return m_bReplayStopRequested;

    CGameSessionClient& session = CGameSessionClient::Instance();
    if (!session.IsConnected())
    {
        m_strReplayStatus = "Replay stop requires server session";
        return false;
    }

    m_bReplayStopRequested = session.SendLobbyCommand(
        Shared::Schema::LobbyCommandKind::StopReplay,
        0,
        eChampion::END,
        0,
        1u);

    m_strReplayStatus = m_bReplayStopRequested
        ? "Replay stop requested"
        : "Replay stop request failed";
    return m_bReplayStopRequested;
}

void CScene_InGame::DrawReplayControlPanel()
{
    ImGui::SetNextWindowSize(ImVec2(280.f, 116.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Replay"))
    {
        ImGui::End();
        return;
    }

    if (m_bReplayPlaybackMode)
    {
        if (m_pReplayPlayer)
        {
            bool_t bPaused = m_pReplayPlayer->IsPaused();
            if (ImGui::Checkbox("Pause", &bPaused))
                m_pReplayPlayer->SetPaused(bPaused);

            f32_t speed = m_pReplayPlayer->GetPlaybackRate();
            if (ImGui::SliderFloat("Speed", &speed, 0.25f, 4.f))
                m_pReplayPlayer->SetPlaybackRate(speed);

            ImGui::Text(
                "Tick: %llu / %llu",
                static_cast<unsigned long long>(m_pReplayPlayer->GetCurrentTick()),
                static_cast<unsigned long long>(m_pReplayPlayer->GetLastTick()));
        }

        ImGui::TextUnformatted(m_strReplayStatus.empty() ? "Replay playback" : m_strReplayStatus.c_str());
        if (ImGui::Button("나의 정보로 돌아가기"))
            m_bExitReplayToMyInfoRequested = true;
        ImGui::End();
        return;
    }

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
