#include "UI/MapTunerPanel.h"
#include "Scene/Scene_InGame.h"
#include "WintersMath.h"
#include <DirectXMath.h>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace UI
{
	void CMapTunerPanel::Render(CScene_InGame* pScene)
	{
        if (!pScene || !pScene->IsShowMapTuner()) return;

        ImGui::SetNextWindowPos(ImVec2(640.f, 320.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.f, 220.f), ImGuiCond_FirstUseEver);
        bool_t bOpen = pScene->IsShowMapTuner();
        if (!ImGui::Begin("Map Rotation Tuner", &bOpen)) { ImGui::End(); return; }
        pScene->SetShowMapTuner(bOpen);

        Vec3 rot = pScene->GetMapRotation();
        Vec3 deg{
            DirectX::XMConvertToDegrees(rot.x),
            DirectX::XMConvertToDegrees(rot.y),
            DirectX::XMConvertToDegrees(rot.z)
        };
        bool changed = false;
        changed |= ImGui::SliderFloat("X (deg)", &deg.x, -180.f, 180.f);
        changed |= ImGui::SliderFloat("Y (deg)", &deg.y, -180.f, 180.f);
        changed |= ImGui::SliderFloat("Z (deg)", &deg.z, -180.f, 180.f);
        if (changed)
        {
            pScene->SetMapRotation({
                DirectX::XMConvertToRadians(deg.x),
                DirectX::XMConvertToRadians(deg.y),
                DirectX::XMConvertToRadians(deg.z),
                });
        }

        if (ImGui::Button("Reset (0,0,0)")) pScene->SetMapRotation({ 0.f, 0.f, 0.f });
        ImGui::SameLine();
        if (ImGui::Button("Y +90")) pScene->SetMapRotation({ 0.f, DirectX::XMConvertToRadians(90.f), 0.f });
        ImGui::SameLine();
        if (ImGui::Button("Y -90")) pScene->SetMapRotation({ 0.f, DirectX::XMConvertToRadians(-90.f), 0.f });
        ImGui::SameLine();
        if (ImGui::Button("Y 180")) pScene->SetMapRotation({ 0.f, DirectX::XMConvertToRadians(180.f), 0.f });

		ImGui::End();
	}
}
