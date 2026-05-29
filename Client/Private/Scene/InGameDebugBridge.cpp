#include "Scene/InGameDebugBridge.h"

#include "Core/CInput.h"
#include "ECS/Systems/GameplayCollisionSystem.h"
#include "ECS/Systems/MinionSeparationSystem.h"
#include "Manager/Minion_Manager.h"
#include "Network/Client/NetworkEventTrace.h"
#include "Renderer/FogOfWarRenderer.h"
#include "Scene/Scene_InGame.h"
#include "UI/ChampionTuner.h"
#include "UI/CombatDebugPanel.h"
#include "UI/EffectTuner.h"
#include "UI/MapTunerPanel.h"
#include "UI/MinimapPanel.h"
#include "UI/RenderDebug.h"
#include "UI/SkillTimingPanel.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

void CInGameDebugBridge::Render(InGameDebugBridgeDesc& desc)
{
    auto& input = CInput::Get();

    if (input.IsKeyPressed(VK_F1))
        desc.scene.SetShowRenderDebug(!desc.scene.IsShowRenderDebug());

    UI::CCombatDebugPanel::Render(desc.world, &desc.scene);
    UI::CMapTunerPanel::Render(&desc.scene);
    UI::CChampionTuner::Render(&desc.scene);
    UI::CEffectTuner::Render(&desc.scene);
    UI::CRenderDebugPanel::Render(&desc.scene);
    UI::CSkillTimingPanel::Render(&desc.scene);
    UI::CMinimapPanel::Render(
        desc.pFogOfWarRenderer,
        desc.world,
        static_cast<u8_t>(desc.playerTeam));
    CNetworkEventTrace::Instance().DrawImGui();

    ImGui::SetNextWindowSize(ImVec2(220.f, 120.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(180.f, 90.f), ImVec2(360.f, 220.f));
    if (ImGui::Begin("Camera"))
    {
        if (desc.pCamera)
        {
            Vec3 eye = desc.pCamera->GetEye();
            ImGui::Text("Eye: (%.1f, %.1f, %.1f)", eye.x, eye.y, eye.z);
        }
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        if (desc.pCamera)
        {
            bool bFollow = desc.pCamera->IsFollowMode();
            if (ImGui::Checkbox("Follow Mode (F2)", &bFollow))
                desc.pCamera->SetFollowMode(bFollow);
        }
        ImGui::Checkbox("Log Frame Events", reinterpret_cast<bool*>(&desc.bLogFrameEvents));
    }
    ImGui::End();

    if (desc.pCamera)
        desc.pCamera->OnImGui();
    CMinion_Manager::Get()->OnImGui_Tuner();

    if (Engine::CGameplayCollisionSystem* pCollision = desc.scene.GetGameplayCollisionSystem())
    {
        if (ImGui::Begin("Gameplay Collision"))
        {
            bool_t bEnabled = pCollision->Get_Enabled();
            if (ImGui::Checkbox("Enabled", &bEnabled))
                pCollision->Set_Enabled(bEnabled);

            i32_t iterations = pCollision->Get_Iterations();
            if (ImGui::SliderInt("Iterations", &iterations, 1, 4))
                pCollision->Set_Iterations(iterations);

            f32_t pushStrength = pCollision->Get_PushStrength();
            if (ImGui::SliderFloat("Push Strength", &pushStrength, 0.f, 1.5f, "%.2f"))
                pCollision->Set_PushStrength(pushStrength);
        }
        ImGui::End();
    }

    if (Engine::CMinionSeparationSystem* pSep = desc.scene.GetMinionSeparationSystem())
    {
        if (ImGui::Begin("Minion Separation"))
        {
            bool_t bEnabled = pSep->Get_Enabled();
            if (ImGui::Checkbox("Enabled", &bEnabled))
                pSep->Set_Enabled(bEnabled);

            f32_t radius = pSep->Get_SeparationRadius();
            if (ImGui::SliderFloat("Radius", &radius, 0.f, 3.f, "%.2f"))
                pSep->Set_SeparationRadius(radius);

            f32_t weight = pSep->Get_SeparationWeight();
            if (ImGui::SliderFloat("Weight", &weight, 0.f, 1.5f, "%.2f"))
                pSep->Set_SeparationWeight(weight);

            i32_t maxNeighbors = pSep->Get_MaxNeighbors();
            if (ImGui::SliderInt("Max Neighbors", &maxNeighbors, 1, 16))
                pSep->Set_MaxNeighbors(maxNeighbors);
        }
        ImGui::End();
    }

}
