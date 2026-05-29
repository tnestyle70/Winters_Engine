# Server Authority Completion Plan

작성일: 2026-05-09
최종 갱신: 2026-05-12
목적: AI 고도화 전에 서버 권위 GameSim으로 이전 Client 단독 전투감을 완전히 복구하고, 5클라 안정화와 Bot AI Stage1 진입 조건을 고정한다.

---

## 0. 현재 결론

이 작업은 선택이 아니라 온라인 게임 출시를 위한 필수 구조 전환이다.

LoL, Fall Guys, PUBG 같은 상용 온라인 게임은 세부 구현은 다르지만 큰 원칙은 같다.

```text
Client:
  input, prediction, interpolation, rendering, animation, fx, ui

Server / host authority:
  movement truth, hp, cooldown, damage, hit validation, projectile, object state, game end
```

Winters의 목표도 동일하다.

```text
Client Input
-> GameCommand
-> Server GameSim
-> Snapshot/Event
-> Client Visual
```

따라서 지금 목표는 "새 기능 추가"가 아니라 **이전 Client-only 상태를 Server-authoritative 구조 위에서 그대로 복구**하는 것이다.

AI는 이 구조가 안정된 뒤에 붙인다. Bot AI도 Transform/HP/Cooldown을 직접 고치지 않고 사람과 같은 `GameCommand`만 생성해야 한다.

### 2026-05-12 최신 런타임 결론

S10 BotAIStage1은 1차 smoke를 통과했다.

검증된 기준선:

```text
Bot AI
-> GameCommand
-> CDefaultCommandExecutor
-> Server GameSim
-> Snapshot/Event
-> Client Visual
```

확인된 장면:

```text
Jax/Fiora lane meet
-> champion fight
-> minion farm
-> tower aggro-safe behavior
```

시작 시점 버그도 닫았다. 서버 InGame tick은 client가 실제 `Scene_InGame::OnEnter()`와 bootstrap 진입을 마친 뒤 ready를 보내는 기준으로 맞춘다.

치명적인 미니언 버그도 닫았다. `FindClosestEnemyCombatTarget`에서 range 밖 후보를 버리기 전에 priority 비교를 적용하면 같은 lane 적 미니언을 맵 전역에서 잡아 top/bot wave가 mid 쪽으로 빨려 들어간다. 모든 combat target scan은 **range reject -> priority/distance compare** 순서를 지킨다.

다음 1순위는 Death / TargetInvalid / Respawn이다.

---

## 1. 절대 규칙

### 서버 소유

- 위치, 이동, yaw, 이동 목적지
- HP, 마나, 쿨다운, 데미지
- 기본 공격 판정, 스킬 판정, projectile/hit 판정
- 포탑/억제기/넥서스 위치, HP, 공격, 파괴
- 미니언 웨이브, 라인 웨이포인트, 타겟 우선순위
- 봇 AI 판단과 봇 Command 생성
- 게임 종료 조건

### 클라이언트 소유

- 입력 수집과 Command 송신
- 약한 prediction, snapshot interpolation
- animation cue 재생
- fx cue 재생
- UI, ImGui, debug trace

### 금지

- 클라이언트가 최종 데미지/쿨타임/사거리/히트 결과를 결정하면 안 된다.
- 서버 권위 모드에서 local prediction FX와 server cue FX가 동시에 같은 효과를 두 번 만들면 안 된다.
- 서버 GameSim과 Client SkillTable의 값이 장기적으로 갈라지면 안 된다.

---

## 2. 전체 진행 순서

현재 S 진행 순서는 아래로 고정한다.

```text
S1_GameplayDataSingleSource:
  SkillTable / ChampionRuntimeDefaults / hook id / timing 값을 하나의 권위 데이터로 맞춘다.

S2_LoadingBarrier:
  모든 클라 preload 완료 후 서버 InGame tick 시작.

S3_StageStructureBinding:
  서버 구조물 netID와 클라 Stage 구조물 visual 연결.

S4_MovementVerticalSlice:
  우클릭 이동, snapshot 위치 복제, remote locomotion 안정화.

S5_AnimationReplicationSingleSource:
  animation은 actionSeq/playbackRate/duration 기준 1회만 재생.
  run -> skill -> skill_to_run -> run 복귀 포함.

S6_FxCueSingleSource:
  Fx는 서버 EffectTrigger cue 기준 1회만 재생.

S7_BasicAttackVerticalSlice:
  BA 명령, chase, attack windup, hit, projectile/fx, combat stance 복구.

S8_ServerSkillStandard:
  Ezreal Q 하나를 기준으로 서버 스킬 표준 완성.

S9_ChampionClientParityBatch:
  Irelia/Kalista/Zed 포함 모든 챔피언을 이전 Client-only 시각/조작감 수준으로 복구.

S10_BotAIStage1:
  봇을 마지막에 command generator로 붙인다.
```

구현 코드 제시나 직접 반영 전에는 반드시 현재 S 위치와 남은 계획을 먼저 보여준다.

---

## 3. 현재 판정

### 완료 또는 1차 완료

- S2 LoadingBarrier: 1차 완료. `Starting -> MatchLoading -> SetReady -> GameStart -> InGame` 흐름 확인.
- S2 LoadingBarrier 2차 수정: server ready는 client local InGame bootstrap 이후 전송한다. 봇/미니언이 human 플레이 가능 시점보다 수십 초 먼저 진행하는 회귀를 막는다.
- S3 StageStructureBinding: 완료. `structure visual bind` 로그 확인.
- S4 MovementVerticalSlice: 1차 완료. Ezreal/Irelia 이동 동기화 확인.
- S5 AnimationReplicationSingleSource: 1차 완료. 단, 서버 timing/playbackRate와 실제 FBX 길이 정렬은 추가 필요.
- S6 FxCueSingleSource: 1차 완료. Ezreal/Irelia 일부 cue 검증.
- 미니언 visual bind: 1차 수복. 미니언이 보이고 snapshot remove도 확인.
- S10 BotAIStage1 1차 smoke: Jax/Fiora/Ashe command generation, champion fight, minion farming, tower aggro-safe behavior 확인.
- Minion lane target bug: 같은 lane enemy를 맵 전역에서 잡던 target scan range gate 순서 오류 수정.

### 아직 닫히지 않은 핵심 문제

1. **데이터 원천 분리**
   - Client `SkillTable.cpp`와 Server `ChampionRuntimeDefaults.cpp`가 따로 값을 가진다.
   - 검증용 fast timing이 켜져 있으면 서버 쿨타임/락 시간이 기존 튜닝값을 덮는다.
   - 결과: 애니메이션 재생 속도, 스킬 lock, BA 타이밍이 이전 Client-only 감각과 달라진다.

2. **제드 Q/W/E visual parity 미완**
   - 현재 Zed Q/W/E는 기존 클라 구현이 아니라 PNG billboard/decal stub에 가깝다.
   - Q는 direction projectile이 아니라 owner attach effect에 가깝고, W는 shadow clone이 아니며, E는 shadow 연계 slash가 아니다.

3. **칼리스타 BA/이동/투사체 타이밍 미완**
   - 기본 공격 후 projectile/fbx 생성 지연이 크다.
   - 공격 명령 이후 우클릭 이동/전투태세 복귀가 서버 action state와 맞지 않는다.

4. **이렐리아 W hold/release와 E visual 위치**
   - W는 press 상태에서 stage1 hold가 유지되고 release 때 stage2가 나가야 한다.
   - E blade/fbx는 지형에 반쯤 묻히지 않도록 visual lift/scale 기준을 정리해야 한다.

5. **미니언 전투 애니메이션**
   - 미니언 이동/스폰/삭제는 일부 수복됐지만 공격 중 attack animation이 아니라 idle/run처럼 보이는 구간이 남아 있다.

6. **BA/chase/combat stance**
   - 우클릭 적 대상이 사거리 밖이면 chase/move가 되어야 하고, 사거리 안이면 BA command가 되어야 한다.
   - 서버가 BA actionSeq를 내려주면 모든 클라에서 BA 애니, projectile/fx, hit, recovery가 같은 순서로 보여야 한다.

7. **Death / TargetInvalid / Respawn**
   - 죽은 챔피언/미니언/구조물을 bot/minion/turret/projectile이 계속 target으로 잡으면 안 된다.
   - projectile hit 직전 target alive/stale 검증이 필요하다.
   - 미니언 death/despawn과 챔피언 death/respawn이 서버 권위로 정리되어야 한다.

---

## 4. 오늘 우선순위

2026-05-12 기준으로 S10 1차 smoke는 통과했다. 이제 우선순위는 "더 똑똑한 AI"가 아니라 첫 기준선을 깨지 않게 하는 생명주기 안정화다.

```text
T1. Death state 확정
T2. TargetInvalid 공통 helper
T3. Projectile stale target 무효화
T4. Minion death/despawn
T5. Champion death/respawn
T6. Tower aggro refinement
T7. 최소 한 판 루프: tower destroy -> nexus destroy -> room reset/result
```

T1~T5가 닫히기 전에는 HFSM/BT/Utility를 붙이지 않는다. 죽은 대상 처리와 respawn이 불안정하면 AI 판단 고도화가 전부 잘못된 target 위에 쌓인다.

---

## 5. 구현 게이트

### Gate A: Data Single Source

목표:

- 서버가 스킬 쿨타임, range, lockDuration, playbackRate를 임시 하드코딩으로 덮지 않는다.
- 최소한 `ChampionRuntimeDefaults`가 Client SkillDef와 같은 값을 갖는다.
- fast verification timing은 명시적 debug flag일 때만 켠다.

검증:

```text
Zed Q/W/E/R cooldown과 lock이 SkillTable 기준과 일치
Irelia W hold window 유지
Kalista BA windup/recovery가 의도 값과 일치
```

### Gate B: Animation Single Source

목표:

- 서버 `animID`, `actionSeq`, `playbackRateQ8`, `flags`, `duration`이 클라 애니의 단일 원천이 된다.
- 클라 local skill dispatch는 command만 보내고, animation/fx는 snapshot/event로만 재생한다.
- `run -> skill -> transition -> idle/run` 복귀가 모든 원격/로컬 클라에서 같다.

검증:

```text
run -> Q -> Q_to_run -> run
run -> W hold -> W release -> recovery
BA -> recovery -> combat/idle/run
```

### Gate C: FX Cue Single Source

목표:

- 서버 `EffectTrigger`가 모든 FX의 단일 시작점이다.
- generic fallback과 champion visual hook이 겹치지 않는다.
- Zed/Kalista/Irelia/Ezreal을 먼저 완성한 뒤 나머지 챔피언에 같은 패턴을 적용한다.

검증:

```text
[EventApplier] fx cue champion=... sourceNet=... effectID=... slot=...
한 번의 스킬 입력에 FX 1회
로컬/원격 클라에서 같은 위치, 방향, 수명
```

### Gate D: Basic Attack Vertical Slice

목표:

- 사거리 밖 우클릭: chase/move command
- 사거리 안 우클릭: basic attack command
- 서버: windup, hit frame, projectile/hit, cooldown 판정
- 클라: BA anim, projectile/fx, hit feedback, recovery/combat stance

검증:

```text
Irelia BA minion
Kalista BA minion
Zed BA minion
원격 클라에서 같은 BA 애니/타격/삭제
```

### Gate E: Minion Combat

목표:

- 미니언은 spawn, lane move, separation, target acquire, attack, death, remove가 모두 서버 기준으로 동작한다.
- 클라는 minion state snapshot을 보고 run/attack/death animation만 재생한다.

검증:

```text
미니언이 세로 라인으로 전진
충돌 보정으로 겹치지 않음
공격 중 attack animation 재생
죽은 미니언은 death 후 stale remove
```

---

## 6. S10 Bot AI Stage1 조건

아래 조건은 S10 진입 게이트였다. 2026-05-12 1차 smoke에서 command generator 기준선은 통과했다.

- 3클라 이상에서 local/remote champion 이동, BA, 스킬 FX가 동기화된다.
- Zed/Kalista/Irelia/Ezreal 중 최소 3챔프가 서버 cue 기반으로 전투 가능하다.
- 미니언 이동/공격/죽음/삭제가 보인다.
- out-of-range BA가 chase로 이어지고, in-range BA가 attack으로 이어진다.
- 서버가 봇의 Transform/HP를 직접 순간이동/직접수정하지 않고 `GameCommand`만 생성할 준비가 되어 있다.

S10 Stage1의 첫 목표는 여전히 고급 AI가 아니다.

```text
Spawn
-> lane gather
-> nearest enemy minion target
-> in range: BA command
-> out of range: Move command
-> attacked by champion: retaliate if simple condition passes
```

BT/GOAP/Utility/MCTS/RL은 이 루프가 안정화된 뒤에 붙인다.

---

## 7. 주요 코드 지도

### Shared / Server

```text
Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp
Shared/GameSim/Definitions/ChampionRuntimeDefaults.h
Shared/GameSim/Systems/CommandExecutor.cpp
Shared/GameSim/Systems/MoveSystem.cpp
Shared/GameSim/Systems/SkillCooldownSystem.cpp
Server/Private/Game/GameRoom.cpp
Server/Private/Game/SnapshotBuilder.cpp
```

### Client gameplay / network

```text
Client/Private/GameObject/SkillTable.cpp
Client/Private/GamePlay/SkillRegistry.cpp
Client/Private/Scene/InGameCombatInputBridge.cpp
Client/Private/Scene/InGameSkillDispatchBridge.cpp
Client/Private/Scene/InGamePlayerControlBridge.cpp
Client/Private/Scene/Scene_InGame.cpp
Client/Private/Network/Client/SnapshotApplier.cpp
Client/Private/Network/Client/EventApplier.cpp
```

### Champion visual parity

```text
Client/Private/GameObject/Champion/Irelia/
Client/Private/GameObject/Champion/Kalista/
Client/Private/GameObject/Champion/Zed/
Client/Private/GameObject/Champion/Ezreal/
Client/Private/GameObject/Champion/*/*_Registration.cpp
Client/Public/GamePlay/VisualHookRegistry.h
```

### Minion

```text
Client/Private/Manager/Minion_Manager.cpp
Client/Public/Manager/Minion_Manager.h
Client/Private/Network/Client/SnapshotApplier.cpp
Shared/GameSim/Systems/MoveSystem.cpp
Server/Private/Game/SnapshotBuilder.cpp
```

---

## 8. 런타임 검증 로그 기준

### Loading / InGame

```text
[Scene_MatchLoading] loading ready sent
[GameRoom] BeginInGame all clients ready revision=...
[InGameBootstrap] network authoritative gameplay=1
```

### Structure / minion bind

```text
[SnapshotApplier] structure visual bind netID=...
[MinionVisual] bind network entity=... type=... team=...
```

### Skill / FX

```text
[Command] client send cast-skill sid=... myNet=... seq=... slot=... stage=...
[EventApplier] animation play netID=... animID=... seq=... rate=... stage=...
[EventApplier] fx cue champion=... sourceNet=... effectID=... slot=...
```

### Basic attack

```text
[Command] client send basic-attack sid=... myNet=... seq=... targetNet=...
[Command] basic-attack accept issuer=... target=...
[EventApplier] projectile spawn ...
[EventApplier] projectile hit ...
```

### Minion death/remove

```text
[ModelRenderer] Playing (loop=false): minion_*_death
[SnapshotApplier] remove stale minion netID=... entity=...
```

---

## 9. 위험 목록

- `SkillTable.cpp`와 `ChampionRuntimeDefaults.cpp`를 계속 수동 동기화하면 다시 틀어진다.
- local prediction과 server cue가 동시에 켜지면 FX/애니가 두 번 나간다.
- action lock, animation duration, playbackRate가 서로 다른 기준이면 조작감이 깨진다.
- champion visual hook은 등록만으로 끝나지 않는다. hook 함수 내부가 실제 FBX/PNG/projectile/clone 동작을 구현해야 한다.
- AI를 먼저 붙이면 봇이 같은 버그를 훨씬 빠르게 반복해서 디버깅이 더 어려워진다.

---

## 10. 다음 작업 선언

다음 구현은 S1/S5/S6/S7/S9를 묶은 긴급 안정화로 진행한다.

```text
1. Server skill timing override 정리
2. Network animation playbackRate/duration 적용
3. Zed Q/W/E visual parity 복구
4. Kalista BA/right-click recovery 복구
5. Irelia W hold/release + E lift 재검증
6. Minion attack animation 재검증
7. 통과 시 S10 BotAIStage1 진입
```

오늘의 AI 목표는 "완성형 AI"가 아니라 **사람과 같은 command path를 타는 첫 봇 루프**다.

---

## 11. 2026-05-11 S9 적용 기록

이번 세션의 목적은 AI 진입 전 챔피언/스킬/애니/FX 패리티를 다시 세우는 것이다.

적용:

- Server `ChampionRuntimeDefaults` fast verification timing 기본값을 `false`로 고정했다.
- Client `SkillTable` fast verification timing 기본값을 `false`로 고정했다.
- Client `SkillRegistry` fast verification timing 기본값을 `false`로 고정했다. 이 값이 켜져 있으면 등록형 챔피언의 lock/cooldown이 다시 0.2초 검증값으로 덮인다.
- Network animation stage 판정에서 `flags >= 2` 오판정을 제거하고 `(flags >> 12)` stage decode 기준으로 바꿨다.
- Client cast-skill command에 `stage`를 실어 보내도록 했다. Irelia W는 press stage1, release stage2가 명확히 분리되어야 한다.
- Server `CommandExecutor`는 stage2 요청을 받았을 때 stage window가 열려 있지 않으면 reject한다. 중복 W press가 곧바로 W2처럼 처리되는 회귀를 막는다.
- Garen/Riven cast hook을 BA 공통 hook으로 뭉개지 않고 slot별 hook으로 분리했다.
- Minion snapshot에서 `BasicAttack` actionSeq가 바뀌면 클라가 즉시 attack animation을 재생하도록 보강했다.
- Zed Q/W는 1차 visual parity로 수정했다. Q는 방향 projectile billboard, W는 존재하는 team indicator/wisp texture 기반 ground cue를 사용한다.
- Irelia E blade visual lift를 2배 올려 지형에 반쯤 묻히는 현상을 줄였다.

빌드:

```text
Client Debug x64 build: pass
Server Debug x64 build: running WintersServer.exe가 출력 exe를 잡고 있으면 link 단계에서 실패할 수 있음
```

다음 런타임 검증:

```text
Irelia W press:  slot=2 stage=1
Irelia W release: slot=2 stage=2
Irelia E: blade/beam이 지형 위로 뜨는지 확인
Zed Q/W/E: Q projectile, W ground cue, E slash가 cue당 1회 보이는지 확인
Kalista BA: 여전히 recovery/passive dash 서버 이식 필요
Minion BA: run/idle 고정이 아니라 attack animation이 1회 재생되는지 확인
```

---

## 12. Irelia Golden Slice 재정립

Irelia를 다시 첫 기준 챔피언으로 삼는다. 목표는 예전 Client-only 튜닝 감각을 복구하되, 최종 판정은 Server GameSim이 소유하는 것이다.

### Legacy Client-only 흐름

```text
Input Q/W/E/R
-> Client DispatchSkillInput
-> Client BuildCastCommand
-> Client ApplyLocalPrediction
-> Client animation 재생
-> Client animation frame event
-> Client SkillHookRegistry
-> Irelia_Skills.cpp
-> Client local dash / FX / damage / stun / blade beam
```

장점:

- 튜닝이 빠르다.
- 애니메이션, FX, 조작감 확인이 쉽다.

문제:

- 클라가 이동, 데미지, 스턴, 쿨타임 결과를 직접 결정한다.
- 멀티 클라에서 중복 실행, 다른 화면 불일치, 치팅 취약점이 생긴다.

### Server GameSim 기준 흐름

```text
Input Q/W/E/R
-> Client CommandSerializer
-> Server CommandExecutor
-> Server cooldown / stage / action lock / gameplay 판정
-> Server NetAnimation + EffectTrigger + Snapshot
-> Client EventApplier
-> Client VisualHookRegistry
-> Irelia visual-only FX
-> Client UpdateNetworkChampionLocomotion transition 복귀
```

서버 소유:

- Q dash 위치 변화
- W hold/release stage 상태
- E blade 위치, beam 판정, stun
- R wave 판정
- damage, cooldown, action lock, skill stage window

클라 소유:

- Irelia animation 재생
- Q trail, W spin/release, E blade/beam mesh, R pulse/wave visual
- skill_to_idle / skill_to_run transition 재생
- local debug/tuning UI

### 현재 코드상 위험 지점

- `Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp`는 legacy gameplay와 visual이 섞여 있다. 서버 권위 모드에서는 visual hook으로만 써야 한다.
- `Shared/GameSim/Systems/CommandExecutor.cpp`는 Irelia accepted hook id를 만들지만, 서버 gameplay hook 등록이 없으면 실제 Q dash/E stun/R wave 판정은 fallback 수준에 머문다.
- `Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp`와 `Client/Private/GameObject/Champion/Irelia/Irelia_Registration.cpp` 값이 계속 갈라지면 action lock, playback rate, cooldown이 다시 틀어진다.
- `Client/Private/Scene/Scene_InGame.cpp::UpdateNetworkChampionLocomotion`이 transition 복귀를 맡으므로 서버 animation duration과 client SkillDef duration이 맞아야 한다.

### Irelia 완성 순서

```text
I1. Irelia SkillDef / ChampionRuntimeDefaults 값 동기화
I2. Irelia Q: server dash + client Q trail + Q_to_idle/run
I3. Irelia W: press hold 유지 + release stage2 + W_to_run
I4. Irelia E: server blade pair state + beam/stun 판정 + client blade/beam visual
I5. Irelia R: server wave 판정 + client R visual
I6. Irelia BA: server BA windup/hit + client BA anim/stance
I7. 3-client Irelia-only runtime smoke
```

Irelia가 이 기준을 통과하면 Zed/Kalista/나머지 챔피언은 같은 구조로 옮긴다.

---

## 13. 2026-05-11 Golden Slice 성공 규칙

상세 규칙은 아래 문서에 한국어로 박제한다.

```text
.md/TODO/05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md
```

현재 Golden baseline:

```text
Irelia:
  BA/move/Q/W/E/R server-authority slice runtime 검증 통과.
  BA range baseline은 1.5f.
  Skill/BA accept 시 server MoveTarget 제거.
  Skill input은 pending network BA intent를 정리.
  NetAnimation stage는 flags >> 12로 decode.
  W press/release는 명시적인 stage1/stage2.
  Q는 caster-target vector 사용.
  W/R은 cursor direction 사용.
  E visual은 지형 위로 lift.

Kalista:
  1차 network passive dash bridge 적용.
  BA/Q action + 우클릭으로 passive dash queue.
  Network action end에서 기존 Kalista recovery hook 호출.
  passive dash가 snapshot 보정으로 튀면 server GameSim으로 올린다.
```

아래 항목이 회귀하면 champion expansion이나 AI로 넘어가지 않는다.

```text
flags >= 2 stage decode
fast timing enabled by default
accepted BA/skill leaving stale MoveTarget active
pending BA intent blocking Q/W/E/R
client local prediction double-spawning server FX
out-of-range right-click freezing instead of chase/move
minion locomotion overwriting attack animation
```

---

## 14. 2026-05-12 S10 AI 진입 선언

S10 BotAIStage1은 1차 runtime smoke를 통과했다. 상세 handoff 문서는 아래에 둔다.

```text
.md/TODO/05-11/AI_STAGE1_SERVER_COMMAND_HANDOFF.md
.md/TODO/05-11/STAGE1_SERVER_AI_SMOKE_SUCCESS.md
```

현재 결론:

```text
AI는 전투 시스템이 아니다.
AI는 사람과 같은 GameCommand를 생성하는 입력 생산자다.
Server GameSim이 이동/판정/피해/쿨다운/스킬 stage를 확정한다.
Client는 Snapshot/Event를 받아 animation/fx만 재생한다.
```

최신 전제:

- Irelia, Kalista, Yasuo에서 나온 gotcha는 S10 문서에 반영했다.
- Fiora/Yone/Jax/Viego/Ashe의 1차 server GameSim/component/visual hook 작업은 AI 입력 검증 대상으로 사용한다.
- 기존 `CBotLaneAISystem`은 이미 `GameRoom::Phase_ServerBotAI`에서 `m_pendingExecCommands`에 command를 넣고, 그 뒤 `Phase_ExecuteCommands`가 `CDefaultCommandExecutor`를 호출하는 구조다.
- Stage1 smoke에서 Jax/Fiora/Ashe가 command path를 통해 lane move, farm, champion fight를 수행했다.
- 미니언이 mid로 빨리던 원인은 target scan에서 range 밖 같은-lane enemy를 먼저 잡던 순서 오류였다. range reject를 priority 비교보다 먼저 수행한다.

S10 첫 smoke:

```text
Ashe bot:
  MoveToLane -> FarmMinion -> BasicAttack
  FightChampion -> W or BasicAttack

Jax bot:
  Q engage -> BasicAttack

Yasuo bot:
  Q stage 1/2/3 smoke
  R은 airborne 조건이 있을 때만 사용
```

S10 완료 기준:

```text
1. Bot AI는 Transform/HP/Cooldown/MoveTarget gameplay 결과를 직접 수정하지 않는다.
2. Bot AI는 GameCommand만 emit한다.
3. bot command와 human command가 같은 CDefaultCommandExecutor를 통과한다.
4. Client는 Snapshot/Event만 보고 animation/fx를 재생한다.
5. Server Debug x64, Client Debug x64 빌드가 통과한다.
```

---

## 15. 2026-05-12 다음 안정화 목표

다음 작업은 AI 지능 고도화가 아니라, 1차 smoke 기준선을 유지하기 위한 생명주기 정리다.

우선순위:

```text
1. Death state:
   HP <= 0 entity를 alive=false/dead state로 고정.

2. Target invalid:
   bot/minion/turret/projectile target scan에서 dead/stale entity 제외.

3. Projectile validity:
   projectile hit 직전 target alive/stale/range 재검증.

4. Minion despawn:
   death animation/window 후 snapshot remove.

5. Champion respawn:
   death -> fountain respawn -> HP reset -> AI decision delay 재시작.

6. Tower aggro refinement:
   minion priority 유지, champion aggro override는 champion이 allied champion을 공격했을 때만.
```

공통 helper 후보:

```text
IsCombatTargetAlive(world, entity)
IsCombatTargetSelectable(world, entity)
IsTargetStillValidForProjectile(world, projectile, target)
```

이 단계가 끝나면 다음 큰 목표는 tower destroy -> nexus destroy -> room reset/result까지 이어지는 최소 한 판 루프다.
