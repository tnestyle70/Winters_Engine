#include "GamePlay/VisualHookRegistry.h"

#include <utility>

CVisualHookRegistry& CVisualHookRegistry::Instance()
{
	static CVisualHookRegistry s_inst;
	return s_inst;
}

void CVisualHookRegistry::Register(u32_t hookId, HookFn fn)
{
	m_Map[hookId] = std::move(fn);
}

bool CVisualHookRegistry::Dispatch(u32_t hookId, VisualHookContext& ctx) const
{
	if (hookId == 0)
		return false;

	auto it = m_Map.find(hookId);
	if (it == m_Map.end())
		return false;

	it->second(ctx);
	return true;
}

bool CVisualHookRegistry::Has(u32_t hookId) const
{
	return m_Map.find(hookId) != m_Map.end();
}
