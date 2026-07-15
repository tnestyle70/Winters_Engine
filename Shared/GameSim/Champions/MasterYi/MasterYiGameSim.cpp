#include "Shared/GameSim/Champions/MasterYi/MasterYiGameSim.h"

#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/MasterYiComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"

#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <functional>

namespace
{
    constexpr u8_t kMasterYiRSlot = static_cast<u8_t>(eSkillSlot::R);
    constexpr u32_t kHighlanderBuffId =
        (static_cast<u32_t>(eChampion::MASTERYI) << 16) |
        GameplayHookVariant::R_CastFrame;

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx || ctx.casterEntity == NULL_ENTITY)
            return;

        CWorld& world = *ctx.pWorld;
        MasterYiSimComponent& state = world.HasComponent<MasterYiSimComponent>(
            ctx.casterEntity)
            ? world.GetComponent<MasterYiSimComponent>(ctx.casterEntity)
            : world.AddComponent<MasterYiSimComponent>(
                ctx.casterEntity,
                MasterYiSimComponent{});
        const f32_t durationSec = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::MASTERYI,
            kMasterYiRSlot,
            eSkillEffectParamId::EffectDurationSec,
            7.f);
        const f32_t moveSpeedMul = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::MASTERYI,
            kMasterYiRSlot,
            eSkillEffectParamId::MoveSpeedMul,
            1.35f);
        const f32_t bonusAttackSpeed = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::MASTERYI,
            kMasterYiRSlot,
            eSkillEffectParamId::BonusAttackSpeed,
            0.25f);

        state.fHighlanderRemainingSec = durationSec;
        BuffComponent& buffs = world.HasComponent<BuffComponent>(ctx.casterEntity)
            ? world.GetComponent<BuffComponent>(ctx.casterEntity)
            : world.AddComponent<BuffComponent>(ctx.casterEntity, BuffComponent{});
        BuffInstance buff{};
        buff.buffDefId = kHighlanderBuffId;
        buff.source = ctx.casterEntity;
        buff.fDurationRemaining = durationSec;
        buff.stackCount = 1u;
        buff.bonusAttackSpeedPerStack = bonusAttackSpeed;
        buff.moveSpeedMul = moveSpeedMul;
        if (CBuffSystem::AddOrRefresh(buffs, buff) &&
            world.HasComponent<StatComponent>(ctx.casterEntity))
        {
            world.GetComponent<StatComponent>(ctx.casterEntity).bDirty = true;
        }
    }
}

namespace MasterYiGameSim
{
	void RegisterHooks()
	{
		static bool_t s_bRegistered = false;
		if (s_bRegistered)
			return;

		CGameplayHookRegistry::Instance().Register(
			MakeGameplayHookId(eChampion::MASTERYI, GameplayHookVariant::R_CastFrame),
			&OnR);
		s_bRegistered = true;
	}

	void Tick(CWorld& world, const TickContext& tc)
	{
		world.ForEach<MasterYiSimComponent>(
			std::function<void(EntityID, MasterYiSimComponent&)>(
				[&](EntityID, MasterYiSimComponent& state)
				{
					state.fHighlanderRemainingSec = std::max(
						0.f,
						state.fHighlanderRemainingSec - tc.fDt);
				}));
	}
}
