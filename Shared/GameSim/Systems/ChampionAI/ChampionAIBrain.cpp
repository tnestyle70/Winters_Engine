#include "Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h"

#include <algorithm>

namespace
{
	// 점수 기반 룰 — 기존 SampleLaneCombatIntent 로직을 그대로 옮긴 기본 brain.
	class CRuleBasedChampionBrain final : public IChampionAIBrain
	{
	public:
		eChampionAIIntent DecideLaneCombatIntent(
			ChampionAIComponent& ai,
			const ChampionAIBrainInput& input) override
		{
			ai.intentHoldTimer = std::max(0.f, ai.intentHoldTimer - input.fDt);

			if (input.bPostComboBAWindow && input.bCanAttackChampion)
			{
				ai.intentHoldTimer = ai.intentHoldDuration;
				return eChampionAIIntent::AttackChampion;
			}

			if (ai.intentHoldTimer > 0.f)
			{
				if (ai.intent != eChampionAIIntent::AttackChampion ||
					input.bCanAttackChampion)
				{
					return ai.intent;
				}
			}

			ai.intentHoldTimer = ai.intentHoldDuration;

			if (!input.bCanAttackChampion)
				return eChampionAIIntent::FarmMinion;

			return (input.fChampionScore >=
					input.fFarmScore + ai.fChampionScoreMargin)
				? eChampionAIIntent::AttackChampion
				: eChampionAIIntent::FarmMinion;
		}
	};

	// 사람같은 봇 1차 구현:
	//  - 한 번 정한 태세를 더 오래 유지해 잦은 태세 전환(봇 특유의 즉답 반응)을 억제
	//  - 체력 우위가 아닐 때는 교전 대신 파밍을 선호
	class CPlayerLikeChampionBrain final : public IChampionAIBrain
	{
	public:
		eChampionAIIntent DecideLaneCombatIntent(
			ChampionAIComponent& ai,
			const ChampionAIBrainInput& input) override
		{
			ai.intentHoldTimer = std::max(0.f, ai.intentHoldTimer - input.fDt);

			if (input.bPostComboBAWindow && input.bCanAttackChampion)
			{
				ai.intentHoldTimer = ai.intentHoldDuration * kCommitScale;
				return eChampionAIIntent::AttackChampion;
			}

			if (ai.intentHoldTimer > 0.f)
			{
				if (ai.intent != eChampionAIIntent::AttackChampion ||
					input.bCanAttackChampion)
				{
					return ai.intent;
				}
			}

			ai.intentHoldTimer = ai.intentHoldDuration * kCommitScale;

			const bool_t bHpAdvantage =
				ai.fDecisionSelfHpRatio >= ai.fDecisionEnemyHpRatio;
			if (!input.bCanAttackChampion || !bHpAdvantage)
				return eChampionAIIntent::FarmMinion;

			return (input.fChampionScore >=
					input.fFarmScore + ai.fChampionScoreMargin)
				? eChampionAIIntent::AttackChampion
				: eChampionAIIntent::FarmMinion;
		}

	private:
		// 사람처럼 결정을 더 오래 유지하는 배율
		static constexpr f32_t kCommitScale = 1.5f;
	};

	// 외부 판단 모듈(플래너/학습 정책) 연동 지점.
	// 모듈이 붙기 전까지는 RuleBased로 위임해 동작을 보장한다.
	class CDecisionChampionBrain final : public IChampionAIBrain
	{
	public:
		eChampionAIIntent DecideLaneCombatIntent(
			ChampionAIComponent& ai,
			const ChampionAIBrainInput& input) override
		{
			// TODO(bot-v2): 외부 판단 결과(eChampionAIIntent)를 여기서 주입한다.
			// 입력은 ChampionAIBrainInput으로 한정할 것 (결정성 보존).
			return m_Fallback.DecideLaneCombatIntent(ai, input);
		}

	private:
		CRuleBasedChampionBrain m_Fallback{};
	};
}

IChampionAIBrain& ResolveChampionAIBrain(eChampionAIBrainType eBrainType)
{
	static CRuleBasedChampionBrain s_RuleBased{};
	static CPlayerLikeChampionBrain s_PlayerLike{};
	static CDecisionChampionBrain s_Decision{};

	switch (eBrainType)
	{
	case eChampionAIBrainType::PlayerLike:
		return s_PlayerLike;
	case eChampionAIBrainType::Decision:
		return s_Decision;
	case eChampionAIBrainType::RuleBased:
	default:
		return s_RuleBased;
	}
}
