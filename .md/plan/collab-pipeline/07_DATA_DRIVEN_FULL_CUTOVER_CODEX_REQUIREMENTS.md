# 07 Data-Driven 완전 컷오버 — Codex 설계 요구사항

작성일: 2026-06-23

이 문서는 Winters의 데이터 주도(Data-Driven) 구조를 "완벽 리팩터링" 수준까지 끝내기 위한 Codex 작업의 **설계 요구사항(requirements)**이다. 실제 Codex에게 줄 실행 프롬프트는 `08_DATA_DRIVEN_FULL_CUTOVER_CODEX_PROMPT.md`에 있다. 이 문서는 그 프롬프트가 지켜야 하는 목표·불변 규칙·Phase·게이트·완료 정의를 고정한다.

상위 북극성 문서: `Downloads/04_협업_파이프라인.md`, `.md/plan/collab-pipeline/00_INDEX_DATA_DRIVEN_ENTITY_PIPELINE.md`
직전 결과 보고서: `.md/TODO/06-23/WINTERS_DATA_DRIVEN_SERVERPRIVATE_SPAWN_OBJECT_PACK_REPORT.md`, `WINTERS_DATA_DRIVEN_QUERY_CUTOVER_REPORT.md`

---

## 0. 용어와 범위

### 0-1. "Data Oriented" 해석

이 작업에서 목표는 **Data-Driven(데이터 주도) 콘텐츠 아키텍처**다. 즉 gameplay/balance/visual 값을 코드 리터럴이 아니라 JSON authoring → generated immutable pack → read-only runtime query로 흐르게 만든다.

이것은 ECS 메모리 레이아웃(SoA/AoS) 관점의 Data-Oriented Design(DOD)과는 다른 축이다. 메모리 레이아웃 최적화는 이 컷오버의 목표가 아니며, 건드리지 않는다. (DOD 최적화가 필요하면 별도 perf 세션으로 분리한다.)

### 0-2. "완벽 리팩터링"의 정의

아래 두 조건이 동시에 성립하면 완료로 본다.

```text
1. gameplay/balance/visual 값을 소유하는 코드 리터럴이 0 이다.
   (런타임 코드는 generated pack만 읽는다. constexpr balance, 하드코딩 SkillDef,
    하드코딩 AI 프로필, 하드코딩 wave tuning이 남아 있지 않다.)
2. legacy value-owning 경로(DB/table/resolver-with-defaults)가 reader 0 도달 후 삭제되어 있다.
   (ChampionGameDataDB, ChampionStatsRegistry, ChampionRuntimeDefaults 기본값,
    MinionCombatDef 하드코딩, ChampionAIPolicy 하드코딩, ServerMinionTuning balance 상수 등)
```

단, **placement(맵 좌표·레인 웨이포인트·스폰 위치 fallback)**은 balance 값이 아니므로 컷오버 대상이지만 우선순위가 낮고, gameplay balance와 분리해서 다룬다.

### 0-3. 비목표 (Non-Goals)

```text
- ECS 메모리 레이아웃/캐시 최적화 (DOD)
- 렌더러/RHI/Engine public API 변경
- 신규 gameplay 기능 추가나 밸런스 변경 (값은 byte-identical 유지)
- Perforce 전환, Hot-Reload 운영 도입 (Tier 2, ownership 컷오버 완료 후)
```

---

## 1. 최종 목표 상태 (End-State)

### 1-1. 정규 흐름 (Canonical Flow)

모든 gameplay/visual 값은 단 하나의 흐름만 탄다.

```text
authoring JSON (사람이 편집)
  -> Build-LoLDefinitionPack.py (정규화 + 검증 + build hash)
  -> generated immutable pack (.generated.cpp)
  -> read-only runtime query (GameplayDefinitionQuery / pack accessor)
  -> entity assembly (server spawn / hook)
  -> GameSim components
```

프레임 코드는 값의 출처를 몰라야 한다. 프레임 코드가 묻는 것은 오직:

```text
entity + slot + 현재 tick definitions -> gameplay fact
```

### 1-2. 소유권 매트릭스 (Ownership Matrix)

```text
Shared/GameSim   = 정의 "모양"(shape) + deterministic component contract + read-only query 인터페이스
ServerPrivate    = 서버 권위 gameplay/balance 값 (Server 바이너리에만 컴파일)
ClientPublic     = 시각/연출 값 (Client 바이너리에만 컴파일)
SharedContract   = 네트워크/세이브/매니페스트용 안정 식별자 (DefinitionKey, manifest)
Server runtime   = pack을 읽어 entity를 조립 (값을 소유하지 않음)
Client runtime   = replicated state 또는 ClientPublic visual data만 소비 (gameplay truth 생성 금지)
```

### 1-3. 식별자 규칙 (Identity Rules) — 이미 고정됨, 유지한다

```text
DefinitionKey  : FNV-1a 32-bit, 안정(persistent). 네트워크/세이브/매니페스트 경계 식별.
ChampionDefId  : dense 1-based, pack-local 전용. 네트워크로 보내지 않는다.
SkillDefId     : dense 1-based, pack-local 전용.
EntityHandle   : process-local 생명주기(gen+index). JSON/네트워크/세이브 경계를 넘지 않는다.
eChampion      : legacy enum. 컷오버 동안 bridge로만 허용. 최종적으로 네트워크는 DefinitionKey로 이관.
```

정의: `Shared/GameSim/Definitions/DefinitionIds.h`

---

## 2. 불변 규칙 (Invariants) — 모든 Phase에서 위반 금지

```text
I1. 의존 방향 불변
    Shared는 ServerPrivate/ClientPublic/Engine/Client/Renderer/DX11을 include하지 않는다.
    ServerPrivate 값은 Client 바이너리에 들어가지 않는다.
    ClientPublic 값은 Server gameplay 권위 경로에 들어가지 않는다.

I2. 값 소유권 단일화
    런타임 코드는 값을 소유하지 않는다. 값은 generated pack이 소유한다.
    Shared struct는 field name + type + deterministic layout(모양)만 갖는다.

I3. 결정성(determinism) 보존
    순수 컷오버 slice는 동작을 바꾸지 않는다 => SimLab same-seed 해시가 변하지 않아야 한다.
    값은 byte-identical로 이관한다. (의도적 밸런스 변경은 이 컷오버의 범위가 아니다.)
    timing은 tick 기반 결정값으로 유지한다. float 경로를 새로 도입하지 않는다.

I4. 무회귀(no-regression) 순서
    legacy 값 추출 -> pack/resolver 패리티 -> 패리티 리포트 -> reader 전환 -> ownership scan+build+smoke
    -> reader 0 확인 후에만 legacy 삭제. 이 순서를 건너뛰지 않는다.

I5. 클라이언트는 gameplay truth를 새로 만들지 않는다
    예외는 명시된 local-only smoke path 뿐이다. 그 path도 ClientPublic/smoke contract 뒤로 격리한다.

I6. 생성물은 하드코딩이 아니다
    *.generated.* 는 의도된 산출물이다. audit 제외 목록을 유지한다.
    단, generated를 사람이 손으로 편집하지 않는다. 오직 generator를 통해서만 바뀐다.

I7. Engine/RHI 경계 불변
    Engine public header에 LoL/Server/GameSim/DX11 concrete 타입을 새로 노출하지 않는다.
    Engine public header 변경 시 검증에 UpdateLib.bat(SDK sync)을 남긴다.
```

---

## 3. 현재 상태와 남은 표면

### 3-1. 완료 (P0~P2)

```text
P0 Foundation : DefinitionKey/ChampionDefId/SkillDefId/EntityHandle,
                immutable pack 생성, JSON 소유권 분리(ServerPrivate/ClientPublic/SharedContract)
P1 Spawn/Object: SpawnObjectGameplayDefs.json -> SpawnObjectDefinitionPack
                -> ServerData::GetLoLSpawnObjectDefinitionPack() -> GameRoom/GameRoomSpawn
P2 Query Bridge: GameplayDefinitionQuery (TickContext::pDefinitions) 로 고빈도 reader 집약
```

### 3-2. 남은 하드코딩/소유 표면 (컷오버 대상)

```text
A. 챔피언 스킬 밸런스 constexpr (~234개)  : Shared/GameSim/Champions/*/*GameSim.cpp
   (damage, cooldown, range, CC duration, scaling, radius)
B. 클라 SkillDef 하드코딩 (16챔피언 x 5슬롯 = 80) : Client/Private/GameObject/Champion/*/*_Registration.cpp
   (cooldownSec, rangeMax, manaCost, castFrame, recoveryFrame, lockDurationSec)
C. ChampionGameDataDB 직접 reader (41 사이트)  : 그 중 다수는 visual yaw / action-visual timing
   (contract 분리 선행 필요)
D. 봇 AI 정책 하드코딩            : Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp (16 프로필 + 콤보)
   + bot skill-rank policy(AssignDefaultBotSkillRanks 류)
E. 미니언/웨이브/맵 placement     : Shared/GameSim/Definitions/MinionCombatDef.h(ResolveMinionCombatDef 클라 fallback),
   Server/Public/Game/ServerMinionTuning.h(17 상수), GameRoomSpawn/MapSpawnPoints placement fallback
F. 네트워크 식별자               : snapshot/command schema가 아직 ubyte championId 사용
   (안정 DefinitionKey 미이관)
G. legacy value-owning 경로      : ChampionGameDataDB, ChampionStatsRegistry,
   ChampionRuntimeDefaults 기본값, SkillTable/ChampionTable (reader 0 도달 후 삭제 대상)
```

(상세 파일·라인·카운트는 작업 시점에 `Collect-LoLLegacyDataAudit.ps1`와 `rg`로 재확인한다. 이 문서에 라인 번호를 박제하지 않는다.)

---

## 4. Phase 로드맵 (P3 ~ P9)

각 Phase는 독립 slice로 쪼개 실행하며, slice 단위로 §5 원자 프로토콜과 §6 게이트를 통과해야 한다. Phase 간 순서는 의존성 때문에 권장 순서가 있다.

### P3. 챔피언 스킬/능력 밸런스 값 데이터화

```text
목표: A(스킬 밸런스 constexpr) + B(클라 SkillDef 하드코딩)를 데이터로 이관.
산출: SkillGameplayDef effect/scaling spec 확장 또는
      Data/LoL/ServerPrivate/Gameplay/SkillEffectDefs.json 신설.
      SkillScalingRegistry(이미 존재) + champions.json scalingTableId 경로 활용.
reader 전환: 각 *GameSim.cpp 훅이 constexpr 대신 pack/query에서 damage/duration/range/scaling을 읽는다.
            클라 _Registration.cpp는 값을 pack/ClientPublic에서 읽거나, 권위 값은 서버 replicated로 대체.
종료조건: 스킬 밸런스 constexpr reader 0, SimLab same-seed 해시 불변, audit balance 카운트 감소.
주의: damage/CC는 ServerPrivate(권위). castFrame/recoveryFrame/animPlaySpeed는 ClientPublic(연출) — P4와 경계 맞춤.
```

### P4. gameplay action timing vs client visual timing contract 분리

```text
목표: C의 잔여 reader를 0으로 만드는 선행 분리. (QUERY_CUTOVER 보고서가 명시한 blocker)
산출: 권위 action timing(action-lock ticks, stage window, windup ticks)은 ServerPrivate/gameplay pack.
      연출 timing(visualYawOffset, animPlaySpeed, castFrame, recoveryFrame)은 ClientPublic visual pack.
reader 전환: CommandExecutor/MoveSystem/Zed/Irelia의 visual yaw read를 ClientPublic 경로 또는
            서버 replicated action data로 분리. Scene_InGame 클라 prediction read도 ClientPublic query로.
종료조건: ChampionGameDataDB 직접 gameplay reader 0(visual은 ClientPublic로 이동), parity 0, 결정성 불변.
```

### P5. 봇 AI 정책 데이터화

```text
목표: D(ChampionAIPolicy 프로필/콤보 + bot skill-rank policy)를 데이터로 이관.
산출: Data/LoL/ServerPrivate/AI/ChampionAIProfiles.json (+ generated server pack).
      프로필(range/aggression/weights), skill rule, combo step, bot skill-rank progression.
reader 전환: ChampionAIPolicy / GameRoomChampionAI / AI setup이 pack에서 프로필을 읽는다.
종료조건: AI 프로필/콤보/skill-rank 하드코딩 reader 0, SimLab 봇 시나리오 결정성 불변.
주의: AI는 command를 생산만 한다(truth 직접 변경 금지) — 기존 규칙 유지.
```

### P6. 미니언/웨이브/맵 placement 데이터화

```text
목표: E를 데이터로 이관.
산출: minion combat(이미 SpawnObjectDefinitionPack.ResolveMinion 존재) + wave timing + placement를 ServerPrivate JSON로.
reader 전환:
  - Client local smoke의 ResolveMinionCombatDef() fallback 제거 또는 ClientPublic/smoke-only contract 뒤로 격리.
  - ServerMinionTuning balance 상수(wave interval, scan, separation 등)를 pack으로.
  - GameRoomSpawn/MapSpawnPoints placement fallback을 stage/map 데이터로.
종료조건: MinionCombatDef 하드코딩 reader 0, wave/placement 하드코딩 reader 0, 결정성 불변.
주의: wave "timing"과 minion "combat stat"은 서로 다른 atom — 결합 끊고 별도 정의로.
```

### P7. 네트워크 식별자 안정화

```text
목표: F. snapshot/command의 ubyte championId를 안정 DefinitionKey 경로로 이관.
산출: schema 필드 추가(DefinitionKey:uint), 송수신 변환, 하위호환 처리.
종료조건: 네트워크가 dense pack-local id가 아닌 안정 식별자를 사용. SimLab/네트워크 smoke 통과.
주의: flatbuffers schema 변경 시 run_codegen 실행. ChampionDefId/SkillDefId는 네트워크로 보내지 않는다(I3/식별자 규칙).
```

### P8. Legacy 삭제

```text
목표: G. reader 0 도달한 legacy value-owning 경로 삭제.
대상: ChampionGameDataDB, ChampionStatsRegistry, ChampionRuntimeDefaults 기본값,
      SkillTable, ChampionTable, MinionCombatDef 하드코딩, ChampionAIPolicy 하드코딩,
      ServerMinionTuning balance 상수, 구 generated.
선행: 각 대상 reader count == 0 을 audit/rg로 증명. build/SimLab/smoke 통과.
종료조건: 삭제 후 전체 파이프라인 PASS, generated pack이 유일한 권위 source.
```

### P9. (Tier 2, 선택) Editor Inspector + Hot-Reload

```text
목표: 협업 가치 증명. 00_INDEX의 S4/S5.
산출: 읽기전용 EntityInspectorPanel(서버 override command 경유 편집),
      dev-only CDataHotReload + JSON overlay(release 비활성).
선행: P3~P8 ownership 컷오버 완료(북극성: "Hot Reload/Perforce는 ownership/cutover 게이트 완료 후 시작").
종료조건: dev 빌드에서 JSON 편집 -> 재시작 없이 live 반영, release 영향 0.
```

### 권장 실행 순서

```text
P3 -> P4 -> P5 -> P6 -> P7 -> P8 -> (P9)
근거: P4는 P8(ChampionGameDataDB 삭제)의 선행 blocker. P7은 P8 전 네트워크 안정화.
      P3~P6는 도메인별로 병렬 가능하나, 각 slice는 독립 게이트를 통과해야 한다.
```

---

## 5. 원자 실행 프로토콜 (Per-Slice, 무회귀)

모든 slice는 아래 순서를 정확히 따른다. 한 slice는 "한 도메인의 한 reader 묶음"이다. 너무 크게 잡지 않는다.

```text
S-1. 추출 : legacy 리터럴/하드코딩 값을 식별하고, byte-identical 값을 authoring JSON에 기입.
S-2. 생성 : Build-LoLDefinitionPack.py가 해당 값을 정규화/검증/hash 포함해 generated pack에 emit.
S-3. 패리티 : pack 값 == legacy 값(byte-unit) 증명. 필요한 parity export/리포트 생성.
S-4. 전환 : 소비 경로(reader)만 pack/query로 바꾼다. 이 단계에서 값/동작을 바꾸지 않는다.
S-5. 검증 : §6 게이트(freshness, audit, parity, build, SimLab same-seed/seed+1, git diff) 통과.
S-6. 삭제 : 해당 legacy 경로 reader count == 0 확인 후에만 삭제. (보통 Phase 말미 P8에서 일괄)
S-7. 보고 : §7 리포트 1건 작성(`.md/TODO/<날짜>/`).
```

핵심: **S-4 까지는 SimLab same-seed 해시가 변하면 안 된다.** 변했다면 값이 byte-identical이 아니거나 동작이 바뀐 것이므로 중단하고 원인 분석.

---

## 6. 검증 게이트 (모든 slice 공통)

실행: `powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`

```text
G0 Freshness : Build-LoLDefinitionPack.py --check (generated가 source와 일치)
G1 Build     : GameSim / Server / Client / SimLab Debug x64 PASS
G2 Parity    : pack/atom == legacy (byte-unit). Client visual timing parity mismatchCount == 0
G3 Ownership : Collect-LoLLegacyDataAudit.ps1 PASS. 금지 의존 0.
               대상 도메인 하드코딩 카운트가 이전 대비 감소(또는 0).
G4 Determinism: SimLab same-seed 해시가 순수 컷오버에서 불변.
               seed+1 해시는 same-seed와 달라야 한다(결정성 살아있음 증명).
G5 Hygiene   : git diff --check PASS (trailing whitespace/CRLF 없음).
```

추가 수동 게이트(F5 런타임 smoke, 서버 로그 기준):

```text
G6 Runtime smoke : spawn / sim component / skill / minion wave / structure / jungle /
                   projectile / action / visual 이 회귀 없이 동작.
                   (서버 로그로 판정. 클라 visual 성공을 로그만으로 단정하지 않는다 — compass 규칙.)
```

Engine public header를 건드린 slice는:

```text
G7 SDK sync : UpdateLib.bat 실행 후 EngineSDK/inc 동기화 확인.
```

---

## 7. 파이프라인 통합 요구사항

```text
- 새 authoring JSON은 Data/LoL/ServerPrivate/** 또는 Data/LoL/ClientPublic/** 아래에만 둔다.
  (gameplay/balance -> ServerPrivate, visual -> ClientPublic)
- generator(Build-LoLDefinitionPack.py)를 확장해:
  새 JSON을 읽고, 필드 정규화/검증, build hash에 포함, generated pack에 emit.
  값 누락/중복 키/범위 위반은 generator가 에러로 막는다.
- audit(Collect-LoLLegacyDataAudit.ps1)을 확장해:
  새로 데이터화된 도메인의 잔여 하드코딩 패턴을 카운트한다.
  generated 디렉터리는 제외 유지(I6).
- verify(Verify-LoLDataDrivenPipeline.ps1)는 게이트 순서를 유지한다.
  새 parity export가 필요하면 freshness/parity 단계에 추가한다.
- generated 파일은 손으로 편집하지 않는다. 항상 generator 재실행으로만 변경.
```

---

## 8. 완료 정의 (Overall DoD)

```text
DoD-1. gameplay/balance/visual 값을 소유하는 런타임 코드 리터럴 0.
       (A/B/D/E 도메인 하드코딩 audit 카운트 0)
DoD-2. ChampionGameDataDB / ChampionStatsRegistry / ChampionRuntimeDefaults 기본값 /
       SkillTable / ChampionTable / MinionCombatDef 하드코딩 / ChampionAIPolicy 하드코딩 /
       ServerMinionTuning balance 상수 삭제됨 (reader 0 후).
DoD-3. 네트워크가 안정 DefinitionKey 식별자를 사용 (ubyte championId 의존 제거).
DoD-4. generated pack이 유일한 권위 gameplay source. 프레임 코드는 출처를 모른다.
DoD-5. 전체 Verify 파이프라인 PASS:
       freshness / audit / parity(mismatch 0) / GameSim+Server+Client+SimLab build /
       SimLab same-seed 안정 + seed+1 상이 / git diff --check.
DoD-6. 소유권 경계 성립: 한 역할(PLANNER/ARTIST/DEV/QA)이 자기 데이터만 바꿔
       챔피언 추가/튜닝을 코드 변경 없이 할 수 있다. (협업 게이트 G5, 00_INDEX 기준)
DoD-7. (선택) P9 Inspector/Hot-Reload가 dev에서 동작, release 영향 0.
```

---

## 9. 리스크와 롤백

```text
R1. 결정성 깨짐: SimLab same-seed 해시가 컷오버에서 바뀌면, 값 byte-parity 실패 또는
    float/순서 차이 도입. -> 해당 slice 되돌리고 값/연산 순서 재확인.
R2. 의존 역전: Shared가 ServerPrivate/Engine을 끌어오면 I1 위반. -> include 방향 재설계.
R3. 클라 권위 누수: ServerPrivate 값이 Client 바이너리에 들어가면 I1 위반. -> ClientPublic/replicated로 분리.
R4. 빌드 산출물 꼬임(vc143.pdb lock, .pch invalid): 코드 실패로 오판 금지.
    -> Engine/GameSim/Server/Client/SimLab MSBuild Clean 후 파이프라인 재실행.
R5. legacy 조기 삭제: reader 0 미확인 상태 삭제 금지(I4). -> audit/rg로 reader count 먼저 증명.
롤백 단위: slice 단위. 각 slice는 독립 커밋/리포트로 남겨 부분 롤백이 가능하게 한다.
```
