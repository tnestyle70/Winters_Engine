# 2026-07-18 ChampionAI 미드 방어 래치 수명 계획서

```text
Session - 타워 상실 후 봇 영구 미드 래치를 위협 기반 수명으로 교체해 파밍/교전 복원
좌표: 신규 좌표 후보 · 축: C4·C8
관련: 없음 (본 세션 워크플로 전수 감사가 근거)
```

Current sequence: 원인 확정(완료) → 본 계획서 → 코드 반영 → Debug/Release 빌드 + SimLab/소크 → 인게임 게이트(사용자).

Goal: 영상 3증상 제거 — ①타워 앞 멍때림 ②챔피언 공격 빈도 급감 ③사거리 내 적을 무시하고 미드로 걸어감 — 그리고 타워 붕괴 후에도 사람처럼 파밍/교전이 지속되게 한다.
Non-goals: 유틸리티 수식·PlayerLike HP게이트(S036) 튜닝, 수비 인원 배분(전원 로테이션 억제), TradeWindow 사어코드 정리, 스냅샷 와이어 확장.

## 1. 결정 기록

```text
① 문제·제약: bMidDefenseActive는 ChampionAISystem.cpp:4390에서 set 후 매치 내 해제 0회(유일한 false는
   GameRoom.cpp:192 사망/리스폰 리셋 — 포탑은 부활하지 않아 즉시 재래치). 아무 레인 외곽 포탑 1개 사망(:219-243)
   으로 팀 전체가 영구 GroupMidDefense: 반경 11 밖에선 ctx.enemyChampion 무판독 이동만 방출(:3861-3873),
   activeLane=Mid 강제(:4198)로 레인 필터(:1836/:1803)가 자기 레인 미니언을 실명시키고(MinionFarmValue
   0.55→0.15), FollowWave는 래치 게이트(:3568)로 차단. 결정 주기 0.20s.
② 순진한 해법의 실패: "해제 조건만 추가"는 (bAlliedOuterTurretLost||bMidLaneTurretLost)가 영구 true라서
   다음 조용한 결정 틱(0.2s)에 즉시 재래치 — 해제·진입이 같은 판정을 공유해야 진동이 없다.
③ 메커니즘: 위협 히스테리시스 — 진입=포탑 상실 && 앵커 반경 20 내 적(챔피언/미니언) 존재, 유지=위협 재감지 시
   홀드 6s 리필, 해제=홀드 소진 시 홈 레인 복원. 로테이션 중 조우(사거리 내 적 챔피언 or 스캔 내 적 미니언)는
   ExecuteLaneCombat 위임. 상세는 §2.
④ 대조: 실제 LoL 플레이는 수비 집결이 "위협의 함수"(웨이브/다이브 감지→로테이션→해소→레인 복귀)다. 현행은
   이벤트(포탑 사망) 원샷 래치. SimLab 골든 프로브(:5189-5196)가 원웨이 래치를 계약으로 박제해 게이트가 회귀를
   통과시켰다(C8) — 프로브도 새 계약으로 확장한다.
⑤ 대가: 위협 판정이 전역 쿼리(시야 무시 = 미니맵 근사)라 부쉬 매복도 감지하는 월핵성. 반경 20/홀드 6s는 실측
   없는 초기값 — 미드에 적 웨이브가 상시 유입되면 사실상 상주 수비가 되고, 값이 틀리면 레인↔미드 왕복 churn.
   5봇 동시 로테이션은 여전히 허용(인원 배분은 다음 슬라이스). 틀리게 되는 때: 위협 없이 백도어 시즈가 강한 메타.
```

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

`ChampionAIComponent` 구조체 끝(기존 offsetof 어서트 보존을 위해 반드시 말미에 추가).

아래로 교체:

기존 코드:

```cpp
	bool_t bDebugForceAction = false;
	u8_t reservedTail[3]{};
};
```

교체:

```cpp
	bool_t bDebugForceAction = false;
	u8_t reservedTail[3]{};

	f32_t midDefenseThreatHoldTimer = 0.f;
	u8_t reservedMidDefenseAlignment[4]{};
};
```

기존 코드:

```cpp
static_assert(sizeof(ChampionAIComponent) == 2928u);
```

교체:

```cpp
static_assert(sizeof(ChampionAIComponent) == 2936u);
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

(a) 상수 — 익명 네임스페이스 상수 블록.

기존 코드:

```cpp
    constexpr f32_t kChampionAIMidDefenseReturnRadius = 11.f;
```

아래에 추가:

```cpp
    constexpr f32_t kChampionAIMidDefenseThreatRadius = 20.f;
    constexpr f32_t kChampionAIMidDefenseThreatHoldSec = 6.f;
```

(b) 위협 판정 헬퍼 — `HasMidLaneTurretLost` 함수 본문 전체 아래에 추가 (`ResolveMidDefenseAnchor` 위):

```cpp
    // 미드 방어선(앵커) 근방의 실제 위협(적 챔피언/미니언) 존재 여부.
    // 포탑 상실 판정과 같은 전역 쿼리다(미니맵 근사) — 집결은 이벤트가
    // 아니라 위협의 함수여야 래치가 영구화되지 않는다.
    bool_t HasMidDefenseThreat(
        CWorld& world,
        eTeam team,
        const Vec3& anchor)
    {
        const eTeam enemyTeam = EnemyTeam(team);
        const f32_t radiusSq =
            kChampionAIMidDefenseThreatRadius *
            kChampionAIMidDefenseThreatRadius;
        bool_t bThreat = false;

        world.ForEach<ChampionComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent& champion, TransformComponent& transform)
            {
                if (bThreat ||
                    champion.team != enemyTeam ||
                    world.HasComponent<PracticeDummyTag>(e) ||
                    !IsAliveTarget(world, e))
                {
                    return;
                }

                if (WintersMath::DistanceSqXZ(anchor, transform.GetPosition()) <= radiusSq)
                    bThreat = true;
            });
        if (bThreat)
            return true;

        world.ForEach<MinionComponent, TransformComponent>(
            [&](EntityID e, MinionComponent& minion, TransformComponent& transform)
            {
                if (bThreat ||
                    minion.team != enemyTeam ||
                    !IsAliveTarget(world, e))
                {
                    return;
                }

                if (WintersMath::DistanceSqXZ(anchor, transform.GetPosition()) <= radiusSq)
                    bThreat = true;
            });
        return bThreat;
    }
```

(c) 홀드 타이머 감쇠 — `Execute` 타이머 블록.

기존 코드:

```cpp
            ai.fSkillCastCooldownTimer =
                std::max(0.f, ai.fSkillCastCooldownTimer - tc.fDt);
```

아래에 추가:

```cpp
            ai.midDefenseThreatHoldTimer =
                std::max(0.f, ai.midDefenseThreatHoldTimer - tc.fDt);
```

(d) 래치 해제 블록 + 진입 조건 — `Execute`의 미드 방어 전환 블록.

아래로 교체:

기존 코드:

```cpp
            if (!ai.bMidDefenseActive &&
                (ctx.bAlliedOuterTurretLost || ctx.bMidLaneTurretLost) &&
                ctx.bCanMove &&
                ctx.enemyChampion == NULL_ENTITY &&
                ai.comboTarget == NULL_ENTITY &&
                ai.divePhase == eChampionAIDivePhase::None)
            {
                ai.bMidDefenseActive = true;
                ai.activeLane = kChampionAIMidLane;
                ai.midDefenseAnchor = ctx.midDefenseAnchor;
                SetChampionAIState(ai, eChampionAIState::GroupMidDefense);
                SetChampionAIIntent(ai, eChampionAIIntent::DefendMid, true);
```

교체:

```cpp
            if (ai.bMidDefenseActive &&
                ai.comboTarget == NULL_ENTITY &&
                ai.divePhase == eChampionAIDivePhase::None)
            {
                if (HasMidDefenseThreat(world, champion.team, ctx.midDefenseAnchor))
                {
                    ai.midDefenseThreatHoldTimer =
                        kChampionAIMidDefenseThreatHoldSec;
                }
                else if (ai.midDefenseThreatHoldTimer <= 0.f)
                {
                    // 위협이 사라진 방어 집결은 수명이 끝난다 — 홈 레인
                    // 파밍/교전 루프로 복귀. 재집결은 위협 재감지가 연다.
                    ai.bMidDefenseActive = false;
                    ai.activeLane = ai.lane;
                    SetChampionAIState(ai, eChampionAIState::MoveToOuterTurret);
                    SetChampionAIIntent(ai, eChampionAIIntent::FarmMinion);

                    ctx = BuildChampionAIContext(
                        world,
                        self,
                        ai,
                        champion,
                        tc,
                        selfPos);
                    UpdateChampionAIDecisionEvidence(ai, tc, ctx, profile);
                }
            }

            if (!ai.bMidDefenseActive &&
                (ctx.bAlliedOuterTurretLost || ctx.bMidLaneTurretLost) &&
                ctx.bCanMove &&
                ctx.enemyChampion == NULL_ENTITY &&
                ai.comboTarget == NULL_ENTITY &&
                ai.divePhase == eChampionAIDivePhase::None &&
                HasMidDefenseThreat(world, champion.team, ctx.midDefenseAnchor))
            {
                ai.bMidDefenseActive = true;
                ai.midDefenseThreatHoldTimer = kChampionAIMidDefenseThreatHoldSec;
                ai.activeLane = kChampionAIMidLane;
                ai.midDefenseAnchor = ctx.midDefenseAnchor;
                SetChampionAIState(ai, eChampionAIState::GroupMidDefense);
                SetChampionAIIntent(ai, eChampionAIIntent::DefendMid, true);
```

(e) 로테이션 중 조우 대응 — `ExecuteGroupMidDefense` 집결 반경 블록과 이동 방출 사이.

기존 코드:

```cpp
        SetChampionAIState(ai, eChampionAIState::GroupMidDefense);
        SetChampionAIIntent(ai, eChampionAIIntent::DefendMid, true);
        EmitMoveCommand(
```

아래로 교체:

```cpp
        // 로테이션 중 조우 대응: 사거리 내 적 챔피언이나 스캔 내 적 미니언을
        // 무시하고 지나치지 않는다 — 일반 레인 전투(교전/파밍)로 위임한다.
        const bool_t bEnemyChampionInAttackRange =
            ctx.enemyChampion != NULL_ENTITY &&
            ctx.enemyDistance <= ctx.attackRange;
        if (bEnemyChampionInAttackRange || ctx.enemyMinion != NULL_ENTITY)
        {
            ExecuteLaneCombat(
                world,
                tc,
                self,
                ai,
                champion,
                selfPos,
                ctx,
                outCommands);
            return;
        }

        SetChampionAIState(ai, eChampionAIState::GroupMidDefense);
        SetChampionAIIntent(ai, eChampionAIIntent::DefendMid, true);
        EmitMoveCommand(
```

### 2-3. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

`ResetChampionAIForLifecycle` 말미.

아래로 교체:

기존 코드:

```cpp
        ai.bPostComboBAAllowed = false;
        ai.bMidDefenseActive = false;
    }
```

교체:

```cpp
        ai.bPostComboBAAllowed = false;
        ai.bMidDefenseActive = false;
        ai.midDefenseThreatHoldTimer = 0.f;
    }
```

### 2-4. C:/Users/user/Desktop/Winters/Client/Private/UI/DebugDrawSystem.cpp

F10 월드 오버레이 라벨 갭(현재 "Unknown" 렌더) — 인게임 검증 표면.

기존 코드:

```cpp
        case eChampionAIState::Dead: return "Dead";
        default: return "Unknown";
```

교체:

```cpp
        case eChampionAIState::Dead: return "Dead";
        case eChampionAIState::GroupMidDefense: return "GroupMidDef";
        default: return "Unknown";
```

기존 코드:

```cpp
        case eChampionAIIntent::Recall: return "Recall";
        default: return "Unknown";
```

교체:

```cpp
        case eChampionAIIntent::Recall: return "Recall";
        case eChampionAIIntent::DefendMid: return "DefendMid";
        default: return "Unknown";
```

### 2-5. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunChampionAIMidDefenseDeterminismProbe` — 새 계약(위협 게이트 진입·해제·조우 교전)으로 확장.

(i) 메인 시나리오에 위협 미니언 추가 — `RunScenario` 람다의 alliedWave 블록.

기존 코드:

```cpp
            if (alliedWave != NULL_ENTITY)
            {
                world.GetComponent<MinionComponent>(alliedWave).laneType =
                    static_cast<u8_t>(Winters::Map::eLane::Mid);
                world.GetComponent<MinionStateComponent>(alliedWave).lane =
                    static_cast<u8_t>(Winters::Map::eLane::Mid);
            }
```

아래에 추가:

```cpp
            // 위협 게이트 진입 계약: 앵커 근방에 적 미니언이 있어야 집결한다.
            const EntityID midThreat = SpawnStatusProbeTarget(
                world,
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                eTeam::Red,
                Vec3{ 18.f, 0.f, 10.f });
            if (midThreat != NULL_ENTITY)
            {
                world.GetComponent<MinionComponent>(midThreat).laneType =
                    static_cast<u8_t>(Winters::Map::eLane::Mid);
                world.GetComponent<MinionStateComponent>(midThreat).lane =
                    static_cast<u8_t>(Winters::Map::eLane::Mid);
            }
```

같은 람다의 스폰 유효성 가드.

기존 코드:

```cpp
            if (deadOuter == NULL_ENTITY ||
                liveMidInner == NULL_ENTITY ||
                liveNexus == NULL_ENTITY ||
                alliedWave == NULL_ENTITY)
            {
                result.bActivated = false;
            }
```

교체:

```cpp
            if (deadOuter == NULL_ENTITY ||
                liveMidInner == NULL_ENTITY ||
                liveNexus == NULL_ENTITY ||
                alliedWave == NULL_ENTITY ||
                midThreat == NULL_ENTITY)
            {
                result.bActivated = false;
            }
```

(ii) 커밋먼트 시나리오에 위협 미니언 추가 — 미드 이너 `SpawnStructure` 호출.

기존 코드:

```cpp
        SpawnStructure(
            commitmentWorld,
            eTeam::Blue,
            Winters::Map::eObjectKind::Structure_Turret,
            Winters::Map::eTurretTier::Inner,
            Winters::Map::eLane::Mid,
            Vec3{ 20.f, 0.f, 10.f },
            false);
```

아래에 추가:

```cpp
        const EntityID commitmentThreat = SpawnStatusProbeTarget(
            commitmentWorld,
            GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
            eTeam::Red,
            Vec3{ 18.f, 0.f, 10.f });
        if (commitmentThreat == NULL_ENTITY)
        {
            std::printf(
                "[SimLab][ChampionAI][MidDefense] FAIL: commitment threat spawn failed\n");
            return false;
        }
```

(iii) 신규 시나리오 3종 — 기존 danger 시나리오 검증 블록(`cross-lane turret danger was ignored ...` printf가 있는 if 블록) 전체 바로 아래, 마지막 PASS printf 위에 추가:

```cpp
        // 신규 계약 1: 위협 소멸 + 홀드 소진 → 래치 해제, 홈 레인 복귀.
        CWorld releaseWorld;
        DeterministicRng releaseRng(20260725ull);
        EntityIdMap releaseEntityMap;
        FlatWalkable releaseWalkable;
        const EntityID releaseBot = SpawnChampion(
            releaseWorld,
            releaseEntityMap,
            eChampion::ASHE,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        releaseWorld.GetComponent<TransformComponent>(releaseBot).SetPosition(
            Vec3{ 20.f, 1.f, 10.f });
        ChampionAIComponent releaseAI{};
        releaseAI.champion = eChampion::ASHE;
        releaseAI.team = eTeam::Blue;
        releaseAI.lane = static_cast<u8_t>(Winters::Map::eLane::Top);
        releaseAI.activeLane = static_cast<u8_t>(Winters::Map::eLane::Mid);
        releaseAI.state = eChampionAIState::GroupMidDefense;
        releaseAI.intent = eChampionAIIntent::DefendMid;
        releaseAI.bMidDefenseActive = true;
        releaseAI.midDefenseThreatHoldTimer = 0.f;
        releaseAI.decisionTimer = 0.f;
        releaseAI.safeAnchor = Vec3{ -10.f, 1.f, -10.f };
        releaseAI.retreatGoal = releaseAI.safeAnchor;
        releaseAI.midDefenseAnchor = Vec3{ 20.f, 1.f, 10.f };
        releaseWorld.AddComponent<ChampionAIComponent>(releaseBot, releaseAI);
        SpawnStructure(
            releaseWorld,
            eTeam::Blue,
            Winters::Map::eObjectKind::Structure_Turret,
            Winters::Map::eTurretTier::Outer,
            Winters::Map::eLane::Top,
            Vec3{ -10.f, 0.f, -10.f },
            true);

        TickContext releaseTick = MakeProbeTickContext(
            1ull,
            releaseRng,
            releaseEntityMap,
            releaseWalkable);
        std::vector<GameCommand> releaseCommands;
        CChampionAISystem::Execute(releaseWorld, releaseTick, releaseCommands);
        const auto& releasedState =
            releaseWorld.GetComponent<ChampionAIComponent>(releaseBot);
        if (releasedState.bMidDefenseActive ||
            releasedState.activeLane !=
                static_cast<u8_t>(Winters::Map::eLane::Top) ||
            releasedState.state != eChampionAIState::MoveToOuterTurret)
        {
            std::printf(
                "[SimLab][ChampionAI][MidDefense] FAIL: threat-free latch did not release active=%u lane=%u state=%u\n",
                releasedState.bMidDefenseActive ? 1u : 0u,
                static_cast<unsigned>(releasedState.activeLane),
                static_cast<unsigned>(releasedState.state));
            return false;
        }

        // 신규 계약 2: 위협 존재 시 래치 유지 + 홀드 리필.
        CWorld holdWorld;
        DeterministicRng holdRng(20260726ull);
        EntityIdMap holdEntityMap;
        FlatWalkable holdWalkable;
        const EntityID holdBot = SpawnChampion(
            holdWorld,
            holdEntityMap,
            eChampion::ASHE,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        holdWorld.GetComponent<TransformComponent>(holdBot).SetPosition(
            Vec3{ 20.f, 1.f, 10.f });
        ChampionAIComponent holdAI{};
        holdAI.champion = eChampion::ASHE;
        holdAI.team = eTeam::Blue;
        holdAI.lane = static_cast<u8_t>(Winters::Map::eLane::Top);
        holdAI.activeLane = static_cast<u8_t>(Winters::Map::eLane::Mid);
        holdAI.state = eChampionAIState::GroupMidDefense;
        holdAI.intent = eChampionAIIntent::DefendMid;
        holdAI.bMidDefenseActive = true;
        holdAI.midDefenseThreatHoldTimer = 0.f;
        holdAI.decisionTimer = 0.f;
        holdAI.safeAnchor = Vec3{ -10.f, 1.f, -10.f };
        holdAI.retreatGoal = holdAI.safeAnchor;
        holdAI.midDefenseAnchor = Vec3{ 20.f, 1.f, 10.f };
        holdWorld.AddComponent<ChampionAIComponent>(holdBot, holdAI);
        SpawnStatusProbeTarget(
            holdWorld,
            GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
            eTeam::Red,
            Vec3{ 18.f, 0.f, 10.f });

        TickContext holdTick = MakeProbeTickContext(
            1ull,
            holdRng,
            holdEntityMap,
            holdWalkable);
        std::vector<GameCommand> holdCommands;
        CChampionAISystem::Execute(holdWorld, holdTick, holdCommands);
        const auto& heldState =
            holdWorld.GetComponent<ChampionAIComponent>(holdBot);
        if (!heldState.bMidDefenseActive ||
            heldState.midDefenseThreatHoldTimer <= 0.f)
        {
            std::printf(
                "[SimLab][ChampionAI][MidDefense] FAIL: threatened latch released early active=%u hold=%.2f\n",
                heldState.bMidDefenseActive ? 1u : 0u,
                heldState.midDefenseThreatHoldTimer);
            return false;
        }

        // 신규 계약 3: 로테이션 중 사거리 내 적 챔피언 → 교전 위임(지나치기 금지).
        CWorld engageWorld;
        DeterministicRng engageRng(20260727ull);
        EntityIdMap engageEntityMap;
        FlatWalkable engageWalkable;
        const EntityID engageBot = SpawnChampion(
            engageWorld,
            engageEntityMap,
            eChampion::ASHE,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        const EntityID engageEnemy = SpawnChampion(
            engageWorld,
            engageEntityMap,
            eChampion::JAX,
            static_cast<u8_t>(eTeam::Red),
            5u);
        engageWorld.GetComponent<TransformComponent>(engageBot).SetPosition(
            Vec3{});
        engageWorld.GetComponent<TransformComponent>(engageEnemy).SetPosition(
            Vec3{ 1.5f, 0.f, 0.f });
        ChampionAIComponent engageAI{};
        engageAI.champion = eChampion::ASHE;
        engageAI.team = eTeam::Blue;
        engageAI.difficulty = 2u;
        engageAI.brainType = eChampionAIBrainType::PlayerLike;
        engageAI.lane = static_cast<u8_t>(Winters::Map::eLane::Top);
        engageAI.activeLane = static_cast<u8_t>(Winters::Map::eLane::Mid);
        engageAI.state = eChampionAIState::GroupMidDefense;
        engageAI.intent = eChampionAIIntent::DefendMid;
        engageAI.bMidDefenseActive = true;
        engageAI.midDefenseThreatHoldTimer =
            6.f;
        engageAI.decisionTimer = 0.f;
        engageAI.safeAnchor = Vec3{ -10.f, 1.f, -10.f };
        engageAI.retreatGoal = engageAI.safeAnchor;
        engageAI.midDefenseAnchor = Vec3{ 20.f, 1.f, 10.f };
        engageWorld.AddComponent<ChampionAIComponent>(engageBot, engageAI);

        TickContext engageTick = MakeProbeTickContext(
            1ull,
            engageRng,
            engageEntityMap,
            engageWalkable);
        std::vector<GameCommand> engageCommands;
        CChampionAISystem::Execute(engageWorld, engageTick, engageCommands);
        const auto& engagedState =
            engageWorld.GetComponent<ChampionAIComponent>(engageBot);
        if (engagedState.state != eChampionAIState::LaneCombat ||
            engagedState.intent != eChampionAIIntent::AttackChampion)
        {
            std::printf(
                "[SimLab][ChampionAI][MidDefense] FAIL: in-range enemy ignored during rotation state=%u intent=%u\n",
                static_cast<unsigned>(engagedState.state),
                static_cast<unsigned>(engagedState.intent));
            return false;
        }
```

## 3. 검증 — 예측을 먼저 쓴다

```text
예측:
- 경계: Bot AI는 GameCommand 생산자다 — 본 변경은 명령 방출 경로/매크로 상태만 수정하며 게임플레이 진실을
  직접 변경하지 않는다. CommandExecutor 검증 경로·스냅샷 와이어 무변경.
- SimLab MidDefense 프로브: 기존 assert 전부 PASS 유지(메인/커밋먼트 시나리오는 위협 미니언 추가로 활성화
  보존) + 신규 3 assert(해제/유지/조우 교전) PASS. 프로브 해시값 자체는 변동(행동 변화 의도, runA==runB만 유효).
- SimLab 전체 exit 0. 풀런 해시 변동은 의도(커밋된 골든 해시 없음, 동일 프로세스 runA/runB 비교만 존재).
- 키프레임 replay 프로브: 새 필드는 구조체 말미 + 명시적 정렬 패딩이라 memcpy 직렬화에 포함, divergence 없음.
  (필드 직렬화 누락이 있다면 이 프로브가 FAIL로 잡는다 — 그게 게이트다.)
- 봇 소크: RESULT status=PASS + 크로스런 replay/world 해시 동일. 해시 값은 이전 세션과 다름(의도).
- 인게임: 타워 붕괴 + 미드 위협 시 F9 Macro=DefendMid로 집결·수비, 위협 해소 ~6s 후 HomeLane 복귀·파밍 재개.
  로테이션 중 사거리 내 적 챔피언 무시 소멸. F10 오버레이가 "GroupMidDef/DefendMid" 표시(기존 "Unknown").
- 깨뜨릴 수 있는 것: sizeof 변경(2928→2936)이 keyframe blob 크기를 바꾼다(파일 영속 없음, 게이트=SimLab
  키프레임 프로브+static_assert). 위협/홀드 상수 체감 오류는 인게임 게이트에서만 잡힘(자동 게이트 없음 — 발견).

검증 명령:
- powershell -ExecutionPolicy Bypass -File Tools\Harness\Run-BotAiValidation.ps1 -Configuration Debug
  (git diff --check + ChampionAI 경계 rg 감사 + GameSim/Server/Client/SimLab 빌드 /m:1 + SimLab 실행)
- Tools\Bin\Debug\SimLab.exe   (기대 로그: "[SimLab][ChampionAI][MidDefense] PASS")
- powershell -ExecutionPolicy Bypass -File Tools\Harness\Check-SharedBoundary.ps1
- powershell -ExecutionPolicy Bypass -File Tools\Harness\RunGameRoomBotMatchSoak.ps1 -TickCount 1800 -Seed 42
  -Runs 3 -Configuration Debug   (기대: RESULT status=PASS, 크로스런 해시 동일)
- Release 재확인: MSBuild 4종(GameSim/Server/Client/SimLab .vcxproj) /t:Build /p:Configuration=Release
  /p:Platform=x64 /m:1 /v:minimal + Tools\Bin\Release\SimLab.exe

미검증:
- 인게임 시각 게이트(사용자 육안): 위협 반경 20/홀드 6s의 체감 — 실매치에서 레인↔미드 churn 여부.
- 수비 인원 배분(5봇 동시 로테이션)은 본 슬라이스 범위 밖.

확인 필요:
- ChampionAIComponent sizeof 실측(2936 예상 — 컴파일러 어서트가 판정). 불일치 시 어서트 값을 실측으로.
- SimLab 조우 시나리오의 Ashe attackRange 실측(정의 팩 기준) — 1.5 거리에서 in-range 전제.

롤백 범위: 본 계획 5개 파일 diff revert로 완전 복원(데이터 팩/스키마/와이어/생성 코드 무변경).
다음 슬라이스: 수비 인원 배분(앵커 최근접 N봇만 로테이션) + 위협 반경/홀드의 JSON 튜닝 노출.
```
