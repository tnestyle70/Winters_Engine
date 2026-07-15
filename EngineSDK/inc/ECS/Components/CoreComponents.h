#pragma once
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "ECS/Components/TransformComponent.h"   // 진실 소스 — 계층/Dirty 버전

// Entity가 저장할 Component들 모음
// 주의: TransformComponent는 TransformComponent.h 단일 정의를 따른다.
//       (과거 중복 정의 제거 — Phase B-6 정리)

struct VelocityComponent
{
	Vec3 vDirection{ 0.f, 0.f, 0.f };
	float fSpeed{ 0.f };
};

struct HealthComponent
{
	float fCurrent{ 100.f };
	float fMaximum{ 100.f };
	bool bIsDead{ false };
	uint8_t reservedTail[3]{};
};

struct ColliderComponent
{
	Vec3 vHalfExtents{ 0.5f, 0.5f, 0.5f };
	Vec3 vOffset{ 0.f, 0.f, 0.f };
	bool bIsTrigger{ false };
	uint8_t reservedTail[3]{};
};

static_assert(sizeof(HealthComponent) == 12u);
static_assert(sizeof(ColliderComponent) == 28u);

struct PlayerTag
{

};

struct SpriteComponent
{
	uint32_t textureID{ 0 };
	float fWidth{ 1.f };
	float fHeight{ 1.f };
};

struct AIStateComponent
{
	enum class State : uint8_t { Idle, Chase, Attack, Flee, Dead };
	State current{ State::Idle }; EntityID target{ NULL_ENTITY };
	float detectRange{ 10.f };    float attackRange{ 2.f };
};

// PvP 전용 (Phase 5)
struct NetworkComponent { uint32_t ownerClientID{ 0 }; uint32_t lastServerTick{ 0 }; bool isLocallyControlled{ false }; };
struct AbilityComponent
{
	static constexpr uint32_t MAX_SKILLS = 4;
	uint32_t skillIDs[MAX_SKILLS]{};  float cooldowns[MAX_SKILLS]{};  float maxCooldowns[MAX_SKILLS]{};
};
