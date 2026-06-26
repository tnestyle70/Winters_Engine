#include "UI/SkillTimingPanel.h"
#include "Scene/Scene_InGame.h"
#include "GameObject/SkillDef.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"   // eChampion

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace UI
{
    void CSkillTimingPanel::Render(CScene_InGame* /*pScene*/)
    {
        ImGui::SetNextWindowPos(ImVec2(970.f, 260.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.f, 420.f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Skill Timing Tuner")) { ImGui::End(); return; }

        for (uint32_t i = 0; i < g_SkillCount; ++i)
        {
            // const 캐스트 — 디버그 전용 런타임 튜너. 확정 값은 Copy 후 SkillTable.cpp 하드코딩.
            SkillDef& d = const_cast<SkillDef&>(g_SkillTable[i]);
            ImGui::PushID((int)i);

            const char* champName =
                (d.champ == eChampion::IRELIA) ? "Irelia" :
                (d.champ == eChampion::YASUO) ? "Yasuo" : "?";
            ImGui::Text("%s slot=%d anim=%s", champName, (int)d.slot,
                d.animKey ? d.animKey : "(null)");
            ImGui::SliderFloat("castFrame", &d.visualCastFrame, 0.f, 60.f);
            ImGui::SliderFloat("recoveryFrame", &d.visualRecoveryFrame, 0.f, 60.f);
            ImGui::SliderFloat("lockDuration", &d.lockDurationSec, 0.1f, 5.f, "%.2f s");
            ImGui::SliderFloat("animPlaySpeed", &d.visualPlaySpeed, 0.2f, 3.f, "%.2fx");
            if (d.stageCount == 2)
            {
                ImGui::SliderFloat("stg2 cast", &d.stage2VisualCastFrame, 0.f, 60.f);
                ImGui::SliderFloat("stg2 recovery", &d.stage2VisualRecoveryFrame, 0.f, 60.f);
                ImGui::SliderFloat("stg2 lock", &d.stage2LockSec, 0.1f, 5.f, "%.2f s");
                ImGui::SliderFloat("stg2 playSpeed", &d.stage2VisualPlaySpeed, 0.2f, 3.f, "%.2fx");
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        ImGui::End();
    }
}