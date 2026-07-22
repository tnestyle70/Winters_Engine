#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Fiora/Fiora_Skills.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kFio_BA_Cast = MakeHookId(eChampion::FIORA, HookVariant::BA_CastFrame);
    constexpr u32_t kFio_Q_Cast  = MakeHookId(eChampion::FIORA, HookVariant::Q_CastFrame);
    constexpr u32_t kFio_W_Cast  = MakeHookId(eChampion::FIORA, HookVariant::W_CastFrame);
    constexpr u32_t kFio_E_Cast  = MakeHookId(eChampion::FIORA, HookVariant::E_CastFrame);
    constexpr u32_t kFio_R_Cast  = MakeHookId(eChampion::FIORA, HookVariant::R_CastFrame);
    constexpr u32_t kFio_Passive = MakeHookId(eChampion::FIORA, HookVariant::Passive_Trigger);
    constexpr u32_t kFio_W_Recovery = MakeHookId(eChampion::FIORA, HookVariant::W_Recovery);
    constexpr u32_t kFio_E_Recovery = MakeHookId(eChampion::FIORA, HookVariant::E_Recovery);
    constexpr u32_t kFio_R_Recovery = MakeHookId(eChampion::FIORA, HookVariant::R_Recovery);

    struct FioraAutoRegister
    {
        FioraAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::FIORA;
            cd.animPrefix    = "fiora_";
            cd.idleAnimKey   = "idle1";
            cd.runAnimKey    = "run";
            cd.basicAttackKey = "attack1";
            cd.basicAttackRange = 1.5f;
            cd.fbxPath = "Texture/Character/Fiora/fiora.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            const wchar_t* fioraBaseTexture =
                L"Texture/Character/Fiora/fiora_base_tx_cm.png";
            cd.defaultTexturePath = fioraBaseTexture;
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = fioraBaseTexture;
            cd.spawnPosition = { 30.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Fiora";
            CChampionRegistry::Instance().Add(eChampion::FIORA, cd);

            // BA
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 0;
                s.animKey = "attack1";
                s.bOneShot = true;
                s.castHookId = kFio_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 0, s);
            }
            // Q
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 1;
                s.animKey = "spell1";
                s.bOneShot = true;
                s.castHookId = kFio_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 1, s);
            }
            // W
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 2;
                s.animKey = "spell2";
                s.bOneShot = true;
                s.castHookId = kFio_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 2, s);
            }
            // E
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 3;
                s.animKey = "attack1";
                s.bOneShot = true;
                s.castHookId = kFio_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 3, s);
            }
            // R
            {
                SkillDef s{};
                s.champ = eChampion::FIORA; s.slot = 4;
                s.animKey = "channel";
                s.bOneShot = true;
                s.castHookId = kFio_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::FIORA, 4, s);
            }

            CVisualHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::Visual::OnCastFrame_R_Visual);
            CVisualHookRegistry::Instance().Register(kFio_Passive, &Fiora::Visual::OnPassiveTrigger_Visual);
            CVisualHookRegistry::Instance().Register(kFio_W_Recovery, &Fiora::Visual::OnRecovery_W_Visual);
            CVisualHookRegistry::Instance().Register(kFio_E_Recovery, &Fiora::Visual::OnRecovery_E_Visual);
            CVisualHookRegistry::Instance().Register(kFio_R_Recovery, &Fiora::Visual::OnRecovery_R_Visual);

        }
    };

    static FioraAutoRegister s_register;
}

void Fiora_KeepAlive()
{
    (void)&s_register;
}
