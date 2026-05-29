#include "GameObject/Champion/Garen/Garen_Skills.h"

#include "GameObject/SkillDef.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"

#include <Windows.h>

namespace
{
    u32_t ResolveGarenCastHook(u8_t slot)
    {
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::Q:
            return MakeHookId(eChampion::GAREN, HookVariant::Q_CastFrame);
        case eSkillSlot::W:
            return MakeHookId(eChampion::GAREN, HookVariant::W_CastFrame);
        case eSkillSlot::E:
            return MakeHookId(eChampion::GAREN, HookVariant::E_CastFrame);
        case eSkillSlot::R:
            return MakeHookId(eChampion::GAREN, HookVariant::R_CastFrame);
        default:
            return MakeHookId(eChampion::GAREN, HookVariant::BA_CastFrame);
        }
    }

    //훅을 만드는 게 서버에 보낼 무언가를 전송한다는 뜻인 걸까?
    void DispatchGarenVisualHook(VisualHookContext& visualCtx)
    {
        SkillHookContext skillCtx{};
        skillCtx.pWorld = visualCtx.pWorld;
        skillCtx.casterEntity = visualCtx.casterEntity;
        skillCtx.pDef = visualCtx.pDef;
        skillCtx.pCommand = visualCtx.pCommand;
        skillCtx.skillStage = visualCtx.skillStage;
        skillCtx.pFxMeshRenderer = visualCtx.pFxMeshRenderer;

        Garen::OnCastFrame(skillCtx);
    }

    struct GarenAutoRegister
    {
        GarenAutoRegister()
        {
            for (u8_t slot = 0; slot < static_cast<u8_t>(eSkillSlot::SLOT_END); ++slot)
            {
                const SkillDef* legacy = FindSkillDef(eChampion::GAREN, slot);
                if (!legacy)
                    continue;

                SkillDef s = *legacy;
                s.castFrameHookId = ResolveGarenCastHook(slot);
                CSkillRegistry::Instance().Add(eChampion::GAREN, slot, s);
            }

            for (u8_t slot = 0; slot < static_cast<u8_t>(eSkillSlot::SLOT_END); ++slot)
            {
                const u32_t hookId = ResolveGarenCastHook(slot);
                CVisualHookRegistry::Instance().Register(
                    hookId,
                    [](VisualHookContext& ctx)
                    {
                        DispatchGarenVisualHook(ctx);
                    }
                );

                CSkillHookRegistry::Instance().Register(hookId, &Garen::OnCastFrame);
            }
            OutputDebugStringA("[Garen] Registration complete\n");
        }
    };

    static GarenAutoRegister s_register;
}

void Garen_KeepAlive()
{
    (void)&s_register;
}
