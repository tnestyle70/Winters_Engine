#include "UI/MinimapPanel.h"
#include "Renderer/FogOfWarRenderer.h"
#include "ECS/World.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/Systems/VisionSystem.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace UI
{
    void CMinimapPanel::Render(CFogOfWarRenderer* pFow, CWorld& world, u8_t localTeam)
    {
        ImGui::SetNextWindowPos(ImVec2(20.f, 720.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260.f, 280.f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Minimap"))
        {
            ImGui::End();
            return;
        }

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const f32_t rawSide = (avail.x < avail.y) ? avail.x : avail.y;
        const f32_t side = (rawSide < 64.f) ? 64.f : rawSide;
        const ImVec2 imagePos = ImGui::GetCursorScreenPos();

        if (pFow && pFow->Get_NativeSRV())
            ImGui::Image(reinterpret_cast<ImTextureID>(pFow->Get_NativeSRV()), ImVec2(side, side));
        else
            ImGui::Dummy(ImVec2(side, side));

        ImDrawList* pDrawList = ImGui::GetWindowDrawList();
        constexpr f32_t kWorld = Engine::CVisionSystem::FOW_TEX_WORLD_SIZE;

        world.ForEach<TransformComponent, SpatialAgentComponent, VisibilityComponent>(
            function<void(EntityID, TransformComponent&, SpatialAgentComponent&, VisibilityComponent&)>(
                [&](EntityID, TransformComponent& xf, SpatialAgentComponent& agent, VisibilityComponent& vis)
                {
                    const bool_t bMine = (agent.team == localTeam);
                    const bool_t bVisible = (vis.teamVisibilityMask & (1u << localTeam)) != 0;
                    if (!bMine && !bVisible)
                        return;

                    const Vec3 pos = xf.GetPosition();
                    const f32_t u = (pos.x + kWorld * 0.5f) / kWorld;
                    const f32_t v = (pos.z + kWorld * 0.5f) / kWorld;
                    if (u < 0.f || u > 1.f || v < 0.f || v > 1.f)
                        return;

                    const ImVec2 center{
                        imagePos.x + u * side,
                        imagePos.y + v * side
                    };

                    const ImU32 color = bMine
                        ? IM_COL32(80, 160, 255, 255)
                        : IM_COL32(255, 80, 80, 255);
                    const f32_t radius = (agent.kind == eSpatialKind::Champion) ? 4.f : 2.f;
                    pDrawList->AddCircleFilled(center, radius, color);
                }));

        ImGui::End();
    }
}
