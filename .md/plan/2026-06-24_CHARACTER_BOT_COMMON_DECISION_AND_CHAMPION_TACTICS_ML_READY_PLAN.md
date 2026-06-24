Session - Character Bot AI를 공통 판단 근거와 챔피언별 전술로 분리하고 ML/RL 연결 토대를 만든다.

1. 반영해야 하는 코드

본질:
- Bot AI는 서버 권위 GameSim의 `GameCommand` 생산자다. Transform, HP, cooldown, damage truth를 직접 고치지 않는다.
- 공통 로직은 "지금 무엇이 가치 있는가"를 판단한다. 골드, 성장, 체력, 미니언/포탑/오브젝트 가치, 시야 위험, 거리, 라인 상태, 전투 가능성을 하나의 decision evidence로 만든다.
- 개별 챔피언 로직은 "공통 판단이 공격/파밍을 선택했을 때 이 챔피언은 어떤 스킬 순서와 조건으로 명령을 낼 것인가"만 담당한다.
- 미래 ML/RL은 같은 evidence와 legal action candidate를 입력으로 받고, 같은 command emitter와 `CommandExecutor` 검증을 통과해야 한다. 모델이 gameplay truth를 직접 만지면 안 된다.

목표 흐름:

```text
World snapshot facts
-> Common Decision Evidence
-> Brain intent decision
-> Champion Tactical Adapter
-> GameCommand candidate/emitter
-> CommandExecutor
-> Server GameSim truth
-> Snapshot/Event
-> Client Visual
```

소유권:
- `Shared/GameSim`: AI 판단 입력, 점수 계산, brain interface, champion tactic, command 후보 생성 규칙.
- `Server`: bot spawn wiring, difficulty/brainType 선택, command producer 실행 위치.
- `Client`: snapshot/debug 표시와 visual. AI 판단 truth를 만들지 않는다.
- `Tools/SimLab`: deterministic scenario, replay hash, future offline decision-frame export 검증.
- 기획/디자인 데이터: 수치/가중치/콤보 의도는 우선 C++ profile로 고정하고, 안정화 후 JSON authoring으로 옮긴다.

현재 코드 증거:
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h`는 `difficulty`, `brainType`, intent/state/action, tuning, decision trace, score 필드를 이미 가진다.
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h`는 `IChampionAIBrain`과 `ChampionAIBrainInput`을 통해 intent 결정만 분리하려는 구조가 이미 있다.
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h`는 `ChampionAIProfile`, `ChampionAIComboPlan`, `ChampionAIComboStep`을 이미 가진다.
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`의 `ChampionAIContext`는 현재 enemyChampion, enemyMinion, enemyStructure, alliedWave, hp ratio, distance, turret danger를 수집한다.
- 같은 파일의 `UpdateChampionAIDecisionEvidence(...)`는 현재 champion/farm/structure score를 계산하고 debug trace에 반영한다.
- 같은 파일의 `ResolveChampionCombatTactics(...)`는 챔피언별 전투 tactic registry의 시작점이다. 현재 Yasuo/Yone만 특수 tactic을 가진다.
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp`는 챔피언별 profile과 일부 combo plan을 이미 가진다.
- `C:/Users/user/Desktop/Winters/ECS/Components/VisionComponents.h`와 Snapshot `Ward` kind, Client FOW/minimap 경로가 있으므로 시야 정보는 별도 AI truth가 아니라 기존 visibility/fog 데이터를 읽는 방향이어야 한다.

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

현재 `ChampionAIContext`를 확장하는 것이 1단계다.

기존 코드:

```cpp
    struct ChampionAIContext
    {
        EntityID enemyChampion = NULL_ENTITY;
        EntityID lowHpEnemyChampion = NULL_ENTITY;
        EntityID diveTarget = NULL_ENTITY;
        EntityID enemyMinion = NULL_ENTITY;
        EntityID enemyStructure = NULL_ENTITY;
        EntityID alliedWave = NULL_ENTITY;
        EntityID airborneChampion = NULL_ENTITY;
        EntityID yasuoDashMinion = NULL_ENTITY;

        f32_t selfHpRatio = 1.f;
        f32_t enemyHpRatio = 1.f;
        f32_t enemyDistance = 999.f;
        f32_t lowHpEnemyRatio = 1.f;
        f32_t lowHpEnemyDistance = 999.f;
        f32_t attackRange = 1.5f;
        f32_t waveDistance = 999.f;
        f32_t turretDanger = 0.f;
        f32_t airborneDistance = 999.f;
        f32_t yasuoDashMinionDistance = 999.f;

        u8_t yasuoQStage = 1u;
        bool_t bYasuoEActive = false;

        bool_t bAlliedWaveNearby = false;
        bool_t bStructureWaveTanking = false;
        bool_t bInsideEnemyTurretDanger = false;
    };
```

CONFIRM_NEEDED:
- 정확한 추가 필드명은 `GoldComponent`, `ChampionScoreComponent`, `VisibilityComponent`, ward/vision ownership 필드를 한 번 더 확인한 뒤 확정한다.
- 목표 필드 범위는 아래를 넘지 않는다.

```cpp
        f32_t selfGoldValue = 0.f;
        f32_t enemyGoldValue = 0.f;
        f32_t goldAdvantageScore = 0.f;
        f32_t levelAdvantageScore = 0.f;
        f32_t healthAdvantageScore = 0.f;
        f32_t minionValueScore = 0.f;
        f32_t structureValueScore = 0.f;
        f32_t objectiveValueScore = 0.f;
        f32_t visionRiskScore = 0.f;
        f32_t retreatScore = 0.f;
        f32_t engageScore = 0.f;

        bool_t bEnemyVisibleToSelfTeam = false;
        bool_t bSelfVisibleToEnemyTeam = false;
        bool_t bNearbyWardKnown = false;
        bool_t bObjectiveAvailable = false;
```

구현 원칙:
- 모든 값은 deterministic world/component에서 읽는다.
- 없는 시스템은 0 또는 false로 둔다. 없는 ward AI를 가정해서 새 gameplay truth를 만들지 않는다.
- score는 `0..1` 정규화 값으로 유지한다. ML/RL feature로 재사용하기 위해 tick마다 의미가 흔들리면 안 된다.

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`BuildChampionAIContext(...)`는 현재 타겟/라인/포탑 위험을 수집한다. 다음 단계에서는 이 함수 끝부분에 공통 evidence 수집을 추가한다.

기존 코드:

```cpp
        ctx.turretDanger = ComputeTurretDanger(
            world,
            champion.team,
            ai.lane,
            selfPos,
            ctx.bInsideEnemyTurretDanger);
```

아래에 추가할 방향:

```cpp
        // CONFIRM_NEEDED: exact code after checking GoldComponent,
        // ChampionScoreComponent, VisibilityComponent, and ward ownership fields.
        // CollectCommonChampionAIEvidence(world, self, ai, champion, tc, selfPos, ctx);
```

의도:
- `BuildChampionAIContext(...)`는 raw fact 수집까지만 한다.
- "공격할지, 파밍할지, 빠질지"는 여기서 결정하지 않는다.
- gold/level/vision/objective는 모두 common evidence로 들어간다.

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`UpdateChampionAIDecisionEvidence(...)`는 점수 계산 책임을 점진적으로 분리한다.

기존 코드:

```cpp
        ai.fFarmDecisionScore = ctx.enemyMinion != NULL_ENTITY ? 0.55f : 0.15f;
        if (ctx.selfHpRatio <= ai.retreatHpRatio + 0.10f)
            ai.fFarmDecisionScore += 0.15f;

        ai.fStructureDecisionScore =
            (ctx.enemyChampion == NULL_ENTITY &&
                ctx.enemyStructure != NULL_ENTITY &&
                ctx.alliedWave != NULL_ENTITY &&
                ctx.bStructureWaveTanking)
            ? 0.70f
            : 0.f;
```

아래 방향으로 교체:

```cpp
        // CONFIRM_NEEDED: extract exact scorer after the context fields are finalized.
        // const ChampionAIDecisionScores scores =
        //     ScoreCommonChampionAIDecision(ai, ctx);
        // ai.fChampionDecisionScore = scores.fChampionScore;
        // ai.fFarmDecisionScore = scores.fFarmScore;
        // ai.fStructureDecisionScore = scores.fStructureScore;
        // ai.fDecisionTurretDanger = scores.fTurretDanger;
```

점수 설계:
- `RetreatScore`: 낮은 체력, 포탑 위험, 시야 불리, 적 성장 우위가 높을수록 증가.
- `ChampionScore`: 내 체력/레벨/골드 우위, 적 체력 낮음, 아군 wave 근처, 스킬 사용 가능성이 높을수록 증가.
- `FarmScore`: 근처 미니언 가치, 막타 가능성, 전투 위험 회피 필요성이 높을수록 증가.
- `StructureScore`: enemyChampion 부재, 아군 wave tanking, 포탑 체력/거리, turret danger 허용 범위에서 증가.
- `ObjectiveScore`: dragon/baron/jungle camp 같은 중립 목표가 GameSim truth로 관측될 때만 증가. 아직 objective command가 없으면 score만 trace하고 실행은 하지 않는다.
- `VisionRiskScore`: 내 위치가 적에게 보일 가능성, 적 위치가 안 보이는 상태, ward/brush/fog 정보 부족에 따라 증가. 구현 전까지는 0으로 둔다.

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h

`ChampionAIBrainInput`은 현재 세 개 score만 가진다. 공통 판단 근거를 brain으로 전달하려면 최소 입력을 넓힌다.

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

아래 방향으로 교체:

```cpp
struct ChampionAIBrainInput
{
	f32_t fDt = 0.f;
	bool_t bCanAttackChampion = false;
	bool_t bPostComboBAWindow = false;
	f32_t fChampionScore = 0.f;
	f32_t fFarmScore = 0.f;
	f32_t fStructureScore = 0.f;
	f32_t fObjectiveScore = 0.f;
	f32_t fRetreatScore = 0.f;
	f32_t fVisionRiskScore = 0.f;
	f32_t fGoldAdvantageScore = 0.f;
	f32_t fHealthAdvantageScore = 0.f;
};
```

의도:
- Brain은 world를 직접 읽지 않는다.
- Brain은 intent만 결정한다.
- 나중에 ML/RL policy도 이 입력 구조와 같은 의미의 feature를 사용한다.

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

debug/snapshot에 남길 필드를 최소 추가한다.

CONFIRM_NEEDED:
- Snapshot schema에 새 debug field를 바로 추가할지, 우선 internal trace만 확장할지 결정해야 한다.
- 첫 구현은 schema churn을 피하기 위해 `ChampionAIComponent` 내부 trace만 확장하고, AIDebugPanel 노출은 2차로 분리하는 편이 안전하다.

추가 후보:

```cpp
	f32_t fObjectiveDecisionScore = 0.f;
	f32_t fRetreatDecisionScore = 0.f;
	f32_t fVisionRiskDecisionScore = 0.f;
	f32_t fGoldAdvantageDecisionScore = 0.f;
	f32_t fHealthAdvantageDecisionScore = 0.f;
```

중단 조건:
- debug field 추가 때문에 Snapshot schema, generated headers, Client UI까지 한 번에 큰 변경이 필요하면 멈추고 별도 "AI debug schema expansion" 계획으로 분리한다.

1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h

챔피언별 특성은 공통 decision score가 아니라 tactic 실행 가중치로 둔다.

기존 코드:

```cpp
struct ChampionAIProfile
{
    eChampion champion = eChampion::END;
    f32_t preferredRange = 1.5f;
    f32_t championScanRange = 6.f;
    f32_t minionScanRange = 10.f;
    f32_t structureScanRange = 18.f;
    f32_t leashRange = 14.f;
    f32_t aggression = 1.f;
    f32_t kiteBias = 0.f;
    f32_t retreatHpRatio = 0.35f;
    f32_t reengageHpRatio = 0.55f;
    f32_t minionPressureWeight = 1.f;
    f32_t turretRiskWeight = 1.f;
    f32_t lastHitWeight = 1.f;
    f32_t siegeWeight = 1.f;
    ChampionAISkillRule skillRules[4]{};
    u8_t skillRuleCount = 0;
};
```

추가 방향:

```cpp
    f32_t visionRiskWeight = 1.f;
    f32_t goldLeadFightWeight = 1.f;
    f32_t objectiveWeight = 1.f;
    f32_t executeBias = 1.f;
    f32_t disengageBias = 1.f;
```

의도:
- 공통 scorer는 "상황 가치"를 계산한다.
- profile은 같은 상황을 챔피언 성격에 맞게 보정한다.
- 예: Ashe/Kalista는 kite/vision risk에 민감, Jax/Yone/Yasuo는 engage/execute와 복귀 조건을 더 강하게 본다.

1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

챔피언별 특성 적용 위치는 `ResolveChampionCombatTactics(...)`와 minion farm tactic이다.

현재 전투 흐름:

```cpp
        if (ChampionCombatTacticsFn tactics = ResolveChampionCombatTactics(champion.id))
        {
            if (tactics(world, tc, self, ai, champion, selfPos, ctx, outCommands))
                return;
        }
```

방향:
- 공통 brain이 `AttackChampion` intent를 선택했을 때만 champion tactic이 실행된다.
- champion tactic은 스킬 후보, 재시전, 콤보, gap close, execute, disengage command만 만든다.
- champion tactic은 `CanAttackChampion(...)`, `IsSkillReady(...)`, `EmitSkillCommand(...)`, `EmitBasicAttackCommand(...)`를 통해서만 command를 낸다.
- Yone은 `E return`을 점수 경쟁이 아닌 생존 override로 유지한다.
- Yasuo는 `airborne R`, `E-Q`, minion gapclose를 champion 특성으로 유지한다.
- 일반 챔피언은 `ChampionAIComboPlan` 또는 basic fallback으로 처리한다.

1-8. 신규 파일 후보: C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIDecisionScorer.h

CONFIRM_NEEDED:
- 첫 구현에서 새 파일이 필요한지, `ChampionAISystem.cpp` 내부 helper로 시작해도 충분한지 결정해야 한다.
- 본질만 남기는 기준으로는 "함수가 커져서 Score/Context/Execute 경계가 흐려질 때"만 새 파일로 분리한다.

목표 인터페이스:

```cpp
struct ChampionAIDecisionScores
{
    f32_t fChampionScore = 0.f;
    f32_t fFarmScore = 0.f;
    f32_t fStructureScore = 0.f;
    f32_t fObjectiveScore = 0.f;
    f32_t fRetreatScore = 0.f;
    f32_t fVisionRiskScore = 0.f;
};
```

1-9. 신규 파일 후보: C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAITactics.h

CONFIRM_NEEDED:
- Yasuo/Yone 특수 tactic이 더 늘어나고 `ChampionAISystem.cpp`가 비대해진 뒤 분리한다.
- 지금 당장 분리하면 vcxproj/filters 변경과 함수 visibility 이동이 커진다.

목표:
- 챔피언별 "공격/미니언 공격 시 실행법"만 둔다.
- 공통 판단, state machine, context collection은 두지 않는다.
- champion tactic은 world를 읽을 수 있지만 truth를 직접 mutate하지 않고 command emitter만 호출한다.

1-10. 미래 ML/RL 연결 토대

런타임 정책:
- `eChampionAIBrainType::Decision`은 외부 모델을 직접 붙이는 자리가 아니라 "동일 feature/candidate contract를 소비하는 decision policy 자리"다.
- 모델/정책이 반환할 수 있는 값은 `Intent` 또는 `CandidateActionId`다.
- 반환값은 반드시 기존 command emitter를 거쳐야 한다.
- invalid action은 RuleBased fallback으로 되돌린다.

수집 artifact:
- tick, champion id, team, lane, difficulty, brainType
- normalized feature vector: hp, gold, level, distance, wave, turret danger, vision risk, objective pressure
- legal action mask: retreat, farm, attack champion, attack structure, recall, flash
- champion candidate list: skill slot, itemId, target mode, score, legality reason
- chosen action and emitted command kind/slot/target
- short-horizon outcome: damage dealt/taken, gold delta, death, objective, structure damage, distance to safe anchor

규칙:
- feature schema는 version/hash를 가진다.
- replay와 같은 seed에서 같은 feature/action이 나와야 한다.
- ML/RL training은 Winters runtime 밖에서 검증하고, runtime에는 distilled deterministic policy만 넣는다.
- OpenAI/ML/RL 고급 기법은 "공통 evidence와 legal candidate contract" 뒤에 붙는다. GameSim truth 경계 앞에 붙지 않는다.

1-11. 단계별 구현 순서

Stage A - 현재 기준선 고정:
- Yone E stage-2 data contract와 runtime return probe를 먼저 안정화한다.
- Bot AI harness와 full data-driven pipeline을 기준선으로 둔다.

Stage B - 공통 evidence 확장:
- `ChampionAIContext`에 gold/level/health/vision/objective score 근거를 추가한다.
- `UpdateChampionAIDecisionEvidence(...)`의 score 계산을 작은 helper로 분리한다.
- Snapshot/UI 변경은 하지 않고 내부 trace와 debug log로만 확인한다.

Stage C - brain input 확장:
- `ChampionAIBrainInput`에 retreat/objective/vision/gold/health score를 추가한다.
- RuleBased/PlayerLike brain은 기존 행동을 최대한 보존하면서 새 score를 반영한다.
- 기본 difficulty 2는 기존 RuleBased 체감을 유지한다.

Stage D - champion tactic 정리:
- Yone, Yasuo부터 champion tactic을 "reaction override + scored candidate" 형태로 정리한다.
- 이후 Jax/Fiora/Riven/Ashe 등 기존 combo plan 보유 챔피언으로 확장한다.
- 미니언 공격 tactic도 champion 특성별로 분리한다. 예: Yasuo E minion gapclose, ranged champion last-hit safety, melee champion wave join.

Stage E - debug/trace 확장:
- AIDebug trace에 new score를 노출한다.
- Debug UI는 Client presentation만 담당한다.
- score가 이상하면 `OutputDebugStringA/W` 또는 기존 debug wrapper로 bounded trace를 남긴다.

Stage F - ML/RL bridge:
- SimLab 또는 별도 offline runner에서 decision frame을 export한다.
- 학습/평가는 runtime 밖에서 수행한다.
- runtime에는 deterministic policy table 또는 distill된 lightweight policy만 붙인다.

2. 검증

기본 검증 명령:

```powershell
msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-BotAiValidation.ps1 -SkipFullPipeline
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
git diff --check
```

Stage별 성공 기준:
- Stage A: Yone E stage-2 contract audit PASS, SimLab Yone E return probe PASS, full pipeline PASS.
- Stage B: 기존 기본 bot이 정상 lane combat을 유지하고, new score가 `0..1` 범위를 벗어나지 않는다.
- Stage C: difficulty 1/2 RuleBased 기본 체감이 유지되고, difficulty 3 이상 PlayerLike만 의도적으로 달라진다.
- Stage D: 챔피언별 tactic은 command만 emit한다. 직접 Transform/HP/cooldown/damage를 바꾸는 코드가 없어야 한다.
- Stage E: AI debug UI/trace가 score 원인을 보여주되, normal F5 runtime behavior를 숨기거나 바꾸지 않는다.
- Stage F: same seed replay에서 feature/action export가 deterministic이어야 한다.

회귀 금지:
- Client에서 AI gameplay truth를 만들지 않는다.
- Server/GameSim 밖의 모델 호출이 frame gameplay를 block하지 않는다.
- ML/RL policy 실패가 command validation을 우회하지 않는다.
- normal F5에서 roster, map, minion, snapshot, champion, UI, FX를 숨겨서 검증 숫자를 만들지 않는다.

핸드오프:
- 기획자는 score 의미와 행동 의도: fight/farm/retreat/objective/vision risk 기준을 정의한다.
- 디자이너는 champion profile, combo/tactic 의도, debug trace를 보고 체감 튜닝한다.
- 개발자는 Shared/GameSim command boundary, deterministic feature, validation harness를 유지한다.
- ML/RL 작업자는 runtime 밖에서 decision frame을 학습/평가하고, runtime에는 검증된 deterministic policy artifact만 넘긴다.
