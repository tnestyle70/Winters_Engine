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

    f32_t ResolveRange(u8_t slot)
    {
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::BasicAttack: return 1.5f;
        case eSkillSlot::Q: return 11.f;
        case eSkillSlot::W: return 7.f;
        case eSkillSlot::E: return 4.f;
        case eSkillSlot::R: return 3.f;
        default: return 0.f;
        }
    }

    void RegisterSkill(u8_t slot, eTargetMode targetMode, const char* animKey, u32_t hookId)
    {
        SkillDef s{};
        s.champ = eChampion::LEESIN;
        s.slot = slot;
        s.targetMode = targetMode;
        s.cooldownSec = 0.6f;
        s.rangeMax = ResolveRange(slot);
        s.animKey = animKey;
        s.lockDurationSec = 0.6f;
        s.bOneShot = true;
        s.rotate = targetMode == eTargetMode::Self ? eRotateMode::None : eRotateMode::TowardsCursor;
        s.castFrame = 4.f;
        s.recoveryFrame = 12.f;
        s.animPlaySpeed = 1.f;
        s.castFrameHookId = hookId;
        if (slot == static_cast<u8_t>(eSkillSlot::Q))
        {
            s.stageCount = 2;
            s.stage2TargetMode = eTargetMode::UnitTarget;
            s.stage2AnimKey = "skinned_mesh_spell1_b";
            s.stage2LockSec = 0.6f;
            s.stage2Rotate = eRotateMode::TowardsTarget;
            s.stageWindowSec = 3.f;
            s.stage2CastFrame = 4.f;
            s.stage2RecoveryFrame = 12.f;
            s.stage2PlaySpeed = 1.f;
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::W))
        {
            s.stageCount = 2;
            s.stage2TargetMode = eTargetMode::Self;
            s.stage2AnimKey = "skinned_mesh_spell2b_idle";
            s.stage2LockSec = 0.45f;
            s.stage2Rotate = eRotateMode::None;
            s.stageWindowSec = 3.f;
            s.stage2CastFrame = 4.f;
            s.stage2RecoveryFrame = 12.f;
            s.stage2PlaySpeed = 1.f;
        }
        else if (slot == static_cast<u8_t>(eSkillSlot::E))
        {
            s.stageCount = 2;
            s.stage2TargetMode = eTargetMode::Self;
            s.stage2AnimKey = "skinned_mesh_spell3_b_idle";
            s.stage2LockSec = 0.45f;
            s.stage2Rotate = eRotateMode::None;
            s.stageWindowSec = 3.f;
            s.stage2CastFrame = 4.f;
            s.stage2RecoveryFrame = 12.f;
            s.stage2PlaySpeed = 1.f;
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
            cd.fbxPath = "Client/Bin/Resource/Texture/Character/LeeSin/leesin.fbx";
            cd.shaderPath = L"Shaders/Mesh3D.hlsl";
            cd.defaultTexturePath = L"Client/Bin/Resource/Texture/Character/LeeSin/leesin_base_tx_cm.png";
            for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
                cd.texturePath[i] = cd.defaultTexturePath;
            cd.spawnPosition = { 39.f, 1.f, 0.f };
            cd.spawnScale = 0.01f;
            cd.displayName = "LeeSin";
            CChampionRegistry::Instance().Add(eChampion::LEESIN, cd);

            RegisterSkill(0, eTargetMode::UnitTarget, "skinned_mesh_attack1", kLeeSin_BA_Cast);
            RegisterSkill(1, eTargetMode::Direction, "skinned_mesh_spell1_cast", kLeeSin_Q_Cast);
            RegisterSkill(2, eTargetMode::Self, "skinned_mesh_spell2_fly", kLeeSin_W_Cast);
            RegisterSkill(3, eTargetMode::Self, "skinned_mesh_spell3_cast", kLeeSin_E_Cast);
            RegisterSkill(4, eTargetMode::UnitTarget, "skinned_mesh_spell4a", kLeeSin_R_Cast);

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
