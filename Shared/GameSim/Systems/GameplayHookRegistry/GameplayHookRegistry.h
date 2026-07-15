#pragma once

#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

struct GameplayHookContext
{
	CWorld* pWorld = nullptr;
	EntityID casterEntity = NULL_ENTITY;
	eTeam casterTeam = eTeam::Blue;
	eChampion casterChampion = eChampion::NONE;
	u8_t skillRank = 1;
	f32_t fPaidManaCost = 0.f;
	const SkillDef* pDef = nullptr;
	const GameCommand* pCommand = nullptr;
	const TickContext* pTickCtx = nullptr;
};

class CGameplayHookRegistry
{
public:
	using HookFn = void(*)(GameplayHookContext&);

	static CGameplayHookRegistry& Instance();
	void Register(u32_t hookId, HookFn fn);
	bool Dispatch(u32_t hookId, GameplayHookContext& ctx) const;
	bool Has(u32_t hookId) const;

private:
	CGameplayHookRegistry() = default;

	static constexpr u32_t kMaxChamp = 256;
	static constexpr u32_t kMaxVariant = 256;
	HookFn m_table[kMaxChamp][kMaxVariant] = {};
};

constexpr u32_t MakeGameplayHookId(eChampion champ, u16_t variant)
{
	return (static_cast<u32_t>(champ) << 16) | variant;
}

namespace GameplayHookVariant
{
	constexpr u16_t BA_KeySwap = 0x0011;
	constexpr u16_t Q_KeySwap = 0x0012;
	constexpr u16_t W_KeySwap = 0x0013;
	constexpr u16_t E_KeySwap = 0x0014;
	constexpr u16_t R_KeySwap = 0x0015;

	constexpr u16_t BA_OnCastAccepted = 0x0021;
	constexpr u16_t Q_OnCastAccepted = 0x0022;
	constexpr u16_t W_OnCastAccepted = 0x0023;
	constexpr u16_t E_OnCastAccepted = 0x0024;
	constexpr u16_t R_OnCastAccepted = 0x0025;

	constexpr u16_t BA_CastFrame = 0x0031;
	constexpr u16_t Q_CastFrame = 0x0032;
	constexpr u16_t W_CastFrame = 0x0033;
	constexpr u16_t E_CastFrame = 0x0034;
	constexpr u16_t R_CastFrame = 0x0035;

	constexpr u16_t BA_Recovery = 0x0041;
	constexpr u16_t Q_Recovery = 0x0042;
	constexpr u16_t W_Recovery = 0x0043;
	constexpr u16_t E_Recovery = 0x0044;
	constexpr u16_t R_Recovery = 0x0045;

	constexpr u16_t Passive_Trigger = 0x0051;
}
