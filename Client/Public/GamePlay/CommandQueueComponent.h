#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"

// ─────────────────────────────────────────────────────────────────────
//  CommandQueueComponent
//
//  아키텍처 문서 §10 결정 ⑤ "InputIntent = Pulse + State 분리" 의 구현체.
//  사용자 용어로 "CommandQueue" 통일.
//
//  Pulse = 1-tick 이벤트 (키 에지 트리거). SkillDispatchSystem 이 consume.
//  State = 지속 의도 (현재 이동 목적지). MovementSystem 이 read-only.
//  Active/Pending = 현재/예약된 행동 추적. SkillDispatchSystem 이 전이 관리.
//
//  엔티티당 1 개. LocalPlayerTag 가 붙은 엔티티만 InputSystem 이 쓴다.
//  네트워크 플레이어는 NetworkInputSystem 이 같은 컴포넌트에 쓰면 됨 (Phase 4).
// ─────────────────────────────────────────────────────────────────────

enum class eCommandType : uint8_t
{
	Move,
	BasicAttack, 
	Skill, 
	End
};
//Pulse = 1 tick 동안 유효한 단일 커맨드의 입력
struct PulseCommand
{
	eCommandType eType = eCommandType::End;
	uint8_t iSlot = 0;
	Vec3 vGroundPos{ 0.f, 0.f, 0.f };
	Vec3 vDir{ 0.f, 0.f, 0.f };
	EntityID target = NULL_ENTITY;
	bool bHasCursor = false;
};

//State = 지속적으로 유지 
struct StateCommand
{
	bool bMoveActive = false;
	Vec3 vMoveDest{ 0.f, 0.f, 0.f };
};

struct CommandQueueComponent
{
	//혹시 k가 어떤 의미? code영역이 아니라 .data, .bss 영역에 프로세스 종료시까지 변하지 않는
	//값으로써 저장되는 공간임을 의미하는 걸까?
	static constexpr size_t kPulseCapacity = 8;
	PulseCommand tPulse[kPulseCapacity] = {};
	uint8_t iPulseHead = 0;
	uint8_t iPulseTail = 0;

	StateCommand tState;
	//현재 실행 중인 행동
	eCommandType eActiveType = eCommandType::End;
	uint8_t iActiveSlot = 0;
	f32_t fActiveRemaining = 0.f;

	//Pending - Skill, BA가 active 일 때 들어온 Move Backup
	bool bPendingMove = false;
	Vec3 vPendingMoveDest{ 0.f, 0.f, 0.f };

	//Pulse API
	bool PushPulse(const PulseCommand& pulse)
	{
		const uint8_t next = (iPulseTail + 1) % kPulseCapacity;
		//overflow drop? 무슨 의미?
		if (next == iPulseHead)
			return false;
		tPulse[iPulseTail] = pulse;
		iPulseTail = next;
		return true;
	}
	//유효한 입력 커맨드가 들어왔을 경우 분석
	bool PopPulse(PulseCommand& out)
	{
		if (iPulseHead == iPulseTail)
			return false;
		out = tPulse[iPulseHead];
		iPulseHead = (iPulseHead + 1) % kPulseCapacity;
		return true;
	}

	bool PeekPulse(PulseCommand& out) const
	{
		if (iPulseHead == iPulseTail)
			return false;
		out = tPulse[iPulseHead];
		return true;
	}

	bool HasPulse() const { return iPulseHead != iPulseTail; }

	size_t PulseCount() const
	{
		return (iPulseTail + kPulseCapacity - iPulseHead) % kPulseCapacity;
	}

	void ClearPulse() { iPulseHead = iPulseTail = 0; }
	
};
