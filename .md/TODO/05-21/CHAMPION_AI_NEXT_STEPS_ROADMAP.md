Session - Champion AI combo/HFSM 1차 빌드 통과 이후 다음 진행 순서를 단계별로 고정한다.

1. 반영해야 하는 코드

1-1. 현재 완료된 1차 Gate

대상 문서:
- C:/Users/user/Desktop/Winters/.md/TODO/05-21/CHAMPION_AI_HFSM_BT_BLACKBOARD_CODE_PLAN.md
- C:/Users/user/Desktop/Winters/.md/TODO/05-21/CHAMPION_ATTACK_COMBO_PIPELINE_PLAN.md

현재 반영 상태:
- `CompleteChampionAICombo`가 추가되어 combo 마지막 step 이후 `FarmMinion` intent로 복귀한다.
- `TryEmitAttackChampionCombo`는 active combo 중 current step을 못 써도 Farm leaf로 넘기지 않는다.
- `ExecuteLaneCombat`은 작은 Behavior Tree selector처럼 leaf helper 단위로 분리되었다.
- Jax/Fiora/Ashe/Riven combo plan은 `ChampionAIPolicy` 중심으로 들어가 있다.
- Riven E away 같은 방향성 step을 위해 `eChampionAIComboTargetMode`가 들어가 있다.

현재 LaneCombat 우선순위:

```text
EmergencyRetreat
-> ContinueActiveChampionCombo
-> AttackStructureIfWaveTanking
-> StartChampionHarassByRoll
-> FarmMinion
-> FollowWave
```

이미 확인된 빌드 Gate:
- `git diff --check` 통과.
- `Server/Include/Server.vcxproj` Debug x64 빌드 통과.
- `Client/Include/Client.vcxproj` Debug x64 빌드 통과.

남은 확인:
- F5 런타임에서 AI Debug override와 Server 로그로 실제 체감 확인이 필요하다.
- combo 완료 이후 `lane-attack-minion-*` 또는 `lane-follow-wave`로 복귀하는지 수동 확인이 필요하다.

1-2. 다음 1순위 - Jax 기준 런타임 스모크

목표:
- 지금 코드가 목표한 파이프라인을 실제 런타임에서 만족하는지 먼저 증명한다.
- 새로운 챔피언을 더 만지기 전에 Jax 하나로 기준선을 고정한다.

확인할 파이프라인:

```text
LaneCombat
-> AttackChampion 10% roll 또는 debug override
-> Jax combo step 진행
-> combo 완료
-> FarmMinion 또는 FollowWave 복귀
-> 위험 조건이면 Retreat/Recalling 우선
```

확인할 로그:
- `reason=combo-attack-champion-skill`
- `reason=combo-attack-champion-ba`
- `reason=lane-attack-minion-ba`
- `reason=lane-follow-wave`
- `reason=retreat-*`

주의:
- combo 중간에 FarmMinion으로 새면 안 된다.
- combo가 끝난 뒤에는 계속 AttackChampion에 묶여 있으면 안 된다.
- Retreat/Recalling은 combo보다 우선해야 한다.

1-3. 다음 2순위 - AI Debug에 combo 상태 표시

목표:
- 기획자/클라 개발자가 "AI가 지금 왜 멈췄는지, 몇 번째 combo step인지"를 눈으로 볼 수 있게 한다.
- Server 로그만 보고 추측하는 시간을 줄인다.

추가 후보:

```text
ChampionAIDebugComponent
├─ comboTargetNetId
├─ comboStep
└─ comboStepCount
```

UI 표시 후보:

```text
State / Intent / Action / TargetNet
ComboTargetNet / ComboStep / ComboStepCount
AvailableActions / AvailableSkills
```

주의:
- Debug 표시만 추가한다.
- AI 판단 자체를 클라이언트 UI로 옮기지 않는다.
- Snapshot/Event 또는 debug replication 경로에 새 필드가 필요한지 먼저 확인한다.

CONFIRM_NEEDED:
- `ChampionAIDebugComponent`가 어디서 채워지고 클라이언트로 어떻게 전달되는지 확인 필요.
- `Client/Private/UI/AIDebugPanel.cpp`가 debug component를 어떤 경로로 읽는지 확인 필요.

1-4. 다음 3순위 - Fiora/Ashe/Riven 런타임 체감 검증

목표:
- Jax 기준선이 맞으면 Fiora, Ashe, Riven을 같은 기준으로 한 명씩 검증한다.
- combo command 순서와 실제 GameSim 체감을 분리해서 본다.

챔피언별 확인:

```text
Fiora
-> Q target direction
-> E
-> BasicAttack
-> BasicAttack
-> W

Ashe
-> W
-> Q
-> BasicAttack
-> BasicAttack
-> BasicAttack

Riven
-> Q
-> BasicAttack
-> Q
-> BasicAttack
-> Q
-> W
-> E away from target
```

주의:
- `ChampionAIPolicy`는 combo step 순서와 조건만 가진다.
- 실제 피해, dash, shield, stun, cooldown 정책은 Champion GameSim에서 확인해야 한다.
- Riven은 command 순서가 맞아도 전용 GameSim 체감이 부족할 수 있으므로 따로 기록한다.

1-5. 다음 4순위 - Riven server GameSim 보강 여부 결정

현재 리스크:
- Riven combo plan은 들어가 있지만 Riven 전용 server GameSim 체감은 약할 수 있다.
- Q 반복, W 근접 효과, E away dash/shield는 generic skill path만으로 부족할 가능성이 있다.

목표:
- Riven이 단순히 command만 찍히는 상태인지, 서버 권위 체감까지 있는 상태인지 구분한다.
- 부족하면 별도 Riven server skill slice로 분리한다.

범위 후보:
- `Shared/GameSim/Champions/RivenGameSim.h`
- `Shared/GameSim/Champions/RivenGameSim.cpp`
- Riven Q dash/stage
- Riven W 근접 타격 또는 stun placeholder
- Riven E away dash/shield placeholder
- `Server.vcxproj`, `Client.vcxproj` shared compile 등록 여부

주의:
- 클라이언트 `Client/Private/GameObject/Champion/Riven/*` legacy visual hook을 서버 결과로 착각하지 않는다.
- 서버 GameSim이 gameplay truth를 소유하고, 클라이언트는 cue/visual만 재생한다.

CONFIRM_NEEDED:
- Riven 전용 server gameplay hook ID가 이미 등록 가능한지 확인 필요.
- Riven Q stage/cooldown 정책을 현재 skill runtime과 어떻게 맞출지 확인 필요.

1-6. 다음 5순위 - combo plan 안정화 데이터 최소 확장

목표:
- 실제 체감에서 현재 step 필드로 부족한 경우에만 데이터를 추가한다.
- 공통 AI는 combo 내부 디테일을 계속 모르게 유지한다.

유지할 경계:

```text
ChampionAISystem
-> 언제 싸울지, 언제 멈출지, 언제 Farm으로 복귀할지 결정

ChampionAIPolicy
-> 챔피언별 combo step 데이터 제공

Champion GameSim
-> 실제 스킬 효과, 피해, 이동, 상태 적용
```

필요할 때만 추가할 후보:

```text
stepWaitPolicy
maxHoldSec
resetOnOutOfRange
resetOnTargetLost
```

주의:
- 지금 당장 추가하지 않는다.
- Jax/Fiora/Ashe/Riven 체감 검증에서 명확한 문제가 나올 때만 추가한다.

1-7. 다음 6순위 - ImGui 튜닝 패널 확장

목표:
- JSON/Lua로 빼기 전에 C++ 데이터와 runtime state를 눈으로 튜닝한다.
- 저장 가능한 데이터 소스로 옮길 값과 단순 debug 값을 구분한다.

우선 표시:
- `attackChampionChance`
- `intentHoldDuration`
- `decisionInterval`
- `comboStep`
- `comboTarget`
- `retreatHpRatio`
- `reengageHpRatio`

우선 조작:
- AttackChampion 강제
- FarmMinion 강제
- Retreat 강제
- combo reset
- champion별 combo step preview

주의:
- ImGui 조작은 디버그/튜닝이다.
- 서버 권위 gameplay truth를 클라이언트 UI가 직접 바꾸는 구조로 만들지 않는다.

1-8. 다음 7순위 - 데이터 외부화 판단

아직 보류:
- JSON
- Lua
- BT asset
- FlatBuffers AI table

도입 조건:
- 챔피언 combo가 8명 이상으로 늘어난다.
- 기획자가 C++ 수정 없이 수치를 자주 바꿔야 한다.
- ImGui에서 바꾼 값 저장/로드가 필요해진다.
- combo step에 condition/action 종류가 늘어나 C++ static plan이 읽기 어려워진다.

추천 순서:

```text
C++ constexpr plan
-> ImGui runtime tuner
-> JSON or FlatBuffers data table
-> 작은 BT asset
-> 필요 시 Lua
```

판단:
- Lua는 행동 자체를 스크립트로 바꿔야 할 때까지 미룬다.
- JSON/FlatBuffers는 수치와 순서 데이터를 빼야 할 때 먼저 검토한다.

1-9. 다음 8순위 - Team Blackboard 도입

도입 조건:
- lane help 요청
- jungle gank/counter gank
- focus target 공유
- objective 판단
- missing enemy / last known position 공유

초기 목표:

```text
TeamBlackboardResource
├─ FocusTarget
├─ RequestedAssistLane
├─ CurrentObjective
├─ SafeRetreatAnchor
└─ LastKnownEnemyPositions
```

주의:
- 지금은 개인 AI combo 체감이 먼저다.
- 팀 Blackboard를 너무 빨리 넣으면 디버깅 원인이 개인 AI인지 팀 판단인지 흐려진다.

1-10. 다음 9순위 - Tiny BT runtime 검토

도입 조건:
- `ExecuteLaneCombat` leaf 함수가 10개 이상으로 늘어난다.
- champion별 특수 condition/action이 공통 if-chain을 오염시킨다.
- debug panel에서 BT node별 Success/Failure/Running을 보고 싶어진다.

초기 형태:

```text
BTStatus
BTContext
BTLeaf function pointer table
Selector tick helper
```

하지 않을 것:
- 대형 editor
- JSON BT parser
- decorator full set
- parallel node

1-11. 당분간 하지 않는 것

보류:
- GOAP
- MCTS
- RL
- imitation learning
- full team macro strategy
- 클라이언트 주도 AI 판단

이유:
- 현재 목표는 "챔피언별 combo 체감 + 공통 LaneCombat 안정화"다.
- 고급 AI는 관측/로그/재현성이 충분히 쌓인 뒤에 들어간다.

2. 검증

현재 단계 검증 명령:
- `git diff --check`
- `msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64`

다음 수동 확인 순서:
- Jax로 AttackChampion override를 눌러 combo가 끝까지 이어지는지 확인.
- combo 완료 뒤 FarmMinion 또는 FollowWave로 복귀하는지 확인.
- Retreat 조건에서 combo가 즉시 끊기는지 확인.
- Fiora combo command 순서와 체감을 확인.
- Ashe combo command 순서와 체감을 확인.
- Riven combo command 순서와 체감을 확인하되, server GameSim 부족분은 별도 기록.

다음 세션 시작 체크리스트:
- 현재 dirty diff가 의도한 AI/doc 변경만 포함하는지 확인.
- AI Debug 패널에서 `State / Intent / Action`이 기대 흐름과 맞는지 확인.
- Server 로그에서 `reason=combo-attack-champion-*`와 `lane-attack-minion-*` 복귀 로그를 확인.
- AI Debug combo field 추가 여부를 먼저 결정.
- Riven server GameSim 보강 여부를 Jax/Fiora/Ashe 검증 뒤 결정.

성공 기준:
- 공통 AI 로직과 개별 champion combo plan의 경계가 유지된다.
- champion별 combo 추가가 `ChampionAIPolicy` 중심으로 가능하다.
- `ChampionAISystem`은 combo 내부 디테일보다 state/intent/BT leaf 흐름에 집중한다.
- 런타임에서 combo 진행 중 FarmMinion으로 새지 않고, combo 완료 뒤에는 Farm/FollowWave로 복귀한다.
