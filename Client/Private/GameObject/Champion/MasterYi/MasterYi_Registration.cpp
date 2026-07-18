#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kMasterYi_BA_Cast = MakeHookId(eChampion::MASTERYI, HookVariant::BA_CastFrame);
    constexpr u32_t kMasterYi_Q_Cast = MakeHookId(eChampion::MASTERYI, HookVariant::Q_CastFrame);
    constexpr u32_t kMasterYi_W_Cast = MakeHookId(eChampion::MASTERYI, HookVariant::W_CastFrame);
    constexpr u32_t kMasterYi_E_Cast = MakeHookId(eChampion::MASTERYI, HookVariant::E_CastFrame);
    constexpr u32_t kMasterYi_R_Cast = MakeHookId(eChampion::MASTERYI, HookVariant::R_CastFrame);

void RegisterSkill(u8_t slot, const char* animKey, u32_t hookId)
    {
        SkillDef s{};
        s.champ = eChampion::MASTERYI;
        s.slot = slot;
        s.animKey = animKey;
        s.bOneShot = true;
        s.castHookId = hookId;
        CSkillRegistry::Instance().Add(eChampion::MASTERYI, slot, s);
    }

    struct MasterYiAutoRegister
    {
        MasterYiAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::MASTERYI;
            cd.animPrefix = "";
            cd.idleAnimKey = "skinned_mesh_masteryi_2013_idle1";
            cd.runAnimKey = "skinned_mesh_masteryi_2013_run";
            cd.basicAttackKey = "skinned_mesh_masteryi_2013_attack1";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Texture/Character/MasterYi/masteryi.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            cd.defaultTexturePath = L"Texture/Character/MasterYi/masteryi_2013_tx_cm.png";
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = cd.defaultTexturePath;
            cd.spawnPosition = { 45.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "MasterYi";
            CChampionRegistry::Instance().Add(eChampion::MASTERYI, cd);

            RegisterSkill(0, "skinned_mesh_masteryi_2013_attack1", kMasterYi_BA_Cast);
            RegisterSkill(1, "skinned_mesh_masteryi_2013_attack2", kMasterYi_Q_Cast);
            RegisterSkill(2, "skinned_mesh_masteryi_2013_spell2", kMasterYi_W_Cast);
            RegisterSkill(3, "skinned_mesh_masteryi_spell3", kMasterYi_E_Cast);
            RegisterSkill(4, "skinned_mesh_masteryi_2013_run_haste", kMasterYi_R_Cast);

        }
    };

    static MasterYiAutoRegister s_register;
}

void MasterYi_KeepAlive()
{
    (void)&s_register;
}
