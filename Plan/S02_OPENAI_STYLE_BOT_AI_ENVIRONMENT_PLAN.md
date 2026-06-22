Session - 현재 rule-based 봇을 OpenAI Five식 observation/action/reward/self-play 환경 계약으로 분리한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAILearningTypes.h

새 파일:

```cpp
#pragma once

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

inline constexpr u8_t kChampionAIObservationMaxEntities = 32u;

enum class eChampionAIObservedKind : u8_t
{
    None,
    Champion,
    Minion,
    Structure,
};

enum class eChampionAIObservedRelation : u8_t
{
    Self,
    Ally,
    Enemy,
    Neutral,
};

enum class eChampionAIPolicyAction : u8_t
{
    Noop,
    Move,
    BasicAttack,
    CastSkill,
    AttackStructure,
    Retreat,
    Recall,
    FlashEscape,
};

struct ChampionAIEntityObservation
{
    EntityID entity = NULL_ENTITY;
    eChampionAIObservedKind kind = eChampionAIObservedKind::None;
    eChampionAIObservedRelation relation = eChampionAIObservedRelation::Neutral;
    eChampion champion = eChampion::NONE;
    u8_t team = 0u;
    u8_t lane = 0u;
    f32_t hpRatio = 1.f;
    f32_t distance = 999.f;
    Vec3 worldPos{};
    Vec3 relativePos{};
};

struct ChampionAIObservation
{
    u64_t tickIndex = 0u;
    EntityID self = NULL_ENTITY;
    eChampion champion = eChampion::NONE;
    eTeam team = eTeam::Blue;
    u8_t lane = 0u;
    u8_t difficulty = 0u;
    eChampionAIState state = eChampionAIState::MoveToOuterTurret;
    eChampionAIIntent intent = eChampionAIIntent::FarmMinion;
    eChampionAIAction lastAction = eChampionAIAction::MoveToSafeAnchor;
    f32_t selfHpRatio = 1.f;
    f32_t championScore = 0.f;
    f32_t farmScore = 0.f;
    f32_t structureScore = 0.f;
    f32_t turretDanger = 0.f;
    f32_t enemyHpRatio = 1.f;
    f32_t enemyDistance = 999.f;
    u8_t entityCount = 0u;
    ChampionAIEntityObservation entities[kChampionAIObservationMaxEntities]{};
};

struct ChampionAIActionMask
{
    u32_t actionMask = 0u;
    u32_t skillMask = 0u;

    bool_t Can(eChampionAIPolicyAction action) const
    {
        const u32_t bit = 1u << static_cast<u8_t>(action);
        return (actionMask & bit) != 0u;
    }
};

struct ChampionAIPolicyDecision
{
    eChampionAIPolicyAction action = eChampionAIPolicyAction::Noop;
    u8_t skillSlot = 0u;
    EntityID target = NULL_ENTITY;
    Vec3 groundPos{};
};

struct ChampionAIRewardSample
{
    f32_t individualReward = 0.f;
    f32_t teamReward = 0.f;
    f32_t shapedReward = 0.f;
    f32_t terminalReward = 0.f;
};

struct ChampionAILearningSample
{
    ChampionAIObservation observation{};
    ChampionAIActionMask actionMask{};
    ChampionAIPolicyDecision decision{};
    ChampionAIRewardSample reward{};
};

static_assert(std::is_trivially_copyable_v<ChampionAIEntityObservation>);
static_assert(std::is_trivially_copyable_v<ChampionAIObservation>);
static_assert(std::is_trivially_copyable_v<ChampionAIActionMask>);
static_assert(std::is_trivially_copyable_v<ChampionAIPolicyDecision>);
static_assert(std::is_trivially_copyable_v<ChampionAIRewardSample>);
static_assert(std::is_trivially_copyable_v<ChampionAILearningSample>);
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAILearningEnv.h

새 파일:

```cpp
#pragma once

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAILearningTypes.h"
#include "WintersMath.h"

#include <algorithm>
#include <cmath>

class CChampionAILearningEnv final
{
public:
    static bool_t BuildObservation(
        CWorld& world,
        EntityID self,
        const TickContext& tc,
        const ChampionAIComponent& ai,
        ChampionAIObservation& outObservation)
    {
        outObservation = ChampionAIObservation{};
        if (self == NULL_ENTITY ||
            !world.IsAlive(self) ||
            !world.HasComponent<ChampionComponent>(self) ||
            !world.HasComponent<TransformComponent>(self))
        {
            return false;
        }

        const auto& selfChampion = world.GetComponent<ChampionComponent>(self);
        const Vec3 selfPos =
            world.GetComponent<TransformComponent>(self).GetPosition();

        outObservation.tickIndex = tc.tickIndex;
        outObservation.self = self;
        outObservation.champion = selfChampion.id;
        outObservation.team = selfChampion.team;
        outObservation.lane = ai.lane;
        outObservation.difficulty = ai.difficulty;
        outObservation.state = ai.state;
        outObservation.intent = ai.intent;
        outObservation.lastAction = ai.lastAction;
        outObservation.selfHpRatio = ResolveHealthRatio(world, self);
        outObservation.championScore = ai.fChampionDecisionScore;
        outObservation.farmScore = ai.fFarmDecisionScore;
        outObservation.structureScore = ai.fStructureDecisionScore;
        outObservation.turretDanger = ai.fDecisionTurretDanger;
        outObservation.enemyHpRatio = ai.fDecisionEnemyHpRatio;
        outObservation.enemyDistance = ai.fDecisionEnemyDistance;

        world.ForEach<ChampionComponent, TransformComponent>(
            [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
            {
                ChampionAIEntityObservation observed{};
                observed.entity = entity;
                observed.kind = eChampionAIObservedKind::Champion;
                observed.relation = ResolveRelation(self, selfChampion.team, champion.team);
                observed.champion = champion.id;
                observed.team = static_cast<u8_t>(champion.team);
                observed.hpRatio = ResolveHealthRatio(world, entity);
                observed.worldPos = transform.GetPosition();
                observed.relativePos = SubtractXZ(observed.worldPos, selfPos);
                observed.distance = DistanceXZ(observed.relativePos);
                InsertNearest(outObservation, observed);
            });

        world.ForEach<MinionComponent, TransformComponent>(
            [&](EntityID entity, MinionComponent& minion, TransformComponent& transform)
            {
                ChampionAIEntityObservation observed{};
                observed.entity = entity;
                observed.kind = eChampionAIObservedKind::Minion;
                observed.relation = ResolveRelation(self, selfChampion.team, minion.team);
                observed.team = static_cast<u8_t>(minion.team);
                observed.lane = minion.laneType;
                observed.hpRatio = ResolveHealthRatio(world, entity);
                observed.worldPos = transform.GetPosition();
                observed.relativePos = SubtractXZ(observed.worldPos, selfPos);
                observed.distance = DistanceXZ(observed.relativePos);
                InsertNearest(outObservation, observed);
            });

        world.ForEach<StructureComponent, TransformComponent>(
            [&](EntityID entity, StructureComponent& structure, TransformComponent& transform)
            {
                ChampionAIEntityObservation observed{};
                observed.entity = entity;
                observed.kind = eChampionAIObservedKind::Structure;
                observed.relation = ResolveRelation(self, selfChampion.team, structure.team);
                observed.team = static_cast<u8_t>(structure.team);
                observed.lane = static_cast<u8_t>(structure.lane);
                observed.hpRatio = ResolveHealthRatio(world, entity);
                observed.worldPos = transform.GetPosition();
                observed.relativePos = SubtractXZ(observed.worldPos, selfPos);
                observed.distance = DistanceXZ(observed.relativePos);
                InsertNearest(outObservation, observed);
            });

        SortEntities(outObservation);
        return true;
    }

    static ChampionAIActionMask BuildActionMask(
        u32_t availableActionMask,
        u32_t availableSkillMask)
    {
        ChampionAIActionMask mask{};
        mask.skillMask = availableSkillMask;

        mask.actionMask |= 1u << static_cast<u8_t>(eChampionAIPolicyAction::Noop);
        if (availableActionMask & kChampionAIActionBitMoveToSafeAnchor)
            mask.actionMask |= 1u << static_cast<u8_t>(eChampionAIPolicyAction::Move);
        if (availableActionMask & kChampionAIActionBitAttackMinion)
            mask.actionMask |= 1u << static_cast<u8_t>(eChampionAIPolicyAction::BasicAttack);
        if (availableActionMask & kChampionAIActionBitAttackChampion)
            mask.actionMask |=
                1u << static_cast<u8_t>(eChampionAIPolicyAction::BasicAttack) |
                1u << static_cast<u8_t>(eChampionAIPolicyAction::CastSkill);
        if (availableActionMask & kChampionAIActionBitAttackStructure)
            mask.actionMask |= 1u << static_cast<u8_t>(eChampionAIPolicyAction::AttackStructure);
        if (availableActionMask & kChampionAIActionBitRetreat)
            mask.actionMask |= 1u << static_cast<u8_t>(eChampionAIPolicyAction::Retreat);
        if (availableActionMask & kChampionAIActionBitUseFlashEscape)
            mask.actionMask |= 1u << static_cast<u8_t>(eChampionAIPolicyAction::FlashEscape);

        return mask;
    }

    static ChampionAIRewardSample BuildRewardSample(const ChampionAIComponent& ai)
    {
        ChampionAIRewardSample reward{};
        reward.individualReward =
            ai.fChampionDecisionScore * 0.35f +
            ai.fFarmDecisionScore * 0.20f +
            ai.fStructureDecisionScore * 0.30f;

        if (ai.fDecisionSelfHpRatio <= ai.retreatHpRatio)
            reward.individualReward -= 0.25f;

        reward.shapedReward =
            reward.individualReward -
            ai.fDecisionTurretDanger * 0.25f;

        reward.teamReward = reward.shapedReward;
        return reward;
    }

private:
    CChampionAILearningEnv() = delete;

    static Vec3 SubtractXZ(const Vec3& lhs, const Vec3& rhs)
    {
        return Vec3{ lhs.x - rhs.x, 0.f, lhs.z - rhs.z };
    }

    static f32_t DistanceXZ(const Vec3& delta)
    {
        return std::sqrt(std::max(0.f, delta.x * delta.x + delta.z * delta.z));
    }

    static eChampionAIObservedRelation ResolveRelation(
        EntityID self,
        eTeam selfTeam,
        eTeam otherTeam)
    {
        if (otherTeam == eTeam::Neutral)
            return eChampionAIObservedRelation::Neutral;
        return selfTeam == otherTeam
            ? eChampionAIObservedRelation::Ally
            : eChampionAIObservedRelation::Enemy;
    }

    static f32_t ResolveHealthRatio(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return 0.f;

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& health = world.GetComponent<HealthComponent>(entity);
            if (health.fMaximum > 0.001f)
                return WintersMath::Clamp01(health.fCurrent / health.fMaximum);
        }

        if (world.HasComponent<ChampionComponent>(entity))
        {
            const auto& champion = world.GetComponent<ChampionComponent>(entity);
            if (champion.maxHp > 0.001f)
                return WintersMath::Clamp01(champion.hp / champion.maxHp);
        }

        if (world.HasComponent<MinionComponent>(entity))
        {
            const auto& minion = world.GetComponent<MinionComponent>(entity);
            if (minion.maxHp > 0.001f)
                return WintersMath::Clamp01(minion.hp / minion.maxHp);
        }

        if (world.HasComponent<StructureComponent>(entity))
        {
            const auto& structure = world.GetComponent<StructureComponent>(entity);
            if (structure.maxHp > 0.001f)
                return WintersMath::Clamp01(structure.hp / structure.maxHp);
        }

        return 1.f;
    }

    static void InsertNearest(
        ChampionAIObservation& observation,
        const ChampionAIEntityObservation& entity)
    {
        if (observation.entityCount < kChampionAIObservationMaxEntities)
        {
            observation.entities[observation.entityCount++] = entity;
            return;
        }

        u8_t replaceIndex = 0u;
        for (u8_t i = 1u; i < observation.entityCount; ++i)
        {
            if (observation.entities[i].distance >
                observation.entities[replaceIndex].distance)
            {
                replaceIndex = i;
            }
        }

        const auto& current = observation.entities[replaceIndex];
        if (entity.distance < current.distance ||
            (entity.distance == current.distance && entity.entity < current.entity))
        {
            observation.entities[replaceIndex] = entity;
        }
    }

    static void SortEntities(ChampionAIObservation& observation)
    {
        std::sort(
            observation.entities,
            observation.entities + observation.entityCount,
            [](const ChampionAIEntityObservation& lhs,
                const ChampionAIEntityObservation& rhs)
            {
                if (lhs.distance != rhs.distance)
                    return lhs.distance < rhs.distance;
                return lhs.entity < rhs.entity;
            });
    }
};
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h

기존 코드:

```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Systems/ChampionAI/ChampionAILearningTypes.h"
```

기존 코드:

```cpp
struct ChampionAIBrainInput
{
	f32_t fDt = 0.f;
	bool_t bCanAttackChampion = false;
	bool_t bPostComboBAWindow = false;
	f32_t fChampionScore = 0.f;
	f32_t fFarmScore = 0.f;
	f32_t fStructureScore = 0.f;
};
```

아래로 교체:

```cpp
struct ChampionAIBrainInput
{
	f32_t fDt = 0.f;
	bool_t bCanAttackChampion = false;
	bool_t bPostComboBAWindow = false;
	f32_t fChampionScore = 0.f;
	f32_t fFarmScore = 0.f;
	f32_t fStructureScore = 0.f;
	ChampionAIObservation observation{};
	ChampionAIActionMask actionMask{};
	ChampionAIRewardSample rewardSample{};
};
```

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAILearningEnv.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
```

기존 코드:

```cpp
void SampleLaneCombatIntent(
    const TickContext& tc,
    ChampionAIComponent& ai,
    const ChampionAIContext& ctx)
{
    // 결정 입력만 평탄화해서 brain에 위임한다.
    // 룰 자체는 ChampionAIBrain.cpp의 brain 구현에 있다.
    ChampionAIBrainInput input{};
    input.fDt = tc.fDt;
    input.bCanAttackChampion = CanAttackChampion(ai, ctx);
    input.bPostComboBAWindow =
        ai.fPostComboBATimer > 0.f && ai.bPostComboBAAllowed;
    input.fChampionScore = ai.fChampionDecisionScore;
    input.fFarmScore = ai.fFarmDecisionScore;
    input.fStructureScore = ai.fStructureDecisionScore;

    ai.intent = ResolveChampionAIBrain(ai.brainType)
        .DecideLaneCombatIntent(ai, input);
}
```

아래로 교체:

```cpp
void SampleLaneCombatIntent(
    CWorld& world,
    EntityID self,
    const TickContext& tc,
    ChampionAIComponent& ai,
    const ChampionAIContext& ctx)
{
    ChampionAIBrainInput input{};
    input.fDt = tc.fDt;
    input.bCanAttackChampion = CanAttackChampion(ai, ctx);
    input.bPostComboBAWindow =
        ai.fPostComboBATimer > 0.f && ai.bPostComboBAAllowed;
    input.fChampionScore = ai.fChampionDecisionScore;
    input.fFarmScore = ai.fFarmDecisionScore;
    input.fStructureScore = ai.fStructureDecisionScore;
    CChampionAILearningEnv::BuildObservation(
        world,
        self,
        tc,
        ai,
        input.observation);
    input.actionMask = CChampionAILearningEnv::BuildActionMask(
        ai.debugAvailableActionMask,
        ai.debugAvailableSkillMask);
    input.rewardSample = CChampionAILearningEnv::BuildRewardSample(ai);

    ai.intent = ResolveChampionAIBrain(ai.brainType)
        .DecideLaneCombatIntent(ai, input);
}
```

기존 코드:

```cpp
bool_t ShouldAttackChampion(
    const TickContext& tc,
    ChampionAIComponent& ai,
    const ChampionAIContext& ctx)
{
    SampleLaneCombatIntent(tc, ai, ctx);
    return ai.intent == eChampionAIIntent::AttackChampion &&
        CanAttackChampion(ai, ctx);
}
```

아래로 교체:

```cpp
bool_t ShouldAttackChampion(
    CWorld& world,
    EntityID self,
    const TickContext& tc,
    ChampionAIComponent& ai,
    const ChampionAIContext& ctx)
{
    SampleLaneCombatIntent(world, self, tc, ai, ctx);
    return ai.intent == eChampionAIIntent::AttackChampion &&
        CanAttackChampion(ai, ctx);
}
```

기존 코드:

```cpp
return ShouldAttackChampion(tc, ai, ctx) &&
    TryEmitAttackChampion(world, tc, self, ai, champion, selfPos, ctx, outCommands);
```

아래로 교체:

```cpp
return ShouldAttackChampion(world, self, tc, ai, ctx) &&
    TryEmitAttackChampion(world, tc, self, ai, champion, selfPos, ctx, outCommands);
```

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp

기존 코드:

```cpp
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
```

아래로 교체:

```cpp
// 외부 판단 모듈(플래너/학습 정책) 연동 지점.
// OpenAI Five식 연결의 본질은 observation/actionMask/rewardSample을 읽고
// 선택한 action을 다시 GameCommand로 제한된 통로에서만 변환하는 것이다.
// 모듈이 붙기 전까지는 RuleBased로 위임해 정상 플레이를 보장한다.
class CDecisionChampionBrain final : public IChampionAIBrain
{
public:
	eChampionAIIntent DecideLaneCombatIntent(
		ChampionAIComponent& ai,
		const ChampionAIBrainInput& input) override
	{
		(void)input.observation;
		(void)input.actionMask;
		(void)input.rewardSample;
		return m_Fallback.DecideLaneCombatIntent(ai, input);
	}

private:
	CRuleBasedChampionBrain m_Fallback{};
};
```

2. 검증

검증 명령:
- `git diff --check -- Plan/S02_OPENAI_STYLE_BOT_AI_ENVIRONMENT_PLAN.md`
- `git diff --check -- Shared/GameSim/Systems/ChampionAI/ChampionAILearningTypes.h Shared/GameSim/Systems/ChampionAI/ChampionAILearningEnv.h Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp`
- `& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal`
- `& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal`
- `Server/Bin/Debug/WintersServer.exe --smoke-seconds=1`

확인 필요:
- 새 header-only 파일이 프로젝트 필터에 표시되어야 하면 `GameSim.vcxproj`와 `.filters`에 등록.
- OpenAI Five식 PPO/LSTM 정책은 이 세션에서 붙이지 않는다. 먼저 observation/action/reward/self-play export 계약을 고정한다.
- `Tools/SimLab`는 이미 deterministic headless runner이므로 다음 세션에서 `ChampionAILearningSample` export를 붙인다.
- 현재 챔피언 봇은 `CChampionAISystem` rule-based FSM이고, 정글 AI는 aggro basic attack command producer이며, 미니언 AI는 `GameRoomMinionAI`의 서버 NPC 전용 state/move/attack 로직이다.
