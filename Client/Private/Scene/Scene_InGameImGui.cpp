// Scene_InGameImGui.cpp — CScene_InGame의 debug/replay UI 책임 TU.
// Stage 1 (mechanical split): Scene_InGame.cpp에서 OnImGui + replay UI 메서드를 verbatim 이동.
// 동작/시그니처/호출 순서 불변. 설계: .md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md
#define _CRT_SECURE_NO_WARNINGS

#include "Network/Client/GameSessionClient.h"
#include "Replay/ReplayPlayer.h"

#include "Scene/Scene_InGame.h"
#include "Scene/Scene_Editor.h"
#include "Core/CInput.h"
#include "GameInstance.h"
#include "ProfilerAPI.h"
#include "Manager/Minion_Manager.h"

#include "UI/AIDebugPanel.h"
#include "UI/CombatDebugPanel.h"
#include "UI/MapTunerPanel.h"
#include "UI/RenderDebug.h"
#include "UI/SkillTimingPanel.h"
#include "UI/ChampionTuner.h"
#include "UI/EffectTuner.h"
#include "UI/WfxEffectToolPanel.h"
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
        m_bShowAIDebug = !m_bShowAIDebug;
    if (input.IsKeyPressed(VK_F8))
        m_bShowUITuner = !m_bShowUITuner;
    if (input.IsKeyPressed(VK_F7))
        m_bShowWfxEffectTool = !m_bShowWfxEffectTool;
    if (input.IsKeyPressed(VK_F6))
        m_bShowReplayControl = !m_bShowReplayControl;
    if (input.IsKeyPressed(VK_F10))
        m_bShowLegacyInGameDebug = !m_bShowLegacyInGameDebug;
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
    }

    if (m_bShowWfxEffectTool)
    {
        WINTERS_PROFILE_SCOPE("UI::WfxEffectTool");
        UI::CWfxEffectToolPanel::Render(this);
    }

    if (m_bReplayPlaybackMode || m_bShowReplayControl || m_bReplayStopRequested)
    {
        WINTERS_PROFILE_SCOPE("UI::Replay");
        DrawReplayControlPanel();
    }

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
        ImGui::End();
        return;
    }

    if (ImGui::Button("Stop"))
        SendStopReplayRequest();

    ImGui::TextUnformatted(m_strReplayStatus.empty() ? "Recording on server" : m_strReplayStatus.c_str());
    ImGui::End();
}
