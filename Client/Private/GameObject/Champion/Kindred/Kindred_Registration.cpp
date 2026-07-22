#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kKindred_BA_Cast = MakeHookId(eChampion::KINDRED, HookVariant::BA_CastFrame);
    constexpr u32_t kKindred_Q_Cast = MakeHookId(eChampion::KINDRED, HookVariant::Q_CastFrame);
    constexpr u32_t kKindred_W_Cast = MakeHookId(eChampion::KINDRED, HookVariant::W_CastFrame);
    constexpr u32_t kKindred_E_Cast = MakeHookId(eChampion::KINDRED, HookVariant::E_CastFrame);
    constexpr u32_t kKindred_R_Cast = MakeHookId(eChampion::KINDRED, HookVariant::R_CastFrame);

void RegisterSkill(u8_t slot, const char* animKey, u32_t hookId)
    {
        SkillDef s{};
        s.champ = eChampion::KINDRED;
        s.slot = slot;
        s.animKey = animKey;
        s.bOneShot = true;
        s.castHookId = hookId;
        CSkillRegistry::Instance().Add(eChampion::KINDRED, slot, s);
    }

    struct KindredAutoRegister
    {
        KindredAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::KINDRED;
            cd.animPrefix = "";
            cd.idleAnimKey = "skinned_mesh_lamb_idle";
            cd.runAnimKey = "skinned_mesh_lamb_run";
            cd.basicAttackKey = "skinned_mesh_lamb_attack1";
            cd.basicAttackRange = 5.5f;
            cd.fbxPath = "Texture/Character/Kindred/kindred.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            cd.defaultTexturePath = L"Texture/Character/Kindred/kindred_base_tx_cm.png";
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = cd.defaultTexturePath;
            cd.spawnPosition = { 42.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Kindred";
            CChampionRegistry::Instance().Add(eChampion::KINDRED, cd);

            RegisterSkill(0, "skinned_mesh_lamb_attack1", kKindred_BA_Cast);
            RegisterSkill(1, "skinned_mesh_lamb_spell1forward", kKindred_Q_Cast);
            RegisterSkill(2, "skinned_mesh_lamb_spell2", kKindred_W_Cast);
            RegisterSkill(3, "skinned_mesh_lamb_spell3", kKindred_E_Cast);
            RegisterSkill(4, "skinned_mesh_lamb_spell4", kKindred_R_Cast);

        }
    };

    static KindredAutoRegister s_register;
}

void Kindred_KeepAlive()
{
    (void)&s_register;
}
