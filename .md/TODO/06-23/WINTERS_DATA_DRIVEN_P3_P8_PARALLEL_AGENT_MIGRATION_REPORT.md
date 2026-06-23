# Winters Data-Driven P3~P8 병렬 에이전트 마이그레이션 보고서

작성일: 2026-06-23

## 1. 결론

P3~P8 전체를 병렬 에이전트로 감사했다. 결론은 명확하다.

```text
P3~P7은 병렬 준비/부분 구현이 가능하다.
P8 legacy 삭제는 병렬로 바로 실행하면 안 된다.
P8은 P3~P7 reader-zero 증명 뒤에만 가능하다.
```

현재 Winters의 DataDriven 북극성은 유지되고 있다.

```text
JSON
-> generated immutable pack
-> TickContext / runtime query
-> GameSim / Server / Client consumer
```

하지만 아직 완전 컷오버 상태는 아니다. 특히 Client runtime은 여전히 `SkillDef/ChampionDef` 중심 경로가 살아 있고, Shared/Server에는 `ChampionGameDataDB`, `ChampionRuntimeDefaults`, `ChampionStatsRegistry`, `ChampionAIPolicy`, `ServerMinionTuning` fallback이 남아 있다.

## 2. 현재 기준 수치

`Tools/LoLData/Collect-LoLLegacyDataAudit.ps1` 기준:

```text
legacy skill registration files       = 17
legacy champion registration files    = 12
SkillDef related lines                = 238
ChampionDef related lines             = 203
visual fields total                   = 1489
visual suspicious authoritative       = 905
skillEffectHardcodeCandidates         = 156
skillEffectDataQueryReaders           = 76
```

현재 generated gameplay pack hash:

```text
0x58295D30
```

## 3. 현재 폴더 기준 소유권

현재 이미 생긴 축:

```text
Data/LoL/ServerPrivate/Gameplay
  ChampionGameplayDefs.json
  SkillGameplayDefs.json
  SkillEffectGameplayDefs.json
  SpawnObjectGameplayDefs.json
  SummonerSpellGameplayDefs.json

Data/LoL/ClientPublic/Visual
  ChampionVisualDefs.json
  Champion/ChampionVisualTimingSeed.json

Data/LoL/SharedContract
  DefinitionManifest.json

Server/Private/Data/Generated
  LoLGameplayDefinitions.generated.cpp

Client/Private/Data/Generated
  LoLVisualDefinitions.generated.cpp

Shared/GameSim/Definitions
  Definition shape, pack, query, legacy bridge
```

아직 필요한 축:

```text
Data/LoL/ServerPrivate/AI
Data/LoL/ServerPrivate/Map
Data/LoL/ServerPrivate/Navigation
Data/LoL/ServerPrivate/Gameplay/MinionWaveGameplayDefs.json
Data/LoL/ServerPrivate/Gameplay/MinionRuntimeGameplayDefs.json
```

## 4. P3 Champion Skill/Balance

### 상태

P3는 진행 중이지만 완료 전이다.

`SkillEffectGameplayDefs.json`은 현재 `annie`, `ashe`, `fiora`, `jax`, `kalista`, `leesin`, `riven` 일부만 커버한다. 아직 본체는 아래 챔피언에 남아 있다.

```text
Irelia
Kindred
Sylas
Viego
Yasuo
Yone
Zed
```

### 핵심 blocker

현재 `SkillEffectGameplayDefs.json`은 canonical skill key만 허용한다.

```text
skill.yasuo.q
skill.zed.r
```

하지만 실제 스킬은 variant가 많다.

```text
Yasuo Q / Q tornado / EQ
Zed W shadow / R mark / R vanish
Viego E mist / soul
Kindred W zone / E mark / R zone
```

따라서 남은 P3는 단순히 숫자를 JSON에 넣는 문제가 아니다. 먼저 effect variant 표현을 정해야 한다.

### 다음 구현 순서

1. Sylas E
   - E1/E2 dash, chain speed, hit radius, gap, airborne, damage
   - variant가 비교적 작고 한 파일 안에 모여 있음

2. Yone Q/W/R
   - Q/W/R은 비교적 단순
   - E는 soul state policy가 섞이므로 뒤로 미룸

3. Viego Q/W/R
   - Q/W/R effect는 안전
   - E mist, soul lifetime/radius는 policy로 분리

4. Irelia W/R
   - `halfWidth`, `length`, `width` param 추가 판단 필요
   - R stage id는 effect value가 아니라 protocol/policy

5. Yasuo/Zed/Kindred
   - variant/policy가 많으므로 가장 마지막

### 추가 원자 후보

SkillEffectParam 후보:

```text
Width
HalfWidth
Length
HitDelaySec
LifetimeSec
DashDelaySec
DamageRatio
BonusDamage
HealBaseAmount
HealAmountPerRank
MinHealth
```

별도 policy 후보:

```text
SkillVariantPolicy
ProjectilePolicy
ZonePolicy
MarkPolicy
ShadowPolicy
SoulPolicy
AuraTickPolicy
RankTablePolicy
```

## 5. P4 Client Visual Timing / SkillDef 분리

### 상태

P4는 아직 완료 전이다.

ClientPublic visual generated pack은 존재하지만, 실제 runtime 경로는 여전히 `SkillDef` 중심이다.

핵심 잔여 경로:

```text
Shared/GameSim/Definitions/SkillDef.h
Client/Private/GamePlay/SkillRegistry.cpp
Client/Public/GameObject/SkillDefVisualDataAdapter.h
Client/Private/Scene/Scene_InGameLocalSkills.cpp
Client/Private/Scene/Scene_InGameNetwork.cpp
Client/Private/Data/LoLVisualDefinitionPack.h
Shared/GameSim/Definitions/ChampionGameData.h
```

### 핵심 문제

`SkillDef` 하나가 너무 많은 것을 동시에 들고 있다.

```text
gameplay:
  cooldown
  range
  mana
  lockDuration

visual:
  animKey
  castFrame
  recoveryFrame
  hookId
  transition
```

이 구조에서는 기획자/디자이너/개발자 협업 경계가 다시 섞인다.

### 다음 구현 순서

1. `ClientData::SkillVisualDefinition` 확장
   - animation key
   - stage2 animation key
   - cast/recovery/keySwap hook id
   - transition idle/run/duration

2. `CSkillRegistry::ResolveSkillVisualData` 전환
   - `SkillDef` adapter보다 ClientPublic visual pack을 우선 읽게 함

3. `Scene_InGameNetwork` action animation 전환
   - network action animation/transition 조회를 `SkillDef`에서 visual pack으로 이동

4. `Scene_InGameLocalSkills` authoritative input 분리
   - gameplay atom은 ServerPrivate gameplay definition/query
   - visual은 ClientPublic visual pack

5. `ChampionGameData` visual fields 삭제 준비
   - `animPlaySpeed`, `castFrame`, `recoveryFrame`, `visualYawOffset` reader-zero 증명 후 삭제

주의:

`visualYawOffset`은 이름은 visual이지만 서버 회전 계산에도 쓰인다. 무조건 ClientPublic으로만 빼면 서버/클라 yaw가 어긋날 수 있다. 필요하면 `modelFacingOffset` 같은 Shared-safe gameplay-facing 원자로 재명명해야 한다.

## 6. P5 AI Policy

### 상태

AI policy는 아직 코드 소유다.

핵심 잔여 경로:

```text
Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp
Shared/GameSim/Components/ChampionAIComponent.h
Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp
Server/Private/Game/GameRoomSpawn.cpp
Server/Private/Game/GameRoomChampionAI.cpp
Client/Private/UI/AIDebugPanel.cpp
```

### 새 폴더

```text
Data/LoL/ServerPrivate/AI/
  ChampionAIProfileDefs.json
  ChampionAIComboDefs.json
  ChampionAIDecisionPolicyDefs.json
  BotSkillRankPlanDefs.json
  BotRosterPolicyDefs.json
```

### 다음 구현 순서

1. AI definition shape 추가
   - `ChampionAIDef`
   - `ChampionAIDefinitionPack`
   - `ChampionAIDefinitionQuery`

2. 현재 `ChampionAIPolicy.cpp` 값을 JSON으로 1:1 이식

3. `CChampionAISystem`이 `TickContext`의 AI pack으로 profile/combo/decision policy 조회

4. `GameRoomSpawn` bot skill rank plan을 JSON으로 전환

5. Client `AIDebugPanel`은 서버 AI profile 직접 조회 제거
   - snapshot debug 값만 표시

6. reader-zero 뒤 `ChampionAIPolicy.cpp` legacy table 삭제

### 결정성 주의

AI score 공식은 floating point 비교가 많다. JSON으로 옮길 때 값 순서와 비교 순서가 바뀌면 SimLab hash가 바뀔 수 있다. 순수 컷오버에서는 same-seed hash가 유지되어야 한다.

## 7. P6 Minion / Wave / Map

### 상태

minion combat은 일부 DataDriven이다.

이미 데이터화된 경로:

```text
SpawnObjectGameplayDefs.json
-> SpawnObjectDefinitionPack
-> GameRoomSpawn ResolveMinion(roleType)
```

아직 코드 소유인 경로:

```text
MinionCombatDef.h ResolveMinionCombatDef fallback
Client/Private/Manager/Minion_Manager.cpp fallback reader
Server/Public/Game/ServerMinionTuning.h
Server/Private/Game/ServerMinionWaveRuntime.cpp
Server/Private/Game/GameRoomMinionAI.cpp
Shared/GameSim/Definitions/MapSpawnPoints.cpp
Server/Private/Game/WorldBootstrap.cpp
Server/Private/Game/GameRoomInternal.cpp
Server/Private/Game/GameRoomNav.cpp
```

### 새 폴더/파일

```text
Data/LoL/ServerPrivate/Gameplay/MinionWaveGameplayDefs.json
Data/LoL/ServerPrivate/Gameplay/MinionRuntimeGameplayDefs.json
Data/LoL/ServerPrivate/Map/SummonersRiftPlacementDefs.json
Data/LoL/ServerPrivate/Navigation/SummonersRiftNavPolicyDefs.json
```

### 분리 원칙

`ServerMinionTuning`을 그대로 JSON 하나로 옮기면 다시 큰 잡탕 table이 된다.

나눠야 한다.

```text
wave:
  delay
  interval
  spawn delay
  lane
  formation

runtime AI/nav:
  scan interval
  path rebuild
  separation
  lane clearance

projectile:
  speed
  hit radius
  start offset
  height
  max distance padding

map placement:
  spawn
  gather
  structure fallback
  fountain
  nav bounds
```

### 다음 구현 순서

1. Client `Minion_Manager`가 combat fallback을 읽지 않도록 visual-only 전환
2. `MinionWaveGameplayDefs.json` 추가
3. `MinionRuntimeGameplayDefs.json` 추가
4. `MapSpawnPoints`를 ServerPrivate Map pack으로 이동
5. structure/fountain/nav fallback을 map/nav pack으로 이동
6. reader-zero 뒤 `ResolveMinionCombatDef` fallback 삭제

## 8. P7 Network DefinitionKey

### 상태

네트워크는 아직 `championId:ubyte` 중심이다.

Schema 표면:

```text
Shared/Schemas/Snapshot.fbs
Shared/Schemas/LobbyTypes.fbs
Shared/Schemas/LobbyCommand.fbs
Shared/Schemas/Hello.fbs
Shared/Schemas/Event.fbs
```

송신 경계:

```text
Server/Private/Game/SnapshotBuilder.cpp
Server/Private/Game/GameRoomLobby.cpp
Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp
```

수신 경계:

```text
Client/Private/Network/Client/SnapshotApplier.cpp
Client/Public/Network/Client/SnapshotApplier.h
Client/Private/Network/Client/GameSessionClient.cpp
Client/Private/Network/Client/EventApplier.cpp
```

### 마이그레이션 원칙

즉시 삭제하면 안 된다.

```text
1. DefinitionKey field append
2. 서버 dual-write
3. 클라 dual-read
4. key 우선, championId fallback
5. reader-zero 뒤 protocol major bump에서 championId 퇴역
```

추가할 wire field 후보:

```text
championKey
baseChampionKey
visualChampionKey
skillChampionKey
spellbookChampionKey
sourceChampionKey
targetChampionKey
```

주의:

`ReplicatedEventSerializer`는 Shared에 있다. 여기서 `ServerData::GetLoLGameplayDefinitionPack()`를 include하면 의존성 위반이다. kill feed key 전환은 `GameplayDefinitionPack*`를 serializer에 넘기거나, `ReplicatedEventComponent`에 key를 미리 저장하는 방식이어야 한다.

## 9. P8 Legacy Deletion

### 현재 결론

P8 삭제는 아직 불가다.

삭제 차단자:

```text
SkillDef / SkillTable
ChampionDef / ChampionTable
ChampionGameDataDB
ChampionStatsRegistry
ChampionRuntimeDefaults
MinionCombatDef fallback
ServerMinionTuning
ChampionAIPolicy
```

### 삭제 전 reader-zero proof

아래 검색이 generated id type 오탐을 제외하고 0이어야 한다.

```powershell
rg -n "\bSkillDef\b|FindSkillDef|g_SkillTable|g_SkillCount|CSkillRegistry|SkillDefAdapters" Client Shared Server Engine Tools -g "*.h" -g "*.cpp"

rg -n "\bChampionDef\b|FindChampionDef|RegisterAllLegacy|CChampionRegistry|ChampionCatalog|s_ChampionTable" Client Shared Server Tools -g "*.h" -g "*.cpp"

rg -n "ChampionGameDataDB|ChampionGameDataGenerated|Data\\Gameplay\\ChampionGameData|champions\\.json" Client Shared Server Tools Data -g "*.h" -g "*.cpp" -g "*.py" -g "*.ps1" -g "*.json"

rg -n "CChampionStatsRegistry|ChampionStatsRegistry|ChampionRuntimeDefaults|GetDefaultChampion|BuildDefaultChampion" Client Shared Server Tools -g "*.h" -g "*.cpp"

rg -n "ResolveMinionCombatDef|ServerMinionTuning|ChampionAIPolicy|GetChampionAIProfile|GetChampionAIComboPlan" Client Shared Server Tools -g "*.h" -g "*.cpp"
```

그 뒤에만 삭제한다.

## 10. 병렬 실행 가능한 Workstream

### 병렬 가능

```text
P3-A: Sylas/Yone/Viego simple skill effect cutover
P3-B: SkillEffect variant schema 설계
P4-A: Client visual definition 확장
P5-A: AI JSON/pack skeleton 추가
P6-A: Minion wave/runtime/map/nav JSON/pack skeleton 추가
P7-A: schema dual-write field append 설계
P8-A: reader-zero audit script 강화
```

### 순차 필요

```text
P4 network/local SkillDef 제거
  -> P8 SkillDef/SkillTable 삭제

P5 AI reader 전환
  -> P8 ChampionAIPolicy 삭제

P6 Minion/Map reader 전환
  -> P8 ServerMinionTuning/ResolveMinionCombatDef 삭제

P7 key dual-read 완료
  -> championId:ubyte legacy 퇴역
```

## 11. 바로 다음 구현 추천

가장 안전한 다음 구현은 P3의 `Sylas E`다.

이유:

```text
1. ServerPrivate gameplay data로 닫힌다.
2. Client visual/SkillDef 분리보다 blast radius가 작다.
3. P7 schema 변경보다 회귀 위험이 낮다.
4. P5/P6처럼 새 pack 축 전체를 열지 않아도 된다.
```

단, P3를 계속하기 전에 `SkillEffectParam`과 `Policy`의 경계를 계속 의심해야 한다.

```text
즉시 효과 수치인가?
상태/표식/zone/projection 생명주기인가?
variant를 표현해야 하는가?
```

이 질문에 답하지 못하는 값은 `SkillEffectParam`에 넣지 않는다.

## 12. 공통 검증 파이프라인

각 slice는 아래를 통과해야 한다.

```powershell
python -m py_compile Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
git diff --check
```

P7 schema 변경 slice는 추가로:

```powershell
Shared/Schemas/run_codegen.bat
```

P8 삭제 slice는 추가로:

```text
reader-zero proof
Client Debug build
Server Debug build
SimLab deterministic regression
runtime smoke
```

## 13. 최종 판단

P3~P8은 병렬 에이전트를 활용해 끝낼 수 있다. 다만 “동시에 삭제”로 끝내는 것이 아니라, 아래 방식으로 끝내야 한다.

```text
병렬:
  도메인별 JSON/pack/query/reader 전환 준비와 일부 안전한 컷오버

순차:
  client visual 분리
  AI/minion/network reader 전환
  reader-zero 증명
  legacy 삭제
```

현재 기준에서 가장 본질적인 다음 작업은 `P3 Sylas E` 또는 `P4 Client visual definition 확장`이다. 대규모 구조 완성을 최단으로 당기려면 둘을 병렬로 가져가되, P8 삭제는 아직 잠가둔다.
