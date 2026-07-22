Session - 2026-07-18 챔피언 AI 공격성·미드 합류·요네 복귀 후 R·미니언 정체 개선
좌표: 게임플레이 코어 · 축: Shared/GameSim AI 결정 + Server 미니언 이동 + 데이터/검증
관련: `.md/plan/2026-07-18_CHAMPION_AI_COMBAT_FARM_YONE_SOUL_UNBOUND_RESULT.md`, `.md/plan/2026-07-18_CHAMPION_AI_LIVE_SAFE_TURRET_RECALL_ANCHOR_RESULT.md`

## 1. 결정 요약

- 문제: 요네·피오라·사일러스가 애쉬·애니보다 소극적이고, 앵커 이동 중 국지전을 무시하며, 요네가 E 콤보 복귀 후 R로 이어지지 않는다.
- 근거: 교전식은 근접 챔피언의 준비된 스킬/대시가 아니라 BA 사거리만 쓰고, `PlayerLike`는 같은 체력 위험을 식과 하드 게이트에서 중복 차감한다.
- 근거: `MoveToOuterTurret`/`WaitForWave`는 아군 웨이브가 없으면 관측한 적 챔피언을 무시하고 앵커 Move를 계속 낸다.
- 근거: 현 미드 집결은 아군 포탑 손실 방어만 다루며, 적 미드 외곽 파괴+적 다수 집결+아군 열세의 공세 합류 계약이 없다.
- 근거: 미니언 soft depenetration은 전진 여부와 무관하게 적용되어 정상 전진을 역방향 보정으로 상쇄할 수 있다.
- 대안: `aggression`만 일괄 상향하는 안은 거리·상태 게이트·이동 정체를 숨기므로 기각한다.
- 선택: 준비된 공격 스킬 사거리, 단일 utility 위험 평가, 국지전 선점, 수적 열세 미드 합류, 전진 보존 분리 보정을 각각 최소 수정한다.
- 경계: AI는 계속 `GameCommand`만 생산하고 Damage/스킬 결과, 클라이언트 시각, 봇 외 플레이어 명령은 바꾸지 않는다.
- 성공: 같은 조건에서 근접 3종이 fight를 선택하고, 앵커 이동 중 관측 적을 즉시 처리하며, 요네가 `E-Q-W-BA-BA-BA-E2-R`을 완료한다.
- 성공: 적 미드 외곽 파괴 후 관측 적 2명 이상·미드 아군 열세이면 side-lane 봇이 미드 웨이브 앵커로 합류한다.
- 성공: 미니언 분리 보정은 현재 lane goal 거리를 악화시키지 않고, 기존 결정론/빌드/SimLab/데이터 계약이 통과한다.
- 천장 30%: 사용자 5v5 재현과 decision trace 해석을 이번 slice의 외부 데모 환전물로 두고, 나머지는 회귀 자동화에 쓴다.
- 비평 게이트: `PASS` — 독립 sub-agent 읽기 전용 비평을 반영했으며 disposition은 §2.15에 기록했다.

## 2. 구현 코드

### 2.1 현재 결정식과 확정 원인

현재 `ChampionAIValuation.cpp`의 점수는 다음이다.

```text
fightRaw = 0.45
         + 0.30 * (selfHp - retreatHp)
         + 0.35 * clamp(selfHp - enemyHp, -1, 1)
         + (enemyDistance <= basicAttackRange ? +0.20 : -0.10)
         + (alliedWave ? +0.10 : 0)
         + 0.10 * economyLead
         - 0.50 * turretDanger * turretRiskWeight
         - 0.45 * incomingComboDamageRatio
fight = clamp(clamp(fightRaw, 0, 1) * aggression, 0, 1)

farm = clamp((enemyMinionInRange ? 0.55 : 0.15)
             + (selfHp <= retreatHp + 0.10 ? 0.15 : 0), 0, 1)
       * minionPressureWeight * lastHitWeight

retreat = clamp(0.75 * healthPressure
                + 0.75 * turretDanger * turretRiskWeight
                + 0.65 * incomingComboDamageRatio, 0, 1)
```

고난도 `PlayerLike`는 위 식 이후에도 `selfHp + 0.12 >= enemyHp`를 다시 요구한다. 예를 들어 같은 레벨·경제·웨이브에서 거리 3, 위협비 0.20인 full-HP 요네는 BA 거리 감점 때문에 fight 약 0.59, farm+margin 약 0.60으로 밀리지만, 같은 거리의 애니는 사거리 가점으로 약 0.70이라 싸운다. 체력이 13% 이상 뒤지면 점수와 무관하게 `PlayerLike` 하드 게이트도 닫힌다.

### 2.2 `Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp`

기존 `CRuleBasedChampionBrain`의 Farm hold 조건:

```cpp
                    (ai.intent == eChampionAIIntent::Retreat &&
                        input.fRetreatScore >= 0.35f) ||
                    ai.intent == eChampionAIIntent::FarmMinion)
                    return ai.intent;
```

아래로 교체한다. 관측 가능한 교전/공성 후보가 생기면 Farm hold를 즉시 재평가한다.

```cpp
                    (ai.intent == eChampionAIIntent::Retreat &&
                        input.fRetreatScore >= 0.35f) ||
                    (ai.intent == eChampionAIIntent::FarmMinion &&
                        !input.bCanAttackChampion &&
                        !input.bCanAttackStructure))
                    return ai.intent;
```

기존 `CPlayerLikeChampionBrain`의 12% 체력 게이트 블록:

```cpp
            const bool_t bHpAdvantage =
                ai.fDecisionSelfHpRatio + kHpDisadvantageTolerance >=
                    ai.fDecisionEnemyHpRatio;
            if (input.bCanAttackChampion && bHpAdvantage &&
                input.fChampionScore >=
                    input.fFarmScore + ai.fChampionScoreMargin &&
                input.fChampionScore >= input.fStructureScore)
            {
                return eChampionAIIntent::AttackChampion;
            }
```

아래로 교체한다. 일반적인 12~30% 열세는 수치 utility가 판단하되, retreat 식이 0이 될 수 있는 `self >= reengageHp` 구간의 극단적 30% 초과 열세는 별도 안전 게이트로 남긴다.

```cpp
            const bool_t bWithinSevereHpDisadvantageLimit =
                ai.fDecisionSelfHpRatio + kSevereHpDisadvantageTolerance >=
                    ai.fDecisionEnemyHpRatio;
            if (input.bCanAttackChampion &&
                bWithinSevereHpDisadvantageLimit &&
                input.fChampionScore >=
                    input.fFarmScore + ai.fChampionScoreMargin &&
                input.fChampionScore >= input.fStructureScore)
            {
                return eChampionAIIntent::AttackChampion;
            }
```

기존 상수는 아래로 교체한다.

```cpp
        static constexpr f32_t kSevereHpDisadvantageTolerance = 0.30f;
```

retreat score `>= 0.65`, 저체력 `CanAttackChampion` 금지, 포탑 위험 금지는 유지한다. SimLab는 20% 열세 진입 허용과 31% 초과 열세 차단을 함께 고정한다.

### 2.3 `Shared/GameSim/Systems/ChampionAI/ChampionAIPerception.h`

`attackRange` 아래에 의사결정용 준비 사거리를 추가한다.

```cpp
    f32_t attackRange = 1.5f;
    f32_t engageRange = 1.5f;
```

`midDefenseAnchor` 주변에 공세 집결 관측을 추가한다. 이 구조체는 tick-local perception이며 checkpoint ABI가 아니다.

```cpp
    Vec3 midDefenseAnchor{};
    Vec3 midTeamfightAnchor{};
    u8_t alliedMidChampionCount = 0u;
    u8_t enemyMidChampionCount = 0u;
```

bool tail에는 아래를 추가한다.

```cpp
    bool_t bEnemyMidOuterTurretLost = false;
```

### 2.4 `Shared/GameSim/Components/ChampionAIComponent.h`

공세 집결 latch를 새 바이트로 늘리지 않고 기존 정렬 예약 바이트를 사용한다.

기존:

```cpp
    f32_t midDefenseThreatHoldTimer = 0.f;
    u8_t reservedMidDefenseAlignment[4]{};
```

교체:

```cpp
    f32_t midDefenseThreatHoldTimer = 0.f;
    bool_t bMidTeamfightActive = false;
    bool_t bYonePostReturnUltimatePending = false;
    u8_t reservedMidDefenseAlignment[2]{};
```

`sizeof(ChampionAIComponent) == 2936u`와 기존 offset 단언은 유지되어야 한다.

### 2.5 `Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h/.cpp`

`ValueInput`의 `attackRange` 아래에 아래를 추가한다.

```cpp
        f32_t engageRange = 1.5f;
```

`ChampionFightValue`의 거리 항만 아래처럼 교체한다.

```cpp
        score += (in.enemyDistance <= in.engageRange) ? 0.20f : -0.10f;
```

`TradeWindow`는 실제 즉시 평타 교환 창을 뜻하므로 `attackRange`를 유지한다. 위협 계수 0.45/0.65도 이번 원인의 중복 하드 게이트 제거 후 다시 측정하기 위해 변경하지 않는다.

추적 항을 점수 코드와 따로 복제하지 않도록 header에 아래 POD를 추가한다.

```cpp
    struct UtilityTerm
    {
        f32_t rawValue = 0.f;
        f32_t weight = 0.f;
        f32_t contribution = 0.f;
    };

    struct CandidateBreakdown
    {
        f32_t score = 0.f;
        UtilityTerm terms[4]{};
        u8_t termCount = 0u;
    };

    struct UtilityBreakdown
    {
        CandidateBreakdown retreat{};
        CandidateBreakdown fight{};
        CandidateBreakdown farm{};
        CandidateBreakdown siege{};
    };

    UtilityBreakdown BuildUtilityBreakdown(const ValueInput& in);
```

`BuildUtilityBreakdown`은 각 candidate의 최종 score와 합이 정확히 일치하도록 아래 네 범주를 계산한다.

```text
fight: positive opportunity / turret risk / observed combo risk / clamp adjustment
retreat: health pressure / turret risk / observed combo risk / threshold-or-clamp adjustment
farm: minion opportunity including low-HP bias / clamp adjustment
siege: exposed-structure opportunity / clamp adjustment
```

`BuildUtilityScores`는 별도 식을 다시 쓰지 않고 `BuildUtilityBreakdown(in)`의 네 score를 반환한다.

### 2.6 `Shared/GameSim/Systems/ChampionAI/ChampionAIResearchTypes.h`

wire 크기와 V1 버전은 유지하고 `AiFeatureIdV1`만 additive하게 확장한다.

```cpp
enum class AiFeatureIdV1 : std::uint16_t
{
    None = 0u,
    UtilityScore = 1u,
    PositiveOpportunity = 2u,
    TurretRisk = 3u,
    ObservedComboRisk = 4u,
    ClampOrThresholdAdjustment = 5u,
    HealthPressure = 6u,
    FarmOpportunity = 7u,
    StructureExposure = 8u,
};
```

`AiDecisionTraceV1 == 528u`, contribution capacity 4는 유지한다. exporter는 이미 `feature_id/raw_value/weight/contribution`을 일반 숫자로 내보내므로 Python wire layout 변경은 없다.

### 2.7 `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp` — engage range

모든 ready QWER의 최대값은 사용하지 않는다. 그것은 요네의 복귀 후 R, Sylas Hijack, self buff, stage 2를 현재 진입기로 오판한다. `BuildChampionAIContext` 위에 “현재 combo가 실제로 처음 실행할 수 있는 hostile entry step”만 해석하는 helper를 추가한다.

```cpp
    bool_t PassesComboEntryHealthGate(
        const ChampionAIComboStep& step,
        const ChampionAIContext& ctx)
    {
        return !(step.selfHpMinRatio > 0.f &&
                ctx.selfHpRatio + 0.001f < step.selfHpMinRatio) &&
            !(step.selfHpMaxRatio < 0.999f &&
                ctx.selfHpRatio > step.selfHpMaxRatio) &&
            !(step.enemyHpMaxRatio < 0.999f &&
                ctx.enemyHpRatio > step.enemyHpMaxRatio);
    }

    bool_t IsHostileComboEntryTargetMode(
        const ChampionAIComboStep& step)
    {
        return step.targetMode == static_cast<u8_t>(
                eChampionAIComboTargetMode::TargetEntity) ||
            step.targetMode == static_cast<u8_t>(
                eChampionAIComboTargetMode::SylasHijackTarget) ||
            step.targetMode == static_cast<u8_t>(
                eChampionAIComboTargetMode::SylasStolenUltimateTarget);
    }

    f32_t ResolveChampionAIEngageRange(
        CWorld& world,
        EntityID self,
        const TickContext& tc,
        eChampion champion,
        const ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
    {
        const ChampionAIComboPlan& combo = GetChampionAIComboPlan(champion);
        const u8_t count = std::min(combo.stepCount, static_cast<u8_t>(10u));
        for (u8_t i = 0u; i < count; ++i)
        {
            const ChampionAIComboStep& step = combo.steps[i];
            if (IsComboStepUnlearned(world, self, step) ||
                !PassesComboEntryHealthGate(step, ctx))
            {
                continue;
            }

            if (step.itemId == 2u)
                continue;

            if (!IsHostileComboEntryTargetMode(step))
            {
                if (step.slot != static_cast<u8_t>(eSkillSlot::BasicAttack) &&
                    (!IsSkillReady(world, self, step.slot) ||
                        !HasSufficientSkillResource(
                            world, tc, self, champion, step.slot)))
                {
                    return ctx.attackRange;
                }
                continue;
            }

            if (step.slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
                return ctx.attackRange;
            if (!IsSkillReady(world, self, step.slot) ||
                !HasSufficientSkillResource(
                    world, tc, self, champion, step.slot))
            {
                return ctx.attackRange;
            }

            f32_t range = step.maxRange;
            if (range <= 0.f)
            {
                range = GameplayDefinitionQuery::ResolveSkillRange(
                    world, self, tc, champion, step.slot);
            }
            return std::min(
                std::max(ctx.attackRange, range),
                ai.championScanRange);
        }

        return ctx.attackRange;
    }
```

`BuildChampionAIContext`의 `ctx.attackRange` 대입 직후에는 기본값만 둔다.

```cpp
        ctx.engageRange = ctx.attackRange;
```

target의 거리/체력/관측 위협을 채운 직후 아래를 추가한다.

```cpp
        ctx.engageRange = ResolveChampionAIEngageRange(
            world,
            self,
            tc,
            champion.id,
            ai,
            ctx);
```

이 resolver는 Fiora R의 `enemyHpMaxRatio`가 실패하면 다음 Q를 보고, Sylas는 현재 첫 Q, Yone은 현재 첫 E만 본다. 앞선 applicable self step이 cooldown이면 실제 combo도 기다리므로 BA range로 되돌린다. 후속 R/stage2/away/ward step은 진입 사거리를 부풀리지 않는다.

`UpdateChampionAIDecisionEvidence`에서는 다음을 적용한다.

```cpp
        ai.fDecisionAttackRange = ctx.engageRange;
        // ...
        vin.attackRange = ctx.attackRange;
        vin.engageRange = ctx.engageRange;
```

### 2.8 `ChampionAISystem.cpp` — 국지전 선점

decision cadence early return 전에 아래를 계산한다.

```cpp
            const bool_t bAnchorStateLocalCombatInterrupt =
                ctx.enemyChampion != NULL_ENTITY &&
                (ai.state == eChampionAIState::MoveToOuterTurret ||
                    ai.state == eChampionAIState::WaitForWave ||
                    ai.state == eChampionAIState::GroupMidDefense);
```

기존 cadence gate:

```cpp
            if (ai.decisionTimer > 0.f &&
                !bHasDebugOverride &&
                !bEmergencyInterrupt)
```

아래로 교체한다.

```cpp
            if (ai.decisionTimer > 0.f &&
                !bHasDebugOverride &&
                !bEmergencyInterrupt &&
                !bAnchorStateLocalCombatInterrupt)
```

`ExecuteMoveToOuterTurret`와 `ExecuteWaitForWave`의 첫 줄에 같은 우선순위를 추가한다.

```cpp
        if (ctx.enemyChampion != NULL_ENTITY)
        {
            ExecuteLaneCombat(world, tc, self, ai, champion, selfPos, ctx, outCommands);
            return;
        }
```

`ExecuteGroupMidDefense`에서 기존 `bEnemyChampionInAttackRange || enemyMinion` 분기는 아래로 교체한다. 이동 중 side-lane 미니언에는 다시 갇히지 않지만, 관측 적 챔피언은 스킬 engage range까지 포함해 처리한다.

```cpp
        if (ctx.enemyChampion != NULL_ENTITY)
        {
            ExecuteLaneCombat(
                world, tc, self, ai, champion, selfPos, ctx, outCommands);
            return;
        }
```

### 2.9 `ChampionAISystem.cpp` — 공세적 미드 합류

상수는 기존 mid-defense 상수 옆에 추가한다.

```cpp
    constexpr f32_t kChampionAIMidTeamfightRadius = 18.f;
    constexpr f32_t kChampionAIMidWaveAnchorRadius = 14.f;
    constexpr u8_t kChampionAIMidTeamfightEnemyMinimum = 2u;
```

`HasMidLaneTurretLost` 아래에 다음 read-only snapshot helper를 추가한다.

```cpp
    struct MidTeamfightSnapshot
    {
        Vec3 anchor{};
        u8_t alliedCount = 0u;
        u8_t enemyCount = 0u;
        bool_t bEnemyMidOuterLost = false;
    };

    MidTeamfightSnapshot BuildMidTeamfightSnapshot(
        CWorld& world,
        EntityID self,
        eTeam team,
        const Vec3& fallback)
    {
        MidTeamfightSnapshot result{};
        result.anchor = fallback;
        const eTeam enemyTeam = EnemyTeam(team);
        bool_t bFoundEnemyOuter = false;

        world.ForEach<StructureComponent, TransformComponent>(
            [&](EntityID entity, StructureComponent& structure, TransformComponent& transform)
            {
                if (structure.team != enemyTeam ||
                    structure.kind != static_cast<u32_t>(
                        Winters::Map::eObjectKind::Structure_Turret) ||
                    structure.tier != static_cast<u32_t>(
                        Winters::Map::eTurretTier::Outer) ||
                    structure.lane != kChampionAIMidLane)
                {
                    return;
                }

                bFoundEnemyOuter = true;
                result.anchor = transform.GetPosition();
                result.bEnemyMidOuterLost = !IsAliveTarget(world, entity);
            });

        if (!bFoundEnemyOuter || !result.bEnemyMidOuterLost)
            return result;

        EntityID bestWave = NULL_ENTITY;
        f32_t bestWaveSq =
            kChampionAIMidWaveAnchorRadius * kChampionAIMidWaveAnchorRadius;
        world.ForEach<MinionComponent, TransformComponent>(
            [&](EntityID entity, MinionComponent& minion, TransformComponent& transform)
            {
                if (minion.team != team ||
                    minion.laneType != kChampionAIMidLane ||
                    !IsAliveTarget(world, entity))
                {
                    return;
                }
                const f32_t distSq = WintersMath::DistanceSqXZ(
                    result.anchor, transform.GetPosition());
                if (distSq < bestWaveSq ||
                    (distSq == bestWaveSq &&
                        (bestWave == NULL_ENTITY || entity < bestWave)))
                {
                    bestWave = entity;
                    bestWaveSq = distSq;
                }
            });
        if (bestWave != NULL_ENTITY)
            result.anchor = world.GetComponent<TransformComponent>(bestWave).GetPosition();

        const f32_t radiusSq =
            kChampionAIMidTeamfightRadius * kChampionAIMidTeamfightRadius;
        world.ForEach<ChampionComponent, TransformComponent>(
            [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
            {
                if (world.HasComponent<PracticeDummyTag>(entity) ||
                    !IsAliveTarget(world, entity) ||
                    WintersMath::DistanceSqXZ(result.anchor, transform.GetPosition()) > radiusSq)
                {
                    return;
                }
                if (champion.team == team)
                    ++result.alliedCount;
                else if (champion.team == enemyTeam &&
                    CanChampionAIObserveTarget(world, self, team, entity))
                    ++result.enemyCount;
            });
        return result;
    }
```

실제 구현에서는 count overflow를 막기 위해 `u8_t max`에서 포화 증가한다.

`BuildChampionAIContext`의 decision-cadence macro refresh 안에서만 snapshot을 채운다. `enemyChampion`이 앵커 상태 cadence를 우회한 tick에는 macro latch를 갱신하지 않고 곧바로 국지전 executor로 내려가 stale/default count를 소비하지 않는다.

```cpp
            const MidTeamfightSnapshot midTeamfight =
                BuildMidTeamfightSnapshot(
                    world, self, champion.team, ctx.midDefenseAnchor);
            ctx.midTeamfightAnchor = midTeamfight.anchor;
            ctx.alliedMidChampionCount = midTeamfight.alliedCount;
            ctx.enemyMidChampionCount = midTeamfight.enemyCount;
            ctx.bEnemyMidOuterTurretLost =
                midTeamfight.bEnemyMidOuterLost;
            if (ai.bMidTeamfightActive)
                ctx.midDefenseAnchor = ctx.midTeamfightAnchor;
```

활성 조건은 기존 방어 조건과 아래 공세 조건의 OR로 교체한다.

```cpp
            const bool_t bOffensiveMidGroupNeeded =
                ctx.bEnemyMidOuterTurretLost &&
                ctx.enemyMidChampionCount >=
                    kChampionAIMidTeamfightEnemyMinimum &&
                ctx.alliedMidChampionCount < ctx.enemyMidChampionCount;
            const bool_t bDefensiveMidGroupNeeded =
                (ctx.bAlliedOuterTurretLost || ctx.bMidLaneTurretLost) &&
                HasMidDefenseThreat(
                    world, champion.team, ctx.midDefenseAnchor);
```

공통 안전 조건은 `ctx.bCanMove`, `ctx.enemyChampion == NULL_ENTITY`, combo/dive 없음이다. 활성 시:

```cpp
                ai.bMidDefenseActive = true;
                ai.bMidTeamfightActive = bOffensiveMidGroupNeeded;
                ai.midDefenseThreatHoldTimer = kChampionAIMidDefenseThreatHoldSec;
                ai.activeLane = kChampionAIMidLane;
                ai.midDefenseAnchor = ai.bMidTeamfightActive
                    ? ctx.midTeamfightAnchor
                    : ctx.midDefenseAnchor;
```

active latch 갱신/해제 블록 전체를 `bInfluenceRefreshDue`로 감싼다. 공세 모드의 hold refresh는 최초 활성과 같은 전체 조건 `enemy outer lost && enemy>=2 && ally<enemy`일 때만 수행하고, 방어 모드는 기존 `HasMidDefenseThreat`를 사용한다. 동수가 되면 hold를 새로 채우지 않되 기존 6초 commit은 보존한다. 해제 시 `bMidTeamfightActive=false`도 함께 초기화한다.

### 2.10 `ChampionAISystem.cpp` — 요네 E2 복귀 후 R

현재 Execute의 아래 블록은 복귀가 시작되는 즉시 combo를 지운다.

```cpp
            if (champion.id == eChampion::YONE &&
                world.HasComponent<YoneSimComponent>(self) &&
                world.GetComponent<YoneSimComponent>(self).bReturning &&
                ai.comboTarget != NULL_ENTITY)
            {
                ClearChampionAICombo(ai);
            }
```

복귀 원인을 구분하지 않고 `bReturning`만 보는 방식은 기각한다. `TryEmitYoneSoulReturnCommand`에서 아래 세 원인을 분리한다.

```cpp
        const bool_t bRetreatUtilityDominates =
            ctx.retreatScore >= ctx.fightScore;
        const bool_t bTimerReturn = yone.fSoulTimer <= 0.75f;
        const bool_t bTargetLost = ctx.enemyChampion == NULL_ENTITY;
        const bool_t bSafetyReturn =
            bRetreatUtilityDominates || bTargetLost;
        const bool_t bShouldReturn = bTimerReturn || bSafetyReturn;
```

현재 combo step이 E2이고 `bTimerReturn && !bSafetyReturn`일 때만 다음 R을 예약한다. 포탑/체력/타깃 소실로 안전 복귀하면 예약하지 않는다.

```cpp
        ai.bYonePostReturnUltimatePending =
            bTimerReturn &&
            !bSafetyReturn &&
            IsCurrentYoneComboReturnStep(ai);
```

Execute의 기존 clear 블록은 아래 의미로 교체한다.

```cpp
            if (champion.id == eChampion::YONE &&
                world.HasComponent<YoneSimComponent>(self) &&
                world.GetComponent<YoneSimComponent>(self).bReturning &&
                ai.comboTarget != NULL_ENTITY)
            {
                const ChampionAIComboPlan& combo =
                    GetChampionAIComboPlan(eChampion::YONE);
                const u8_t stepCount =
                    std::min(combo.stepCount, static_cast<u8_t>(10u));
                const u8_t stepIndex = stepCount > 0u
                    ? static_cast<u8_t>(ai.comboStep % stepCount)
                    : 0u;
                const ChampionAIComboStep& step = combo.steps[stepIndex];
                const bool_t bPlannedReturnStep =
                    stepCount > 0u &&
                    step.slot == static_cast<u8_t>(eSkillSlot::E) &&
                    step.itemId == 2u;
                const bool_t bPostReturnUltimateStep =
                    stepCount > 0u &&
                    step.slot == static_cast<u8_t>(eSkillSlot::R);
                if (ai.bYonePostReturnUltimatePending &&
                    bPlannedReturnStep &&
                    stepIndex + 1u < stepCount)
                {
                    ai.comboStep = static_cast<u8_t>(stepIndex + 1u);
                }
                else if (ai.bYonePostReturnUltimatePending &&
                    bPostReturnUltimateStep)
                {
                    // forced return이 끝날 때까지 R step을 유지한다.
                }
                else
                {
                    ClearChampionAICombo(ai);
                }
            }
```

`ClearChampionAICombo`는 `bYonePostReturnUltimatePending=false`도 함께 수행한다. `bReturning`이 여러 AI tick 유지되므로 R step은 유지하지만, R 명령은 귀환 forced motion/action lock이 끝난 뒤 기존 `TickActiveChampionCombo`를 통해서만 제출한다. command reject 뒤에는 다음 tick의 원인 판정이 예약 값을 다시 계산하므로 일회성 잘못된 예약이 잔류하지 않는다.

### 2.11 `Data/LoL/ServerPrivate/AI/ChampionAIGameplayDefs.json`

다음 세 profile의 `championScanRange`만 변경한다. aggression/retreat 수치는 먼저 구조 오류를 제거한 뒤 측정하기 위해 유지한다.

```json
"FIORA": 6.0 -> 9.0
"SYLAS": 7.0 -> 9.0
"YONE": 7.0 -> 9.0
```

YONE combo plan 전체를 아래로 교체한다.

```json
{
  "champion": "YONE",
  "steps": [
    { "slot": "E", "itemId": 0, "minRange": 0.0, "maxRange": 4.0, "selfHpMinRatio": 0.42, "enemyHpMaxRatio": 1.0, "targetMode": "TargetEntity" },
    { "slot": "Q", "itemId": 0, "minRange": 0.0, "maxRange": 4.75, "selfHpMinRatio": 0.42, "enemyHpMaxRatio": 1.0, "targetMode": "TargetEntity" },
    { "slot": "W", "itemId": 0, "minRange": 0.0, "maxRange": 6.0, "selfHpMinRatio": 0.42, "enemyHpMaxRatio": 1.0, "targetMode": "TargetEntity" },
    { "slot": "BasicAttack", "itemId": 0, "minRange": 0.0, "maxRange": 1.75, "selfHpMinRatio": 0.42, "enemyHpMaxRatio": 1.0, "targetMode": "TargetEntity" },
    { "slot": "BasicAttack", "itemId": 0, "minRange": 0.0, "maxRange": 1.75, "selfHpMinRatio": 0.42, "enemyHpMaxRatio": 1.0, "targetMode": "TargetEntity" },
    { "slot": "BasicAttack", "itemId": 0, "minRange": 0.0, "maxRange": 1.75, "selfHpMinRatio": 0.42, "enemyHpMaxRatio": 1.0, "targetMode": "TargetEntity" },
    { "slot": "E", "itemId": 2, "minRange": 0.0, "maxRange": 0.0, "selfHpMinRatio": 0.0, "enemyHpMaxRatio": 1.0, "targetMode": "Self" },
    { "slot": "R", "itemId": 0, "minRange": 0.0, "maxRange": 10.0, "selfHpMinRatio": 0.42, "enemyHpMaxRatio": 1.0, "targetMode": "TargetEntity" }
  ]
}
```

생성물 `Shared/GameSim/Generated/ChampionAIPolicyData.generated.inl`은 직접 편집하지 않고 generator로 갱신한다.

### 2.12 미니언 soft separation 전진 보존

단순히 “goal까지 거리가 늘면 depenetration을 거부”하는 안은 겹친 미니언을 그대로 남겨 stuck을 악화시키므로 기각한다. soft-minion-only push의 역방향 성분만 제거하고, push가 사라지면 entity ID로 결정적인 좌/우 측면과 작은 전진 편향을 만들어 **분리와 전진을 동시에** 보존한다. 구조물/중립 유닛 충돌 탈출은 기존 동작을 유지한다.

새 파일: `Shared/GameSim/Systems/Move/MinionSoftSeparationPolicy.h`

```cpp
#pragma once

#include "WintersMath.h"

#include <cstdint>

namespace MinionSoftSeparationPolicy
{
    inline Vec3 ResolveForwardSafeDirection(
        const Vec3& rawPush,
        const Vec3& preferredForward,
        std::uint32_t entityTieBreaker)
    {
        const Vec3 forward = WintersMath::NormalizeXZ(
            preferredForward,
            Vec3{ 0.f, 0.f, 1.f });
        Vec3 corrected{ rawPush.x, 0.f, rawPush.z };
        const f32_t projection =
            corrected.x * forward.x + corrected.z * forward.z;
        if (projection < 0.f)
        {
            corrected.x -= forward.x * projection;
            corrected.z -= forward.z * projection;
        }

        if (WintersMath::LengthSqXZ(corrected) <= 0.000001f)
        {
            const f32_t sideSign =
                (entityTieBreaker & 1u) != 0u ? 1.f : -1.f;
            corrected = Vec3{
                -forward.z * sideSign + forward.x * 0.25f,
                0.f,
                forward.x * sideSign + forward.z * 0.25f };
        }

        return WintersMath::NormalizeXZ(corrected, forward);
    }
}
```

`Server/Public/Game/GameRoom.h`의 `TryResolveMinionDepenetrationStep` 선언과 `Server/Private/Game/GameRoomUnitAI.cpp` 정의에 `const Vec3& vPreferredForward`를 추가한다. `softMinionCount > 0 && staticCount == 0 && dynamicCount == 0`인 경우에만 `vPush`를 위 policy로 교정하고, 기존 `pushStep`만큼 적용한다.

각 호출부는 다음 실제 진행 방향을 넘긴다.

```text
Phase_ServerMinionDepenetration: active path waypoint, chase target, current lane waypoint 순으로 선택
TryMoveServerMinionToward: vMoveGoal - vPos
TryMoveServerMinionByFlowFields: 이미 계산된 vDir
```

phase 후처리는 soft separation을 정상 물리 분리로 간주하되 `BlockedMoveFrames`를 무조건 0으로 초기화하지 않는다. 이로써 static blocker에서 path fallback을 요구하는 신호가 사라지지 않는다. bounded trace에는 raw push, corrected direction, forward dot, blocker 분류를 남긴다.

### 2.13 `ChampionAISystem.cpp` — 수학 trace 연결

`BuildResearchDecisionTrace`는 `ChampionAIValuation::BuildUtilityBreakdown`을 같은 perception/profile로 다시 구성하고, 기존 `UtilityScore` 한 칸 대신 candidate별 최대 4개 항을 채운다. candidate.score는 authoritative `ai.f*DecisionScore`를 유지한다.

```cpp
        auto setContribution = [](
            AiCandidateEvidenceV1& candidate,
            u8_t index,
            AiFeatureIdV1 feature,
            const ChampionAIValuation::UtilityTerm& term)
        {
            AiFeatureContributionV1& out = candidate.contributions[index];
            out.featureId = static_cast<u16_t>(feature);
            out.rawValue = term.rawValue;
            out.weight = term.weight;
            out.contribution = term.contribution;
        };
```

Fight trace의 합은 아래와 같이 candidate.score와 일치해야 한다.

```text
PositiveOpportunity + TurretRisk + ObservedComboRisk + ClampAdjustment == fight score
```

SimLab는 모든 candidate에서 `abs(sum(contribution)-score) <= 0.001`을 단언한다. 이 trace는 selection 전 observation/mask/score와 command 제출·executor 결과를 이어 보므로, “왜 Farm/Retreat/Fight가 선택됐는지”를 replay 가능한 숫자로 남긴다.

### 2.14 `Tools/SimLab/main.cpp`

기존 `RunChampionAIMidDefenseDeterminismProbe`에 다음 fixture를 추가한다.

```text
1. 적 Mid Outer dead, 적 챔피언 3명 mid outer 반경, 아군 1명, side-lane bot 2명.
2. 첫 bot 합류 후에도 2<3이므로 둘째 bot도 GroupMidDefense로 전환.
3. 두 봇의 home lane은 유지, active lane은 Mid, 첫 Move target은 allied mid wave 또는 dead outer anchor.
4. 같은 seed 2회 hash 동일.
5. 앵커 이동 중 관측 적 챔피언을 두면 Move가 아니라 LaneCombat/AttackChampion.
6. side-lane enemy minion만 두면 미드 Move를 계속하여 wave-anchor trap이 재발하지 않음.
```

새 `RunChampionAIAggressionTraceProbe`를 추가한다.

```text
- Yone/Fiora/Sylas: 거리 3~5, 준비된 gap-close/공격 스킬, 20% 관측 콤보 위협에서 fight > farm+margin.
- selfHp=0.80/enemyHp=1.00이라도 retreat<0.65이고 fight가 이기면 PlayerLike가 AttackChampion.
- selfHp+0.30<enemyHp인 심각한 체력 열세에서는 score와 무관하게 PlayerLike 진입을 차단.
- 실제 selfHp<=retreatHp 또는 포탑 danger이면 여전히 Retreat.
- Fight/Farm/Retreat/Siege trace contribution 합 == candidate.score.
- ChampionAIComponent size/trace wire size 불변.
```

`RunYoneEReturnProbe`의 data/accepted action 기대값을 아래로 교체한다.

```cpp
        constexpr u8_t kExpectedSlots[] =
        {
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::BasicAttack),
            static_cast<u8_t>(eSkillSlot::BasicAttack),
            static_cast<u8_t>(eSkillSlot::BasicAttack),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
        };
```

accepted executor sequence도 `E,Q,W,BA,BA,BA,E2,R`을 단언하고, `R.acceptedTick > return-complete tick`을 확인한다. 정상 timer return만 `bYonePostReturnUltimatePending`을 세우며, 별도 retreat 우세/타깃 소실 fixture는 조기 E2 후 combo가 취소되고 R이 나가지 않음을 단언한다.

새 `RunMinionSoftSeparationPolicyProbe`를 추가한다.

```text
- raw push가 진행 방향의 정반대여도 결과 dot(forward)>0.
- 같은 입력/entity ID는 byte-identical 결과.
- 짝/홀 entity ID는 서로 반대 측면을 선택하면서 둘 다 전진 성분을 유지.
- 이미 측면+전진인 raw push는 방향을 불필요하게 반전하지 않음.
```

### 2.15 생성/문서/비평 처리

- 생성기 입력은 AI JSON 하나이며, 생성된 `ChampionAIPolicyData.generated.inl` 및 manifest/hash 변화만 수용한다.
- `.md/process/GOAL_OPERATING_DOCTRINE.md`에 따라 이번 5v5 행동 개선은 천장 출력으로 분류하고, 구현 범위는 본 PLAN의 성공 기준을 넘기지 않는다.
- 독립 비평 결과는 이 문단 아래에 `채택/기각/이유`로 추가한다.

구현 중 SimLab accepted-action trace에서 추가로 확인된 피드백 비대칭을 함께 수정한다. `CommandExecutor.cpp::FinalizeChampionAICommandTrace`는 거절된 `CastSkill`만 `comboStep`을 제출 전 값으로 되돌리고, 거절된 `BasicAttack`은 이미 증가한 step을 그대로 두고 있었다. 따라서 데이터에 연속 BA가 있어도 실행 거절 1회가 다음 step/E2로 건너뛰게 했다. 거절 command kind와 무관하게 AI combo가 활성 상태면 제출 trace의 `comboStep`으로 복원하되, fresh-cast cooldown commit은 기존처럼 accepted stage-1 cast에만 적용한다.

기본 공격 속도 SimLab에서 고정 5초 E 창 안에 `E-Q-W-BA-BA-BA-E2` 전부가 들어가지 않는 것도 실측했다. 데이터의 3 BA 의도는 유지하되, 정상 timer return 시 현재 step부터 뒤의 `E2-R` tail을 찾아 남은 중간 BA만 포기하고 `E2 -> 귀환 완료 -> R`을 보존한다. target-lost/retreat 안전 복귀는 이 보정을 사용하지 않아 R을 예약하지 않는다. 런타임 fixture는 고공속 강제 대신 기본 공격 속도에서 `E-Q-W-BA-BA-E2-R`을 검증하고, 데이터 fixture는 3 BA authored sequence를 별도로 검증한다.

#### 독립 비평 disposition

- 채택 — 모든 ready QWER 중 최대 사거리를 쓰지 않고, 현재 combo의 **첫 legal hostile entry step**만 진입 사거리로 사용한다. 후속 Yone R/self/stage2가 교전 점수를 부풀릴 수 있다는 지적이 정확하다.
- 채택 — 12% HP gate의 완전 제거 대신 30% 심각 열세 gate를 유지한다. 일반 열세는 utility가 판단하되 무모한 진입 안전선은 남긴다.
- 채택 — local combat cadence bypass tick은 macro snapshot/latch를 갱신하지 않는다. default count가 활성 latch를 오염시킬 수 있기 때문이다.
- 채택 — 공세 mid hold refresh도 최초 활성과 같은 `enemy>=2 && ally<enemy` 전체 조건을 요구한다. 적 수만 확인하면 동수 이후에도 영구 latch될 수 있다.
- 채택 — Yone 복귀 원인을 timer/retreat/target-lost로 분리하고 정상 계획 복귀에만 post-return R을 예약한다. 안전 복귀 뒤 R 강제는 사용자 의도와 반대다.
- 채택 — 역행 depenetration 단순 거부 대신 forward-safe lateral policy와 deterministic probe를 둔다. 겹침을 방치하는 방식은 stuck을 악화시킨다.
- 기각 — 없음. 다만 실제 3-wave GameRoom 정체 해소는 자동 pure-policy probe만으로 완전 증명하지 않고 수동 `CONFIRM_NEEDED`로 남긴다.

## 3. 예상 결과와 검증

### 3.1 먼저 기록할 예상 관찰

1. 현행 baseline 수식에서 애니/애쉬는 BA 사거리 가점, 요네/사일러스는 같은 거리에서 -0.10 감점을 받아 위협비 약 0.20부터 farm에 밀린다.
2. 현행 `PlayerLike`는 체력 13% 열세에서 score와 무관하게 Fight를 거부한다.
3. 현행 `MoveToOuterTurret`/`WaitForWave`는 enemyChampion이 있어도 allied wave가 없으면 safe anchor Move를 낸다.
4. 현행 요네 combo는 E2가 마지막이고 Execute가 `bReturning`에서 combo를 clear하므로 복귀 후 R은 구조적으로 불가능하다.
5. 현행 공세 미드 합류는 적 outer 파괴/수적 열세를 입력으로 사용하지 않아 side lane bot이 합류하지 않는다.
6. 현행 minion depenetration은 lane distance를 확인하지 않아 soft push가 전진을 상쇄해도 성공으로 취급할 수 있다.

### 3.2 검증 순서

```powershell
git diff --check
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/AIResearch/RunValidation.ps1
msbuild Winters.sln /m:1 /t:Build /p:Configuration=Debug /p:Platform=x64
Client/Bin/Debug/SimLab.exe 600 1234
Client/Bin/Debug/SimLab.exe 600 1234
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Run-BotAiValidation.ps1 -Configuration Debug
```

동일 seed 두 번의 SimLab 최종 hash와 새 mid-group fixture hash가 같아야 한다. 데이터 계약 hash가 의도 변경으로 달라지면 생성물과 SimLab의 정확한 expected anchor만 갱신하고, 전투/봇 golden hash의 비의도 변동은 원인 분석 없이 수용하지 않는다.

### 3.3 서버 미니언 런타임 게이트

full build 뒤 서버 Debug trace에서 다음을 확인한다.

```text
[MinionMove][Depenetrate]는 lane goal 역행 candidate를 적용하지 않음
[MinionMove][Stuck] 동일 entity의 blocked 값이 영구 0으로 리셋되는 진동 없음
[UnitAI] flow fallback/path fallback 뒤 lane waypoint distance가 감소
```

자동 GameRoom 장기 wave fixture가 현 target에 없으므로 실제 5v5 bottom-wave 3개 wave 통과는 `CONFIRM_NEEDED` 수동 게이트다. 이 항목 때문에 자동 검증 통과를 “인게임 정체 완전 재현 PASS”로 과장하지 않는다.

### 3.4 인게임 인수 조건

```text
Blue: 3 player + Kindred/Lee Sin bot
Red: Annie top, Sylas/Ashe mid, Fiora/Yone bottom

- Annie가 top anchor 이동 중 피격/관측 적을 무시하지 않는다.
- Yone/Fiora/Sylas가 full~중간 체력 교전에서 tower 밖 적을 선제 탐지·스킬 진입한다.
- Yone 정상 콤보가 E-Q-W-BA-BA-BA-E2-return-R 순서로 보인다.
- 위험/포탑/시간 만료 조기 E2는 안전 복귀하고 R을 억지로 쓰지 않는다.
- 적 mid outer 파괴+적 2명 이상 집결+아군 열세에서 bottom bot이 mid wave로 합류한다.
- bottom minion wave가 겹쳐도 3개 wave 연속 lane waypoint를 통과하고 Kindred/Lee가 정지하지 않는다.
```

### 3.5 RESULT/handoff

구현 시작 후 같은 이름의 `_RESULT.md`를 반드시 작성한다. RESULT는 ①예상 대 실제 관찰 ②최종 판정 ③갱신 비용/트레이드오프 세 섹션만 사용하며, 자동 PASS와 수동 `CONFIRM_NEEDED`를 분리한다.
