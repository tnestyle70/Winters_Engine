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
#include "UI/EffectTuner.h"
#include "UI/WfxEffectToolPanel.h"
#include "UI/ModelAnimPanel.h"
#include "Network/Client/NetworkEventTrace.h"

#include <algorithm>

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
    // F4 = 서버 권위 수치를 카테고리별로 조절하는 통합 Balance Lab.
    // 엔진 전역 프로파일러 JSON 캡처는 F12 로 이전 (CEngineApp.cpp).
    if (input.IsKeyPressed(VK_F4))
    {
        m_bShowBalanceTuner = !m_bShowBalanceTuner;
        if (m_bShowBalanceTuner)
            UI::CChampionTuner::Open(UI::eBalanceTunerCategory::Champions);
    }
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
        struct MinimapTunerContext
        {
            bool_t bProjectionSyncAvailable = false;
            bool_t bProjectionChanged = false;
            UI::MinimapProjection AppliedProjection{};
        };
        MinimapTunerContext minimapContext{};
        minimapContext.bProjectionSyncAvailable = m_pVisionSystem != nullptr;
        const auto drawExternalTabs = [](void* user)
        {
            auto* context = static_cast<MinimapTunerContext*>(user);
            if (!context || !ImGui::BeginTabItem("Minimap"))
                return;
            context->bProjectionChanged =
                UI::CMinimapPanel::DrawTunerContentsImGui(
                    context->bProjectionSyncAvailable,
                    context->AppliedProjection);
            ImGui::EndTabItem();
        };
        const auto saveExternalTabs = [](void*) -> bool_t
        {
            return UI::CMinimapPanel::SaveTunerSettings();
        };
        CGameInstance::Get()->UI_OnImGui_Tuner(
            drawExternalTabs,
            saveExternalTabs,
            &minimapContext);
        if (minimapContext.bProjectionChanged && m_pVisionSystem)
        {
            Engine::CVisionSystem::FowProjection fowProjection{};
            fowProjection.vWorldAtUv00 =
                minimapContext.AppliedProjection.vWorldAtUv00;
            fowProjection.vWorldAtUv10 =
                minimapContext.AppliedProjection.vWorldAtUv10;
            fowProjection.vWorldAtUv01 =
                minimapContext.AppliedProjection.vWorldAtUv01;
            m_pVisionSystem->SetFowProjection(fowProjection);
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

    if (m_bShowBalanceTuner)
    {
        WINTERS_PROFILE_SCOPE("UI::BalanceTuner");
        UI::CChampionTuner::Render(this);
    }

    DrawGameEndOverlay();

    if (!m_bShowLegacyInGameDebug)
        return;

    UI::CCombatDebugPanel::Render(m_World, this);
    UI::CMapTunerPanel::Render(this);
    if (!m_bShowBalanceTuner)
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

bool_t CScene_InGame::ApplyReplaySpectatorFocus()
{
    if (!m_pEntityIdMap)
        return false;

    if (m_replayPerspectiveNetId != NULL_NET_ENTITY)
    {
        if (ApplyAuthoritativePlayerNetId(m_replayPerspectiveNetId))
            return true;
        m_strReplayStatus = "Replay account perspective is unavailable";
        return false;
    }

    NetEntityId focusNetId = NULL_NET_ENTITY;
    m_pEntityIdMap->ForEachBinding(
        [this, &focusNetId](NetEntityId netId, EntityID entity)
        {
            if (m_World.IsAlive(entity) &&
                m_World.HasComponent<ChampionComponent>(entity) &&
                (focusNetId == NULL_NET_ENTITY || netId < focusNetId))
            {
                focusNetId = netId;
            }
        });
    return ApplyAuthoritativePlayerNetId(focusNetId);
}

void CScene_InGame::UpdateReplayPlayback(f32_t dt)
{
    if (!m_pReplayPlayer || !m_pEntityIdMap || !m_pSnapshotApplier || !m_pEventApplier)
        return;

    const bool_t bWasFinished = m_pReplayPlayer->IsFinished();
    if (m_pReplayPlayer->Update(
        dt,
        m_World,
        *m_pEntityIdMap,
        *m_pSnapshotApplier,
        *m_pEventApplier))
    {
        const bool_t bFocusApplied = ApplyReplaySpectatorFocus();
        ProjectGameplayActorsToMapSurface();
        if (!bFocusApplied)
            return;
    }

    const std::string& playbackError = m_pReplayPlayer->GetPlaybackError();
    if (!playbackError.empty())
    {
        m_strReplayStatus = playbackError;
        m_fReplayAutoReturnRemainingSec = -1.f;
        return;
    }

    constexpr f32_t kReplayAutoReturnDelaySec = 2.f;
    if (!bWasFinished &&
        m_pReplayPlayer->IsFinished() &&
        !m_pReplayPlayer->IsPaused())
    {
        m_fReplayAutoReturnRemainingSec = kReplayAutoReturnDelaySec;
    }
    else if (m_fReplayAutoReturnRemainingSec >= 0.f)
    {
        m_fReplayAutoReturnRemainingSec =
            (std::max)(0.f, m_fReplayAutoReturnRemainingSec - dt);
    }

    if (m_fReplayAutoReturnRemainingSec < 0.f)
        return;

    m_strReplayStatus = "Replay complete - returning to main menu";
    if (m_fReplayAutoReturnRemainingSec <= 0.f)
        m_bReturnToMainMenuRequested = true;
}

bool_t CScene_InGame::SeekReplayToTick(u64_t targetTick)
{
    if (!m_pReplayPlayer || !m_pEntityIdMap || !m_pSnapshotApplier || !m_pEventApplier)
        return false;

    m_fReplayAutoReturnRemainingSec = -1.f;
    const bool_t bApplied = m_pReplayPlayer->SeekToTick(
        targetTick,
        m_World,
        *m_pEntityIdMap,
        *m_pSnapshotApplier,
        *m_pEventApplier);
    if (bApplied)
    {
        const bool_t bFocusApplied = ApplyReplaySpectatorFocus();
        ProjectGameplayActorsToMapSurface();
        if (!bFocusApplied)
            return false;
        m_strReplayStatus = "Replay Chrono seek complete";
        return true;
    }

    const std::string& error = m_pReplayPlayer->GetPlaybackError();
    m_strReplayStatus = error.empty() ? "Replay Chrono seek failed" : error;
    return false;
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
    if (m_bReplayPlaybackMode)
    {
        ImGuiIO& io = ImGui::GetIO();
        constexpr f32_t kPanelTop = 4.f;
        constexpr f32_t kPerfOverlayLeft = 10.f;
        constexpr f32_t kPanelGap = 16.f;
        constexpr f32_t kMatchHudLeftReserve = 400.f;
        const f32_t panelLeft =
            kPerfOverlayLeft +
            ImGui::CalcTextSize(
                "Draw: 2147483647 verts, 2147483647 indices").x +
            ImGui::GetStyle().WindowPadding.x * 2.f +
            kPanelGap;
        const f32_t panelRight =
            io.DisplaySize.x - kMatchHudLeftReserve - kPanelGap;
        const f32_t availableWidth = (std::max)(
            240.f,
            panelRight - panelLeft);
        const ImVec2 windowSize((std::min)(640.f, availableWidth), 160.f);
        ImGui::SetNextWindowPos(
            ImVec2(panelLeft, kPanelTop),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings;
        if (!ImGui::Begin("Replay Chrono Break", nullptr, flags))
        {
            ImGui::End();
            return;
        }

        if (m_pReplayPlayer)
        {
            const u64_t firstTick = m_pReplayPlayer->GetFirstSeekableTick();
            const u64_t lastTick = m_pReplayPlayer->GetLastTick();
            u64_t selectedTick = (std::max)(
                firstTick,
                m_pReplayPlayer->GetCurrentTick());
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::SliderScalar(
                "##ReplayTimeline",
                ImGuiDataType_U64,
                &selectedTick,
                &firstTick,
                &lastTick,
                "%llu"))
            {
                m_pReplayPlayer->SetPaused(true);
                SeekReplayToTick(selectedTick);
            }

            const f32_t tickRate = m_pReplayPlayer->GetTickRate();
            const double currentSec = tickRate > 0.f
                ? static_cast<double>(selectedTick - firstTick) / tickRate
                : 0.0;
            const double totalSec = tickRate > 0.f
                ? static_cast<double>(lastTick - firstTick) / tickRate
                : 0.0;
            ImGui::Text("Time: %.2f / %.2f sec", currentSec, totalSec);

            if (ImGui::Button("Restart"))
            {
                if (SeekReplayToTick(firstTick))
                    m_pReplayPlayer->SetPaused(false);
            }
            ImGui::SameLine();
            const bool_t bPaused = m_pReplayPlayer->IsPaused();
            if (ImGui::Button(bPaused ? "Play" : "Pause"))
                m_pReplayPlayer->SetPaused(!bPaused);
            ImGui::SameLine();
            f32_t speed = m_pReplayPlayer->GetPlaybackRate();
            ImGui::SetNextItemWidth(80.f);
            if (ImGui::SliderFloat(
                "##ReplaySpeed",
                &speed,
                0.25f,
                4.f,
                "%.2fx"))
                m_pReplayPlayer->SetPlaybackRate(speed);

            if (ImGui::Button("프로필로 돌아가기"))
                m_bExitReplayToMyInfoRequested = true;
            ImGui::SameLine();
            if (ImGui::Button("메인 메뉴로"))
                m_bReturnToMainMenuRequested = true;
        }

        const char* pStatus = m_strReplayStatus.empty()
            ? "Replay playback"
            : m_strReplayStatus.c_str();
        if (ImGui::CalcTextSize(pStatus).x <= ImGui::GetContentRegionAvail().x)
        {
            ImGui::TextUnformatted(pStatus);
        }
        else
        {
            ImGui::TextUnformatted("Replay status (hover for details)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", pStatus);
        }
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(320.f, 100.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Replay Recording"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Stop & Save Replay"))
        SendStopReplayRequest();

    ImGui::TextUnformatted(
        m_strReplayStatus.empty() ? "Recording on server" : m_strReplayStatus.c_str());
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
        if (ImGui::Button("프로필 / 다시보기", ImVec2(180.f, 0.f)))
            m_bExitReplayToMyInfoRequested = true;
        ImGui::SameLine();
        if (ImGui::Button("메인 메뉴로", ImVec2(150.f, 0.f)))
            m_bReturnToMainMenuRequested = true;
    }
    ImGui::End();
}
