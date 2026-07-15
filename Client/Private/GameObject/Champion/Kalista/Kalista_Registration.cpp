#include "GameObject/Champion/Kalista/Kalista_Skills.h"
#include "GameObject/SkillDef.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"

#include "ECS/World.h"

#include <Windows.h>

namespace
{
    constexpr u32_t kKal_BA_Cast = MakeHookId(eChampion::KALISTA, HookVariant::BA_CastFrame);
    constexpr u32_t kKal_Q_Cast = MakeHookId(eChampion::KALISTA, HookVariant::Q_CastFrame);
    constexpr u32_t kKal_E_OnAccept = MakeHookId(eChampion::KALISTA, HookVariant::E_OnCastAccepted);
    constexpr u32_t kKal_BA_Recovery = MakeHookId(eChampion::KALISTA, HookVariant::BA_Recovery);
    constexpr u32_t kKal_Q_Recovery = MakeHookId(eChampion::KALISTA, HookVariant::Q_Recovery);

    eTeam ResolveVisualCasterTeam(CWorld* pWorld, EntityID caster)
    {
        if (!pWorld || caster == NULL_ENTITY)
            return eTeam::Blue;

        if (pWorld->HasComponent<ChampionComponent>(caster))
            return pWorld->GetComponent<ChampionComponent>(caster).team;
        if (pWorld->HasComponent<MinionComponent>(caster))
            return pWorld->GetComponent<MinionComponent>(caster).team;
        if (pWorld->HasComponent<TurretComponent>(caster))
            return pWorld->GetComponent<TurretComponent>(caster).team;

        return eTeam::Blue;
    }

    void DispatchKalistaVisualHook(
        VisualHookContext& visualCtx,
        void(*handler)(SkillHookContext&))
    {
        SkillHookContext skillCtx{};
        skillCtx.pWorld = visualCtx.pWorld;
        skillCtx.casterEntity = visualCtx.casterEntity;
        skillCtx.casterTeam = ResolveVisualCasterTeam(
            visualCtx.pWorld,
            visualCtx.casterEntity);
        skillCtx.pDef = visualCtx.pDef;
        skillCtx.pCommand = visualCtx.pCommand;
        skillCtx.skillStage = visualCtx.skillStage;
        skillCtx.pKeyOut = visualCtx.pKeyOut;
        skillCtx.pFxMeshRenderer = visualCtx.pFxMeshRenderer;

        handler(skillCtx);
    }

    struct KalistaAutoRegister
    {
        KalistaAutoRegister()
        {
            {
                SkillDef s{};
                s.champ = eChampion::KALISTA;
                s.slot = 0;
                s.targetMode = eTargetMode::UnitTarget;
                s.cooldownSec = 0.5f;
                s.rangeMax = 5.5f;
                s.manaCost = 0.f;
                s.animKey = "attack1";
                s.lockDurationSec = 0.6f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsTarget;
                s.visualCastFrame = 6.f;
                s.visualRecoveryFrame = 14.f;
                s.visualPlaySpeed = 1.0f;
                s.stage2VisualPlaySpeed = 1.f;
                s.castHookId = kKal_BA_Cast;
                s.recoveryHookId = kKal_BA_Recovery;
                CSkillRegistry::Instance().Add(eChampion::KALISTA, 0, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::KALISTA;
                s.slot = 1;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 0.2f;
                s.rangeMax = 16.5f;
                s.manaCost = 50.f;
                s.animKey = "spell1";
                s.lockDurationSec = 0.3f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.visualCastFrame = 4.f;
                s.visualRecoveryFrame = 10.f;
                s.visualPlaySpeed = 2.8f;
                s.stage2VisualPlaySpeed = 1.f;
                s.castHookId = kKal_Q_Cast;
                s.recoveryHookId = kKal_Q_Recovery;
                CSkillRegistry::Instance().Add(eChampion::KALISTA, 1, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::KALISTA;
                s.slot = 2;
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 1.f;
                s.rangeMax = 12.f;
                s.manaCost = 50.f;
                s.animKey = "spell2";
                s.lockDurationSec = 0.5f;
                s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                CSkillRegistry::Instance().Add(eChampion::KALISTA, 2, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::KALISTA;
                s.slot = 3;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 3.f;
                s.rangeMax = 12.f;
                s.manaCost = 30.f;
                s.animKey = "spell3";
                s.lockDurationSec = 0.4f;
                s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.onCastAcceptedHookId = kKal_E_OnAccept;
                CSkillRegistry::Instance().Add(eChampion::KALISTA, 3, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::KALISTA;
                s.slot = 4;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 120.f;
                s.rangeMax = 0.f;
                s.manaCost = 100.f;
                s.animKey = "spell4_call";
                s.stageCount = 2u;
                s.stageWindowSec = 4.f;
                s.stage2TargetMode = eTargetMode::Direction;
                s.stage2AnimKey = "spell4_throw";
                s.stage2LockSec = 0.45f;
                s.stage2Rotate = eRotateMode::TowardsCursor;
                s.stage2VisualCastFrame = 4.f;
                s.stage2VisualRecoveryFrame = 10.f;
                s.stage2VisualPlaySpeed = 1.f;
                s.lockDurationSec = 0.5f;
                s.bOneShot = true;
                s.rotate = eRotateMode::None;
                CSkillRegistry::Instance().Add(eChampion::KALISTA, 4, s);
            }

            CSkillHookRegistry::Instance().Register(kKal_BA_Cast, &Kalista::OnCastFrame_BA);
            CSkillHookRegistry::Instance().Register(kKal_Q_Cast, &Kalista::OnCastFrame_Q);
            CSkillHookRegistry::Instance().Register(kKal_E_OnAccept, &Kalista::OnCastAccepted_E);
            CSkillHookRegistry::Instance().Register(kKal_BA_Recovery, &Kalista::OnRecoveryFrame_PassiveDash);
            CSkillHookRegistry::Instance().Register(kKal_Q_Recovery, &Kalista::OnRecoveryFrame_PassiveDash);

            CVisualHookRegistry::Instance().Register(
                kKal_BA_Cast,
                [](VisualHookContext& ctx)
                {
                    if (!ctx.bAuthoritativeEvent)
                        DispatchKalistaVisualHook(ctx, &Kalista::OnCastFrame_BA);
                });
            CVisualHookRegistry::Instance().Register(
                kKal_Q_Cast,
                [](VisualHookContext& ctx)
                {
                    if (!ctx.bAuthoritativeEvent)
                        DispatchKalistaVisualHook(ctx, &Kalista::OnCastFrame_Q);
                });
            CVisualHookRegistry::Instance().Register(
                kKal_E_OnAccept,
                [](VisualHookContext& ctx)
                {
                    DispatchKalistaVisualHook(ctx, &Kalista::OnCastAccepted_E);
                });

        }
    };

    static KalistaAutoRegister s_register;
}

void Kalista_KeepAlive()
{
    (void)&s_register;
}
