#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/SkillDef.h"
#include "GameObject/Champion/Annie/Annie_Skills.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kAnn_BA_Cast = MakeHookId(eChampion::ANNIE, HookVariant::BA_CastFrame);
    constexpr u32_t kAnn_Q_Cast = MakeHookId(eChampion::ANNIE, HookVariant::Q_CastFrame);
    constexpr u32_t kAnn_W_Cast = MakeHookId(eChampion::ANNIE, HookVariant::W_CastFrame);
    constexpr u32_t kAnn_E_Cast = MakeHookId(eChampion::ANNIE, HookVariant::E_CastFrame);
    constexpr u32_t kAnn_R_Cast = MakeHookId(eChampion::ANNIE, HookVariant::R_CastFrame);
    constexpr u32_t kAnn_Q_Accepted = MakeHookId(eChampion::ANNIE, HookVariant::Q_OnCastAccepted);
    constexpr u32_t kAnn_W_Accepted = MakeHookId(eChampion::ANNIE, HookVariant::W_OnCastAccepted);
    constexpr u32_t kAnn_E_Accepted = MakeHookId(eChampion::ANNIE, HookVariant::E_OnCastAccepted);
    constexpr u32_t kAnn_R_Accepted = MakeHookId(eChampion::ANNIE, HookVariant::R_OnCastAccepted);
    constexpr f32_t kAnnieSkillTestSeconds = 0.2f;

    struct AnnieAutoRegister
    {
        AnnieAutoRegister()
        {
            ChampionDef cd{};
            cd.id = eChampion::ANNIE;
            cd.animPrefix = "";
            cd.idleAnimKey = "annie_2012_idle1";
            cd.runAnimKey = "annie_2012_run";
            cd.basicAttackKey = "annie_2012_attack1";
            cd.basicAttackRange = 6.25f;
            cd.fbxPath = "Client/Bin/Resource/Texture/Character/Annie/annie.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            const wchar_t* annieBaseTexture =
                L"Client/Bin/Resource/Texture/Character/Annie/annie_base_2012_cm.png";
            cd.defaultTexturePath = annieBaseTexture;
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = annieBaseTexture;
            cd.spawnPosition = { 36.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "Annie";
            CChampionRegistry::Instance().Add(eChampion::ANNIE, cd);

            {
                SkillDef s{};
                s.champ = eChampion::ANNIE; s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = kAnnieSkillTestSeconds; s.rangeMax = 6.25f; s.manaCost = 0.f;
                s.animKey = "annie_2012_attack1";
                s.lockDurationSec = kAnnieSkillTestSeconds; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 6.f; s.recoveryFrame = 14.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAnn_BA_Cast;
                CSkillRegistry::Instance().Add(eChampion::ANNIE, 0, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::ANNIE; s.slot = 1;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = kAnnieSkillTestSeconds; s.rangeMax = 6.25f; s.manaCost = 60.f;
                s.animKey = "annie_2012_spell1";
                s.lockDurationSec = kAnnieSkillTestSeconds; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.castFrame = 5.f; s.recoveryFrame = 10.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAnn_Q_Cast;
                CSkillRegistry::Instance().Add(eChampion::ANNIE, 1, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::ANNIE; s.slot = 2;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = kAnnieSkillTestSeconds; s.rangeMax = 6.0f; s.manaCost = 80.f;
                s.animKey = "annie_2012_spell2";
                s.lockDurationSec = kAnnieSkillTestSeconds; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 5.f; s.recoveryFrame = 12.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAnn_W_Cast;
                CSkillRegistry::Instance().Add(eChampion::ANNIE, 2, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::ANNIE; s.slot = 3;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = kAnnieSkillTestSeconds; s.rangeMax = 0.f; s.manaCost = 30.f;
                s.animKey = "annie_spell3";
                s.lockDurationSec = kAnnieSkillTestSeconds; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 1.f; s.recoveryFrame = 8.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAnn_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::ANNIE, 3, s);
            }
            {
                SkillDef s{};
                s.champ = eChampion::ANNIE; s.slot = 4;
                s.targetMode = eTargetMode::GroundTarget;
                s.cooldownSec = kAnnieSkillTestSeconds; s.rangeMax = 6.0f; s.manaCost = 100.f;
                s.animKey = "annie_2012_spell4";
                s.lockDurationSec = kAnnieSkillTestSeconds; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.castFrame = 12.f; s.recoveryFrame = 24.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kAnn_R_Cast;
                CSkillRegistry::Instance().Add(eChampion::ANNIE, 4, s);
            }

            CSkillHookRegistry::Instance().Register(kAnn_BA_Cast, &Annie::OnCastFrame_BA);
            CSkillHookRegistry::Instance().Register(kAnn_Q_Cast, &Annie::OnCastFrame_Q);
            CSkillHookRegistry::Instance().Register(kAnn_W_Cast, &Annie::OnCastFrame_W);
            CSkillHookRegistry::Instance().Register(kAnn_E_Cast, &Annie::OnCastFrame_E);
            CSkillHookRegistry::Instance().Register(kAnn_R_Cast, &Annie::OnCastFrame_R);

            CVisualHookRegistry::Instance().Register(kAnn_BA_Cast, &Annie::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kAnn_Q_Accepted, &Annie::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kAnn_W_Accepted, &Annie::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kAnn_E_Accepted, &Annie::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kAnn_R_Accepted, &Annie::Visual::OnCastFrame_R_Visual);

            OutputDebugStringA("[Annie] Registration complete\n");
        }
    };

    static AnnieAutoRegister s_register;
}

void Annie_KeepAlive()
{
    (void)&s_register;
}
