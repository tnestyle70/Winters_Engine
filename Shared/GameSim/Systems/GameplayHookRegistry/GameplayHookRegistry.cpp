#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"

CGameplayHookRegistry& CGameplayHookRegistry::Instance()
{
	static CGameplayHookRegistry s;
	return s;
}

void CGameplayHookRegistry::Register(u32_t hookId, HookFn fn)
{
	const u32_t champ = (hookId >> 16) & 0xFF;
	const u32_t variant = hookId & 0xFF;
	m_table[champ][variant] = fn;
}

bool CGameplayHookRegistry::Dispatch(u32_t hookId, GameplayHookContext& ctx) const 
{
	if (hookId == 0) return false;
	const u32_t champ = (hookId >> 16) & 0xFF;
	const u32_t variant = hookId & 0xFF;
	HookFn fn = m_table[champ][variant];
	if (!fn) return false;
	fn(ctx);
	return true;
}

bool CGameplayHookRegistry::Has(u32_t hookId) const
{
	const u32_t champ = (hookId >> 16) & 0xFF;
	const u32_t variant = hookId & 0xFF;
	return m_table[champ][variant] != nullptr;
}
