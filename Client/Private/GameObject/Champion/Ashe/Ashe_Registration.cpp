#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Ashe/Ashe_Skills.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kAsh_BA_Cast = MakeHookId(eChampion::ASHE, HookVariant::BA_CastFrame);
    constexpr u32_t kAsh_Q_Cast = MakeHookId(eChampion::ASHE, HookVariant::Q_CastFrame);
    constexpr u32_t kAsh_W_Cast = MakeHookId(eChampion::ASHE, HookVariant::W_CastFrame);
    constexpr u32_t kAsh_E_Cast = MakeHookId(eChampion::ASHE, HookVariant::E_CastFrame);
    constexpr u32_t kAsh_R_Cast = MakeHookId(eChampion::ASHE, HookVariant::R_CastFrame);

    struct AsheAutoRegister
    {
        AsheAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::ASHE;
            cd.animPrefix = "ashe_";
            cd.idleAnimKey = "idle1";
            cd.runAnimKey = "run";
            cd.basicAttackKey = "attack1";
            cd.basicAttackRange = 6.0f;
            cd.fbxPath = "Texture/Character/Ashe/ashe.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            const wchar_t* asheBaseTexture =
                L"Texture/Character/Ashe/ashe_base_2011_tx_cm.png";
            cd.defaultTexturePath = asheBaseTexture;
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = asheBaseTexture;
            cd.spawnPosition = { 39.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Ashe";
            CChampionRegistry::Instance().Add(eChampion::ASHE, cd);

            {
                SkillDef s{};
                s.champ = eChampion::ASHE; s.slot = 0;
                s.animKey = "attack1";
                s.bOneShot = true;
                s.castHookId = kAsh_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::ASHE, 0, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::ASHE; s.slot = 1;
                s.animKey = "spell1";
                s.bOneShot = true;
                s.castHookId = kAsh_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::ASHE, 1, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::ASHE; s.slot = 2;
                s.animKey = "spell2";
                s.bOneShot = true;
                s.castHookId = kAsh_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::ASHE, 2, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::ASHE; s.slot = 3;
                s.animKey = "spell3";
                s.bOneShot = true;
                s.castHookId = kAsh_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::ASHE, 3, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::ASHE; s.slot = 4;
                s.animKey = "channel";
                s.bOneShot = true;
                s.castHookId = kAsh_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::ASHE, 4, s);
            }

            CSkillHookRegistry::Instance().Register(kAsh_BA_Cast, &Ashe::OnCastFrame_BA);
            CSkillHookRegistry::Instance().Register(kAsh_Q_Cast, &Ashe::OnCastFrame_Q);
            CSkillHookRegistry::Instance().Register(kAsh_W_Cast, &Ashe::OnCastFrame_W);
            CSkillHookRegistry::Instance().Register(kAsh_E_Cast, &Ashe::OnCastFrame_E);
            CSkillHookRegistry::Instance().Register(kAsh_R_Cast, &Ashe::OnCastFrame_R);

            CGameplayHookRegistry::Instance().Register(kAsh_BA_Cast, &Ashe::Gameplay::OnCastFrame_BA);
            CGameplayHookRegistry::Instance().Register(kAsh_Q_Cast, &Ashe::Gameplay::OnCastFrame_Q);
            CGameplayHookRegistry::Instance().Register(kAsh_W_Cast, &Ashe::Gameplay::OnCastFrame_W);
            CGameplayHookRegistry::Instance().Register(kAsh_E_Cast, &Ashe::Gameplay::OnCastFrame_E);
            CGameplayHookRegistry::Instance().Register(kAsh_R_Cast, &Ashe::Gameplay::OnCastFrame_R);

            CVisualHookRegistry::Instance().Register(kAsh_BA_Cast, &Ashe::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kAsh_Q_Cast, &Ashe::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kAsh_W_Cast, &Ashe::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kAsh_E_Cast, &Ashe::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kAsh_R_Cast, &Ashe::Visual::OnCastFrame_R_Visual);

        }
    };

    static AsheAutoRegister s_register;
}

void Ashe_KeepAlive()
{
    (void)&s_register;
}
