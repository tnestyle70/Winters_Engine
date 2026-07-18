Session - Fiora의 패시브/R 방향 표식, 5초 회복장, E 2타 치명타와 W CC 감지 색 전환을 서버 권위와 단일 FX cue 경로로 완성한다.
좌표: 신규 좌표 후보 · 축: C7 · C8
관련: 2026-07-16_FIORA_VITAL_RIPOSTE_JAX_DODGE_SERVER_AUTHORITY_PLAN.md · 2026-07-17_ZED_PASSIVE_E_R_AUTHORITATIVE_FX_PLAN.md

## 1. 결정 기록

① 문제·제약: 현재 패시브는 0개, R은 표식 1개와 즉시 80 피해·즉시 힐존, E는 5초/2타 모두 +30, W는 0.75초 플래그와 즉시 slow뿐이다. 목표는 패시브 1방향, R 4방향, 반경 6·5초 회복장, E 2타, W hard-CC 감지 1회다.
② 순진한 해법의 실패: 클라이언트가 방향·표식 파괴·회복·CC 성공을 판정하면 두 클라이언트와 replay가 갈라지고, 현재 BA visual에 spark를 붙이면 E가 아닌 모든 BA에서 노란 spark가 나온다.
③ 메커니즘: ServerPrivate 수치 → FioraSimComponent의 고정 상태 → DamageQueue/StatusEffect의 실제 성공 경계 → EffectTrigger 즉시 전이 + Snapshot 지속 상태 → Fiora visual hook/WFX로 고정한다.
④ 대조: 별도 Ability/Heal/AreaAura 시스템은 만들지 않는다. 최대 체력 피해는 기존 DamageRequest, 반경 판정·HP clamp는 Kindred R, 시각 cue는 기존 VisualHookRegistry를 재사용한다.
⑤ 대가: 패시브는 Fiora당 가까운 적 챔피언 1명만 표식하고, W는 이번 범위에서 hard CC 차단·성공 색·release slow/stun을 소유하되 모든 피해 무효화까지 넓히지 않는다. 다중 passive target 또는 full damage parry가 필요하면 이 선택은 틀린다.

이 문서는 2026-07-16 계획의 Fiora 부분을 대체한다. Jax 계획은 그대로 남긴다.

수치 기본안은 다음으로 고정한다. balance 변경은 JSON만 고친다.

- passive acquire range 6.0, vital offset 1.15, side threshold 0.45, lifetime 8초, respawn 1.5초
- vital damage: 대상 최대 체력의 3% 고정 피해
- R mark 8초, 네 방향, ring/heal radius 6.0, heal zone 5초
- heal tick 0.5초마다 아군 champion 40 HP
- E: 5초 안에 2회, 두 타 모두 기존 +30, 두 번째 타는 source StatComponent.critDamage를 쓰는 강제 치명타
- W: parry window 0.75초, Stun/Airborne 포착 시 indicator red/CC glow, release 적중 대상 1.5초 stun, 미포착 시 1.5초 slow

확인 필요: 위 3%와 40 HP/0.5초는 요청에 숫자가 없어 둔 적용 가능한 기본값이다. 최종 밸런스가 다르면 구현 시작 전에 두 값만 교체한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

현재 skill.fiora.basic_attack의 params:

~~~json
      "params": {},
~~~

아래로 교체:

~~~json
      "params": {
        "effectDurationSec": 8.0,
        "gap": 1.15,
        "halfAngleCos": 0.45,
        "radius": 0.9,
        "range": 6.0,
        "refreshDurationSec": 1.5
      },
~~~

현재 skill.fiora.r의 params와 damage 전체:

~~~json
      "params": {
        "baseDamage": 80.0
      },
      "damage": {
        "type": "Physical",
        "flags": [],
        "flatByRank": [
          80.0,
          80.0,
          80.0
        ],
        "totalAdRatioByRank": [
          0.0,
          0.0,
          0.0
        ],
        "bonusAdRatioByRank": [
          0.0,
          0.0,
          0.0
        ],
        "apRatioByRank": [
          0.0,
          0.0,
          0.0
        ],
        "targetMaxHpRatioByRank": [
          0.0,
          0.0,
          0.0
        ],
        "targetMissingHpRatioByRank": [
          0.0,
          0.0,
          0.0
        ]
      }
~~~

아래로 교체:

~~~json
      "params": {
        "effectDurationSec": 5.0,
        "gap": 1.15,
        "halfAngleCos": 0.45,
        "halfWidth": 0.9,
        "healBaseAmount": 40.0,
        "markDurationSec": 8.0,
        "maxStacks": 4.0,
        "radius": 6.0,
        "tickIntervalSec": 0.5
      },
      "damage": {
        "type": "True",
        "flags": [],
        "flatByRank": [
          0.0,
          0.0,
          0.0
        ],
        "totalAdRatioByRank": [
          0.0,
          0.0,
          0.0
        ],
        "bonusAdRatioByRank": [
          0.0,
          0.0,
          0.0
        ],
        "apRatioByRank": [
          0.0,
          0.0,
          0.0
        ],
        "targetMaxHpRatioByRank": [
          0.03,
          0.03,
          0.03
        ],
        "targetMissingHpRatioByRank": [
          0.0,
          0.0,
          0.0
        ]
      }
~~~

현재 skill.fiora.w의 params:

~~~json
      "params": {
        "moveSpeedMul": 0.5,
        "radius": 0.8,
        "range": 6.0,
        "slowDurationSec": 1.5
      },
~~~

아래로 교체:

~~~json
      "params": {
        "effectDurationSec": 0.75,
        "moveSpeedMul": 0.5,
        "radius": 0.8,
        "range": 6.0,
        "slowDurationSec": 1.5,
        "stunDurationSec": 1.5
      },
~~~

이 파일은 Zed 세션이 현재 수정 중이다. 위 block은 Zed handoff 뒤 같은 최신 파일을 다시 읽고 한 번에 적용한다. generated C++는 직접 고치지 않는다.

### 2-2. C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

Fiora slot 2의 기존 코드:

~~~json
                    "slot": 2,
                    "targetMode": "Conditional",
                    "stageCount": 1,
                    "stageWindowSec": 0.0,
                    "cooldownSec": 3.0,
                    "rangeMax": 0.0,
~~~

아래로 교체:

~~~json
                    "slot": 2,
                    "targetMode": "Direction",
                    "stageCount": 1,
                    "stageWindowSec": 0.0,
                    "cooldownSec": 3.0,
                    "rangeMax": 6.0,
~~~

기존 lockDurationSec 1.5는 유지한다. 생성된 SkillGameplayDefs.json은 수동 수정하지 않는다.

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/DamageTypes.h

기존 코드:

~~~cpp
enum eDamageFlags : uint32_t
{
    DamageFlag_None = 0,
    DamageFlag_CanCrit = 1u << 0,
    DamageFlag_CanLifesteal = 1u << 1,
    DamageFlag_OnHit = 1u << 2,
};
~~~

아래로 교체:

~~~cpp
enum eDamageFlags : uint32_t
{
    DamageFlag_None = 0,
    DamageFlag_CanCrit = 1u << 0,
    DamageFlag_CanLifesteal = 1u << 1,
    DamageFlag_OnHit = 1u << 2,
    DamageFlag_ForceCrit = 1u << 3,
};
~~~

ForceCrit는 Fiora 전용 이름이 아니라 범용 피해 의미다. OnHit이나 lifesteal을 새로 발동시키지 않고 기존 BA request의 치명타 판정만 확정한다.

### 2-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamagePipeline.cpp

ApplyCritIfNeeded의 기존 시작:

~~~cpp
        outCrit = false;
        if ((flags & DamageFlag_CanCrit) == 0u)
            return amount;
        if (req.source == NULL_ENTITY || !world.HasComponent<StatComponent>(req.source))
            return amount;

        const auto& sourceStat = world.GetComponent<StatComponent>(req.source);
        const f32_t chance = std::clamp(sourceStat.critChance, 0.f, 1.f);
~~~

아래로 교체:

~~~cpp
        outCrit = false;
        const bool_t bForceCrit = (flags & DamageFlag_ForceCrit) != 0u;
        if (!bForceCrit && (flags & DamageFlag_CanCrit) == 0u)
            return amount;
        if (req.source == NULL_ENTITY || !world.HasComponent<StatComponent>(req.source))
            return amount;

        const auto& sourceStat = world.GetComponent<StatComponent>(req.source);
        if (bForceCrit)
        {
            outCrit = true;
            return amount * std::max(1.f, sourceStat.critDamage);
        }

        const f32_t chance = std::clamp(sourceStat.critChance, 0.f, 1.f);
~~~

이 파일은 Zed 세션의 lethal preview가 현재 수정 중이다. Zed handoff 전에는 적용하지 않는다.

### 2-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/FioraSimComponent.h

기존 FioraSimComponent 전체를 아래로 교체:

~~~cpp
enum class eFioraVitalMode : u8_t
{
    None = 0u,
    Passive = 1u,
    GrandChallenge = 2u,
};

struct FioraVitalState
{
    EntityID entityTarget = NULL_ENTITY;
    eFioraVitalMode eMode = eFioraVitalMode::None;
    u8_t uActiveSideMask = 0u;
    u8_t uTriggeredSideMask = 0u;
    u8_t uGenerationOrdinal = 0u;
    u64_t uSpawnTick = 0u;
    u64_t uExpireTick = 0u;
    u64_t uNextSpawnTick = 0u;
};

struct FioraHealZoneState
{
    bool_t bActive = false;
    Vec3 vCenter{};
    u64_t uStartTick = 0u;
    u64_t uExpireTick = 0u;
    u64_t uNextHealTick = 0u;
};

struct FioraSimComponent
{
    bool_t bBladeworkActive = false;
    u8_t uBladeworkHitsRemaining = 0u;
    u8_t uBladeworkRank = 1u;
    u8_t uReservedBladework = 0u;
    u64_t uBladeworkExpireTick = 0u;

    bool_t bRiposteActive = false;
    bool_t bRiposteCaughtHardCC = false;
    bool_t bRiposteIndicatorDirty = false;
    u8_t uRiposteRank = 1u;
    u64_t uRiposteReleaseTick = 0u;
    Vec3 vRiposteDirection{ 0.f, 0.f, 1.f };

    FioraVitalState passiveVital{};
    FioraVitalState grandChallengeVital{};
    FioraHealZoneState healZone{};
};

static_assert(std::is_trivially_copyable_v<FioraSimComponent>);
~~~

fixed-size POD라 기존 keyframe registry 등록을 그대로 탄다. client EntityID나 vector는 이 Shared component에 넣지 않는다.

### 2-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Fiora/FioraGameSim.h

기존 forward declaration:

~~~cpp
class CWorld;
struct TickContext;
~~~

아래로 교체:

~~~cpp
class CWorld;
struct DamageRequest;
struct DamageResult;
struct StatusEffectApplyDesc;
struct TickContext;

struct FioraDamageAugment
{
    bool_t bBladeworkHit = false;
    bool_t bForcedCrit = false;
    u8_t uBladeworkStage = 0u;
};
~~~

namespace FioraGameSim의 기존 ConsumeBasicAttackDamage 선언 아래에 추가:

~~~cpp
	FioraDamageAugment PrepareOutgoingDamage(
		CWorld& world,
		const TickContext& tc,
		DamageRequest& request);
	void FinalizeOutgoingDamage(
		CWorld& world,
		const TickContext& tc,
		const DamageRequest& request,
		const DamageResult& result,
		const FioraDamageAugment& augment);
	bool_t TryTriggerVitalFromHit(
		CWorld& world,
		const TickContext& tc,
		EntityID source,
		EntityID target);
	bool_t TryInterceptIncomingStatus(
		CWorld& world,
		EntityID target,
		const StatusEffectApplyDesc& desc);
~~~

### 2-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Fiora/FioraGameSim.cpp

include 목록에 추가하고, 현재 중복된 GameplayStateQuery.h include 한 줄은 이 변경이 건드린 범위에서만 제거한다:

~~~cpp
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
~~~

EnqueuePhysicalDamage 안에서 request.rank = rank; 아래에 추가:

~~~cpp
        request.iSourceSlot = static_cast<u8_t>(skillId & 0xffu);
        request.eSourceKind = request.iSourceSlot ==
            static_cast<u8_t>(eSkillSlot::BasicAttack)
            ? eDamageSourceKind::BasicAttack
            : eDamageSourceKind::Skill;
~~~

OnW는 즉시 cone slow block을 삭제하고 아래 arm-only block으로 교체한다:

~~~cpp
        const f32_t riposteWindowSec = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::EffectDurationSec,
            0.75f);
        const Vec3 direction = ctx.pCommand
            ? WintersMath::NormalizeXZOrZero(ctx.pCommand->direction)
            : Vec3{};
        if (direction.x == 0.f && direction.z == 0.f)
            return;

        FioraSimComponent& state = EnsureFioraState(world, ctx.casterEntity);
        state.bRiposteActive = true;
        state.bRiposteCaughtHardCC = false;
        state.bRiposteIndicatorDirty = false;
        state.uRiposteRank = ctx.skillRank;
        state.uRiposteReleaseTick = ctx.pTickCtx->tickIndex +
            SecondsToTicksCeil(riposteWindowSec);
        state.vRiposteDirection = direction;
        ClearMove(world, ctx.casterEntity);
        EnqueueFioraStageEffect(
            world, *ctx.pTickCtx, ctx.casterEntity, NULL_ENTITY,
            GameplayHookVariant::W_Recovery,
            static_cast<u8_t>(eSkillSlot::W),
            1u,
            world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition(),
            static_cast<u32_t>(riposteWindowSec * 1000.f + 0.5f));
~~~

OnE의 첫 guard는 `!ctx.pWorld || !ctx.pTickCtx`로 교체하고 state 설정 block을 아래로 교체:

기존 local bonus block은 DamageQueue의 `PrepareOutgoingDamage`가 data를 읽으므로 삭제:

~~~cpp
        const f32_t eDamageBonus = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::BaseDamage,
            30.f);
~~~

~~~cpp
        FioraSimComponent& state = EnsureFioraState(*ctx.pWorld, ctx.casterEntity);
        state.bBladeworkActive = true;
        state.uBladeworkHitsRemaining = static_cast<u8_t>(
            (std::clamp)(eMaxStacks, 0.f, 255.f));
        state.uBladeworkRank = ctx.skillRank;
        state.uBladeworkExpireTick = ctx.pTickCtx->tickIndex +
            SecondsToTicksCeil(eWindowSec);
~~~

OnR의 old bGrandChallengeActive/grandChallengeTimerSec/grandChallengeTarget와 즉시 EnqueuePhysicalDamage block 전체를 아래로 교체:

~~~cpp
        const f32_t markDurationSec = ResolveFioraSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::MarkDurationSec,
            8.f);
        FioraSimComponent& state = EnsureFioraState(world, ctx.casterEntity);
        if (state.passiveVital.entityTarget != NULL_ENTITY &&
            state.passiveVital.uActiveSideMask != 0u)
        {
            const u8_t side = ResolveFirstActiveSide(
                state.passiveVital.uActiveSideMask);
            EnqueueFioraStageEffect(
                world, *ctx.pTickCtx, ctx.casterEntity,
                state.passiveVital.entityTarget,
                GameplayHookVariant::Passive_Trigger,
                static_cast<u8_t>(eSkillSlot::BasicAttack),
                static_cast<u8_t>(5u + side),
                {},
                300u);
        }
        state.passiveVital = FioraVitalState{};
        state.grandChallengeVital = FioraVitalState{};
        state.grandChallengeVital.entityTarget = target;
        state.grandChallengeVital.eMode = eFioraVitalMode::GrandChallenge;
        state.grandChallengeVital.uActiveSideMask = 0x0fu;
        state.grandChallengeVital.uSpawnTick = ctx.pTickCtx->tickIndex;
        state.grandChallengeVital.uExpireTick = ctx.pTickCtx->tickIndex +
            SecondsToTicksCeil(markDurationSec);
        const Vec3 targetPosition =
            world.GetComponent<TransformComponent>(target).GetPosition();
        EnqueueFioraStageEffect(
            world, *ctx.pTickCtx, ctx.casterEntity, target,
            GameplayHookVariant::R_Recovery,
            static_cast<u8_t>(eSkillSlot::R),
            9u,
            targetPosition,
            static_cast<u32_t>(markDurationSec * 1000.f + 0.5f));
~~~

ConsumeBasicAttackDamage 전체는 아래로 교체한다. E 소비는 피해가 실제 적용되는 DamageQueue로 옮긴다.

~~~cpp
    f32_t ConsumeBasicAttackDamage(CWorld&, EntityID, f32_t baseDamage)
    {
        return baseDamage;
    }
~~~

anonymous namespace의 `OnQ` 위에 다음 helper 계약을 추가한다. OnW/OnR도 사용하므로 함수 정의 순서를 이 위치로 고정한다:

~~~cpp
    constexpr u8_t kFioraSideCount = 4u;

    u64_t SecondsToTicksCeil(f32_t seconds)
    {
        if (!std::isfinite(seconds) || seconds <= 0.f)
            return 0u;
        return static_cast<u64_t>(std::ceil(
            static_cast<f64_t>(seconds) *
            static_cast<f64_t>(DeterministicTime::kTicksPerSecond)));
    }

    Vec3 DirectionFromFioraSide(u8_t side)
    {
        switch (side & 0x03u)
        {
        case 0u: return { 0.f, 0.f, 1.f };
        case 1u: return { 1.f, 0.f, 0.f };
        case 2u: return { 0.f, 0.f, -1.f };
        default: return { -1.f, 0.f, 0.f };
        }
    }

    u8_t ResolveFirstActiveSide(u8_t sideMask)
    {
        for (u8_t side = 0u; side < kFioraSideCount; ++side)
        {
            if ((sideMask & static_cast<u8_t>(1u << side)) != 0u)
                return side;
        }
        return 0u;
    }

    u8_t ResolveFioraHitSide(const Vec3& targetPosition, const Vec3& sourcePosition)
    {
        const Vec3 outward = WintersMath::NormalizeXZOrZero(
            Vec3{ sourcePosition.x - targetPosition.x, 0.f,
                  sourcePosition.z - targetPosition.z });
        u8_t bestSide = 0u;
        f32_t bestDot = -2.f;
        for (u8_t side = 0u; side < kFioraSideCount; ++side)
        {
            const Vec3 direction = DirectionFromFioraSide(side);
            const f32_t dot = outward.x * direction.x + outward.z * direction.z;
            if (dot > bestDot)
            {
                bestDot = dot;
                bestSide = side;
            }
        }
        return bestSide;
    }

    void EnqueueFioraStageEffect(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        u16_t variant,
        u8_t slot,
        u8_t stage,
        const Vec3& position,
        u32_t durationMs)
    {
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = source;
        event.targetEntity = target;
        event.effectId = MakeGameplayHookId(eChampion::FIORA, variant);
        event.slot = slot;
        event.rank = 1u;
        event.sourceChampion = eChampion::FIORA;
        if (source != NULL_ENTITY &&
            world.HasComponent<ChampionComponent>(source))
        {
            event.sourceTeam = static_cast<u8_t>(
                world.GetComponent<ChampionComponent>(source).team);
        }
        event.flags = static_cast<u16_t>(
            (static_cast<u16_t>(stage & 0x0fu) << 12) |
            static_cast<u16_t>(slot));
        event.position = position;
        event.startTick = tc.tickIndex;
        event.durationMs = static_cast<u16_t>((std::min)(durationMs, 65535u));
        EnqueueReplicatedEvent(world, event);
    }
~~~

namespace FioraGameSim에 PrepareOutgoingDamage를 추가:

~~~cpp
    FioraDamageAugment PrepareOutgoingDamage(
        CWorld& world,
        const TickContext& tc,
        DamageRequest& request)
    {
        FioraDamageAugment augment{};
        if (request.eSourceKind != eDamageSourceKind::BasicAttack ||
            request.source == NULL_ENTITY ||
            request.target == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(request.source) ||
            world.GetComponent<ChampionComponent>(request.source).id != eChampion::FIORA ||
            !world.HasComponent<FioraSimComponent>(request.source))
        {
            return augment;
        }

        FioraSimComponent& state =
            world.GetComponent<FioraSimComponent>(request.source);
        if (!state.bBladeworkActive ||
            state.uBladeworkHitsRemaining == 0u ||
            tc.tickIndex >= state.uBladeworkExpireTick)
        {
            return augment;
        }

        const f32_t bonus = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            request.source,
            tc,
            eChampion::FIORA,
            static_cast<u8_t>(eSkillSlot::E),
            eSkillEffectParamId::BaseDamage,
            30.f);
        augment.bBladeworkHit = true;
        augment.uBladeworkStage =
            state.uBladeworkHitsRemaining >= 2u ? 1u : 2u;
        augment.bForcedCrit = augment.uBladeworkStage == 2u;
        request.flatAmount += bonus;
        if (augment.bForcedCrit)
            request.flags |= DamageFlag_ForceCrit;
        return augment;
    }
~~~

FinalizeOutgoingDamage는 result.finalAmount > 0일 때만 E charge를 소비하고 BA/Q vital을 판정한다. 아래 body를 추가:

~~~cpp
    void FinalizeOutgoingDamage(
        CWorld& world,
        const TickContext& tc,
        const DamageRequest& request,
        const DamageResult& result,
        const FioraDamageAugment& augment)
    {
        if (result.finalAmount <= 0.f ||
            request.source == NULL_ENTITY ||
            request.target == NULL_ENTITY)
        {
            return;
        }

        if (augment.bBladeworkHit &&
            world.HasComponent<FioraSimComponent>(request.source))
        {
            FioraSimComponent& state =
                world.GetComponent<FioraSimComponent>(request.source);
            if (state.uBladeworkHitsRemaining > 0u)
                --state.uBladeworkHitsRemaining;
            if (state.uBladeworkHitsRemaining == 0u)
                state.bBladeworkActive = false;

            const Vec3 position = world.HasComponent<TransformComponent>(request.target)
                ? world.GetComponent<TransformComponent>(request.target).GetPosition()
                : Vec3{};
            EnqueueFioraStageEffect(
                world, tc, request.source, request.target,
                GameplayHookVariant::E_Recovery,
                static_cast<u8_t>(eSkillSlot::E),
                augment.uBladeworkStage,
                position,
                400u);
        }

        const bool_t bVitalCapable =
            request.eSourceKind == eDamageSourceKind::BasicAttack ||
            request.iSourceSlot == static_cast<u8_t>(eSkillSlot::Q);
        if (bVitalCapable)
            TryTriggerVitalFromHit(world, tc, request.source, request.target);
    }
~~~

위 block이 호출하는 TryTriggerVitalFromHit는 다음 규칙의 실제 함수 body로 같은 namespace에 추가한다.

~~~cpp
    bool_t TryTriggerVitalFromHit(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target)
    {
        if (!world.HasComponent<FioraSimComponent>(source) ||
            !world.HasComponent<ChampionComponent>(source) ||
            !world.HasComponent<TransformComponent>(source) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        FioraSimComponent& state = world.GetComponent<FioraSimComponent>(source);
        const Vec3 sourcePosition =
            world.GetComponent<TransformComponent>(source).GetPosition();
        const Vec3 targetPosition =
            world.GetComponent<TransformComponent>(target).GetPosition();
        const u8_t side = ResolveFioraHitSide(targetPosition, sourcePosition);
        const u8_t bit = static_cast<u8_t>(1u << side);

        FioraVitalState* vital = nullptr;
        if (state.grandChallengeVital.entityTarget == target &&
            (state.grandChallengeVital.uActiveSideMask & bit) != 0u)
        {
            vital = &state.grandChallengeVital;
        }
        else if (state.passiveVital.entityTarget == target &&
            (state.passiveVital.uActiveSideMask & bit) != 0u)
        {
            vital = &state.passiveVital;
        }
        if (!vital)
            return false;

        const eSkillEffectParamId thresholdParam =
            eSkillEffectParamId::HalfAngleCos;
        const u8_t dataSlot = vital->eMode == eFioraVitalMode::GrandChallenge
            ? static_cast<u8_t>(eSkillSlot::R)
            : static_cast<u8_t>(eSkillSlot::BasicAttack);
        const f32_t threshold = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world, source, tc, eChampion::FIORA, dataSlot, thresholdParam, 0.45f);
        const Vec3 sideDirection = DirectionFromFioraSide(side);
        const Vec3 outward = WintersMath::NormalizeXZOrZero(
            Vec3{ sourcePosition.x - targetPosition.x, 0.f,
                  sourcePosition.z - targetPosition.z });
        if (outward.x * sideDirection.x + outward.z * sideDirection.z < threshold)
            return false;

        vital->uActiveSideMask &= static_cast<u8_t>(~bit);
        vital->uTriggeredSideMask |= bit;

        DamageRequest vitalDamage{};
        if (GameplayDefinitionQuery::BuildSkillDamageRequest(
                world, source, target, tc, eChampion::FIORA,
                static_cast<u8_t>(eSkillSlot::R), 1u,
                world.GetComponent<ChampionComponent>(source).team,
                eDamageSourceKind::Skill,
                vitalDamage))
        {
            vitalDamage.skillId = 0u;
            vitalDamage.iSourceSlot = static_cast<u8_t>(eSkillSlot::SLOT_END);
            EnqueueDamageRequest(world, vitalDamage);
        }

        if (vital->eMode == eFioraVitalMode::Passive)
        {
            EnqueueFioraStageEffect(
                world, tc, source, target,
                GameplayHookVariant::Passive_Trigger,
                static_cast<u8_t>(eSkillSlot::BasicAttack),
                static_cast<u8_t>(5u + side),
                targetPosition,
                300u);
            vital->entityTarget = NULL_ENTITY;
            const f32_t refreshDurationSec =
                GameplayDefinitionQuery::ResolveSkillEffectParam(
                    world, source, tc, eChampion::FIORA,
                    static_cast<u8_t>(eSkillSlot::BasicAttack),
                    eSkillEffectParamId::RefreshDurationSec,
                    1.5f);
            vital->uNextSpawnTick = tc.tickIndex +
                SecondsToTicksCeil(refreshDurationSec);
            return true;
        }

        EnqueueFioraStageEffect(
            world, tc, source, target,
            GameplayHookVariant::R_Recovery,
            static_cast<u8_t>(eSkillSlot::R),
            static_cast<u8_t>(1u + side),
            targetPosition,
            300u);

        if (vital->uActiveSideMask == 0u)
        {
            const f32_t healDurationSec =
                GameplayDefinitionQuery::ResolveSkillEffectParam(
                    world, source, tc, eChampion::FIORA,
                    static_cast<u8_t>(eSkillSlot::R),
                    eSkillEffectParamId::EffectDurationSec,
                    5.f);
            state.healZone.bActive = true;
            state.healZone.vCenter = targetPosition;
            state.healZone.uStartTick = tc.tickIndex;
            state.healZone.uExpireTick = tc.tickIndex +
                SecondsToTicksCeil(healDurationSec);
            state.healZone.uNextHealTick = tc.tickIndex;
            EnqueueFioraStageEffect(
                world, tc, source, NULL_ENTITY,
                GameplayHookVariant::R_Recovery,
                static_cast<u8_t>(eSkillSlot::R),
                5u,
                targetPosition,
                static_cast<u32_t>(healDurationSec * 1000.f + 0.5f));
            state.grandChallengeVital = FioraVitalState{};
        }
        return true;
    }
~~~

TryInterceptIncomingStatus는 모든 status overload가 호출할 수 있게 TickContext 없이 기록만 하고, 다음 Fiora Tick이 cue를 보낸다.

~~~cpp
    bool_t TryInterceptIncomingStatus(
        CWorld& world,
        EntityID target,
        const StatusEffectApplyDesc& desc)
    {
        constexpr u32_t kHardCrowdControl =
            kGameplayStateStunnedFlag |
            kGameplayStateAirborneFlag;
        if ((desc.stateFlags & kHardCrowdControl) == 0u ||
            desc.sourceEntity == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(target) ||
            world.GetComponent<ChampionComponent>(target).id != eChampion::FIORA ||
            !world.HasComponent<FioraSimComponent>(target))
        {
            return false;
        }

        const eTeam sourceTeam =
            GameplayStateQuery::ResolveEntityTeam(world, desc.sourceEntity);
        const eTeam targetTeam =
            GameplayStateQuery::ResolveEntityTeam(world, target);
        FioraSimComponent& state = world.GetComponent<FioraSimComponent>(target);
        if (!state.bRiposteActive ||
            sourceTeam == eTeam::Neutral ||
            sourceTeam == targetTeam)
        {
            return false;
        }

        state.bRiposteCaughtHardCC = true;
        state.bRiposteIndicatorDirty = true;
        return true;
    }
~~~

FindEnemyInCone의 동률 판정은 EntityID까지 고정한다. 기존 `if (distSq < bestDistSq)`를 아래로 교체:

~~~cpp
                    if (distSq < bestDistSq ||
                        (distSq == bestDistSq &&
                         (best == NULL_ENTITY || entity < best)))
~~~

anonymous namespace에 다음 단일-use Tick helper를 추가한다. 새 system/class는 만들지 않는다.

~~~cpp
    bool_t IsLiveChampion(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY ||
            !world.IsAlive(entity) ||
            !world.HasComponent<ChampionComponent>(entity) ||
            !world.HasComponent<HealthComponent>(entity) ||
            !world.HasComponent<TransformComponent>(entity))
        {
            return false;
        }
        const HealthComponent& health =
            world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    void MirrorChampionHealth(
        CWorld& world,
        EntityID entity,
        const HealthComponent& health)
    {
        ChampionComponent& champion =
            world.GetComponent<ChampionComponent>(entity);
        champion.hp = health.fCurrent;
        champion.maxHp = health.fMaximum;
    }

    EntityID FindNearestEnemyChampion(
        CWorld& world,
        EntityID source,
        f32_t range)
    {
        if (!IsLiveChampion(world, source))
            return NULL_ENTITY;

        const ChampionComponent& sourceChampion =
            world.GetComponent<ChampionComponent>(source);
        const Vec3 sourcePosition =
            world.GetComponent<TransformComponent>(source).GetPosition();
        const f32_t rangeSq = range * range;
        EntityID best = NULL_ENTITY;
        f32_t bestDistanceSq = rangeSq;
        const auto candidates =
            DeterministicEntityIterator<TransformComponent>::CollectSorted(world);
        for (EntityID candidate : candidates)
        {
            if (candidate == source ||
                !IsLiveChampion(world, candidate) ||
                world.GetComponent<ChampionComponent>(candidate).team ==
                    sourceChampion.team ||
                !GameplayStateQuery::CanBeTargetedBy(world, source, candidate))
            {
                continue;
            }
            const f32_t distanceSq = WintersMath::DistanceSqXZ(
                sourcePosition,
                world.GetComponent<TransformComponent>(candidate).GetPosition());
            if (distanceSq > rangeSq)
                continue;
            if (distanceSq < bestDistanceSq ||
                (distanceSq == bestDistanceSq &&
                 (best == NULL_ENTITY || candidate < best)))
            {
                best = candidate;
                bestDistanceSq = distanceSq;
            }
        }
        return best;
    }

    void TickBladework(
        const TickContext& tc,
        FioraSimComponent& state)
    {
        if (state.bBladeworkActive &&
            tc.tickIndex >= state.uBladeworkExpireTick)
        {
            state.bBladeworkActive = false;
            state.uBladeworkHitsRemaining = 0u;
        }
    }

    void TickRiposte(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        FioraSimComponent& state)
    {
        if (!state.bRiposteActive)
            return;

        if (state.bRiposteIndicatorDirty)
        {
            state.bRiposteIndicatorDirty = false;
            const u64_t remainingTicks = state.uRiposteReleaseTick > tc.tickIndex
                ? state.uRiposteReleaseTick - tc.tickIndex
                : 0u;
            EnqueueFioraStageEffect(
                world, tc, source, NULL_ENTITY,
                GameplayHookVariant::W_Recovery,
                static_cast<u8_t>(eSkillSlot::W),
                2u,
                world.GetComponent<TransformComponent>(source).GetPosition(),
                static_cast<u32_t>(remainingTicks * 1000u /
                    DeterministicTime::kTicksPerSecond));
        }
        if (tc.tickIndex < state.uRiposteReleaseTick)
            return;

        const ChampionComponent& sourceChampion =
            world.GetComponent<ChampionComponent>(source);
        const Vec3 origin =
            world.GetComponent<TransformComponent>(source).GetPosition();
        const f32_t range = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world, source, tc, eChampion::FIORA,
            static_cast<u8_t>(eSkillSlot::W),
            eSkillEffectParamId::Range,
            6.f);
        const f32_t radius = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world, source, tc, eChampion::FIORA,
            static_cast<u8_t>(eSkillSlot::W),
            eSkillEffectParamId::Radius,
            0.8f);
        const EntityID hitTarget = FindEnemyInCone(
            world, source, sourceChampion.team, origin,
            state.vRiposteDirection, range, radius);
        if (hitTarget != NULL_ENTITY)
        {
            if (state.bRiposteCaughtHardCC)
            {
                const f32_t stunDurationSec =
                    GameplayDefinitionQuery::ResolveSkillEffectParam(
                        world, source, tc, eChampion::FIORA,
                        static_cast<u8_t>(eSkillSlot::W),
                        eSkillEffectParamId::StunDurationSec,
                        1.5f);
                GameplayStatus::ApplyStun(
                    world, tc, hitTarget, source,
                    eChampion::FIORA, eSkillSlot::W, stunDurationSec);
            }
            else
            {
                const f32_t slowDurationSec =
                    GameplayDefinitionQuery::ResolveSkillEffectParam(
                        world, source, tc, eChampion::FIORA,
                        static_cast<u8_t>(eSkillSlot::W),
                        eSkillEffectParamId::SlowDurationSec,
                        1.5f);
                const f32_t moveSpeedMul =
                    GameplayDefinitionQuery::ResolveSkillEffectParam(
                        world, source, tc, eChampion::FIORA,
                        static_cast<u8_t>(eSkillSlot::W),
                        eSkillEffectParamId::MoveSpeedMul,
                        0.5f);
                GameplayStatus::ApplySlow(
                    world, tc, hitTarget, source,
                    eChampion::FIORA, eSkillSlot::W,
                    slowDurationSec, moveSpeedMul);
            }
            FioraGameSim::TryTriggerVitalFromHit(
                world, tc, source, hitTarget);
        }

        EnqueueFioraStageEffect(
            world, tc, source, NULL_ENTITY,
            GameplayHookVariant::W_Recovery,
            static_cast<u8_t>(eSkillSlot::W),
            3u,
            origin,
            200u);
        state.bRiposteActive = false;
        state.bRiposteCaughtHardCC = false;
        state.bRiposteIndicatorDirty = false;
    }

    void TickPassiveVital(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        FioraSimComponent& state)
    {
        const f32_t range = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world, source, tc, eChampion::FIORA,
            static_cast<u8_t>(eSkillSlot::BasicAttack),
            eSkillEffectParamId::Range,
            6.f);
        const f32_t refreshDurationSec =
            GameplayDefinitionQuery::ResolveSkillEffectParam(
                world, source, tc, eChampion::FIORA,
                static_cast<u8_t>(eSkillSlot::BasicAttack),
                eSkillEffectParamId::RefreshDurationSec,
                1.5f);
        FioraVitalState& vital = state.passiveVital;
        if (vital.entityTarget != NULL_ENTITY)
        {
            const bool_t bRClaimsTarget =
                state.grandChallengeVital.entityTarget == vital.entityTarget &&
                state.grandChallengeVital.uActiveSideMask != 0u;
            const bool_t bOutOfRange = IsLiveChampion(world, vital.entityTarget)
                ? WintersMath::DistanceSqXZ(
                    world.GetComponent<TransformComponent>(source).GetPosition(),
                    world.GetComponent<TransformComponent>(vital.entityTarget)
                        .GetPosition()) > range * range
                : true;
            if (bRClaimsTarget || bOutOfRange ||
                tc.tickIndex >= vital.uExpireTick)
            {
                const u8_t side = ResolveFirstActiveSide(vital.uActiveSideMask);
                EnqueueFioraStageEffect(
                    world, tc, source, vital.entityTarget,
                    GameplayHookVariant::Passive_Trigger,
                    static_cast<u8_t>(eSkillSlot::BasicAttack),
                    static_cast<u8_t>(5u + side),
                    {},
                    300u);
                const u8_t ordinal = vital.uGenerationOrdinal;
                vital = FioraVitalState{};
                vital.uGenerationOrdinal = ordinal;
                vital.uNextSpawnTick = tc.tickIndex +
                    SecondsToTicksCeil(refreshDurationSec);
            }
            else
            {
                return;
            }
        }
        if (tc.tickIndex < vital.uNextSpawnTick)
            return;

        const EntityID target = FindNearestEnemyChampion(world, source, range);
        if (target == NULL_ENTITY ||
            state.grandChallengeVital.entityTarget == target)
        {
            return;
        }
        const f32_t durationSec =
            GameplayDefinitionQuery::ResolveSkillEffectParam(
                world, source, tc, eChampion::FIORA,
                static_cast<u8_t>(eSkillSlot::BasicAttack),
                eSkillEffectParamId::EffectDurationSec,
                8.f);
        const u64_t subSeed = tc.pRng
            ? tc.pRng->MakeSubSeed(
                tc.tickIndex,
                static_cast<u32_t>(source),
                static_cast<u16_t>(
                    GameplayHookVariant::Passive_Trigger +
                    vital.uGenerationOrdinal))
            : (tc.tickIndex ^ (static_cast<u64_t>(source) << 32u) ^
                vital.uGenerationOrdinal);
        DeterministicRng localRng(subSeed);
        const u8_t side = static_cast<u8_t>(
            localRng.NextU32() % kFioraSideCount);
        vital.entityTarget = target;
        vital.eMode = eFioraVitalMode::Passive;
        vital.uActiveSideMask = static_cast<u8_t>(1u << side);
        ++vital.uGenerationOrdinal;
        vital.uSpawnTick = tc.tickIndex;
        vital.uExpireTick = tc.tickIndex + SecondsToTicksCeil(durationSec);
        EnqueueFioraStageEffect(
            world, tc, source, target,
            GameplayHookVariant::Passive_Trigger,
            static_cast<u8_t>(eSkillSlot::BasicAttack),
            static_cast<u8_t>(1u + side),
            world.GetComponent<TransformComponent>(target).GetPosition(),
            static_cast<u32_t>(durationSec * 1000.f + 0.5f));
    }

    void TickGrandChallenge(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        FioraSimComponent& state)
    {
        FioraVitalState& vital = state.grandChallengeVital;
        if (vital.entityTarget == NULL_ENTITY ||
            vital.uActiveSideMask == 0u)
        {
            return;
        }
        if (IsLiveChampion(world, vital.entityTarget) &&
            tc.tickIndex < vital.uExpireTick)
        {
            return;
        }
        EnqueueFioraStageEffect(
            world, tc, source, vital.entityTarget,
            GameplayHookVariant::R_Recovery,
            static_cast<u8_t>(eSkillSlot::R),
            6u,
            {},
            300u);
        vital = FioraVitalState{};
    }

    void TickHealZone(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        FioraSimComponent& state)
    {
        FioraHealZoneState& zone = state.healZone;
        if (!zone.bActive)
            return;
        if (tc.tickIndex >= zone.uExpireTick)
        {
            zone = FioraHealZoneState{};
            return;
        }
        if (tc.tickIndex < zone.uNextHealTick ||
            !world.HasComponent<ChampionComponent>(source))
        {
            return;
        }

        const f32_t radius = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world, source, tc, eChampion::FIORA,
            static_cast<u8_t>(eSkillSlot::R),
            eSkillEffectParamId::Radius,
            6.f);
        const f32_t healAmount = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world, source, tc, eChampion::FIORA,
            static_cast<u8_t>(eSkillSlot::R),
            eSkillEffectParamId::HealBaseAmount,
            40.f);
        const f32_t intervalSec = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world, source, tc, eChampion::FIORA,
            static_cast<u8_t>(eSkillSlot::R),
            eSkillEffectParamId::TickIntervalSec,
            0.5f);
        const eTeam sourceTeam =
            world.GetComponent<ChampionComponent>(source).team;
        const auto candidates =
            DeterministicEntityIterator<TransformComponent>::CollectSorted(world);
        for (EntityID candidate : candidates)
        {
            if (!IsLiveChampion(world, candidate) ||
                world.GetComponent<ChampionComponent>(candidate).team != sourceTeam ||
                WintersMath::DistanceSqXZ(
                    zone.vCenter,
                    world.GetComponent<TransformComponent>(candidate).GetPosition()) >
                    radius * radius)
            {
                continue;
            }
            HealthComponent& health =
                world.GetComponent<HealthComponent>(candidate);
            health.fCurrent = (std::min)(
                health.fMaximum,
                health.fCurrent + healAmount);
            MirrorChampionHealth(world, candidate, health);
        }
        zone.uNextHealTick += (std::max)(
            1ull,
            SecondsToTicksCeil(intervalSec));
    }
~~~

Tick의 기존 `world.ForEach<FioraSimComponent>` timer block 전체를 아래로 교체한다. 순서는 E → W → passive → R timeout → heal로 고정한다.

~~~cpp
        const auto fioraEntities =
            DeterministicEntityIterator<FioraSimComponent>::CollectSorted(world);
        for (EntityID entity : fioraEntities)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<FioraSimComponent>(entity) ||
                !world.HasComponent<ChampionComponent>(entity) ||
                !world.HasComponent<TransformComponent>(entity))
            {
                continue;
            }
            FioraSimComponent& state =
                world.GetComponent<FioraSimComponent>(entity);
            TickBladework(tc, state);
            TickRiposte(world, tc, entity, state);
            TickPassiveVital(world, tc, entity, state);
            TickGrandChallenge(world, tc, entity, state);
            TickHealZone(world, tc, entity, state);
        }
~~~

### 2-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp

include 목록의 ViegoGameSim.h 아래에 추가:

~~~cpp
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
~~~

기존 코드:

~~~cpp
        DamageRequest request = world.GetComponent<DamageRequestComponent>(entity);
        ApplyDataDrivenSkillFormula(world, tc, request);
        AccumulateBasicAttackItemOnHit(world, request);
        const bool_t bYasuoPassiveShieldReady =
            IsYasuoPassiveShieldReady(world, request.target);
        const DamageResult result = ApplyDamageRequest(world, tc, request);
~~~

아래로 교체:

~~~cpp
        DamageRequest request = world.GetComponent<DamageRequestComponent>(entity);
        ApplyDataDrivenSkillFormula(world, tc, request);
        const FioraDamageAugment fioraAugment =
            FioraGameSim::PrepareOutgoingDamage(world, tc, request);
        AccumulateBasicAttackItemOnHit(world, request);
        const bool_t bYasuoPassiveShieldReady =
            IsYasuoPassiveShieldReady(world, request.target);
        const DamageResult result = ApplyDamageRequest(world, tc, request);
        FioraGameSim::FinalizeOutgoingDamage(
            world, tc, request, result, fioraAugment);
~~~

이 위치를 쓰면 E charge와 vital은 miss/invalid cast가 아니라 실제 DamagePipeline 결과 뒤에만 소비된다. CombatActionSystem.cpp는 Zed 소유 그대로 둔다.

### 2-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp

include 목록에 추가:

~~~cpp
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
~~~

두 TryApplyStatusEffect overload의 첫 줄에 동일하게 추가:

~~~cpp
        if (FioraGameSim::TryInterceptIncomingStatus(world, target, desc))
            return false;
~~~

TickContext overload에서는 이 호출이 ApplyStatusEffectInternal과 InterruptActionsForCrowdControl보다 먼저여야 한다. 그래야 W가 caught flag를 기록한 뒤 자신의 action이 stun으로 끊기지 않는다.

### 2-10. C:/Users/user/Desktop/Winters/Shared/Schemas/Snapshot.fbs

기존 코드:

~~~fbs
enum GameplayStateKind : ushort {
    None = 0,
    EzrealRisingSpellForce = 1,
    EzrealEssenceFlux = 2,
    YasuoWindWall = 3
}
~~~

아래로 교체:

~~~fbs
enum GameplayStateKind : ushort {
    None = 0,
    EzrealRisingSpellForce = 1,
    EzrealEssenceFlux = 2,
    YasuoWindWall = 3,
    FioraVitalSet = 4,
    FioraHealZone = 5
}
~~~

FioraVitalSet encoding:

~~~text
sourceNet = Fiora
targetNet = marked enemy
startTick/expireTick = authoritative lifetime
flags bits 0..1 = eFioraVitalMode
flags bits 4..7 = active side mask
flags bits 8..11 = triggered side mask
magnitude0 = visual offset
magnitude1 = hit side threshold
~~~

FioraHealZone encoding:

~~~text
sourceNet = Fiora
position = fixed completion center
startTick/expireTick = 5 second authoritative lifetime
magnitude0 = radius
magnitude1 = heal per tick
~~~

### 2-11. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

ProjectileBarrierComponent row 생성 block 아래에 FioraSimComponent sorted iteration을 추가한다.

~~~cpp
    const auto fioraEntities =
        DeterministicEntityIterator<FioraSimComponent>::CollectSorted(world);
    for (EntityID entity : fioraEntities)
    {
        const FioraSimComponent& state =
            world.GetComponent<FioraSimComponent>(entity);
        const NetEntityId sourceNet = entityMap.ToNet(entity);
        if (sourceNet == NULL_NET_ENTITY)
            continue;

        const FioraVitalState* vitals[] = {
            &state.passiveVital,
            &state.grandChallengeVital
        };
        for (const FioraVitalState* vital : vitals)
        {
            if (!vital ||
                vital->entityTarget == NULL_ENTITY ||
                vital->uActiveSideMask == 0u ||
                vital->uExpireTick <= serverTick)
            {
                continue;
            }
            const NetEntityId targetNet = entityMap.ToNet(vital->entityTarget);
            if (targetNet == NULL_NET_ENTITY)
                continue;

            GameplayStateRow row{};
            row.kind = Shared::Schema::GameplayStateKind::FioraVitalSet;
            row.sourceNet = sourceNet;
            row.targetNet = targetNet;
            row.startTick = vital->uSpawnTick;
            row.expireTick = vital->uExpireTick;
            row.flags =
                static_cast<u32_t>(vital->eMode) |
                (static_cast<u32_t>(vital->uActiveSideMask) << 4u) |
                (static_cast<u32_t>(vital->uTriggeredSideMask) << 8u);
            row.magnitude0 = 1.15f;
            row.magnitude1 = 0.45f;
            gameplayStateRows.push_back(row);
        }

        if (state.healZone.bActive &&
            state.healZone.uExpireTick > serverTick)
        {
            GameplayStateRow row{};
            row.kind = Shared::Schema::GameplayStateKind::FioraHealZone;
            row.sourceNet = sourceNet;
            row.startTick = state.healZone.uStartTick;
            row.expireTick = state.healZone.uExpireTick;
            row.position = state.healZone.vCenter;
            row.magnitude0 = 6.f;
            row.magnitude1 = 40.f;
            gameplayStateRows.push_back(row);
        }
    }
~~~

include 목록에 `Shared/GameSim/Components/FioraSimComponent.h`를 추가한다. row sort comparator의 마지막 줄:

~~~cpp
            return lhs.startTick < rhs.startTick;
~~~

아래로 교체한다:

~~~cpp
            if (lhs.startTick != rhs.startTick)
                return lhs.startTick < rhs.startTick;
            return lhs.flags < rhs.flags;
~~~

### 2-12. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Fiora/Fiora_Components.h

기존 `FioraStateComponent` 전체를 presentation handle만 소유하도록 아래로 교체:

~~~cpp
#pragma once

#include "Engine_Defines.h"

#include <array>
#include <vector>

struct FioraStateComponent
{
    std::vector<EntityID> vecPassiveVitalVisuals;
    std::array<std::vector<EntityID>, 4> arrRVitalVisuals;
    std::vector<EntityID> vecRRingVisuals;
    std::vector<EntityID> vecRHealVisuals;
    std::vector<EntityID> vecWIndicatorVisuals;
};
~~~

기존 client `bBladeworkActive/bRiposteActive/bRActive` gameplay mirror는 삭제한다. 이 handle state는 presentation 전용이며 Shared/GameSim state와 gameplay 결과를 만들지 않는다.

### 2-13. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Fiora/Fiora_FxPresets.h

파일 전체를 아래로 교체:

~~~cpp
#pragma once

#include "Defines.h"
#include "ECS/Entity.h"
#include "WintersMath.h"

#include <array>
#include <vector>

class CWorld;

namespace Fiora::Fx
{
    void SpawnQSlash(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime);
    void SpawnWIndicator(CWorld& world, EntityID owner, bool_t bCaughtHardCC,
        f32_t fDuration, std::vector<EntityID>* pOutVisuals);
    void SpawnEBladeworkBuff(CWorld& world, EntityID owner, f32_t fDuration,
        std::vector<EntityID>* pOutVisuals = nullptr);
    void SpawnEHitSpark(CWorld& world, EntityID target, f32_t fLifetime);
    void SpawnPassiveVital(CWorld& world, EntityID target, u8_t uSide,
        f32_t fDuration, std::vector<EntityID>* pOutVisuals);
    void SpawnRActive(CWorld& world, EntityID target, f32_t fDuration,
        std::array<std::vector<EntityID>, 4>& arrVitalVisuals,
        std::vector<EntityID>* pOutRingVisuals);
    void SpawnRHealZone(CWorld& world, const Vec3& vCenter, f32_t fDuration,
        std::vector<EntityID>* pOutVisuals);
    void DestroyVisuals(CWorld& world, std::vector<EntityID>& vecVisuals);
}
~~~

### 2-14. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Fiora/Fiora_FxPresets.cpp

파일 전체를 아래로 교체:

~~~cpp
#include "GameObject/Champion/Fiora/Fiora_FxPresets.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "WintersMath.h"

namespace
{
    constexpr const char* kCueQCast = "Fiora.Q.Cast";
    constexpr const char* kCueWNormal = "Fiora.W.Indicator.Normal";
    constexpr const char* kCueWCC = "Fiora.W.Indicator.CC";
    constexpr const char* kCueECast = "Fiora.E.Buff";
    constexpr const char* kCueEHit = "Fiora.E.Hit";
    constexpr const char* kCuePassiveVital[4] = {
        "Fiora.Passive.Vital.North",
        "Fiora.Passive.Vital.East",
        "Fiora.Passive.Vital.South",
        "Fiora.Passive.Vital.West"
    };
    constexpr const char* kCueRActive = "Fiora.R.Active";
    constexpr const char* kCueRHeal = "Fiora.R.Heal";

    Vec3 ResolvePosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(entity))
        {
            return world.GetComponent<TransformComponent>(entity).GetPosition();
        }
        return {};
    }

    Vec3 ResolveForward(const Vec3& direction)
    {
        const Vec3 forward = WintersMath::NormalizeXZOrZero(direction);
        return (forward.x != 0.f || forward.z != 0.f)
            ? forward
            : Vec3{ 0.f, 0.f, 1.f };
    }

    void PlayAttached(
        CWorld& world,
        const char* cueName,
        EntityID owner,
        const Vec3& direction,
        f32_t lifetime,
        std::vector<EntityID>* pOutVisuals)
    {
        if (owner == NULL_ENTITY)
            return;
        FxCueContext cue{};
        cue.attachTo = owner;
        cue.vWorldPos = ResolvePosition(world, owner);
        cue.vForward = ResolveForward(direction);
        if (lifetime > 0.f)
        {
            cue.bOverrideLifetime = true;
            cue.fLifetimeOverride = lifetime;
        }
        CFxCuePlayer::PlayAll(world, cueName, cue, pOutVisuals);
    }

    void PlayWorld(
        CWorld& world,
        const char* cueName,
        const Vec3& position,
        f32_t lifetime,
        std::vector<EntityID>* pOutVisuals)
    {
        FxCueContext cue{};
        cue.vWorldPos = position;
        cue.attachTo = NULL_ENTITY;
        if (lifetime > 0.f)
        {
            cue.bOverrideLifetime = true;
            cue.fLifetimeOverride = lifetime;
        }
        CFxCuePlayer::PlayAll(world, cueName, cue, pOutVisuals);
    }
}

namespace Fiora::Fx
{
    void SpawnQSlash(
        CWorld& world,
        EntityID owner,
        const Vec3& dir,
        f32_t fLifetime)
    {
        PlayAttached(world, kCueQCast, owner, dir, fLifetime, nullptr);
    }

    void SpawnWIndicator(
        CWorld& world,
        EntityID owner,
        bool_t bCaughtHardCC,
        f32_t fDuration,
        std::vector<EntityID>* pOutVisuals)
    {
        PlayAttached(
            world,
            bCaughtHardCC ? kCueWCC : kCueWNormal,
            owner,
            {},
            fDuration,
            pOutVisuals);
    }

    void SpawnEBladeworkBuff(
        CWorld& world,
        EntityID owner,
        f32_t fDuration,
        std::vector<EntityID>* pOutVisuals)
    {
        PlayAttached(
            world, kCueECast, owner, {}, fDuration, pOutVisuals);
    }

    void SpawnEHitSpark(
        CWorld& world,
        EntityID target,
        f32_t fLifetime)
    {
        PlayAttached(
            world, kCueEHit, target, {}, fLifetime, nullptr);
    }

    void SpawnPassiveVital(
        CWorld& world,
        EntityID target,
        u8_t uSide,
        f32_t fDuration,
        std::vector<EntityID>* pOutVisuals)
    {
        if (uSide >= 4u)
            return;
        PlayAttached(
            world,
            kCuePassiveVital[uSide],
            target,
            {},
            fDuration,
            pOutVisuals);
    }

    void SpawnRActive(
        CWorld& world,
        EntityID target,
        f32_t fDuration,
        std::array<std::vector<EntityID>, 4>& arrVitalVisuals,
        std::vector<EntityID>* pOutRingVisuals)
    {
        std::vector<EntityID> spawned;
        PlayAttached(
            world, kCueRActive, target, {}, fDuration, &spawned);
        for (size_t index = 0u; index < spawned.size(); ++index)
        {
            if (index < arrVitalVisuals.size())
                arrVitalVisuals[index].push_back(spawned[index]);
            else if (pOutRingVisuals)
                pOutRingVisuals->push_back(spawned[index]);
        }
    }

    void SpawnRHealZone(
        CWorld& world,
        const Vec3& vCenter,
        f32_t fDuration,
        std::vector<EntityID>* pOutVisuals)
    {
        PlayWorld(world, kCueRHeal, vCenter, fDuration, pOutVisuals);
    }

    void DestroyVisuals(CWorld& world, std::vector<EntityID>& vecVisuals)
    {
        for (EntityID entity : vecVisuals)
        {
            if (entity != NULL_ENTITY && world.IsAlive(entity))
                world.DestroyEntity(entity);
        }
        vecVisuals.clear();
    }
}
~~~

`SpawnRActive`의 emitter 0..3=표식, 4=ring 순서는 2-22의 WFX 전문과 contract로 고정한다.

### 2-15. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Fiora/Fiora_Skills.h

파일 전체를 아래로 교체한다. client gameplay mutation hook 선언은 남기지 않는다.

~~~cpp
#pragma once

#include "GamePlay/VisualHookRegistry.h"

namespace Fiora::Visual
{
        void OnCastFrame_Q_Visual(VisualHookContext& ctx);
        void OnCastFrame_E_Visual(VisualHookContext& ctx);
        void OnPassiveTrigger_Visual(VisualHookContext& ctx);
        void OnRecovery_W_Visual(VisualHookContext& ctx);
        void OnRecovery_E_Visual(VisualHookContext& ctx);
        void OnRecovery_R_Visual(VisualHookContext& ctx);
}

void Fiora_KeepAlive();
~~~

### 2-16. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Fiora/Fiora_Skills.cpp

파일 전체를 아래로 교체한다. 기존 `ApplyDamage`, Q 위치 직접 변경, E/W/R client state mutation은 전부 제거한다.

~~~cpp
#include "GameObject/Champion/Fiora/Fiora_Skills.h"
#include "GameObject/Champion/Fiora/Fiora_Components.h"
#include "GameObject/Champion/Fiora/Fiora_FxPresets.h"

#include "ECS/World.h"

namespace Fiora::Visual
{
    void OnCastFrame_Q_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;
        Fx::SpawnQSlash(
            *ctx.pWorld,
            ctx.casterEntity,
            ctx.pCommand->direction,
            0.4f);
    }

    void OnCastFrame_E_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;
        Fx::SpawnEBladeworkBuff(
            *ctx.pWorld,
            ctx.casterEntity,
            5.f);
    }

    void OnPassiveTrigger_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand ||
            !ctx.pWorld->HasComponent<FioraStateComponent>(ctx.casterEntity))
        {
            return;
        }
        FioraStateComponent& state =
            ctx.pWorld->GetComponent<FioraStateComponent>(ctx.casterEntity);
        if (ctx.skillStage >= 1u && ctx.skillStage <= 4u)
        {
            Fx::DestroyVisuals(*ctx.pWorld, state.vecPassiveVitalVisuals);
            Fx::SpawnPassiveVital(
                *ctx.pWorld,
                ctx.pCommand->targetEntityId,
                static_cast<u8_t>(ctx.skillStage - 1u),
                ctx.fEffectLifetimeSec > 0.f
                    ? ctx.fEffectLifetimeSec
                    : 8.f,
                &state.vecPassiveVitalVisuals);
        }
        else if (ctx.skillStage >= 5u && ctx.skillStage <= 8u)
        {
            Fx::DestroyVisuals(*ctx.pWorld, state.vecPassiveVitalVisuals);
        }
    }

    void OnRecovery_W_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld ||
            !ctx.pWorld->HasComponent<FioraStateComponent>(ctx.casterEntity))
        {
            return;
        }
        FioraStateComponent& state =
            ctx.pWorld->GetComponent<FioraStateComponent>(ctx.casterEntity);
        Fx::DestroyVisuals(*ctx.pWorld, state.vecWIndicatorVisuals);
        if (ctx.skillStage == 1u || ctx.skillStage == 2u)
        {
            Fx::SpawnWIndicator(
                *ctx.pWorld,
                ctx.casterEntity,
                ctx.skillStage == 2u,
                ctx.fEffectLifetimeSec > 0.f
                    ? ctx.fEffectLifetimeSec
                    : 0.75f,
                &state.vecWIndicatorVisuals);
        }
    }

    void OnRecovery_E_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand ||
            ctx.pCommand->targetEntityId == NULL_ENTITY)
        {
            return;
        }
        Fx::SpawnEHitSpark(
            *ctx.pWorld,
            ctx.pCommand->targetEntityId,
            0.4f);
    }

    void OnRecovery_R_Visual(VisualHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand ||
            !ctx.pWorld->HasComponent<FioraStateComponent>(ctx.casterEntity))
        {
            return;
        }
        FioraStateComponent& state =
            ctx.pWorld->GetComponent<FioraStateComponent>(ctx.casterEntity);
        if (ctx.skillStage == 9u)
        {
            for (auto& visuals : state.arrRVitalVisuals)
                Fx::DestroyVisuals(*ctx.pWorld, visuals);
            Fx::DestroyVisuals(*ctx.pWorld, state.vecRRingVisuals);
            Fx::DestroyVisuals(*ctx.pWorld, state.vecRHealVisuals);
            Fx::SpawnRActive(
                *ctx.pWorld,
                ctx.pCommand->targetEntityId,
                ctx.fEffectLifetimeSec > 0.f
                    ? ctx.fEffectLifetimeSec
                    : 8.f,
                state.arrRVitalVisuals,
                &state.vecRRingVisuals);
            return;
        }
        if (ctx.skillStage >= 1u && ctx.skillStage <= 4u)
        {
            Fx::DestroyVisuals(
                *ctx.pWorld,
                state.arrRVitalVisuals[ctx.skillStage - 1u]);
            return;
        }
        for (auto& visuals : state.arrRVitalVisuals)
            Fx::DestroyVisuals(*ctx.pWorld, visuals);
        Fx::DestroyVisuals(*ctx.pWorld, state.vecRRingVisuals);
        Fx::DestroyVisuals(*ctx.pWorld, state.vecRHealVisuals);
        if (ctx.skillStage == 5u)
        {
            Fx::SpawnRHealZone(
                *ctx.pWorld,
                ctx.pCommand->groundPos,
                ctx.fEffectLifetimeSec > 0.f
                    ? ctx.fEffectLifetimeSec
                    : 5.f,
                &state.vecRHealVisuals);
        }
    }
}
~~~

### 2-17. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Fiora/Fiora_Registration.cpp

아래 include를 삭제:

~~~cpp
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
~~~

기존 hook constant 아래에 추가:

~~~cpp
    constexpr u32_t kFio_Passive =
        MakeHookId(eChampion::FIORA, 0x0051u);
    constexpr u32_t kFio_W_Recovery =
        MakeHookId(eChampion::FIORA, HookVariant::W_Recovery);
    constexpr u32_t kFio_E_Recovery =
        MakeHookId(eChampion::FIORA, HookVariant::E_Recovery);
    constexpr u32_t kFio_R_Recovery =
        MakeHookId(eChampion::FIORA, HookVariant::R_Recovery);
~~~

W SkillDef의 기존 target block:

~~~cpp
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 12.f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "spell2";
                s.lockDurationSec = 1.5f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
~~~

아래로 교체:

~~~cpp
                s.targetMode = eTargetMode::Direction;
                s.cooldownSec = 12.f; s.rangeMax = 6.f; s.manaCost = 0.f;
                s.animKey = "spell2";
                s.lockDurationSec = 1.5f; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
~~~

아래 client gameplay registrations를 삭제한다. 서버 FioraGameSim의 registry와 별개인 local damage/Q teleport 경로다.

~~~cpp
            CGameplayHookRegistry::Instance().Register(kFio_BA_Cast, &Fiora::Gameplay::OnCastFrame_BA);
            CGameplayHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::Gameplay::OnCastFrame_Q);
            CGameplayHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::Gameplay::OnCastFrame_W);
            CGameplayHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::Gameplay::OnCastFrame_E);
            CGameplayHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::Gameplay::OnCastFrame_R);
~~~

기존 visual registrations:

~~~cpp
            CVisualHookRegistry::Instance().Register(kFio_BA_Cast, &Fiora::Visual::OnCastFrame_BA_Visual);
            CVisualHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::Visual::OnCastFrame_W_Visual);
            CVisualHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::Visual::OnCastFrame_R_Visual);
~~~

아래로 교체한다. W/R 지속 FX는 server EffectTrigger만 재생하고 BA에는 E spark를 붙이지 않는다.

~~~cpp
            CVisualHookRegistry::Instance().Register(
                kFio_Q_Cast, &Fiora::Visual::OnCastFrame_Q_Visual);
            CVisualHookRegistry::Instance().Register(
                kFio_E_Cast, &Fiora::Visual::OnCastFrame_E_Visual);
            CVisualHookRegistry::Instance().Register(
                kFio_Passive, &Fiora::Visual::OnPassiveTrigger_Visual);
            CVisualHookRegistry::Instance().Register(
                kFio_W_Recovery, &Fiora::Visual::OnRecovery_W_Visual);
            CVisualHookRegistry::Instance().Register(
                kFio_E_Recovery, &Fiora::Visual::OnRecovery_E_Visual);
            CVisualHookRegistry::Instance().Register(
                kFio_R_Recovery, &Fiora::Visual::OnRecovery_R_Visual);
~~~

### 2-18. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Fiora/passive_vital_north.wfx

~~~json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.Passive.Vital.North",
  "emitters": [
    {
      "name": "passive_vital_north",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_crest.png",
      "lifetime": 8.0,
      "fade_in": 0.08,
      "fade_out": 0.45,
      "width": 1.35,
      "height": 1.35,
      "color": [0.55, 1.20, 1.35, 0.95],
      "attach_offset": [0.0, 1.15, 1.15],
      "billboard": true
    }
  ]
}
~~~

### 2-19. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Fiora/passive_vital_east.wfx

~~~json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.Passive.Vital.East",
  "emitters": [
    {
      "name": "passive_vital_east",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_crest.png",
      "lifetime": 8.0,
      "fade_in": 0.08,
      "fade_out": 0.45,
      "width": 1.35,
      "height": 1.35,
      "color": [0.55, 1.20, 1.35, 0.95],
      "attach_offset": [1.15, 1.15, 0.0],
      "billboard": true
    }
  ]
}
~~~

### 2-20. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Fiora/passive_vital_south.wfx

~~~json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.Passive.Vital.South",
  "emitters": [
    {
      "name": "passive_vital_south",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_crest.png",
      "lifetime": 8.0,
      "fade_in": 0.08,
      "fade_out": 0.45,
      "width": 1.35,
      "height": 1.35,
      "color": [0.55, 1.20, 1.35, 0.95],
      "attach_offset": [0.0, 1.15, -1.15],
      "billboard": true
    }
  ]
}
~~~

### 2-21. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Fiora/passive_vital_west.wfx

~~~json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.Passive.Vital.West",
  "emitters": [
    {
      "name": "passive_vital_west",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_crest.png",
      "lifetime": 8.0,
      "fade_in": 0.08,
      "fade_out": 0.45,
      "width": 1.35,
      "height": 1.35,
      "color": [0.55, 1.20, 1.35, 0.95],
      "attach_offset": [-1.15, 1.15, 0.0],
      "billboard": true
    }
  ]
}
~~~

### 2-22. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Fiora/r_mark.wfx

파일 전체를 아래로 교체:

~~~json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.R.Active",
  "emitters": [
    {
      "name": "r_vital_north",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_timeout_alphaslice.png",
      "lifetime": 8.0,
      "width": 1.45,
      "height": 1.45,
      "fade_in": 0.08,
      "fade_out": 0.50,
      "color": [0.65, 0.95, 1.45, 0.95],
      "attach_offset": [0.0, 1.20, 1.15],
      "billboard": true
    },
    {
      "name": "r_vital_east",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_timeout_alphaslice.png",
      "lifetime": 8.0,
      "width": 1.45,
      "height": 1.45,
      "fade_in": 0.08,
      "fade_out": 0.50,
      "color": [0.65, 0.95, 1.45, 0.95],
      "attach_offset": [1.15, 1.20, 0.0],
      "billboard": true
    },
    {
      "name": "r_vital_south",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_timeout_alphaslice.png",
      "lifetime": 8.0,
      "width": 1.45,
      "height": 1.45,
      "fade_in": 0.08,
      "fade_out": 0.50,
      "color": [0.65, 0.95, 1.45, 0.95],
      "attach_offset": [0.0, 1.20, -1.15],
      "billboard": true
    },
    {
      "name": "r_vital_west",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_timeout_alphaslice.png",
      "lifetime": 8.0,
      "width": 1.45,
      "height": 1.45,
      "fade_in": 0.08,
      "fade_out": 0.50,
      "color": [0.65, 0.95, 1.45, 0.95],
      "attach_offset": [-1.15, 1.20, 0.0],
      "billboard": true
    },
    {
      "name": "r_active_ring",
      "render_type": "GroundDecal",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_healzone_ring.png",
      "lifetime": 8.0,
      "width": 12.1,
      "height": 12.1,
      "fade_in": 0.10,
      "fade_out": 0.65,
      "color": [0.95, 0.75, 0.30, 0.68],
      "attach_offset": [0.0, 0.03, 0.0],
      "billboard": false
    }
  ]
}
~~~

### 2-23. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Fiora/r_heal.wfx

파일 전체를 아래로 교체:

~~~json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.R.Heal",
  "emitters": [
    {
      "name": "r_heal_zone",
      "render_type": "GroundDecal",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_healzone.png",
      "lifetime": 5.0,
      "fade_in": 0.10,
      "fade_out": 0.80,
      "width": 12.1,
      "height": 12.1,
      "color": [0.65, 1.15, 1.20, 0.72],
      "attach_offset": [0.0, 0.03, 0.0],
      "billboard": false
    }
  ]
}
~~~

### 2-24. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Fiora/e_buff.wfx

현재 entity-center e_sword_glow emitter를 아래 Weapon bone history ribbon으로 교체:

~~~json
    {
      "name": "e_weapon_history_trail",
      "render_type": "Ribbon",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_cas_trails_blue.png",
      "lifetime": 5.0,
      "fade_in": 0.05,
      "fade_out": 0.35,
      "width": 0.32,
      "color": [0.35, 1.25, 1.55, 0.82],
      "ribbon_point_count": 20,
      "history_trail": true,
      "trail_sample_interval": 0.02,
      "trail_head_width_scale": 0.85,
      "trail_tail_width_scale": 0.05,
      "trail_head_alpha_scale": 0.85,
      "trail_tail_alpha_scale": 0.0,
      "anchor": {
        "type": "Bone",
        "name": "Weapon",
        "offset": [0.0, 0.0, 0.0],
        "inherit_rotation": true,
        "fallback": "Entity"
      },
      "depth_write": false
    }
~~~

Fiora wskel에 Weapon bone이 실제 존재한다. bone 위치와 폭은 인게임 캡처로만 최종 미세 조정한다.

### 2-25. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Fiora/ba_hit.wfx

기존 name:

~~~json
  "name": "Fiora.BA.Hit",
~~~

아래로 교체:

~~~json
  "name": "Fiora.E.Hit",
~~~

기존 emitter name:

~~~json
      "name": "ba_hit_spark",
~~~

아래로 교체:

~~~json
      "name": "e_hit_spark",
~~~

texture fiora_base_e_hit_spark_yellow.png는 유지한다.

### 2-26. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Fiora/w_indicator_normal.wfx

~~~json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.W.Indicator.Normal",
  "emitters": [
    {
      "name": "w_indicator_blue",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_w_indicator_blue.png",
      "lifetime": 1.5,
      "fade_in": 0.04,
      "fade_out": 0.20,
      "width": 1.6,
      "height": 3.2,
      "color": [0.45, 0.85, 1.45, 0.88],
      "attach_offset": [0.0, 1.25, 0.0],
      "billboard": true
    }
  ]
}
~~~

### 2-27. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Fiora/w_indicator_cc.wfx

~~~json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.W.Indicator.CC",
  "emitters": [
    {
      "name": "w_indicator_red",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_w_indicator_red.png",
      "lifetime": 1.5,
      "fade_in": 0.02,
      "fade_out": 0.20,
      "width": 1.6,
      "height": 3.2,
      "color": [1.45, 0.25, 0.18, 0.92],
      "attach_offset": [0.0, 1.25, 0.0],
      "billboard": true
    },
    {
      "name": "w_indicator_cc_glow",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_w_indicator_cc_glow.png",
      "lifetime": 1.5,
      "fade_in": 0.02,
      "fade_out": 0.22,
      "width": 1.8,
      "height": 3.6,
      "color": [1.25, 0.95, 0.70, 0.86],
      "attach_offset": [0.0, 1.25, 0.0],
      "billboard": true
    }
  ]
}
~~~

### 2-28. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/EventApplier.h

Snapshot late-join/replay 수렴을 위해 다음 public upsert와 private cleanup을 추가해야 한다.

~~~cpp
    void UpsertFioraVitalSnapshot(...);
    void UpsertFioraHealZoneSnapshot(...);
    void DestroyFioraVitalVisuals(CWorld& world, u64_t uKey);
    void DestroyFioraHealZoneVisuals(CWorld& world, u64_t uKey);
~~~

확인 필요: EventApplier.cpp와 VisualHookContext lifetime 전달은 현재 Zed 세션이 수정 중이다. Zed handoff 뒤 최신 public/private layout과 Ezreal/Yasuo reconciliation anchor를 다시 읽고 exact member map body를 작성한다. 이 확인이 끝나기 전에는 snapshot schema/codegen을 실행하지 않는다.

### 2-29. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

기존 GameplayStateKind switch에 FioraVitalSet/FioraHealZone case를 추가해 CEventApplier upsert로 넘긴다.

확인 필요: 2-28의 exact signature가 Zed handoff 뒤 확정되어야 case code를 고정할 수 있다. event 전이는 먼저 구현할 수 있지만 이 snapshot 수렴까지 끝나야 기능 완료로 판정한다.

### 2-30. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunFioraVitalGrandChallengeProbe()`와 main의 `--fiora-vital-only` 분기를 추가한다.

확인 필요: 이 파일은 Zed probe가 현재 수정 중이므로 handoff 뒤 최신 `RunZedPassiveDeathMarkProbe`와 main 선두 분기 anchor를 다시 읽고 exact body를 작성한다. probe는 다음 결과를 모두 한 함수에서 고정해야 한다.

- 같은 seed/keyframe restore에서 passive target·side·spawn tick이 동일하다.
- BA/Q/W의 맞는 side hit만 표식을 제거하고 R damage definition의 targetMaxHpRatio 0.03 true damage request를 정확히 1회 만든다.
- R 3개 파괴/timeout은 heal zone 0개, 4개 파괴는 고정 center·radius 6·5초 zone 1개와 0.5초 tick heal만 만든다.
- E 첫 BA는 +30 non-forced, 둘째 BA는 +30 forced crit, 셋째 BA는 보너스 없음이다.
- W 중 enemy stun/airborne은 적용되지 않고 caught flag를 세우며, ally/slow는 caught flag를 세우지 않는다.

성공 출력은 아래 한 줄로 고정한다.

~~~text
[SimLab][FioraVital] PASS: deterministic passive side, BA/Q/W proc, 3% max-HP true damage, four-mark heal zone, E two-hit crit, W hard-CC capture
~~~

### 2-31. 생성 산출물

다음 파일은 직접 patch하지 않는다.

~~~text
Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json
Data/LoL/SharedContract/DefinitionManifest.json
Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp
Shared/Schemas/Generated/cpp/*
Shared/Schemas/Generated/go/*
~~~

Zed handoff 뒤 canonical JSON과 Snapshot.fbs를 모두 반영한 상태에서 codegen 한 번으로 갱신한다. 기존 Fiora C++/header는 프로젝트에 이미 등록되어 있고 WFX는 Loader의 Data/LoL/FX 재귀 preload 대상이므로 vcxproj/filters 변경은 없다.

## 3. 검증

예측:

- Passive는 range 6 안의 enemy champion 1명에게 N/E/S/W 중 1개만 생기고, BA/Q/W가 표시된 side에서 실제 적중했을 때만 최대 HP 3% true damage와 함께 사라진다.
- R cast 직후 fiora_base_r_timeout_alphaslice 4개와 fiora_base_r_healzone_ring 1개만 대상에 붙는다. healzone은 아직 없어야 한다.
- R side를 하나씩 파괴하면 해당 visual만 즉시 사라진다. 네 번째에서 target의 그 tick 위치에 지름 12.1 healzone이 고정되고 5초 동안 같은 team champion만 0.5초마다 40 회복한다.
- R이 8초 안에 끝나지 않으면 mark/ring만 사라지고 healzone은 생기지 않는다.
- E cast는 Weapon bone을 따라 blue history trail을 만들고, 첫 E BA는 +30/spark, 둘째는 +30/ForceCrit/spark와 DamageEvent.bWasCrit=true, 셋째 BA는 정상이다.
- W는 blue indicator로 시작하고 enemy Stun/Airborne attempt를 server가 잡으면 red+CC glow로 교체한다. caught hard CC는 적용되지 않고 release는 stun, 미포착 release는 slow다.
- Bot AI는 계속 GameCommand 생산자이며 vital/HP/CC/회복 truth를 직접 변경하지 않는다.
- 깨질 수 있는 것: generic crit RNG, Zed lethal preview, snapshot full refresh, rewind/reconnect, FX handle stale cleanup. 각각 SimLab, Zed probe, 2-client full snapshot, Chrono keyframe, client visual cleanup gate로 잡는다.

검증 명령:

~~~powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
Shared\Schemas\run_codegen.bat
Client/Bin/Debug/SimLab.exe --fiora-vital-only
git diff --check -- Shared/GameSim/Champions/Fiora Shared/GameSim/Components/FioraSimComponent.h Shared/GameSim/Definitions/DamageTypes.h Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp Shared/GameSim/Systems/Damage/DamagePipeline.cpp Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp Shared/Schemas/Snapshot.fbs Server/Private/Game/SnapshotBuilder.cpp Data/LoL/FX/Champions/Fiora Client/Private/GameObject/Champion/Fiora Client/Public/GameObject/Champion/Fiora Client/Private/Network/Client/SnapshotApplier.cpp Client/Private/Network/Client/EventApplier.cpp Client/Public/Network/Client/EventApplier.h
~~~

빌드는 사용자 디버깅 종료 뒤 다음 순서로만 실행한다.

~~~powershell
msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
~~~

미검증:

- 이번 계획 세션에서는 사용자가 Client/Server를 디버깅 중이므로 codegen, build, SimLab, exe 실행을 하지 않는다.
- Weapon ribbon 폭/본 위치, vital 높이, ring alpha, 지름 12.1 카메라 투영은 인게임 눈검증 전까지 미검증이다.
- passive 3%와 heal 40/0.5초는 제품 밸런스 확정 전 기본안이다.

확인 필요:

- Zed handoff 후 EventApplier/SnapshotApplier exact reconciliation body와 Tools/SimLab/main.cpp 최신 anchor를 다시 읽는다.
- R 네 번째 proc과 target 사망이 같은 tick일 때 healzone 생성 여부를 SimLab로 고정한다. 기본 판정은 네 번째 proc이 먼저면 생성이다.
- W가 모든 incoming damage와 non-hard CC까지 막아야 한다면 이전 계획의 full payload disposition을 별도 packet으로 이어간다. 이번 packet은 hard CC intercept만 확정한다.

## 4. 충돌 없는 병렬 작업과 handoff

### Packet A - 지금 병렬 가능

~~~text
ID: 2026-07-17_fiora_exclusive_vital_fx
상태: Reserved
Agent: 다음 Fiora 구현 세션
Owner: Desktop
Owned:
  Shared/GameSim/Components/FioraSimComponent.h
  Shared/GameSim/Champions/Fiora/**
  Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp
  Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp
  Client/Public/GameObject/Champion/Fiora/**
  Client/Private/GameObject/Champion/Fiora/**
  Data/LoL/FX/Champions/Fiora/**
Read-only:
  Shared/GameSim/Systems/Combat/CombatActionSystem.cpp
  Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp
  Shared/GameSim/Systems/Damage/DamagePipeline.*
  Client/Private/Network/Client/EventApplier.cpp
  Data/LoL/ServerPrivate/Gameplay/*.json
  Tools/SimLab/main.cpp
Validation: diff check만, build/exe 금지
~~~

### Packet B - Zed handoff 뒤 단일 직렬 병합

~~~text
ID: 2026-07-17_fiora_shared_merge_window
선행: 2026-07-17_ZED_PASSIVE_E_R_AUTHORITATIVE_FX_PLAN handoff
Owned:
  Shared/GameSim/Definitions/DamageTypes.h
  Shared/GameSim/Systems/Damage/DamagePipeline.cpp
  Data/Gameplay/ChampionGameData/champions.json
  Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json
  generated gameplay pack/manifest
  Shared/Schemas/Snapshot.fbs + generated schema
  Server/Private/Game/SnapshotBuilder.cpp
  Client/Public/Network/Client/EventApplier.h
  Client/Private/Network/Client/EventApplier.cpp
  Client/Private/Network/Client/SnapshotApplier.cpp
  Tools/SimLab/main.cpp
Handoff:
  Zed diff를 먼저 보존
  최신 anchor 재확인
  Fiora block 적용
  codegen 1회
  SimLab/빌드는 사용자 디버깅 종료 뒤
~~~

CombatActionSystem.cpp와 CommandExecutor.cpp는 Fiora 계획에서 수정하지 않는다. BA의 실제 성공 후처리를 DamageQueueSystem에 둬 Zed passive BA anchor와 직접 충돌하지 않게 한다.

30% ceiling 배분:

- 바닥 70%: server truth, codegen, snapshot/replay, SimLab, build.
- 천장 30%: 동일 대상에서 Passive N/E/S/W, R 4→heal 전환, E 1타/2타, W blue→red를 한 번에 보여주는 30초 캡처.
- 외부 마감 제안: 2026-07-19까지 캡처와 한 줄 SimLab 결과를 RESULT 문서로 환전한다.
