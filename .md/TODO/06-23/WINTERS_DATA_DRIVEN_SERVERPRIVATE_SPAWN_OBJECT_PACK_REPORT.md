# Winters DataDriven ServerPrivate Spawn/Object Pack 결과 보고서

작성일: 2026-06-23

## 결과

ServerPrivate spawn/object 데이터 전환을 완료했고, 전체 DataDriven 검증 파이프라인을 통과했습니다.

이번 반영의 핵심은 이전의 중간 단계였던 "Shared가 작은 resolver DB를 통해 LoL gameplay 값을 들고 있는 구조"를 제거한 것입니다. 이제 Shared는 spawn/object 정의의 모양과 deterministic contract만 갖고, 실제 LoL gameplay 값은 `Data/LoL/ServerPrivate`에서 작성한 뒤 서버 전용 generated definition pack으로 들어갑니다.

런타임 동작은 회귀하지 않도록 유지했습니다. 챔피언 시작 loadout, respawn delay, champion collider profile, structure HP/turret AI, jungle camp stat, server minion combat stat은 여전히 같은 값을 사용하지만, 값을 읽는 위치가 server-owned pack으로 바뀌었습니다.

## 본질

이번 구조의 원자 단위 흐름은 다음과 같습니다.

```text
data value
-> server-private JSON
-> generated server pack
-> read-only runtime query
-> entity assembly
-> GameSim components
```

중요한 소유권 규칙은 다음과 같습니다.

```text
Shared/GameSim = 정의 모양 + deterministic component contract
ServerPrivate = 서버 권위 gameplay/balance 값
Server runtime = pack을 읽어서 entity를 조립
Client = replicated state 또는 client-public visual data만 소비
```

이번 slice의 북극성은 다음 한 줄입니다.

```text
SpawnObjectGameplayDefs.json
-> ServerData::GetLoLSpawnObjectDefinitionPack()
-> GameRoom / GameRoomSpawn
-> StatComponent / HealthComponent / TurretAI / JungleAI / Rune / Respawn
```

즉, server spawn 코드는 "entity를 만든다"만 책임지고, balance 값 자체를 소유하지 않습니다.

## 코드 변경

Shared에는 pack의 모양만 추가했습니다.

```text
Shared/GameSim/Definitions/SpawnObjectDefinitionPack.h
Shared/GameSim/Definitions/SpawnObjectDefinitionPack.cpp
```

서버 전용 authoring source를 추가했습니다.

```text
Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json
```

definition pack generator를 확장했습니다.

```text
Tools/LoLData/Build-LoLDefinitionPack.py
```

generator는 이제 다음을 수행합니다.

```text
SpawnObjectGameplayDefs.json 읽기
spawn/object gameplay field 정규화 및 검증
spawn/object 데이터를 definition build hash에 포함
ServerData::GetLoLSpawnObjectDefinitionPack() 생성
```

서버 pack 진입점과 generated 출력을 갱신했습니다.

```text
Server/Private/Data/LoLGameplayDefinitionPack.h
Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp
```

서버 런타임 reader를 갱신했습니다.

```text
Server/Private/Game/GameRoom.cpp
Server/Private/Game/GameRoomSpawn.cpp
```

잘못된 방향의 Shared-owned value DB를 제거했습니다.

```text
Shared/GameSim/Registries/SpawnPolicy/SpawnPolicyDB.h
Shared/GameSim/Registries/SpawnPolicy/SpawnPolicyDB.cpp
Shared/GameSim/Registries/ObjectGameData/ObjectGameDataDB.h
Shared/GameSim/Registries/ObjectGameData/ObjectGameDataDB.cpp
```

프로젝트 파일에 새 compile unit을 반영했습니다.

```text
Shared/GameSim/Include/GameSim.vcxproj
```

legacy audit도 함께 수정했습니다. generated server data는 의도된 생성물이지 legacy hardcoding이 아니므로, audit에서 generated server pack을 오탐하지 않도록 제외했습니다.

```text
Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
```

## 실제로 바뀐 점

이전 구조는 다음과 같았습니다.

```text
GameRoomSpawn.cpp
-> Shared DB
-> Shared default values
```

이 구조도 첫 단계로는 나쁘지 않았지만, 본질까지 내려가면 여전히 틀렸습니다. Shared가 LoL balance 값을 소유하고 있었기 때문입니다.

이제 구조는 다음과 같습니다.

```text
Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json
-> generated server pack
-> ServerData::GetLoLSpawnObjectDefinitionPack()
-> GameRoomSpawn.cpp / GameRoom.cpp
```

다음 Shared struct들은 더 이상 LoL gameplay 기본값을 갖지 않습니다.

```text
SpawnLoadoutPolicyDef
ChampionColliderProfileDef
StructureGameDef
JungleCampGameDef
```

이 struct들은 이제 값의 "소유자"가 아니라 값의 "모양"입니다. 더 원자 단위로 말하면 다음만 남긴 것입니다.

```text
field name + type + deterministic layout
```

실제 gameplay 값은 server generated pack이 소유합니다.

```text
startGold = 10000
startLevel = 6
startRune = LethalTempo
startRuneCount = 1
respawnDelaySec = 3.0
turret / inhibitor / nexus HP
turret AI combat values
jungle camp HP/radius/attack/move/armor/MR
server minion combat values
```

## 런타임 안전성 검토

프레임 중 읽히는 핵심 값은 전체 파이프라인으로 확인했습니다.

서버 death/respawn 경로는 이제 다음 값을 읽습니다.

```text
ServerData::GetLoLSpawnObjectDefinitionPack().spawnLoadout.respawnDelaySec
```

서버 spawn 경로는 이제 다음 값을 읽습니다.

```text
ResolveStructureMaxHp(...)
ResolveJungleCamp(...)
ResolveMinion(...)
championCollider
spawnLoadout
structure.turretAI
```

Client/Server/Shared 의존성 방향도 뒤집지 않았습니다.

```text
Server includes Server/Private/Data + Shared definition shapes
Shared does not include ServerPrivate data
Client does not receive ServerPrivate JSON
```

즉, 클라이언트가 플레이어 파일로 받으면 안 되는 server-private gameplay 값은 ClientPublic 경로로 노출하지 않았습니다.

## 검증

실행 명령:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
```

결과:

```text
Definition pack freshness: PASS
Build hash: 0xB9A5F523
Legacy ownership audit: PASS
serverObjectHardcode: 29
Client visual timing parity: mismatchCount 0
GameSim Debug x64: PASS
Server Debug x64: PASS
Client Debug x64: PASS
SimLab Debug x64: PASS
SimLab same-seed hash: 67F2A97563B8DB04
SimLab seed+1 hash: 5DA19645E291A29B
git diff --check: PASS
LoLDataDriven pipeline: PASS
```

첫 재실행 중에는 MSVC 중간 산출물 문제가 있었습니다.

```text
vc143.pdb lock
WintersEngine.pch invalid argument
```

이 문제는 코드 실패가 아니라 빌드 intermediate/PDB/PCH 산출물 꼬임으로 판단했습니다. Engine/GameSim/Server/Client/SimLab을 MSBuild Clean한 뒤 전체 파이프라인을 다시 실행했고, clean build 상태에서 통과했습니다.

남아 있는 경고는 기존 경고입니다.

```text
C4828 encoding warnings
C4251 / C4275 DLL-interface warnings
C4858 std::async warning
LNK4099 debug PDB warning
LF will be replaced by CRLF warnings
```

## 남은 의심 지점

이번 반영은 이전 Shared DB 단계보다 본질에 더 가깝습니다. 하지만 아직 완전히 끝난 것은 아닙니다.

가장 먼저 의심해야 할 원자는 다음입니다.

```text
Shared/GameSim/Definitions/MinionCombatDef.h
-> ResolveMinionCombatDef()
-> Client/Private/Manager/Minion_Manager.cpp local smoke path
```

Server는 더 이상 authoritative minion combat을 이 resolver에서 읽지 않습니다. Server는 server pack의 `ResolveMinion()`을 읽습니다. 다만 Client local smoke 경로가 아직 `ResolveMinionCombatDef()`를 legacy fallback으로 사용합니다. old local mode가 더 이상 필요 없어진 시점에는 이 경로를 삭제하거나, client-public visual/smoke-only contract 뒤로 밀어야 합니다.

다음으로 의심해야 할 server-side ownership 원자는 다음입니다.

```text
bot skill-rank policy
map/stage placement fallback
minion wave timing and lane flow data
basic attack/action timing still coupled to visual timing
remaining ChampionGameDataDB compatibility readers
```

## 다음 반영 방향

1. Client local minion fallback을 `ResolveMinionCombatDef()`에서 분리하거나 legacy local-only path를 삭제합니다.
2. Bot skill-rank policy를 데이터로 분리해서 server AI setup이 champion progression 값을 소유하지 않게 만듭니다.
3. `GameRoomSpawn.cpp`에 남아 있는 fallback map/stage placement data를 데이터로 이동합니다.
4. Minion wave timing과 minion combat stat을 분리합니다.
5. 직접 `ChampionGameDataDB`를 읽는 runtime reader를 계속 줄여서, generated pack이 유일한 authoritative gameplay source가 되도록 만듭니다.
