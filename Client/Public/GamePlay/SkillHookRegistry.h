#pragma once

#include "Defines.h"
#include "GamePlay/SkillHookContext.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"

#include <cstddef>
#include <functional>
#include <unordered_map>

class CSkillHookRegistry
{
public:
	using HookFn = std::function<void(SkillHookContext&)>;

	static CSkillHookRegistry& Instance();

	void Register(u32_t hookId, HookFn fn);
	bool Dispatch(u32_t hookId, SkillHookContext& ctx) const;
	bool Has(u32_t hookId) const;

	std::size_t Count() const { return m_Map.size(); }

private:
	CSkillHookRegistry() = default;
	~CSkillHookRegistry() = default;
	CSkillHookRegistry(const CSkillHookRegistry&) = delete;
	CSkillHookRegistry& operator=(const CSkillHookRegistry&) = delete;

	std::unordered_map<u32_t, HookFn> m_Map{};
};

constexpr u32_t MakeHookId(eChampion champ, u16_t variant)
{
	return (static_cast<u32_t>(champ) << 16) | variant;
}

namespace HookVariant
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
}
