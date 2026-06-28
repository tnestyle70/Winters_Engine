Session - 미니언 wave 스케줄/구성, 클라 미니언 combat fallback, 맵 placement를 데이터로 옮긴다. wave "timing"과 minion "combat stat"은 별도 원자로 분리한다(combat은 이미 pack에 있음).

배경(한 줄): minion combat / structure / jungle은 이미 `SpawnObjectDefinitionPack`(ResolveMinion/ResolveJungleCamp/structure)으로 데이터화됐고 서버가 쓴다. 남은 하드코딩은 (A)wave 스케줄/구성, (B)클라 `Minion_Manager`의 `ResolveMinionCombatDef` 직접 호출, (C)맵 placement다. 규칙·게이트는 07.

검토 반영(2026-06-28) — 확정 정정:
- (★경로 정정) authoring JSON 경로는 `Data/LoL/ServerPrivate/Gameplay/MinionWaveData.json`이다. generator(`Build-LoLDefinitionPack.py:1844`)가 `ServerPrivate/Gameplay/`에서 읽으므로 `Game/Object/`가 아니다.
- (★필드 정정) spawn slot 구조체 필드는 `kSpawnSlots[]`(ServerMinionWaveRuntime.cpp:405)와 동일하게 `role`로 쓴다(roleType 아님).
- (확정) 클라 minion combat은 이미 `MinionCombatDef.h`(Shared) include로 접근 가능 → I1 위반 아님. dedup은 클라가 `ResolveMinionCombatDef` 직접 호출 대신 **pack 경유 단일 진입점**(`SpawnObjectDefinitionPack::ResolveMinion`)을 읽게 하는 것. 클라가 pack 인스턴스를 어떻게 얻는지(공용 accessor) 1-4에서 명시.
- (선행) generator 확장(MinionWaveData emit/검증/hash)이 freshness G0의 선행이다 — 데이터만 추가하고 generator 미반영이면 G0 실패.
- (경계) `ServerMinionTuning.h:26-27`(kTargetScanIntervalSec/StaggerBuckets)은 algorithm/balance 경계의 회색지대 → balance로 분류할지 결정 후 처리.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/MinionWaveData.json (새 파일)

wave 스케줄(timing)과 spawn slot 구성을 담는다. minion combat stat과 결합하지 않는다(07 P6 주의).

```json
{
  "schemaVersion": 1,
  "schedule": { "initialWaveDelayTicks": 300, "waveIntervalTicks": 900, "perMinionSpawnDelayTicks": 10, "waveStartX": 5.0 },
  "spawnSlots": [
    { "role": "Melee",  "forwardOffset": 3.6, "sideOffset": -0.9 },
    { "role": "Melee",  "forwardOffset": 4.8, "sideOffset":  0.0 },
    { "role": "Melee",  "forwardOffset": 6.0, "sideOffset":  0.9 },
    { "role": "Ranged", "forwardOffset": 0.0, "sideOffset": -0.9 },
    { "role": "Ranged", "forwardOffset": 1.2, "sideOffset":  0.0 },
    { "role": "Ranged", "forwardOffset": 2.4, "sideOffset":  0.9 }
  ]
}
```

값 출처(byte-identical): `ServerMinionTuning.h:29-33`(스케줄), `ServerMinionWaveRuntime.cpp:396-413`(kSpawnSlots).

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SpawnObjectDefinitionPack.h (+ generator)

`SpawnObjectDefinitionPack`에 `MinionWaveScheduleDef`와 spawn slot 배열을 추가한다(이미 minion/jungle/structure가 있으므로 같은 pack 확장). generator(`Build-LoLDefinitionPack.py`)가 MinionWaveData.json을 정규화/검증/hash 포함 emit.

```cpp
struct MinionWaveScheduleDef { u32_t initialWaveDelayTicks; u32_t waveIntervalTicks; u32_t perMinionSpawnDelayTicks; f32_t waveStartX; };
struct MinionWaveSpawnSlot { u8_t role; f32_t forwardOffset; f32_t sideOffset; };
// pack에 MinionWaveScheduleDef wave{}; const MinionWaveSpawnSlot* waveSlots; size_t waveSlotCount; 추가
```

확인 필요: timing(tick)은 결정론 값으로 유지(07 I3, float 도입 금지). waveStartX/offset은 위치라 float 유지 OK.

1-3. C:/Users/tnest/Desktop/Winters/Server/Private/Game/ServerMinionWaveRuntime.cpp + Server/Public/Game/ServerMinionTuning.h

`kSpawnSlots[]`와 wave 스케줄 상수 reader를 pack(`ServerData::GetLoLSpawnObjectDefinitionPack().wave / waveSlots`)으로 전환한다. 알고리즘 튜닝(`ServerMinionTuning.h:11-27`: path rebuild/scan/flow field)은 balance가 아니므로 코드에 둔다(07 분류).

확인 필요: balance(wave) vs algorithm(path) 분리 — 1-1에는 wave만, ServerMinionTuning에는 path만 남긴다.

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

`ResolveMinionCombatDef(ms.type)` 직접 호출(1163)을 서버와 동일한 `ServerData::GetLoLSpawnObjectDefinitionPack().ResolveMinion(roleType)` 경로로 바꾼다(클라가 같은 pack을 읽거나, 클라용 접근자 추가). 시각 scale(0.006/Tibbers 0.01)은 visual이므로 유지.

확인 필요: 클라가 SpawnObjectDefinitionPack(Server 소유)에 접근 가능한가. 불가하면 클라 전용 minion combat 접근자를 ClientPublic 또는 공용 경로로 노출(I1 준수). 이게 안 되면 클라 `ResolveMinionCombatDef` 의존이 남아 P8 minion 하드코딩 삭제를 막는다.

1-5. 맵 placement (우선순위 낮음, 별도 slice)

`MapSpawnPoints.cpp:38-99`(roster spawn/lane gather)와 `GameRoomSpawn.cpp` placement fallback을 stage/map 데이터로. 07 §0-2: placement는 balance가 아니므로 우선순위 낮음. 이번 Phase 후반 slice로 분리.

2. 검증 (07 §6)

미검증:
- wave/슬롯 pack 미생성, reader 전환 미반영

검증 명령:
- python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
- GameSim/Server/Client/SimLab Debug x64 빌드
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1

통과 기준:
- G4 SimLab same-seed 해시 불변: wave 스케줄/구성/미니언 스탯 값 byte-identical.
- G3: ServerMinionTuning balance 상수 + 클라 ResolveMinionCombatDef 직접 호출 reader가 (fallback 삭제 후) 0.

확인 필요:
- 1-4 클라 pack 접근(I1). minion combat을 양 진영 공용으로 읽는 경로가 P6의 핵심 설계점.
- placement(1-5)는 balance와 분리해 후순위.
