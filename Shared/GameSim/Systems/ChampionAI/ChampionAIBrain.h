#pragma once

// ─────────────────────────────────────────────────────────────
// ChampionAI Brain — 봇 의사결정 확장 시즈
//
// 협업 규약:
//  - CChampionAISystem : 컨텍스트 수집 / 상태머신 / 명령 방출 (공용 인프라, 수정 최소화)
//  - IChampionAIBrain  : "지금 무엇을 할 것인가"(intent) 결정만 담당
//  - 새 봇 유형 추가 절차:
//      1) ChampionAIBrain.cpp에 brain 구현 추가
//      2) ResolveChampionAIBrain switch에 분기 추가
//      3) 스폰 시 ChampionAIComponent::brainType만 지정
//  - brain은 결정적(deterministic)이어야 한다:
//      입력은 ai/input으로 한정, 난수·시간은 GameSim 틱 데이터만 사용
//      (서버 권위 시뮬레이션과 리플레이 재현성 보존)
// ─────────────────────────────────────────────────────────────

#include "Shared/GameSim/Components/ChampionAIComponent.h"

// 시스템이 수집해 brain에 넘기는 결정 입력.
// ChampionAISystem 내부 컨텍스트(ChampionAIContext)에 의존하지 않도록
// 필요한 값만 평탄화해서 전달한다.
struct ChampionAIBrainInput
{
	f32_t fDt = 0.f;
	bool_t bCanAttackChampion = false;
	bool_t bPostComboBAWindow = false;
	f32_t fChampionScore = 0.f;
	f32_t fFarmScore = 0.f;
	f32_t fStructureScore = 0.f;
};

class IChampionAIBrain
{
public:
	virtual ~IChampionAIBrain() = default;

	// LaneCombat 의사결정 주기마다 호출된다.
	// intent를 반환하고, 결정 유지 시간은 ai.intentHoldTimer로 관리한다.
	virtual eChampionAIIntent DecideLaneCombatIntent(
		ChampionAIComponent& ai,
		const ChampionAIBrainInput& input) = 0;
};

// brainType별 brain을 돌려준다.
// brain 자체는 무상태(stateless)이고 모든 상태는 ChampionAIComponent에 두므로
// 엔티티 간 공유/재진입이 안전하다.
IChampionAIBrain& ResolveChampionAIBrain(eChampionAIBrainType eBrainType);
