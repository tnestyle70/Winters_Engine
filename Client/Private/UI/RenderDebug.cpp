#include "UI/RenderDebug.h"
#include "Scene/Scene_InGame.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace UI
{
	void CRenderDebugPanel::Render(CScene_InGame* pScene)
	{
        if (!pScene) return;

        ImGui::SetNextWindowPos(ImVec2(970.f, 50.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.f, 460.f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Render Debug")) { ImGui::End(); return; }

        bool b = pScene->IsShowRenderDebug();
        if (ImGui::Checkbox("Master (F1)", &b)) pScene->SetShowRenderDebug(b);

        ImGui::Separator();
        ImGui::SeparatorText("SSAO");
        const bool bSSAOAvailable = pScene->IsSSAOAvailable();
        ImGui::Text("Reference path: DX11 SSAO");
        if (!bSSAOAvailable)
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "SSAO unavailable on this backend");

        ImGui::BeginDisabled(!bSSAOAvailable);
        {
            bool bSSAO = pScene->GetSSAOEnabled();
            if (ImGui::Checkbox("SSAO", &bSSAO)) pScene->SetSSAOEnabled(bSSAO);

            f32_t fSSAORadius = pScene->GetSSAORadius();
            if (ImGui::SliderFloat("SSAO radius", &fSSAORadius, 0.05f, 4.0f, "%.2f"))
                pScene->SetSSAORadius(fSSAORadius);

            f32_t fSSAOIntensity = pScene->GetSSAOIntensity();
            if (ImGui::SliderFloat("SSAO intensity", &fSSAOIntensity, 0.1f, 4.0f, "%.2f"))
                pScene->SetSSAOIntensity(fSSAOIntensity);

            f32_t fSSAOThickness = pScene->GetSSAOThicknessHeuristic();
            if (ImGui::SliderFloat("SSAO thickness", &fSSAOThickness, 0.0f, 0.5f, "%.3f"))
                pScene->SetSSAOThicknessHeuristic(fSSAOThickness);

            if (ImGui::Button("Soft"))
            {
                pScene->SetSSAOEnabled(true);
                pScene->SetSSAORadius(0.9f);
                pScene->SetSSAOIntensity(1.1f);
                pScene->SetSSAOThicknessHeuristic(0.04f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Reference"))
            {
                pScene->SetSSAOEnabled(true);
                pScene->SetSSAORadius(1.2f);
                pScene->SetSSAOIntensity(1.6f);
                pScene->SetSSAOThicknessHeuristic(0.05f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Stress"))
            {
                pScene->SetSSAOEnabled(true);
                pScene->SetSSAORadius(2.5f);
                pScene->SetSSAOIntensity(3.5f);
                pScene->SetSSAOThicknessHeuristic(0.10f);
            }

            if (void* pAOSRV = pScene->GetSSAOOutputSRVNative())
            {
                ImGui::Text("AO preview");
                ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(pAOSRV)), ImVec2(220.f, 124.f));
            }
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Debug Draw");
        ImGui::BeginDisabled(!b);
        {
            bool bn = pScene->IsDbgShowNavGrid();
            if (ImGui::Checkbox("NavGrid blocked cells", &bn)) pScene->SetDbgShowNavGrid(bn);

            bool bp = pScene->IsDbgShowPathNavGrid();
            if (ImGui::Checkbox("PathGrid blocked cells", &bp)) pScene->SetDbgShowPathNavGrid(bp);

            bool bs = pScene->IsDbgShowStructures();
            if (ImGui::Checkbox("Structures / Jungle", &bs)) pScene->SetDbgShowStructures(bs);

            bool bc = pScene->IsDbgShowColliders();
            if (ImGui::Checkbox("ECS Colliders", &bc)) pScene->SetDbgShowColliders(bc);

            bool bh = pScene->IsDbgShowChampions();
            if (ImGui::Checkbox("Champions (cylinder)", &bh)) pScene->SetDbgShowChampions(bh);

            bool bm = pScene->IsDbgShowMinionMovement();
            if (ImGui::Checkbox("Minion cells / move vectors", &bm)) pScene->SetDbgShowMinionMovement(bm);

            f32_t r = pScene->GetDbgNavRadius();
            if (ImGui::SliderFloat("NavGrid radius (m)", &r, 10.f, 200.f))
                pScene->SetDbgNavRadius(r);

            ImGui::SeparatorText("Authored NavGrid");

            if (ImGui::Button("Reload Authored NavGrid"))
                pScene->RebuildMapWalkableNavGridForDebug();
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        if (ImGui::Button("All On"))
        {
            pScene->SetDbgShowNavGrid(true);
            pScene->SetDbgShowPathNavGrid(true);
            pScene->SetDbgShowStructures(true);
            pScene->SetDbgShowColliders(true);
            pScene->SetDbgShowChampions(true);
            pScene->SetDbgShowMinionMovement(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Sylas Only"))
        {
            pScene->SetDbgShowNavGrid(false);
            pScene->SetDbgShowPathNavGrid(false);
            pScene->SetDbgShowStructures(false);
            pScene->SetDbgShowColliders(false);
            pScene->SetDbgShowChampions(true);
            pScene->SetDbgShowMinionMovement(false);
        }

        ImGui::End();
	}
}
