# 데이터 주도 엔티티 파이프라인 인덱스 (북극성: 04 협업 파이프라인)

작성일: 2026-06-22
상태: 개념 리팩터링 기준 / 세션별 코드 계획서 인덱스
북극성 문서: `C:/Users/tnest/Downloads/04_협업_파이프라인.md`
기준 흐름: `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event/FxCue -> Client Visual`

## 이 문서의 목적

북극성(`04_협업_파이프라인.md`)이 말하는 "기획자의 JSON -> 살아있는 ECS 엔티티"를 Winters 현재 코드 위에서 **코드 리팩터링 + 개념 리팩터링**으로 끝까지 끌고 가기 위한 인덱스다.

북극성의 4개 기계장치를 Winters 현재 상태에 매핑한다.

| 북극성 기계장치 | Winters 현재 상태 | 이 파이프라인에서의 위치 |
|---|---|---|
| 1. 데이터 주도 컴포넌트 (JSON -> 엔티티) | 데이터 경로는 있으나 **조립이 하드코딩** | S1~S3 (핵심) |
| 1.3 검증 (빌드가 기획자 실수 차단) | `build_champion_game_data.py`가 일부 검증 | S1 + 기존 16 시리즈 |
| 2. 인엔진 ImGui 에디터/인스펙터 | `Scene_Editor` 인스펙터는 맵 전용, 챔피언 튜너는 비활성 | S4 |
| 2.2 reflection 자동 인스펙터 | 없음 (그린필드) | S6 (Tier 2) |
| 3. 핫리로드 | Lua만 수동 reload, 데이터 핫리로드 없음 | S5 |
| 4. Perforce 워크플로우 | 코드 아님 (운영 트랙) | 본 파이프라인 범위 밖, 아래 "비코드 트랙" 참고 |

## 이미 깔려 있는 것 (재사용 기준점, 다시 만들지 않는다)

데이터 경로는 상당 부분 구축되어 있다. 새로 만들 것은 **데이터 -> 엔티티 조립(Factory)** 과 **편집/핫리로드**다.

- 저작 데이터: `Data/Gameplay/ChampionGameData/champions.json` (17 champion, 94 skill stage)
- 오프라인 코드젠: `Tools/ChampionData/build_champion_game_data.py` -> `Shared/GameSim/Generated/ChampionGameData.generated.h/.cpp`
- read-only resolver: `Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h` (`FindChampion`, `ResolveStats`, `BuildStat`, `ResolveSkillTiming`, `ResolvePassiveDash*` 등)
- 통계 resolver(중복 후보): `Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry` (`Instance().Resolve()`)
- 첫 오브젝트 데이터 추출(진행 중): `Shared/GameSim/Definitions/MinionCombatDef.h` (`MinionCombatDef` POD + `ResolveMinionCombatDef(roleType)`)
- S0/S1 데이터 권위 경계: `Shared/GameSim/Definitions/DataPackManifest.h`, `LoLPublicGameHintData.h`
- 비주얼 타이밍 seed 추출 + parity 0 mismatch: `Data/LoL/ClientPublic/Visual/Champion/ChampionVisualTimingSeed.json`
- 엔진 generic 조립 인프라(미사용): `Engine/Public/ECS/Systems/EntityBlueprint.h`(`CEntityBlueprint::Add(Installer)`, `Spawn`), `EntityBlueprintRegistry.h`
- ECS API: `Engine/Public/ECS/World.h` (`CreateEntity`, `AddComponent<T>(EntityID, const T&)`, `TryGetComponent<T>`)
- 인스펙터 레퍼런스: `Client/Private/Scene/Scene_Editor.cpp` (Hierarchy + Inspector, Transform 바인딩, Save/Load)

진행 중인 데이터 소유권/원자화는 아래 기존 시리즈가 소유한다. 본 파이프라인은 **중복하지 않고 그 위에 조립층을 얹는다**.

- `.md/plan/refactor/09_LOL_DATA_ATOM_EXTRACTION_COLLAB_PLAN.md`
- `.md/plan/refactor/10_LOL_DATA_AUTHORITY_PATCH_PIPELINE_PLAN.md`
- `.md/plan/refactor/11_LOL_VISUAL_TIMING_SEED_EXTRACTION_PLAN.md`
- `.md/plan/sim/14_CHAMPION_GAMEDATA_SERVER_AUTH_PIPELINE.md`
- `.md/plan/sim/15_CHAMPION_GAMEDATA_S1_READONLY_DB_PLAN.md`
- `.md/plan/sim/16_CHAMPION_GAMEDATA_DESIGNER_PIPELINE_INDEX.md`

## 핵심 갭 (북극성 1.2가 가리키는 빈자리)

북극성 1.2는 `CChampionFactory::Spawn(championId, team)`처럼 **데이터를 읽어 엔티티를 조립하는 함수**를 보여준다. Winters에는 그 조립이 **함수가 아니라 GameRoom에 박힌 하드코딩 시퀀스**로 존재한다.

```text
현재:  champions.json -> codegen -> generated table -> ChampionGameDataDB(resolver)
       그리고 GameRoom 안에 CreateEntity + AddComponent x약 25 (하드코딩) 가 따로 존재
목표:  champions.json -> codegen -> generated table -> resolver -> [Entity Factory] -> Entity
```

하드코딩 조립 지점 (리팩터 대상, 실측):

- 챔피언: `Server/Private/Game/GameRoomSpawn.cpp:726` `SpawnChampionForLobbySlot`
  - level=6(738), gold=10000(766), `eRuneId::LethalTempo`(773), collider `{r,1.8,r}`/offset`{0,0.9,0}`(823-824), respawn `kDefaultChampionRespawnDelaySec`(GameRoomInternal.h:31), 챔피언별 sim component `if` 체인(795-814)
- 미니언: `GameRoomSpawn.cpp:586` `SpawnServerMinion` — scale 0.006(595), spatial 0.5(642), collider `{0.5,1,0.5}`(646), 스탯은 `ResolveMinionCombatDef`(MinionCombatDef.h)
- 구조물: `GameRoomSpawn.cpp:501` `SpawnServerStructure` — turret AI range 7.75/dmg 180·150/cooldown 1.0/speed 18(538-543), 구조물 HP 3000·4000·5500(107-114), fallback 위치(344-356)
- 정글: `GameRoomSpawn.cpp:419` `SpawnServerJungleFromStageEntry` — `Resolve*Jungle*` subKind 하드코딩 스위치(116-210)

클라이언트 중복 조립(같은 하드코딩이 두 번):

- `Client/Private/GameObject/ChampionSpawnService.cpp:162` `CChampionSpawnService::Spawn` (서버와 같은 `CChampionStatsRegistry` 사용)
- `Client/Private/Manager/Minion_Manager.cpp:1047` `Spawn_Minion` (minion scale 0.006, `ResolveMinionCombatDef` 중복)
- `Client/Private/Manager/Structure_Manager.cpp:333` `Spawn_FromEntry` (turret AI 7.75/180/150/18, 구조물 HP 중복)

## 본질 분류 (어떤 값이 어디로 가는가)

북극성과 09 시리즈의 소유권 기준을 조립 데이터에 적용한다.

```text
Spawn/Loadout Policy data (match 시작 시 1회 읽어 component로 변환)
- 시작 gold, 시작 rune, 시작 level, respawn delay
- bot skill rank policy, spawn slot/position
- 소유: ServerPrivate/Game/Match, Server만 읽는다.

Object Game data (서버 판정 대상 오브젝트 수치)
- minion role 스탯, structure HP, turret AI(range/damage/cooldown/projectile speed), jungle 캠프 스탯
- 소유: ServerPrivate/Game/Object, Server/Shared GameSim만 읽는다.

Collision/Spatial profile (판정용 충돌/공간 프로파일)
- collider halfExtents/offset, spatial radius, transform scale(게임플레이 영향분)
- 소유: ServerPrivate/Game/Object 또는 Champion, Server가 읽는다. 단 순수 표시용 scale은 Visual.

Component composition (어떤 엔티티가 어떤 component 집합을 갖는가)
- 챔피언별 sim component 부착 표(YasuoStateComponent 등)
- 소유: 데이터화하면 ServerPrivate/Game/Champion, 또는 코드 등록표. (S3에서 결정)

Visual data (조립과 무관, 클라 표시)
- mesh/particle scale, trail color, yaw offset(표시분), animPlaySpeed/castFrame/recoveryFrame
- 소유: ClientPublic/Visual. 이미 visual timing seed로 추출 진행 중(11 시리즈).
```

## 금지 방향 (16 시리즈와 동일하게 유지)

```text
- Client가 gameplay truth를 새로 만들지 않는다. Client Factory는 presentation/prediction만.
- ImGui가 gameplay 값을 직접 SetDamage/SetRange/SetCooldown 하지 않는다. debug override는 server command를 거친다.
- Engine public header에 Shared/GameSim 타입, Server 타입, LoL 타입을 새로 노출하지 않는다.
  (CEntityBlueprint은 generic installer만 받는다. champion/component를 아는 Factory는 Server/Shared가 소유한다.)
- mutable global DataCenter singleton을 만들지 않는다. resolver는 read-only.
- 회귀 없이: legacy 값 추출 -> resolver 경유 -> parity -> reader 전환 -> 하드코딩 삭제 순서를 지킨다.
```

## 세션 인덱스 (코드 계획서)

각 세션은 별도 계획서 파일이며 `/plan-rules` 형식(`Session - ...` / `1. 반영해야 하는 코드` / `2. 검증`)을 따른다.

### S1. Spawn/Object 데이터 정의 + resolver (데이터 층)
파일: `01_SPAWN_DATA_DEFS_PLAN.md`
- 새 POD def: `SpawnLoadoutPolicyDef`, `ChampionColliderProfileDef`, `StructureGameDef`, `JungleCampGameDef` (MinionCombatDef 패턴 그대로)
- 새 resolver namespace: `SpawnPolicyDB`, `ObjectGameDataDB`
- 초기값은 legacy와 동일(parity). 데이터 소스는 codegen/runtime JSON 선택을 S5로 미룬다.
- 성공 기준: resolver가 현재 하드코딩과 같은 값을 반환. 빌드 통과.

### S2. Server Entity Factory + 스폰 리팩터 (핵심: JSON -> Entity)
파일: `02_SERVER_ENTITY_FACTORY_PLAN.md`
- 새 모듈: `ServerEntityFactory` (Server 소유, resolver를 읽어 component를 Add)
- `SpawnChampionForLobbySlot`/`SpawnServerMinion`/`SpawnServerStructure`/`SpawnServerJungleFromStageEntry`의 하드코딩을 resolver/factory 호출로 교체
- 챔피언별 sim component `if` 체인을 데이터/등록표 기반 부착으로 정리
- 성공 기준: 하드코딩 리터럴이 factory/resolver 경유로 이동, F5 동작 불변.

### S3. Client 스폰 중복 제거 (presentation only)
파일: `03_CLIENT_SPAWN_DEDUP_PLAN.md`
- `CChampionSpawnService`/`CMinion_Manager`/`CStructure_Manager`의 gameplay 성격 값을 같은 Shared resolver에서 읽도록 전환
- Client에는 Render/Model/Scale(표시분)만 남긴다.
- 성공 기준: client/server가 같은 resolver를 읽는다. client 전용 gameplay 리터럴 0.

### S4. Entity Inspector + Server Debug Override (북극성 2)
파일: `04_ENTITY_INSPECTOR_DEBUG_OVERRIDE_PLAN.md`
- `Scene_Editor` 인스펙터 패턴을 generic ECS 컴포넌트 인스펙터로 일반화
- 값 변경은 client mutation이 아니라 server debug override command 경유(16 시리즈 S18과 정합)
- 성공 기준: 선택 엔티티의 component를 읽어 표시, 편집은 server 왕복으로 반영.

### S5. 데이터 핫리로드 (북극성 3)
파일: `05_DATA_HOTRELOAD_PLAN.md`
- dev 빌드 한정 파일 watcher -> JSON 재로드 -> resolver overlay -> live entity 재적용
- 코드젠 baked table 위에 dev-only runtime JSON overlay를 얹는 설계 분기 명시
- 성공 기준: champions.json/object data 수정 시 재시작 없이 반영(dev), release 비활성.

### S6. Reflection 자동 인스펙터 (Tier 2, 북극성 2.2)
파일: 본 인덱스의 "Tier 2 후속" 절. 별도 계획서는 S4 착지 후 작성.
- 필드 메타데이터 등록(`REFLECT`/`FIELD`)으로 인스펙터/직렬화 자동화
- S4의 수동 인스펙터로 먼저 가치 증명 후 도입.

## 권장 착수 순서

```text
S1(데이터 def/resolver) -> S2(factory + 스폰 리팩터) -> S3(client dedup)
   -> S4(inspector + debug override) -> S5(hot reload) -> S6(reflection, 선택)
```

S1~S3가 북극성 1(JSON -> 엔티티)의 코드 본체다. S4~S6은 기획자 자립(편집/즉시 반복) 기계장치다.

## 완전 컷오버 트랙 (P3~P9, Codex 실행용)

S0~S2 슬라이스 위에서 gameplay/balance/visual 값을 "완전히" 데이터로 빼는 잔여 컷오버는 별도 트랙으로 관리한다. 남은 표면(스킬 밸런스 constexpr, 클라 SkillDef 하드코딩, ChampionGameDataDB reader, 봇 AI 정책, wave/placement, 네트워크 식별자, legacy 삭제)을 Phase P3~P9로 정의하고 Codex로 실행한다.

- 설계 요구사항(헌법): `07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md`
- Codex 실행 프롬프트: `08_DATA_DRIVEN_FULL_CUTOVER_CODEX_PROMPT.md`
- 남은 구조 설계 계획서(P3 진행분 반영): `09_DATA_DRIVEN_REMAINING_STRUCTURE_DESIGN.md`

권장 순서: `P3(스킬 밸런스) -> P4(timing contract 분리) -> P5(봇 AI) -> P6(미니언/웨이브/맵) -> P7(네트워크 식별자) -> P8(legacy 삭제) -> P9(Inspector/Hot-Reload, 선택)`

진행 실측(2026-06-23, 미커밋 working tree): P0~P2 완료. P3 진행 중 — skill-effect 원자(`SkillAtomData.h::eSkillEffectParamId`) + `SkillEffectGameplayDefs.json` + `GameplayDefinitionQuery::ResolveSkillEffectParam` 메커니즘으로 Annie/Ashe/Jax/Kalista/Riven/Fiora/LeeSin 전환(reader 76, hardcodeCandidate 156, pack hash 0x58295D30). fallback 상수는 회귀 방지로 유지 중 → P3d에서 일괄 삭제 예정.

## 비코드 트랙 (참고)

북극성 4(Perforce)는 코드 리팩터링이 아니다. 기존 `.md/TODO/05-15/07_PERFORCE_P4_WORKFLOW_DEMO.md`가 소유한다. 본 파이프라인 범위 밖으로 둔다.

## Tier 2 후속 (S6 reflection 설계 메모)

```text
- 최소 구현: struct별 FieldDescriptor 목록(name, offset, type) + REGISTER 매크로.
- 제너릭 ImGui 바인더: float->DragFloat, Vec4->ColorEdit4.
- 또는 codegen: *_Def.h 파싱 -> *_Def.reflect.cpp 생성.
- 적용 1순위: SpawnLoadoutPolicyDef, ObjectGameData def, *_Tuning.h(visual).
- 도입 조건: S4 수동 인스펙터가 가치 증명된 뒤. 그 전에는 손-인스펙터 유지.
```

## 전 세션 공통 검증 기준

```text
- 빌드: Server/Client Debug x64 통과.
- parity: resolver 반환값이 legacy 하드코딩과 동일(의도적 변경은 별도 patch).
- forbidden dependency: Engine이 Shared/GameSim/LoL을 새로 include하지 않는다.
- server authority: client가 gameplay truth를 만들지 않는다(F5 smoke로 확인, 서버 로그만으로 판정 금지).
- visual parity: visual timing seed(11 시리즈)와 충돌하지 않는다.
```
