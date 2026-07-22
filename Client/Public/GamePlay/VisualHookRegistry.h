#pragma once
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include <functional>
#include <unordered_map>
#include <string>

class CWorld;

namespace Engine
{
	class CFxStaticMeshRenderer;
}

struct VisualHookContext
{
	//const view 변경 X - assert로 강제
	CWorld* pWorld = nullptr;
	EntityID casterEntity = NULL_ENTITY;
	const SkillDef* pDef = nullptr;
	const CastSkillCommand* pCommand = nullptr;
	u8_t skillStage = 1;
	f32_t fEffectLifetimeSec = 0.f;
	f32_t fEffectLength = 0.f;
	f32_t fEffectHalfWidth = 0.f;
	std::string* pKeyOut = nullptr; //keyswap channel
	Engine::CFxStaticMeshRenderer* pFxMeshRenderer = nullptr;
	bool_t bAuthoritativeEvent = false;
};

class CVisualHookRegistry
{
public:
	//구조체로 선언된 visualHookContext를 void()로 담는 callback 함수? 
	//HookFn을 사용한다?
	using HookFn = std::function<void(VisualHookContext&)>;
	static CVisualHookRegistry& Instance();
	void Register(u32_t hookId, HookFn fn);
	bool Dispatch(u32_t hookId, VisualHookContext& ctx) const;
	bool Has(u32_t hookId) const;

private:
	CVisualHookRegistry() = default;
	//복사 금지
	CVisualHookRegistry(const CVisualHookRegistry&) = delete;
	//대입 연산 금지
	CVisualHookRegistry operator=(CVisualHookRegistry&) = delete;
	std::unordered_map<u32_t, HookFn> m_Map; //client 전용
};
