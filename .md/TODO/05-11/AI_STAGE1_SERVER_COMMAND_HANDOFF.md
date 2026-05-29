# AI Stage1 Server Command Handoff

작성일: 2026-05-12
목적: 다음 세션에서 바로 Bot AI Stage1을 끝낼 수 있도록, 현재 서버 권위 챔피언 작업 결과와 AI 입력 생산자 설계를 한 곳에 고정한다.

---

## 2026-05-12 Runtime Result

S10 BotAIStage1은 1차 runtime smoke를 통과했다.

확인된 것:

```text
Jax/Fiora lane meet
-> champion fight
-> minion farm
-> tower aggro-safe behavior
-> BotAI command logs
-> CommandExecutor accept logs
```

이제 이 문서는 "S10 진입 전 handoff"가 아니라 **S10 1차 성공 기준선 + 다음 안정화 handoff**로 본다.

상세 기준선:

```text
.md/TODO/05-11/STAGE1_SERVER_AI_SMOKE_SUCCESS.md
```

다음 최우선:

```text
Death / TargetInvalid / Respawn
```

죽은 entity는 bot/minion/turret/projectile target이 될 수 없다. 이 공통 규칙을 닫은 뒤 AI 판단을 더 똑똑하게 만든다.

---

## Current Sequence

```text
S7_BasicAttackVerticalSlice
-> S8_ServerSkillStandard
-> S9_ChampionClientParityBatch
-> S10_BotAIStage1
```

현재는 S10 1차 smoke pass 이후 안정화로 진입한다.

AI의 첫 목표는 고급 판단이 아니다. 목표는 **사람이 보내던 입력과 같은 `GameCommand`를 서버 내부에서 안정적으로 생산하는 봇**이다.

---

## Goal

```text
Bot reads Server GameSim state
-> chooses one intent
-> emits one GameCommand
-> CDefaultCommandExecutor validates and applies it
-> Server sends Snapshot + Event
-> Client only renders animation / FX
```

절대 기준:

- AI는 `TransformComponent`, `HealthComponent`, `SkillStateComponent`, `MoveTargetComponent`를 직접 gameplay 결과로 고치지 않는다.
- AI가 직접 바꿔도 되는 것은 `BotLaneAIComponent`의 내부 판단 상태와 debug 필드뿐이다.
- 이동, 평타, 스킬, 쿨다운, 피해, 대시, 넉업, 투사체, FX cue는 기존 `CommandExecutor`와 champion GameSim이 처리한다.

---

## Why This Order

챔피언 서버 권위 이식 중 확인된 핵심 교훈은 이것이다.

```text
사람이 조작해도 서버 Command만으로 챔피언이 정상 동작한다.
같은 Command를 AI가 만들어도 완전히 동일하게 동작한다.
```

따라서 AI는 새 전투 시스템이 아니다. AI는 **입력 생산자**다.

이미 챔피언 쪽은 다음 축이 들어와 있다.

- Irelia: server authority golden slice와 gotcha 문서화 완료.
- Kalista: passive dash는 action end/recovery hook, `actionSeq` 기준 1회 latch 필수.
- Yasuo: Q stage 1/2/3/4는 서버가 확정하고, R은 airborne target만 허용.
- Fiora/Yone/Jax/Viego/Ashe: 서버 GameSim component/hook, client VisualHook/FX 연결 1차 반영.
- Debug cooldown: 현재 검증 목적상 `ChampionRuntimeDefaults`의 fast cooldown이 켜져 있으면 스킬 쿨타임이 0.2초로 덮일 수 있다.

이 상태에서 AI를 붙이면 AI는 기존 사람이 만들던 `Move`, `BasicAttack`, `CastSkill` 세 종류만 안정적으로 만들면 된다.

---

## Current Code Map

### AI Entry

```text
Server/Private/Game/GameRoom.cpp
```

검증된 현재 실행 순서:

```text
Tick()
  -> Phase_DrainCommands(tc)
  -> Phase_ServerBotAI(tc)
  -> Phase_ExecuteCommands(tc)
  -> Phase_SimulationSystems(tc)
  -> Phase_BroadcastEvents(tc)
  -> Phase_BroadcastSnapshot(tc)
```

현재 anchor:

```text
Server/Private/Game/GameRoom.cpp:683  Phase_DrainCommands(tc);
Server/Private/Game/GameRoom.cpp:684  Phase_ServerBotAI(tc);
Server/Private/Game/GameRoom.cpp:685  Phase_ExecuteCommands(tc);
Server/Private/Game/GameRoom.cpp:805  void CGameRoom::Phase_ServerBotAI(TickContext& tc)
Server/Private/Game/GameRoom.cpp:810  CBotLaneAISystem::Execute(m_world, tc, m_pendingExecCommands);
```

이 순서는 유지한다. 봇이 만든 command도 사람 command와 같은 `m_pendingExecCommands`를 거쳐 `CDefaultCommandExecutor::ExecuteCommand()`로 들어간다.

### Existing Bot Component

```text
Shared/GameSim/Components/BotLaneAIComponent.h
```

현재 상태:

```text
MoveToLane
FarmMinion
FightChampion
AttackStructure
Dead
```

현재 필드:

```text
champion / team / difficulty / lane
state / laneGoal
lockedChampion / targetMinion / targetStructure
decisionTimer / decisionInterval
scan ranges / leashRange
```

### Existing Bot System

```text
Shared/GameSim/Systems/BotLaneAISystem.h
Shared/GameSim/Systems/BotLaneAISystem.cpp
```

현재 owner:

```text
Shared/GameSim/Systems/BotLaneAISystem.cpp:106
void CBotLaneAISystem::Execute(CWorld& world, const TickContext& tc, std::vector<GameCommand>& outCommands)
```

현재 구조:

```text
FindEnemyChampion
-> cast primary skill if ready/range
-> basic attack if ready/range
-> move toward target
-> else minion farm
-> else structure
-> else lane goal
```

### Command Executor Contract

```text
Shared/GameSim/Systems/ICommandExecutor.h
Shared/GameSim/Systems/CommandExecutor.cpp
```

`GameCommand` 종류:

```text
Move
CastSkill
BasicAttack
LevelSkill
BuyItem
UseItem
Recall
```

중요 anchor:

```text
Shared/GameSim/Systems/CommandExecutor.cpp:791  HandleMove
Shared/GameSim/Systems/CommandExecutor.cpp:832  HandleCastSkill
Shared/GameSim/Systems/CommandExecutor.cpp:917  LogCastSkill("accept"...)
Shared/GameSim/Systems/CommandExecutor.cpp:918  ClearMoveTarget(world, effectiveCmd.issuerEntity);
Shared/GameSim/Systems/CommandExecutor.cpp:1028 HandleBasicAttack
Shared/GameSim/Systems/CommandExecutor.cpp:1086 ClearMoveTarget(world, cmd.issuerEntity);
```

따라서 AI는 command를 만들기 전에 `ClearMoveTarget()`을 직접 부르지 않는다. accepted skill/BA는 executor가 이동 취소를 한다.

### Debug Snapshot / Panel

```text
Server/Private/Game/SnapshotBuilder.cpp
Client/Private/Network/Client/SnapshotApplier.cpp
Client/Private/UI/AIDebugPanel.cpp
```

현재 debug 흐름:

```text
SnapshotBuilder: stateFlags에 BotLaneAI present/state encode
SnapshotApplier: BotLaneAIDebugComponent 갱신
AIDebugPanel: state / targetNet / pos / hp / move / anim 출력
```

현재 anchor:

```text
Server/Private/Game/SnapshotBuilder.cpp:229  if (world.HasComponent<BotLaneAIComponent>(entity))
Client/Private/Network/Client/SnapshotApplier.cpp:420  bHasAIDebug
Client/Private/UI/AIDebugPanel.cpp:52  BotLaneStateName
```

---

## Files Touched In Next Session

### Mandatory

```text
Shared/GameSim/Components/BotLaneAIComponent.h
Shared/GameSim/Systems/BotLaneAISystem.h
Shared/GameSim/Systems/BotLaneAISystem.cpp
Server/Private/Game/GameRoom.cpp
Client/Private/UI/AIDebugPanel.cpp
Server/Private/Game/SnapshotBuilder.cpp
Client/Private/Network/Client/SnapshotApplier.cpp
Server/Include/Server.vcxproj
Server/Include/Server.vcxproj.filters
```

### Optional after Stage1 is stable

```text
Shared/GameSim/AI/BotAIProfile.h
Shared/GameSim/AI/BotAIProfile.cpp
Shared/GameSim/AI/BotPerception.h
Shared/GameSim/AI/BotPerception.cpp
Shared/GameSim/AI/BotDecision.h
Shared/GameSim/AI/BotDecision.cpp
```

권장: 첫 구현은 `BotLaneAISystem.cpp` 안에서 안정화하고, 300~400줄을 넘기면 위 파일로 분리한다.

---

## Stage1 Design

### A1. Bot Component 확장

`BotLaneAIComponent`에 다음 값을 추가한다.

```text
commandSeq:
  bot synthetic sequence number.
  사람이 보내는 network sequence와 충돌하지 않아도 되지만, 로그/디버그용으로 증가시킨다.

lastCommandTick:
  같은 tick 또는 너무 짧은 간격의 command spam 방지.

lastCastSlot:
  최근 사용 스킬 슬롯 기록.

lastTarget:
  champion/minion/structure를 통합한 마지막 타겟.

attackHoldTimer:
  너무 자주 move/attack을 번갈아 보내는 흔들림 방지.

retreatTimer:
  HP 낮을 때 laneGoal 또는 safeGoal로 빠지는 최소 시간.

safeGoal:
  현재 lane의 아군 쪽 안전 지점.

primaryScore fields:
  debug only. championScore/minionScore/structureScore/retreatScore.
```

### A2. Perception

매 decision tick마다 정렬된 deterministic scan을 수행한다.

검색 순서:

```text
1. 내 상태: alive, hpRatio, position, team, champion, cooldowns
2. 적 챔피언: range, hpRatio, distance, airborne/stunned 같은 status 가능하면 추후 추가
3. 적 미니언: range, low hp, distance
4. 적 구조물: range, distance, targetable
5. lane goal / safe goal
```

정렬 규칙:

```text
distanceSq 오름차순
동률이면 EntityID 오름차순
```

이 규칙이 없으면 같은 상황에서 tick마다 target이 흔들린다.

### A3. Decision Priority

Stage1 우선순위:

```text
Dead:
  no command

Low HP retreat:
  Move to safeGoal

Enemy champion in threat range:
  use champion profile skill if ready and useful
  else BA if in range
  else Move toward target only if leash allows

Farm:
  target low hp enemy minion
  BA if in range
  else Move toward minion

Structure:
  BA if in range
  else Move toward structure

Lane:
  Move to laneGoal
```

Stage1에서는 Utility/BT/GOAP를 붙이지 않는다. 대신 점수는 debug용으로만 계산한다.

### A4. Command Arbitration

한 bot은 decision tick당 최대 1개의 command만 emit한다.

Command 생성 규칙:

```text
Move:
  cmd.kind = eCommandKind::Move
  cmd.issuerEntity = self
  cmd.issuedAtTick = tc.tickIndex
  cmd.sequenceNum = ++ai.commandSeq
  cmd.groundPos = desiredPosition

BasicAttack:
  cmd.kind = eCommandKind::BasicAttack
  cmd.issuerEntity = self
  cmd.issuedAtTick = tc.tickIndex
  cmd.sequenceNum = ++ai.commandSeq
  cmd.slot = 0
  cmd.targetEntity = target

CastSkill:
  cmd.kind = eCommandKind::CastSkill
  cmd.issuerEntity = self
  cmd.issuedAtTick = tc.tickIndex
  cmd.sequenceNum = ++ai.commandSeq
  cmd.slot = chosenSlot
  cmd.targetEntity = target or NULL_ENTITY
  cmd.groundPos = target/cursor position
  cmd.direction = normalized XZ direction
```

금지:

```text
AI system에서 ClearMoveTarget 직접 호출 금지
AI system에서 DamageRequest 직접 enqueue 금지
AI system에서 TransformComponent 위치 직접 변경 금지
AI system에서 SkillState cooldown 직접 변경 금지
```

### A5. Champion Profile Stage1

첫 세션은 모든 챔피언을 완성하지 말고, 이미 서버 권위 경로가 있는 챔피언을 우선한다.

권장 우선순위:

```text
1. Ashe
   W poke, BA kite/farm. 안전하고 원거리라 AI smoke에 좋다.

2. Jax
   Q engage, W empower, E defensive/offensive, BA. 근접 전투 smoke에 좋다.

3. Yasuo
   Q stack, E target dash, EQ, R airborne 조건. 콤보 검증용.

4. Irelia
   Q dash reset/approach, E/R visual. Golden slice 회귀 검증용.

5. Viego/Yone/Fiora/Kalista
   이미 1차 sim/visual이 있으나 특수 이동/상태가 많으므로 2차.
```

Profile 함수는 처음에는 switch로 충분하다.

```text
ChoosePrimarySkill(champion, world, self, target):
  Ashe -> W if target in W range
  Jax -> Q if out of BA range but in Q range, W if in BA range, E if surrounded
  Yasuo -> Q if ready, E if Q not ready and target dash allowed, R only if airborne
  Irelia -> Q if ready and target in Q range
  Viego -> Q if ready
  Yone -> Q if ready, W if multiple enemies/close
  Fiora -> Q if ready and target in dash range
  Kalista -> Q if ready and target lined up
```

### A6. Debug Panel

`AIDebugPanel`에는 다음을 추가한다.

```text
state
targetNetID
lastCommand kind/slot/seq
scores: champion/minion/structure/retreat
cooldown summary: BA/Q/W/E/R
position + laneGoal + safeGoal
```

Snapshot bandwidth가 부담되면 처음에는 `stateFlags`와 `ownerNet`만 유지하고, 상세 debug는 server log로 충분하다.

---

## Gotchas From Champion Migration

AI 구현 중 반드시 기억할 것.

### Kalista

- passive dash는 `Move` command가 들어왔을 때 서버가 `KalistaPassiveDashComponent`를 소비한다.
- AI가 Kalista를 조작할 때도 사람처럼 `BasicAttack -> Move` 순서로 command를 만들어야 dash가 발동한다.
- `kalista_attack1_dash_0`와 `kalista_spell1_dash_0`는 자동 반복이 아니다. 같은 `actionSeq`에서 한 번만 재생되어야 한다.

### Yasuo

- Q stage는 서버 `YasuoGameSim::ResolveQVariantStage`가 확정한다.
- stage 3/4가 `NetAnimation.flags >> 12`로 보존되어야 `spell1_wind`, `spell1c`가 재생된다.
- R은 airborne target이 없으면 reject가 정상이다. AI는 R을 누르기 전에 airborne target을 찾거나 그냥 executor reject 로그를 받아들여야 한다.

### Irelia

- Q는 target vector, W/R은 cursor direction 기준.
- W hold/release는 stage1/stage2가 분리되어야 한다.
- AI가 Irelia W stage2를 만들려면 stage window 동안 release command를 따로 내야 한다. Stage1에서는 Irelia W는 보류해도 된다.

### Yone

- E는 영혼 상태/원본 복귀가 있으므로 AI Stage1에서는 Q/W/BA 위주로 시작한다.
- R은 애니메이션 후 돌진/이동이 들어간다. AI가 R을 난사하면 위치 디버깅이 어려워진다.

### Fiora

- Q는 순간이동이 아니라 dash로 유지한다.
- AI Stage1에서는 Q engage + BA만 써도 충분하다.

### Ashe / Viego / Jax

- Ashe E는 FBX가 없고 PNG Hawkshot visual이다.
- Viego는 이번에 client VisualHook/FBX mesh 연결이 추가된 챔피언이다. 최신 Client 빌드 기준으로 확인한다.
- Jax W/R은 BA damage consume 경로가 있으므로 AI가 BA를 잘 만들면 효과 확인이 쉽다.

### Global

- local prediction이 남아 있으면 AI가 command를 빠르게 반복하면서 FX/animation 중복이 훨씬 잘 보인다.
- AI가 command를 만드는 속도는 사람보다 빠르다. `decisionInterval`, `lastCommandTick`, `attackHoldTimer`로 spam을 막는다.
- dead/stale minion을 target으로 잡으면 reject 로그만 쌓인다. target alive check는 command emit 직전에 한 번 더 한다.
- targetNetID와 local EntityID를 혼동하지 않는다. AI는 server world의 `EntityID`를 쓴다. Snapshot/Event만 `NetEntityId`를 쓴다.

---

## Implementation Steps

### Step 0. Start Context

다음 세션 첫 메시지:

```text
먼저 이 문서 읽고 진행:
C:\Users\user\Desktop\Winters\.md\TODO\05-11\AI_STAGE1_SERVER_COMMAND_HANDOFF.md

그리고 아래도 같이 읽기:
C:\Users\user\Desktop\Winters\.md\TODO\05-11\CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md
C:\Users\user\Desktop\Winters\.md\TODO\05-09\ServerAICompletion.md
```

### Step 1. Existing AI rule violation 제거

파일:

```text
Shared/GameSim/Systems/BotLaneAISystem.cpp
```

현재 `ClearMove(world, self)` helper와 호출이 있다. Stage1 원칙상 제거한다.

이유:

```text
CastSkill accept: CommandExecutor.cpp:918에서 ClearMoveTarget 처리
BasicAttack accept: CommandExecutor.cpp:1086에서 ClearMoveTarget 처리
Move accept: CommandExecutor.cpp:791 이후 MoveTarget 처리
```

AI는 command만 생산하고 accepted/rejected 결과 처리는 executor에 맡긴다.

### Step 2. BotLaneAIComponent 확장

파일:

```text
Shared/GameSim/Components/BotLaneAIComponent.h
```

추가 anchor:

```text
f32_t leashRange = 14.f;
```

그 아래에 command/debug/tactical 필드를 추가한다.

### Step 3. Emit command helper 작성

파일:

```text
Shared/GameSim/Systems/BotLaneAISystem.cpp
```

새 helper:

```text
PushMoveCommand(...)
PushBasicAttackCommand(...)
PushCastSkillCommand(...)
```

세 helper는 반드시 `issuedAtTick`과 `sequenceNum`을 채운다.

### Step 4. Deterministic target scan

현재 `FindEnemyChampion`, `FindEnemyMinion`, `FindEnemyStructure`는 best distance 기반이라 괜찮지만, 동률 tie-breaker가 없다.

수정:

```text
if (d < bestSq || (d == bestSq && e < best))
```

float equality가 부담되면 epsilon 비교를 사용한다.

### Step 5. Profile switch 추가

첫 구현은 별도 파일 없이 `BotLaneAISystem.cpp` anonymous namespace에 둔다.

함수:

```text
u8_t ChoosePrimarySkillSlot(CWorld& world, EntityID self, EntityID target, eChampion champion)
bool_t TryBuildChampionSkillCommand(...)
```

Stage1은 `Q/W` 중심으로 제한한다. R은 Yasuo airborne처럼 서버 조건이 분명한 챔피언만 허용한다.

### Step 6. Debug 출력 보강

파일:

```text
Client/Private/UI/AIDebugPanel.cpp
```

`BotLaneStateName`에 새 state가 있으면 추가한다.

처음부터 snapshot payload를 늘리지 말고, state/targetNet/anim/hp가 보이면 충분하다. 상세 score는 server log로 시작해도 된다.

### Step 7. Build

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

### Step 8. Runtime Smoke

권장 smoke:

```text
1 human + 1 bot:
  bot slot champion Ashe
  bot walks laneGoal
  bot targets enemy minion
  bot sends BA
  bot sends W when enemy champion is in range

1 human + Jax bot:
  bot Q engages
  bot BA damage applies

1 human + Yasuo bot:
  bot Q casts stage 1/2/3
  R is not required in Stage1 unless airborne is already present
```

기대 server log:

```text
[BotAI] entity=... state=FarmMinion cmd=BasicAttack target=...
[Command] basic-attack accept issuer=... target=...
[EventApplier] animation play netID=... animID=10 ...

[BotAI] entity=... state=FightChampion cmd=CastSkill slot=...
[Command] cast-skill accept reason=ok champion=...
[EventApplier] fx cue champion=...
```

기대 client:

```text
AI Debug panel:
  bot state changes MoveToLane -> FarmMinion/FightChampion
  Target Net updates
  Anim ID changes on BA/skill
```

---

## Definition Of Done

Stage1 완료 기준:

```text
1. Bot AI가 Transform/HP/Cooldown/MoveTarget을 직접 gameplay 결과로 수정하지 않는다.
2. Bot AI가 `GameCommand`만 emit한다.
3. 사람 command와 bot command가 같은 `CDefaultCommandExecutor`를 통과한다.
4. Ashe bot이 lane move, minion farm, W poke, BA를 한다.
5. Jax 또는 Yasuo bot이 champion target을 상대로 skill/BA를 한다.
6. Client는 Snapshot/Event만 보고 animation/FX를 재생한다.
7. Server/Client Debug x64 빌드가 통과한다.
```

Stage1 이후:

```text
A2_HFSM:
  Laning/Fighting/Retreat/Recall/Objectives 상태를 붙인다.

A3_BT:
  champion별 skill combo를 BT node로 분리한다.

A4_Utility:
  target/retreat/objective score를 실제 의사결정에 사용한다.

A5_Influence:
  미니언/타워/챔피언 위험도를 맵 점수화한다.
```

---

## Final Reminder

AI를 똑똑하게 만들기 전에 먼저 **같은 command를 넣으면 사람과 봇이 같은 결과를 보는가**를 닫는다.

이번 단계에서 잘 만든 AI란 이거다.

```text
멋진 판단을 하는 AI가 아니라,
서버 권위 전투 파이프라인을 망가뜨리지 않고
사람과 같은 입력 경로를 타는 AI.
```
