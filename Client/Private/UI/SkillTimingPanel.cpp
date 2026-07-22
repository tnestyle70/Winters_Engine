#include "UI/SkillTimingPanel.h"

#include "Client/Private/Data/LoLVisualDefinitionPack.h"
#include "GamePlay/SkillRegistry.h"
#include "Scene/Scene_InGame.h"
#include "WintersPaths.h"

#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#include <imgui.h>
#pragma pop_macro("new")

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{
    using json = nlohmann::json;

    constexpr std::array<eChampion, 17> kChampions =
    {
        eChampion::IRELIA, eChampion::YASUO, eChampion::KALISTA,
        eChampion::SYLAS, eChampion::VIEGO, eChampion::ANNIE,
        eChampion::ASHE, eChampion::FIORA, eChampion::GAREN,
        eChampion::RIVEN, eChampion::ZED, eChampion::EZREAL,
        eChampion::YONE, eChampion::JAX, eChampion::MASTERYI,
        eChampion::KINDRED, eChampion::LEESIN,
    };
    constexpr std::array<const char*, 5> kSlotNames = { "BA", "Q", "W", "E", "R" };
    constexpr const wchar_t* kVisualDraftRelativePath =
        L"Resource/Config/Practice/practice_visual_timing_overrides.json";
    constexpr const wchar_t* kPracticeAnchorRelativePath =
        L"Resource/Config/Practice/practice_balance_overrides.json";

    const char* ResolveChampionName(eChampion champion)
    {
        const ClientData::ChampionModelVisualDefinition* pModel =
            ClientData::FindChampionModelVisualDefinition(champion);
        return pModel && pModel->displayName ? pModel->displayName : "Unknown";
    }

    std::filesystem::path ResolveVisualDraftPath()
    {
        wchar_t resolved[1024]{};
        if (WintersResolveContentPath(
                kPracticeAnchorRelativePath,
                resolved,
                static_cast<u32_t>(std::size(resolved))))
        {
            return std::filesystem::path{ resolved }.parent_path() /
                L"practice_visual_timing_overrides.json";
        }
        return std::filesystem::path{ kVisualDraftRelativePath };
    }

    bool_t SaveVisualDraft(std::string& status)
    {
        json root{};
        root["version"] = 1;
        root["source"] = "Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json";
        root["scope"] = "visual-only-debug-draft";
        root["baseBuildHash"] = ClientData::GetLoLClientVisualDefinitionBuildHash();
        root["overrides"] = json::array();

        for (const eChampion champion : kChampions)
        {
            for (u8_t slot = 0u; slot < kSlotNames.size(); ++slot)
            {
                const SkillDef* pDef = CSkillRegistry::Instance().Find(champion, slot);
                if (!pDef)
                    continue;

                json stages = json::array();
                stages.push_back(
                    {
                        { "animationPlaybackSpeed", pDef->visualPlaySpeed },
                        { "castFrame", pDef->visualCastFrame },
                        { "recoveryFrame", pDef->visualRecoveryFrame },
                    });
                if (pDef->stageCount >= 2u)
                {
                    stages.push_back(
                        {
                            { "animationPlaybackSpeed", pDef->stage2VisualPlaySpeed },
                            { "castFrame", pDef->stage2VisualCastFrame },
                            { "recoveryFrame", pDef->stage2VisualRecoveryFrame },
                        });
                }

                root["overrides"].push_back(
                    {
                        { "champion", ResolveChampionName(champion) },
                        { "slot", kSlotNames[slot] },
                        { "stages", std::move(stages) },
                    });
            }
        }

        const std::filesystem::path path = ResolveVisualDraftPath();
        std::error_code ec{};
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
        {
            status = std::string{ "Save failed: " } + ec.message();
            return false;
        }

        std::ofstream file{ path, std::ios::binary | std::ios::trunc };
        if (!file.is_open())
        {
            status = "Save failed: draft file could not be opened.";
            return false;
        }
        file << root.dump(2) << '\n';
        if (!file.good())
        {
            status = "Save failed while writing the visual timing draft.";
            return false;
        }

        status = std::string{ "Saved visual-only draft: " } + path.string();
        return true;
    }

    void RenderStageEditor(
        eChampion champion,
        u8_t slot,
        u8_t stage,
        f32_t playbackSpeed,
        f32_t castFrame,
        f32_t recoveryFrame,
        std::string& status)
    {
        ImGui::PushID(static_cast<int>(stage));
        ImGui::Text("Stage %u", static_cast<u32_t>(stage + 1u));
        bool_t changed = false;
        changed |= ImGui::SliderFloat("Playback Speed", &playbackSpeed, 0.2f, 3.f, "%.2fx");
        changed |= ImGui::SliderFloat("Cast Frame", &castFrame, 0.f, 60.f, "%.1f");
        changed |= ImGui::SliderFloat("Recovery Frame", &recoveryFrame, 0.f, 90.f, "%.1f");
        recoveryFrame = std::max(recoveryFrame, castFrame);
        if (changed)
        {
            if (CSkillRegistry::Instance().ApplyVisualTimingOverride(
                    champion, slot, stage, playbackSpeed, castFrame, recoveryFrame))
            {
                status = "Applied visual timing to the current client session.";
            }
            else
            {
                status = "Visual timing rejected: check stage and frame ordering.";
            }
        }
        ImGui::PopID();
    }
}

namespace UI
{
    void CSkillTimingPanel::Render(CScene_InGame* /*pScene*/)
    {
        static int selectedChampion = 0;
        static int selectedSlot = 0;
        static std::string status =
            "Canonical timing is loaded from ChampionVisualDefs.json at registration.";

        ImGui::SetNextWindowPos(ImVec2(970.f, 260.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(440.f, 470.f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Skill Timing Tuner"))
        {
            ImGui::End();
            return;
        }

        ImGui::TextDisabled(
            "Visual only: playback/cast/recovery. Server cooldown and action lock are unchanged.");
        ImGui::TextWrapped(
            "Release source: Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json");

        const eChampion selected = kChampions[static_cast<size_t>(selectedChampion)];
        if (ImGui::BeginCombo("Champion", ResolveChampionName(selected)))
        {
            for (size_t index = 0u; index < kChampions.size(); ++index)
            {
                const bool_t isSelected = static_cast<int>(index) == selectedChampion;
                if (ImGui::Selectable(ResolveChampionName(kChampions[index]), isSelected))
                    selectedChampion = static_cast<int>(index);
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Combo("Slot", &selectedSlot, kSlotNames.data(), static_cast<int>(kSlotNames.size()));

        const eChampion champion = kChampions[static_cast<size_t>(selectedChampion)];
        const u8_t slot = static_cast<u8_t>(selectedSlot);
        const SkillDef* pDef = CSkillRegistry::Instance().Find(champion, slot);
        if (!pDef)
        {
            ImGui::TextDisabled("This champion/slot is not registered in the client.");
        }
        else
        {
            ImGui::Separator();
            ImGui::Text("Animation: %s", pDef->animKey ? pDef->animKey : "(none)");
            RenderStageEditor(
                champion, slot, 0u,
                pDef->visualPlaySpeed,
                pDef->visualCastFrame,
                pDef->visualRecoveryFrame,
                status);
            if (pDef->stageCount >= 2u)
            {
                RenderStageEditor(
                    champion, slot, 1u,
                    pDef->stage2VisualPlaySpeed,
                    pDef->stage2VisualCastFrame,
                    pDef->stage2VisualRecoveryFrame,
                    status);
            }

            const ClientData::ChampionVisualDefinition* pCanonical =
                ClientData::FindChampionVisualDefinition(champion);
            if (pCanonical && ImGui::Button("Reset Selected From Canonical JSON"))
            {
                const ClientData::SkillVisualDefinition& skill = pCanonical->skills[slot];
                for (u8_t stage = 0u; stage < skill.stageCount; ++stage)
                {
                    const ClientData::SkillVisualStageDef& timing = skill.stages[stage];
                    CSkillRegistry::Instance().ApplyVisualTimingOverride(
                        champion,
                        slot,
                        stage,
                        timing.animationPlaybackSpeed,
                        timing.castFrame,
                        timing.recoveryFrame);
                }
                status = "Selected timing reset from the generated canonical visual pack.";
            }
        }

        if (ImGui::Button("Save Visual Draft"))
            SaveVisualDraft(status);
        ImGui::TextWrapped("%s", status.c_str());
        ImGui::TextDisabled(
            "Drafts are not release truth: merge into ChampionVisualDefs.json, codegen, and build.");
        ImGui::End();
    }
}
