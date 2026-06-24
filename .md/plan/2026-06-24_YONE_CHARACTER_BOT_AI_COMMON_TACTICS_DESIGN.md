Session - Yone Bot AI를 목표로 Character Bot 공통 의사결정과 챔피언별 전술/콤보 설계를 코드베이스 기준으로 고정한다.

# Yone Character Bot AI Common Tactics Design

- 작성일: 2026-06-24
- 목적: 요네 Bot AI 구현을 다음 목표로 두고, 모든 Character Bot에게 공통 적용될 AI 본질과 챔피언별 개성을 분리한 설계/협업 기준을 정리한다.
- 성격: 설계/핸드오프 문서. 이 문서는 코드 패치가 아니며, 다음 구현 세션에서 파일별 앵커와 검증 단위로 쪼갠다.

## 결론

Bot AI의 본질은 "서버에서 월드 상태를 읽고, 의도를 고르고, 합법적인 GameCommand만 발행하는 것"이다. 체력, 위치, 쿨타임, 피해, FX를 Bot AI가 직접 바꾸면 안 된다. 그 결과는 반드시 `CommandExecutor`, 챔피언 GameSim 훅, 스냅샷/이벤트 파이프라인을 통해 나온다.

요네 Bot AI의 첫 번째 구현 목표는 화려한 콤보가 아니라 `E 진입 -> 전투 판단 -> E 복귀`가 서버 권위 파이프라인에서 안정적으로 닫히는 것이다. 현재 AI는 E 복귀 명령을 `cmd.itemId = 2u`로 발행하지만, `skill.yone.e` 데이터가 2단계 스킬 계약을 만족하지 않아 `CommandExecutor`에서 `stage2-window`로 거절될 수 있다. 따라서 다음 실제 코드 작업의 P0는 요네 E stage-2 데이터 계약 수정과 표적 시나리오 검증이다.

공통 Character Bot 구조는 `Perception -> Evidence -> Brain Intent -> Safety/State Arbitration -> Champion Tactics -> Command Emission -> Executor`로 고정한다. 챔피언별 개성은 이 공통 흐름 위에서 "반응형 우선 행동"과 "점수화된 액션 후보"로만 얹는다.

## 최신 문서 검토 결과

검토한 Bot AI/Yone 관련 문서는 다음 흐름으로 같은 결론을 가리킨다.

- `.md/plan/Champion/2026-06-24_YONE_BOT_AI_CODEBASE_AUDIT_AND_CONTINUATION_PLAN.md`: Y0/Y1 레지스트리 방향은 맞고, 현재 가장 큰 갭은 `skill.yone.e`의 stage-2 계약이다.
- `.md/build/2026-06-24_BOT_AI_COLLAB_PIPELINE_REPORT.md`: Bot AI 하네스는 전체 WARN이며, 경고 원인은 알려진 Yone E stage-2 데이터 갭이다.
- `.md/collab/work-packets/2026-06-24_yone_bot_ai_collab_pipeline.md`: 다음 작업 패킷은 Yone E stage-2 계약 수정과 표적 시나리오 검증을 소유해야 한다.
- `.md/TODO/06-24/WINTERS_Y0_Y1_YONE_AI_REGISTRY_IMPLEMENTATION_REPORT.md`: Y0/Y1로 `ChampionAISystem.cpp` 안에 챔피언 전술 레지스트리와 Yone 전술이 이미 들어갔다.
- `.md/plan/Champion/31_YONE_BOT_AI_AND_COMBAT_POLICY_ARCHITECTURE_DESIGN.md`: 선형 콤보만으로는 부족하므로 반응 override와 점수 후보 기반의 하이브리드가 필요하다고 정리한다.
- `.md/plan/ai/16_BOT_AI_HUMANLIKE_COMPLETION_PIPELINE_GUIDE.md`: 실제 코드 기준 Bot AI 흐름은 `CGameRoom::Tick -> Phase_ServerBotAI -> CServerAICommandProducer::Execute -> CChampionAISystem::Execute -> GameCommand -> CommandExecutor`이다.
- `.md/plan/ai/codex/BOT_AI_ROSTER_ADD_PIPELINE.md`: 일부 명칭은 오래됐지만, "Bot은 월드 상태를 읽고 GameCommand만 발행한다"는 핵심 불변식은 여전히 유효하다.
- `Tools/Harness/Run-BotAiValidation.ps1`: Bot AI 검증은 `git diff --check`, Shared/GameSim 의존성 경계 감사, Yone E stage-2 계약 감사, 선택적 전체 LoL data-driven 파이프라인으로 구성된다.

## 1. 반영해야 하는 코드

### 1.1 현재 코드 증거

서버 진입점은 `Server/Private/Game/GameRoomChampionAI.cpp`의 `CGameRoom::Phase_ServerBotAI`이다. 인게임 단계에서만 `CServerAICommandProducer::Execute(m_world, tc, m_pendingExecCommands)`를 호출한다.

`Server/Private/Game/ServerAICommandProducer.cpp`는 현재 별도 판단을 하지 않고 `CChampionAISystem::Execute(world, tc, outCommands)`로 위임한다. 따라서 Character Bot의 공통 판단 핵심은 Shared/GameSim의 `ChampionAISystem`에 있다.

`Server/Private/Game/GameRoomSpawn.cpp`는 Bot 스폰 시 `ChampionAIComponent`를 붙인다. 현재 채워지는 값은 champion, team, difficulty, lane, decisionTimer, scan/leash/retreat profile, safe anchor 등이다. `brainType`은 필드가 있지만 스폰에서 명시적으로 연결되지 않아 기본 `RuleBased`에 머무는 구조다.

`Shared/GameSim/Components/ChampionAIComponent.h`는 AI 런타임 상태의 중심이다. 여기에는 `state`, `intent`, `lastAction`, `difficulty`, `brainType`, target ids, combo/dive state, decision scores, hp/distance/turret evidence, debug trace, tuning override가 들어 있다.

`Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h/.cpp`는 챔피언별 정적 프로필을 제공한다. 현재 Yone profile은 근접형 값과 Q/W skill rule만 가진다. 선형 `ChampionAIComboPlan`은 Jax/Fiora/Ashe/Riven 같은 일부 챔피언에 있고, Yone은 별도 combo plan 없이 전술 함수로 처리된다.

`Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp`는 `RuleBased`, `PlayerLike`, `Decision` brain을 제공한다. `Decision`은 아직 외부 판단 모듈이 붙지 않아 RuleBased로 fallback한다.

`Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`는 현재 다음 일을 모두 수행한다.

- `BuildChampionAIContext`: 적 챔피언, 미니언, 구조물, 웨이브, 포탑 위험, 거리, 체력 등 의사결정 입력을 만든다.
- `UpdateChampionAIDecisionEvidence`: 점수와 디버그 evidence를 `ChampionAIComponent`에 기록한다.
- `SampleLaneCombatIntent`: `brainType`에 따라 lane combat intent를 고른다.
- `ExecuteLaneCombat`: 안전/퇴각, 활성 콤보, 구조물 공격, 챔피언별 전술, 일반 챔피언 공격, 미니언 파밍 순서로 명령을 발행한다.
- `EmitBasicAttackCommand`와 `EmitSkillCommand`: 실제 Bot AI 출력을 `GameCommand`로 제한한다.

현재 챔피언 전술 레지스트리는 `ChampionCombatTacticsFn` 함수 포인터 배열이다. 등록된 챔피언은 Yasuo와 Yone이며, `ExecuteLaneCombat`에서 일반 챔피언 공격보다 먼저 호출된다.

`Shared/GameSim/Champions/Yone/YoneGameSim.cpp`는 요네 스킬 권위 실행을 갖는다. `OnE`는 영혼 상태가 이미 활성화되어 있으면 `StartSoulReturn`을 호출하고, 아니면 soul out dash를 시작한다. `YoneGameSim::ResolveEStage`는 `YoneSimComponent::bSoulUnboundActive`가 true이면 stage 2를 반환한다.

`Shared/GameSim/Components/YoneSimComponent.h`는 AI가 읽을 수 있는 요네 전용 최소 상태를 갖는다. 핵심 필드는 `bSoulUnboundActive`, `bReturning`, `soulTimerSec`, `soulDurationSec`, `anchorPosition`이다.

`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`는 `cmd.itemId == 2u`를 stage-2 요청으로 해석한다. 단, `slot.currentStage == 1`, `slot.stageWindow > 0.f`, `GameplayDefinitionQuery::IsSkillTwoStage(...)`가 모두 참이어야 stage 2로 인정한다. 실패하면 `stage2-window`로 거절한다.

`Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`의 현재 `skill.yone.e`는 `stage.count = 1`, `stage.windowSeconds = 0.0`이다. 이 값은 AI가 발행하는 E 복귀 stage-2 요청과 맞지 않는다.

### 1.2 소유권 경계

공통 Bot AI의 권위 판단은 `Shared/GameSim/Systems/ChampionAI`에 둔다. 이 계층은 Engine, Client, Renderer, UI, ImGui, DX 타입을 포함하지 않는다.

서버는 Bot을 생성하고 lane/anchor/difficulty/brainType 같은 운영 입력을 붙인다. 서버는 AI 판단 결과를 직접 실행하지 않고 pending `GameCommand`로 넘긴다.

챔피언 GameSim은 스킬의 실제 효과와 상태 변화를 소유한다. Yone의 E 영혼 상태, R dash, Q/W damage 같은 결과는 `YoneGameSim`이 소유한다.

Client는 스냅샷과 replicated event를 재생한다. Client는 Bot 판단의 진실을 만들지 않는다. FX, 애니메이션, UI 디버그 표시만 담당한다.

기획자는 "어떤 상황에서 어떤 행동이 자연스러운가"를 시나리오와 acceptance criteria로 정의한다. 디자이너는 profile/tuning/data 숫자를 조정한다. 개발자는 명령 파이프라인, 데이터 계약, 검증 하네스를 유지한다.

### 1.3 목표 아키텍처

목표 흐름은 다음과 같다.

```text
Server Tick
-> ChampionAIComponent bot loop
-> Perception: visible world facts
-> Evidence: hp, distance, wave, turret, cooldown, special champion state
-> Brain Intent: farm / trade / all-in / retreat / siege / recall
-> Safety Arbitration: death risk, turret risk, action lock, return/escape window
-> Champion Tactics: champion-specific reactions and combo candidates
-> Command Emission: GameCommand only
-> CommandExecutor
-> Champion GameSim hooks
-> Snapshot / Replicated Event
-> Client visuals
```

이 구조에서 "공통"은 사실 수집, 점수화, 안전 판단, 명령 발행이다. "개별 챔피언"은 같은 입력을 받아 어떤 스킬/기본공격/이동 후보를 우선할지 결정하는 작은 전술 레이어다.

### 1.4 모든 Character Bot에게 공통 적용할 AI 본질

공통 AI는 매 tick마다 모든 것을 새로 결정하지 않는다. 이미 `decisionTimer`, `intentHoldTimer`, `action lock`, `debug override`가 있으므로 이 리듬을 유지한다. 사람처럼 보이는 기본은 초고속 반응이 아니라 일관된 의도 유지다.

공통 perception은 챔피언별 예외를 모른다. 적 챔피언, 저체력 적, 미니언, 구조물, 아군 웨이브, 포탑 위험, 거리, 공격 가능 여부, 쿨타임/락 여부를 수집한다.

공통 evidence는 디자이너와 디버거가 볼 수 있어야 한다. 이미 `fChampionDecisionScore`, `fFarmDecisionScore`, `fStructureDecisionScore`, hp ratio, enemy distance, turret danger, available action/skill mask, decision trace가 있으므로 이를 확장 우선 표면으로 삼는다.

공통 intent는 다음 정도로 닫는다.

- `FarmMinion`: 적 챔피언보다 CS/라인 유지가 우선.
- `AttackChampion`: 안전 범위 안에서 견제 또는 교전.
- `ExecuteDive`: 저체력 적 처치가 가능하고 포탑/웨이브 조건이 맞음.
- `SiegeStructure`: 적 챔피언 압박이 없고 웨이브가 구조물 어그로를 받음.
- `Retreat`: 체력/포탑/쿨타임/특수 상태가 위험.
- `Recall`: 라인 이탈 조건이 명확하고 전투가 없음.

공통 safety arbitration은 챔피언 전술보다 먼저 평가한다. 단, Yone E return 같은 "복귀/탈출 반응"은 안전 판단과 같은 레벨의 emergency override로 둔다. 이것은 공격 콤보가 아니라 생존 계약이다.

공통 command emission은 기존 `EmitSkillCommand`, `EmitBasicAttackCommand`, `EmitRetreat` 계열을 재사용한다. 새 전술이 직접 position, hp, cooldown, damage를 바꾸는 코드는 금지한다.

### 1.5 챔피언별 전술/콤보의 본질

챔피언별 전술은 "공통 AI가 고른 의도 안에서, 해당 챔피언답게 한 수를 고르는 함수"다. 지금의 함수 포인터 레지스트리는 첫 형태로 충분하다. 다음 단계에서 파일 분리는 가능하지만, 처음부터 과한 플러그인 시스템을 만들 필요는 없다.

챔피언 전술은 두 층으로 나눈다.

1. Reaction override
   즉시 처리해야 하는 반응이다. 예: Yone E return, Yasuo R airborne target, 방어기, 탈출기.

2. Scored action candidates
   여러 합법 행동 후보를 만들고 점수로 고른다. 예: Q poke, W shield/trade, E engage, R execute, BasicAttack, kite move.

선형 `ChampionAIComboPlan`은 삭제하지 않는다. Jax/Fiora/Ashe/Riven처럼 단순 연계가 충분한 챔피언은 계속 쓸 수 있다. 다만 Yone/Yasuo처럼 상태와 반응이 중요한 챔피언은 선형 콤보만으로 표현하지 않는다.

챔피언 전술 후보의 최소 입력은 다음이다.

- 공통 context: target id, enemy distance, hp ratio, turret danger, wave state, attack range.
- cooldown/learn/action lock: `EmitSkillCommand`와 `CommandExecutor`가 최종 검증.
- champion special state: Yone의 soul state, Yasuo의 Q stage/airborne target 같은 Shared/GameSim 상태.
- designer knobs: profile/tuning/data로 조정할 수 있는 거리, 체력, 점수 가중치, 리턴 임계값.

챔피언 전술 후보의 최소 출력은 하나의 `GameCommand` 또는 no-op이다. 한 decision tick에 여러 스킬을 한꺼번에 밀어 넣지 않는다. 콤보는 action lock과 다음 decision tick을 통해 이어진다.

### 1.6 Yone Bot 목표 설계

요네 Bot의 정체성은 "짧게 찌르고, E로 들어가서 이득을 본 뒤, 위험해지면 반드시 돌아오는 근접 교전형"이다. 구현 목표는 멋진 풀콤보보다 안정적인 전투 왕복이다.

Yone이 읽어야 하는 특별 상태는 `YoneSimComponent` 하나로 충분하다.

- `bSoulUnboundActive`: E로 영혼 상태가 켜져 있는가.
- `bReturning`: 이미 복귀 중인가.
- `soulTimerSec`: 강제 복귀까지 남은 시간.
- `anchorPosition`: 복귀 지점.

Yone의 P0 계약은 `skill.yone.e`가 진짜 2단계 스킬이어야 한다는 점이다.

```text
skill.yone.e stage.count >= 2
skill.yone.e stage.windowSeconds > 0
AI E return command: slot = E, itemId = 2u
CommandExecutor: bRequestedStage2 == true -> bStage2 accepted
YoneGameSim::OnE -> StartSoulReturn
```

Yone reaction override는 다음 조건 중 하나가 맞으면 공격 후보보다 먼저 E return을 발행한다.

- `soulTimerSec <= returnTimerThreshold`
- `selfHpRatio <= returnHpThreshold`
- `bInsideEnemyTurretDanger`
- target이 사라졌거나 untargetable/dead
- `selfHpRatio + margin < enemyHpRatio`
- anchor에서 너무 멀어졌거나 이동 경로가 위험함

현재 구현은 이 방향을 이미 일부 따른다. `TryExecuteYoneChampionCombat`은 `soulTimerSec <= 0.75f`, 낮은 체력, 적 포탑 위험, 타깃 없음, 교전 손해를 E return 조건으로 보고 `cmd.itemId = 2u`를 발행한다. 설계상 다음 개선은 이 상수를 profile/tuning 또는 Yone tactic local constants로 정리하고, trace reason을 남기는 것이다.

Yone action candidates는 다음 우선순위로 설계한다.

- `EReturn`: emergency reaction. 공격 후보보다 항상 우선.
- `RExecute`: 적 체력이 낮거나 soul out 상태에서 킬각이 있고, 라인/방향 조건이 맞을 때.
- `EEngage`: soul out이 아니고, 적이 기본 공격 범위 밖이지만 E 진입 거리 안이며, 복귀 anchor가 안전할 때.
- `QTrade`: Q가 닿으면 짧은 견제. Q3/강화 상태가 코드로 안정 노출되면 knockup 시도 후보를 별도 점수화한다.
- `WTrade`: 근접 교전 중이며 방어/딜 교환 가치가 있을 때.
- `BasicAttack`: 스킬보다 나은 경우, 또는 post-combo BA window가 열렸을 때.
- `Kite/RetreatMove`: 스킬/공격이 불리하거나 action lock 이후 거리 조절이 필요할 때.

Yone 기본 시나리오는 다음 네 개만 먼저 통과시키면 된다.

1. Poke
   적이 가까우면 Q 또는 W 후 기본 공격 가능 여부를 본다. 무리한 E는 쓰지 않는다.

2. Commit
   적이 기본 공격 범위 밖이고 E 진입이 합리적이면 E로 접근한다. 이후 Q/W/BA/R 후보를 tick 단위로 이어간다.

3. Return
   soul out 상태에서 타이머/체력/포탑/타깃 상실/교전 손해 조건이 맞으면 즉시 E return을 발행한다.

4. Execute
   R이 준비되어 있고 적 체력이 낮거나 E 교전 중 마무리 각이 있으면 R을 후보로 올린다. 단, 실제 적중/피해는 YoneGameSim과 CommandExecutor가 검증한다.

요네 콤보는 코드에서 다음처럼 "한 번에 실행되는 스크립트"가 아니라 "상태 기반 후보 선택의 연속"이어야 한다.

```text
E engage
-> next decision after action lock
-> Q or W or BasicAttack
-> next decision
-> R if execute score wins
-> E return if reaction override fires
```

이 방식이면 디자이너는 E 진입 거리, 리턴 체력, 리턴 타이머, R execute 체력, Q/W 선호도 같은 숫자를 조정하고, 개발자는 명령/상태/검증 계약을 유지할 수 있다.

### 1.7 전체 Character Bot으로 확장하는 방식

공통 Character Bot은 챔피언이 늘어도 동일한 pipeline을 유지한다. 챔피언별 코드는 다음 세 가지만 추가한다.

- profile: 선호 거리, scan/leash, 공격성, retreat/reengage, 기본 skill rule.
- tactics: reaction override와 후보 생성/점수화.
- validation scenario: 최소 1개 이상의 deterministic 전투 시나리오.

Yasuo는 Yone 다음으로 같은 구조에 맞춰 정리하기 좋은 대상이다. 이미 레지스트리에 들어 있고, airborne target/R 같은 reaction override 성격이 분명하다.

Jax/Fiora/Ashe/Riven은 기존 `ChampionAIComboPlan`을 유지하면서 후보 점수 체계로 감싸는 쪽이 좋다. 단순 선형 콤보가 잘 맞는 챔피언은 굳이 전술 함수를 크게 만들지 않는다.

난이도와 brainType은 전체 Bot 공통 확장 포인트다. 현재 `difficulty`는 스폰에서 들어오지만 전술 선택에 거의 반영되지 않고, `brainType`은 기본값에 머문다. 다음 단계에서는 난이도별 reaction delay, score margin, risk tolerance, combo continuation chance를 공통 tuning으로 연결한다.

`Decision` brain은 NYPC/Mushroom GameBot류 판단 모델을 붙일 수 있는 자리지만, Winters에 바로 학습/탐색 엔진을 끌어오지 않는다. 먼저 deterministic rule/scorer가 같은 입력/출력 계약으로 안정화되어야 한다.

### 1.8 협업 분리 구조

기획자는 챔피언별 Bot 목표를 문장과 시나리오로 작성한다.

- 예: "요네는 E로 들어간 뒤 체력이 불리하거나 포탑 위험이 생기면 반드시 돌아온다."
- 예: "요네는 체력 50% 이하 적에게 R 마무리를 시도하지만, 포탑 안으로 무리하게 들어가지 않는다."

디자이너는 숫자와 우선순위를 조정한다.

- `ChampionAIProfile`: 선호 거리, scan range, retreat/reengage ratio, aggression/kite.
- `ChampionAI tuning`: decision score margin, turret danger threshold, low hp execute threshold.
- `SkillGameplayDefs.json`: cooldown, range, stage, effect params.
- 추후 Yone tactic tuning: return timer threshold, E engage max delta, R execute threshold.

개발자는 계약과 파이프라인을 유지한다.

- Bot AI는 `GameCommand`만 발행한다.
- `CommandExecutor`가 모든 명령을 검증한다.
- 챔피언 GameSim만 실제 스킬 결과를 만든다.
- Snapshot/Event가 Client visual로 전달된다.
- 하네스가 경계 위반, 데이터 계약, 빌드/시나리오를 검증한다.

디버깅은 세 역할이 같은 evidence를 보게 만든다. `ChampionAIComponent`의 decision score, block reason, last command, trace, available action/skill mask를 먼저 신뢰 가능한 표면으로 확장한다.

### 1.9 단계별 구현 계획

P0. Yone E stage-2 계약 수정

- `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`에서 `skill.yone.e`를 stage-2 recast 가능 상태로 맞춘다.
- generated gameplay definition 갱신이 필요한지 확인한다.
- `Run-BotAiValidation.ps1`의 Yone E contract audit가 PASS가 되어야 한다.

P1. Yone E return 표적 시나리오

- soul out 상태에서 AI가 `itemId = 2u` E return 명령을 발행하는 deterministic 시나리오를 만든다.
- `CommandExecutor`가 stage2를 accept하고 `YoneGameSim::OnE -> StartSoulReturn`까지 이어지는지 확인한다.
- return cue/animation/FX는 서버 이벤트 경로로 1회만 재생되는지 확인한다.

P2. Yone tactics scoring 정리

- 지금의 hard priority를 reaction override + scored candidates로 정리한다.
- E return은 emergency override로 유지한다.
- R/E/Q/W/BA 후보에 reason과 score를 남긴다.
- 처음에는 같은 `ChampionAISystem.cpp` 안에서 작게 정리하고, 동작이 안정된 뒤 파일 분리를 검토한다.

P3. brainType/difficulty wiring

- 스폰 시 `ChampionAIComponent::brainType`을 lobby/debug 설정 또는 난이도 정책으로 연결한다.
- difficulty가 reaction delay, score margin, risk tolerance, combo continuation에 영향을 주게 한다.
- 기본값은 현행 RuleBased 동작을 깨지 않는 쪽으로 둔다.

P4. 챔피언 전술 파일 분리

- `ChampionAISystem.cpp`가 더 커지면 `ChampionAITactics_Yone.cpp`, `ChampionAITactics_Yasuo.cpp` 같은 Shared/GameSim 내부 파일로 분리한다.
- 분리 후에도 공개 계약은 `ChampionCombatTacticsFn` 또는 그에 준하는 작은 인터페이스로 유지한다.
- Engine/Client/UI/DX 의존성은 계속 금지한다.

P5. 다른 Character Bot 확장

- Yasuo: airborne/R/Q stage 반응 override 정리.
- Jax/Fiora/Ashe/Riven: 기존 combo plan을 scored candidate로 감싸고, 불필요한 특수 코드를 줄인다.
- 신규 챔피언: profile + tactic + validation scenario를 필수 작업 단위로 둔다.

P6. 디자이너 조정 표면

- 이미 있는 debug/tuning UI와 snapshot debug fields를 활용한다.
- 챔피언별 tactic knob를 무작정 늘리지 않고, 실제 반복 조정이 필요한 값만 추가한다.
- 문서와 하네스에서 "이 숫자를 바꾸면 어떤 행동이 달라지는지"를 확인 가능하게 한다.

## 2. 검증

정적 검증은 다음을 기본으로 한다.

```powershell
git diff --check
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-BotAiValidation.ps1 -SkipFullPipeline
```

Yone E stage-2 계약을 수정한 뒤에는 `-AllowKnownYoneEContractGap` 없이 contract audit가 PASS해야 한다. 이 audit는 AI가 `cmd.itemId = 2u`를 발행하는지, `CommandExecutor`가 itemId stage-2 계약을 갖는지, `skill.yone.e`가 `stage.count >= 2`와 `stage.windowSeconds > 0`을 만족하는지 본다.

빌드 검증은 작업 범위에 따라 다음 순서로 한다.

- Shared/GameSim 또는 Data 계약 변경: GameSim 관련 target build.
- Server AI/spawn/harness 변경: Server Debug x64 build.
- Client debug UI/FX/animation 확인 변경: Client Debug x64 build.
- Data-driven 변경: `Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug`.

시나리오 검증은 최소 다음 케이스를 포함한다.

- Yone E engage: 적이 기본 공격 범위 밖, E range 안에 있을 때 E out 명령이 발행된다.
- Yone E return by timer: soul out 상태에서 `soulTimerSec`가 임계값 이하이면 E return 명령이 발행되고 stage2가 accept된다.
- Yone E return by danger: enemy turret danger 또는 체력 불리 조건에서 E return이 공격 후보보다 먼저 선택된다.
- Yone no target return: soul out 중 타깃이 사라지면 anchor로 복귀한다.
- Yone R execute: 적이 낮은 체력이고 R이 합법이면 R 후보가 Q/W/BA보다 높은 우선순위를 가진다.
- Regression: Yasuo/Jax/Fiora/Ashe/Riven 기존 Bot 동작이 의도치 않게 멈추지 않는다.

수동 검증은 정상 F5/F9 흐름에서 한다. Bot AI 검증을 위해 roster, map, minion, snapshot, champion, UI, FX 시스템을 숨기는 방식은 사용하지 않는다. 격리가 필요하면 별도 lab/smoke path를 명시한다.

완료 기준은 다음이다.

- Bot AI가 직접 gameplay truth를 변경하지 않는다.
- Yone E return이 `stage2-window`로 거절되지 않는다.
- YoneGameSim의 soul state와 AI 판단이 같은 계약을 사용한다.
- decision trace에서 왜 E/R/Q/W/BA/Retreat이 선택됐는지 확인할 수 있다.
- 기획자는 시나리오 결과를 읽을 수 있고, 디자이너는 숫자를 조정할 위치를 알 수 있고, 개발자는 어떤 빌드/하네스를 돌려야 하는지 안다.

## 다음 작업의 첫 커밋 후보

첫 커밋은 작게 가야 한다.

1. `skill.yone.e` stage-2 data contract 수정.
2. generated data 갱신 여부 확인.
3. `Run-BotAiValidation.ps1 -SkipFullPipeline`로 Yone E contract PASS 확인.
4. 가능하면 Yone E return deterministic scenario 또는 최소 smoke trace 추가.

이 첫 커밋이 지나야 요네 콤보 점수화, brainType/difficulty 연결, 전술 파일 분리를 안전하게 이어갈 수 있다.
