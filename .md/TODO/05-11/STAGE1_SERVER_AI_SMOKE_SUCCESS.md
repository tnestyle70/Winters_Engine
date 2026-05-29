# Stage1 Server AI Smoke Success

작성일: 2026-05-12

목적: S10 BotAIStage1의 첫 성공 기준선과 이번 세션에서 잡은 치명적인 미니언 라인 버그를 다음 세션에 그대로 복구할 수 있게 박제한다.

---

## 1. 결론

S10 BotAIStage1은 1차 runtime smoke를 통과했다.

검증된 기준선:

```text
Server owns gameplay state
Bot AI emits GameCommand
CDefaultCommandExecutor validates/applies command
Snapshot/Event drives client visuals
```

확인된 플레이 루프:

```text
Jax/Fiora meet in lane
-> fight using server command path
-> minions follow lanes and fight/farm
-> bots continue farming after champion interaction
-> bots do not blindly dive tower aggro
```

이 기준선이 깨지면 새 AI 기능보다 회귀 수정이 먼저다.

---

## 2. Start Timing Fix

이전 문제:

```text
Server simulation started while Client was still heavy-loading InGame.
Bot/minion simulation ran for a long time before the human could actually play.
First human command appeared around tick 3300+ in the log.
```

고정 원칙:

```text
Client MatchLoading should not tell server "I am in-game ready" too early.
Ready is sent after Scene_InGame::OnEnter and local bootstrap enter path.
Server BeginInGame must represent "client can actually observe/play", not just "network loading packet processed".
```

관련 파일:

```text
Client/Private/Scene/Scene_MatchLoading.cpp
Client/Public/Scene/Scene_MatchLoading.h
Client/Private/Scene/Scene_InGame.cpp
```

기대 로그:

```text
[Scene_MatchLoading] entering local InGame before server ready
[Scene_InGame] server ready sent after OnEnter
[GameRoom] BeginInGame all clients ready revision=...
```

---

## 3. Smoke Roster

현재 검증용 roster:

```text
slot 0: human, blue team
slot 1: blue Jax top
slot 5: red Sylas dummy
slot 6: red Fiora top
slot 7: red Ashe mid
```

기대 행동:

```text
Jax:
  lane move -> wave follow/farm -> Fiora fight -> tower-safe farming

Fiora:
  lane move -> wave follow/farm -> Jax fight -> tower-safe farming

Ashe:
  mid lane move -> farm/poke -> W projectile smoke
```

---

## 4. Critical Minion Bug

증상:

```text
Top/bottom minions spawned with correct lane ids and correct waypoint data.
But they drifted/fought around mid-ish z positions.
Lane0 logs showed combat around z=8~10 instead of top z=60~70.
```

핵심 원인:

```text
Server/Private/Game/GameRoom.cpp::FindClosestEnemyCombatTarget
```

target selection에서 priority 비교가 range reject보다 먼저 사실상 먹혔다.

문제 구조:

```text
bestDistSq = maxRange * maxRange

if priority is better:
  select candidate

range was not rejected before priority selection
```

결과:

```text
same-lane enemy minion anywhere on the map
-> selected as combat target
-> minion ignores waypoint lane movement
-> minion moves diagonally/directly toward enemy wave
-> top/bottom waves collapse toward mid
```

고정 규칙:

```text
const f32_t maxRangeSq = maxRange * maxRange;
const f32_t distSq = DistanceSqXZ(myPos, pos);
if (distSq > maxRangeSq)
    return;

then compare priority/distance/tie-breaker
```

이 규칙은 minion뿐 아니라 champion, turret, projectile target scan에도 적용한다.

검증 기준:

```text
lane0 blue/red minions should not fight near z=8~10 early.
lane0 should meet near the top route, roughly z=60~70.
lane1 should meet near mid, roughly z=0.
lane2 should meet near the bottom route, roughly z=-60~-70.
```

---

## 5. Current Success Signal

성공 신호:

```text
[BotAI] ... cmd=Move reason=lane-goal/wave-follow/farm-move
[BotAI] ... cmd=BasicAttack reason=farm-ba/fight-ba
[BotAI] ... cmd=CastSkill reason=fight-skill
[Command] basic-attack accept ...
[Command] cast-skill accept ...
[MinionAI] attack tick=...
[TurretAI] projectile tick=...
```

중요한 해석:

```text
BotAI log means decision produced command.
Command log means human/bot command path shared CDefaultCommandExecutor.
MinionAI/TurretAI logs mean server-owned non-player gameplay is active.
Client should only render Snapshot/Event results.
```

---

## 6. Next Priority

다음 1순위는 Death / TargetInvalid / Respawn이다.

구체 목표:

```text
1. HP <= 0이면 alive=false/dead state 확정.
2. Dead entity는 bot/minion/turret target scan에서 제외.
3. Projectile hit target이 dead/stale이면 hit 무효.
4. Turret projectile target이 dead/stale이면 retarget 또는 expire.
5. Minion death -> short death window -> despawn/remove snapshot.
6. Champion death -> death animation/state -> fountain respawn.
7. Respawn 후 bot initial decision delay 또는 lane re-entry delay 재시작.
```

공통 helper가 필요하다:

```text
IsCombatTargetAlive(world, entity)
IsCombatTargetSelectable(world, entity)
IsTargetStillValidForProjectile(world, projectile, target)
```

최우선 gotcha:

```text
AI가 죽은 entity를 계속 target으로 잡으면 reject log만 쌓이고,
미니언/타워/projectile은 stale target 때문에 이상한 공격/피격 로그를 만든다.
```

---

## 7. After Death/Respawn

그 다음 순서:

```text
1. AI Debug UI: bot state, targetNetID, command kind/slot/seq, lane, HP.
2. Tower aggro rule: minion priority, champion aggro override when champion attacks allied champion.
3. Bot leash: lane/wave/safe tower 기준으로 과추격 방지.
4. Structure push loop: tower damage, destroy, lane advance.
5. Nexus/end condition: 한 판 종료 루프.
```

최종 목표:

```text
Spawn -> lane -> farm/fight -> tower push -> structure destroy -> nexus destroy -> room reset/result
```
