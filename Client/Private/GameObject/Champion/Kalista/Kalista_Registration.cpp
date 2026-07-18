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
                s.animKey = "attack1";
                s.bOneShot = true;
                s.castHookId = kKal_BA_Cast;
                s.recoveryHookId = kKal_BA_Recovery;
                CSkillRegistry::Instance().Add(eChampion::KALISTA, 0, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::KALISTA;
                s.slot = 1;
                s.animKey = "spell1";
                s.bOneShot = true;
                s.castHookId = kKal_Q_Cast;
                s.recoveryHookId = kKal_Q_Recovery;
                CSkillRegistry::Instance().Add(eChampion::KALISTA, 1, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::KALISTA;
                s.slot = 2;
                s.animKey = "spell2";
                s.bOneShot = true;
                CSkillRegistry::Instance().Add(eChampion::KALISTA, 2, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::KALISTA;
                s.slot = 3;
                s.animKey = "spell3";
                s.bOneShot = true;
                s.onCastAcceptedHookId = kKal_E_OnAccept;
                CSkillRegistry::Instance().Add(eChampion::KALISTA, 3, s);
            }

            {
                SkillDef s{};
                s.champ = eChampion::KALISTA;
                s.slot = 4;
                s.animKey = "spell4_call";
                s.stage2AnimKey = "spell4_throw";
                s.bOneShot = true;
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
