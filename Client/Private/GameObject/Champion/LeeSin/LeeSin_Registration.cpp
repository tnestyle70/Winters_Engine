#include "GameObject/Champion/LeeSin/LeeSin_Skills.h"

#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kLeeSin_BA_Cast = MakeHookId(eChampion::LEESIN, HookVariant::BA_CastFrame);
    constexpr u32_t kLeeSin_Q_Cast = MakeHookId(eChampion::LEESIN, HookVariant::Q_CastFrame);
    constexpr u32_t kLeeSin_W_Cast = MakeHookId(eChampion::LEESIN, HookVariant::W_CastFrame);
    constexpr u32_t kLeeSin_E_Cast = MakeHookId(eChampion::LEESIN, HookVariant::E_CastFrame);
    constexpr u32_t kLeeSin_R_Cast = MakeHookId(eChampion::LEESIN, HookVariant::R_CastFrame);

void RegisterSkill(u8_t slot, const char* animKey, u32_t hookId)
    {
        SkillDef s{};
        s.champ = eChampion::LEESIN;
        s.slot = slot;
        s.animKey = animKey;
        s.bOneShot = true;
        s.castHookId = hookId;
        if (slot == static_cast<u8_t>(eSkillSlot::Q))
        {
            s.stage2AnimKey = "skinned_mesh_spell1_b";
            }
        else if (slot == static_cast<u8_t>(eSkillSlot::W))
        {
            s.stage2AnimKey = "skinned_mesh_spell2b_idle";
            }
        else if (slot == static_cast<u8_t>(eSkillSlot::E))
        {
            s.stage2AnimKey = "skinned_mesh_spell3_b_idle";
            }
        CSkillRegistry::Instance().Add(eChampion::LEESIN, slot, s);
    }

    struct LeeSinAutoRegister
    {
        LeeSinAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::LEESIN;
            cd.animPrefix = "";
            cd.idleAnimKey = "skinned_mesh_idle_passive";
            cd.runAnimKey = "skinned_mesh_run_base";
            cd.basicAttackKey = "skinned_mesh_attack1";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Texture/Character/LeeSin/leesin.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            cd.defaultTexturePath = L"Texture/Character/LeeSin/leesin_base_tx_cm.png";
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = cd.defaultTexturePath;
            cd.spawnPosition = { 39.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "LeeSin";
            CChampionRegistry::Instance().Add(eChampion::LEESIN, cd);

            RegisterSkill(0, "skinned_mesh_attack1", kLeeSin_BA_Cast);
            RegisterSkill(1, "skinned_mesh_spell1_cast", kLeeSin_Q_Cast);
            RegisterSkill(2, "skinned_mesh_spell2_fly", kLeeSin_W_Cast);
            RegisterSkill(3, "skinned_mesh_spell3_cast", kLeeSin_E_Cast);
            RegisterSkill(4, "skinned_mesh_spell4a", kLeeSin_R_Cast);

            CVisualHookRegistry::Instance().Register(kLeeSin_Q_Cast, &LeeSin::Visual::OnQCastFrame);
            CVisualHookRegistry::Instance().Register(kLeeSin_W_Cast, &LeeSin::Visual::OnWCastFrame);
            CVisualHookRegistry::Instance().Register(kLeeSin_E_Cast, &LeeSin::Visual::OnECastFrame);
            CVisualHookRegistry::Instance().Register(kLeeSin_R_Cast, &LeeSin::Visual::OnRCastFrame);

        }
    };

    static LeeSinAutoRegister s_register;
}

void LeeSin_KeepAlive()
{
    (void)&s_register;
}
