Session - Champion AI를 현재 server command bot 구조에서 HFSM, Behavior Tree, Blackboard 방향으로 작게 전환한다.

0. 용어 고정

- HFSM: Hierarchical Finite State Machine. 큰 흐름 상태를 유지하고, 각 큰 상태 안에 작은 하위 상태를 둔다.
- Behavior Tree: 한 상태 안에서 "지금 무엇을 먼저 시도할지"를 Selector/Sequence/Leaf로 정리한다.
- Blackboard: AI 판단에 필요한 관측값, 의도, 진행 중인 작업 기억을 공유하는 상태 저장소다.
- 이번 목표는 MCTS/RL 진입이 아니다. 지금 목표는 기획자와 클라 개발자가 읽고 튜닝 가능한 규칙형 AI 골격을 단단하게 만드는 것이다.

1. 반영해야 하는 코드

1-1. 현재 유지할 서버 권위 경계

유지:

```text
Client Input or Debug Override
-> GameCommand
-> Server GameSim
-> Snapshot/Event
-> Client Visual/UI
```

원칙:
- `CChampionAISystem`은 직접 결과를 적용하지 않고 서버 `GameCommand`를 생성한다.
- 챔피언 스킬 결과, 피해, 이동, FX cue는 기존 Server GameSim/CommandExecutor/GameplayHook 경로를 따른다.
- 클라이언트는 AI 의사결정 소유자가 아니라 디버그 표시, override 입력, 시각 재생 담당이다.

1-2. 현재 FSM을 HFSM으로 해석하는 1차 목표

현재 root state:

```text
MoveToOuterTurret
WaitForWave
LaneCombat
Retreat
Recalling
Dead
```

HFSM 1차 해석:

```text
ChampionAI
├─ LaneSetup
│  ├─ MoveToOuterTurret
│  └─ WaitForWave
├─ LaneCombat
│  ├─ FarmMinion
│  ├─ HarassChampion
│  ├─ ContinueChampionCombo
│  ├─ SiegeStructure
│  └─ FollowWave
├─ Recovery
│  ├─ Retreat
│  └─ Recalling
└─ Dead
```

1차 구현 방향:
- 별도 범용 `CHFSM` 런타임은 아직 만들지 않는다.
- 기존 `eChampionAIState`를 root state로 유지한다.
- 기존 `eChampionAIIntent`를 LaneCombat 하위 상태처럼 사용한다.
- 상태 전이와 intent 변경은 `ExecuteLaneCombat`, `ExecuteRetreat`, `ExecuteRecalling` 같은 task 단위 함수에서 명시한다.

1-3. ChampionAIComponent를 개인 Blackboard로 정리

현재 `ChampionAIComponent`는 이미 개인 Blackboard 역할을 하고 있다.

개인 Blackboard로 보는 필드:

```text
state
intent
lastAction
lockedChampion
targetMinion
targetStructure
alliedWave
comboTarget
comboStep
decisionTimer
intentHoldTimer
lastDecisionRoll
debugAvailableActionMask
debugAvailableSkillMask
```

1차 구현 방향:
- 지금은 별도 `BlackboardComponent`를 추가하지 않는다.
- `ChampionAIComponent`를 `Champion AI personal blackboard`로 간주하고 이름/정리 함수만 늘린다.
- 팀 단위 Blackboard는 objective, gank, group, vision 판단이 들어오는 시점까지 미룬다.

우선 추가 후보 함수:

```cpp
void ClearChampionAICombatMemory(ChampionAIComponent& ai);
void CompleteChampionAICombo(ChampionAIComponent& ai);
void SetChampionAIIntent(ChampionAIComponent& ai, eChampionAIIntent intent);
```

`CompleteChampionAICombo`의 필수 역할:

```text
comboTarget = NULL_ENTITY
comboStep = 0
intent = FarmMinion
intentHoldTimer = intentHoldDuration
```

이 함수가 들어가야 AttackChampion 10% 선택 이후 combo가 끝났을 때 바로 다시 Harass intent를 물고 재진입하는 일을 줄일 수 있다.

1-4. LaneCombat 내부를 작은 Behavior Tree로 정리

현재 `ExecuteLaneCombat`은 이미 우선순위 Selector처럼 동작한다.

목표 BT 해석:

```text
LaneCombatSelector
├─ EmergencyRetreat
├─ ContinueActiveChampionCombo
├─ AttackStructureIfWaveTanking
├─ StartChampionHarassByRoll
├─ FarmMinion
└─ FollowWave
```

각 leaf 규칙:
- 한 tick에 최대 하나의 `GameCommand`만 낸다.
- `Success`: 명령을 냈거나 해당 행동을 유지하기로 했다.
- `Failure`: 조건이 맞지 않아 다음 leaf로 넘긴다.
- `Running`: 진행 중인 행동을 잡고 있으며 Farm으로 넘기면 안 된다.

특히 `ContinueActiveChampionCombo` 규칙:

```text
if comboTarget is valid and CanHarassChampion:
    if current combo step can be emitted:
        emit step
        if combo finished:
            CompleteChampionAICombo()
        return Success
    else:
        return Running
```

`Running`을 반환하는 이유:
- 콤보 step 사이에 basic attack lock, skill cooldown, 거리 조정이 생길 수 있다.
- 이 틱에 스킬이 안 나갔다고 FarmMinion으로 떨어지면 콤보 체감이 깨진다.

1-5. 챔피언별 ComboPlan은 아직 C++ 데이터로 유지

현재 유지:

```cpp
const ChampionAIComboPlan& GetChampionAIComboPlan(eChampion champion);
```

유지 이유:
- Jax/Fiora/Ashe/Riven 단계에서는 스킬 순서와 거리 조건이 작고 명확하다.
- JSON/Lua/BT asset으로 빼기 전에 서버 GameSim 체감과 로그 검증이 먼저다.
- 기획자 튜닝은 우선 ImGui debug override와 C++ 상수 조정으로 충분하다.

나중에 BT로 승격할 때의 leaf:

```text
RunChampionComboPlan(champion, target)
```

이 leaf 내부는 현재 `TryEmitAttackChampionCombo`를 감싼다.

1-6. 팀 Blackboard는 나중 단계

팀 Blackboard 도입 조건:
- 라인별 help 요청
- 정글 gank/카운터 gank
- 드래곤/바론/타워 objective
- 여러 봇의 focus target 공유
- missing enemy/last known position 공유

초기 형태:

```text
TeamBlackboardResource
├─ Blue
│  ├─ FocusTarget
│  ├─ RequestedAssistLane
│  ├─ CurrentObjective
│  └─ SafeRetreatAnchor
└─ Red
   ├─ FocusTarget
   ├─ RequestedAssistLane
   ├─ CurrentObjective
   └─ SafeRetreatAnchor
```

보류 이유:
- 지금은 1:1 lane combat과 champion combo 체감이 먼저다.
- 팀 Blackboard를 너무 빨리 넣으면 문제 원인이 개인 AI인지 팀 의사결정인지 흐려진다.

1-7. 이번 단계에서 하지 않는 것

하지 않음:
- 범용 BT runtime 대공사
- BT JSON/Lua asset 파서
- GOAP
- MCTS
- RL
- champion combo를 전부 데이터 파일로 이전
- 클라이언트가 AI 결과를 직접 적용하는 구조

2. 검증

미검증:
- HFSM/BT/Blackboard 전환은 아직 문서 계획 단계다.
- `CompleteChampionAICombo` 함수는 코드에 반영되었지만 런타임 체감은 아직 미검증이다.
- 팀 Blackboard는 아직 코드에 반영하지 않는다.

검증 명령:
- `git diff --check`
- `msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- AttackChampion debug override 이후 combo가 끝나기 전 FarmMinion으로 떨어지지 않는지 확인.
- combo 마지막 step 이후 `intent=FarmMinion`, `comboTarget=NULL_ENTITY`, `comboStep=0`으로 정리되는지 확인.
- LaneCombat 로그에서 `ContinueActiveChampionCombo -> FarmMinion` 흐름이 보이는지 확인.
- Retreat/Recalling은 combo보다 우선되어 위험 상황에서 콤보를 끊는지 확인.

다음 구현 순서:

```text
1. CompleteChampionAICombo 추가
2. TryEmitAttackChampionCombo 마지막 step에서 CompleteChampionAICombo 호출
3. ExecuteLaneCombat을 leaf 함수 단위로 이름 정리
4. ChampionAIComponent 필드를 개인 Blackboard로 문서화/디버그 표시
5. 필요해질 때 TeamBlackboardResource 도입
```
