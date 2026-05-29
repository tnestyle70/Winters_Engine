#include "GamePlay/SkillHookRegistry.h"

#include <utility>

CSkillHookRegistry& CSkillHookRegistry::Instance()
{
	static CSkillHookRegistry s_inst;
	return s_inst;
}

void CSkillHookRegistry::Register(u32_t hookId, HookFn fn)
{
	auto [it, inserted] = m_Map.try_emplace(hookId, std::move(fn));
	if (!inserted)
	{
		char buf[160];
		sprintf_s(buf, "[SkillHookRegistry] DUPLICATE hookId=0x%08X - keeping first\n", hookId);
		OutputDebugStringA(buf);
	}
}

bool CSkillHookRegistry::Dispatch(u32_t hookId, SkillHookContext& ctx) const
{
	if (hookId == 0)
		return false;

	auto it = m_Map.find(hookId);
	if (it == m_Map.end())
		return false;

	it->second(ctx);
	return true;
}

bool CSkillHookRegistry::Has(u32_t hookId) const
{
	return m_Map.find(hookId) != m_Map.end();
}
