# 08 Data-Driven 완전 컷오버 — Codex 실행 프롬프트

이 문서의 "프롬프트 본문" 블록을 Codex에 그대로 붙여넣어 사용한다. Phase는 한 번에 하나만 실행한다(`이번 실행 Phase` 한 줄만 바꾼다). 설계 근거·게이트·완료 정의는 `07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md`에 있고, 프롬프트는 그 문서를 참조한다.

---

## 사용법

```text
1) 아래 "프롬프트 본문"을 복사한다.
2) [이번 실행 Phase] 줄을 P3 ~ P9 중 하나로 지정한다.
3) [이번 slice 범위] 줄에 이번에 끝낼 reader 묶음 1개를 적는다(작게 잡는다).
4) Codex 실행 -> 리포트 확인 -> 다음 slice로 [이번 slice 범위]만 교체해 반복한다.
```

---

## 프롬프트 본문 (복사 영역)

```text
너는 Winters 리포지토리(C:/Users/tnest/Desktop/Winters)에서 Data-Driven 콘텐츠
아키텍처 "완전 컷오버"를 수행하는 시니어 엔지니어다. 목표는 gameplay/balance/visual
값을 코드 리터럴에서 제거하고, JSON authoring -> generated immutable pack -> read-only
runtime query -> entity assembly -> components 흐름만 남기는 것이다.

[이번 실행 Phase] P3
[이번 slice 범위] (예: Annie/Jax/Ashe 의 스킬 damage/CC constexpr 를 SkillEffect 데이터로 이관)

=== 0. 먼저 읽어라 (작업 전 필수) ===
- .md/plan/collab-pipeline/07_DATA_DRIVEN_FULL_CUTOVER_CODEX_REQUIREMENTS.md  (이 작업의 헌법)
- .md/plan/collab-pipeline/00_INDEX_DATA_DRIVEN_ENTITY_PIPELINE.md            (북극성/소유권)
- .md/architecture/WINTERS_CODEBASE_COMPASS.md                                (계층/의존 규칙)
- .claude/gotchas.md
- .md/TODO/06-23/WINTERS_DATA_DRIVEN_*_REPORT.md                              (직전 컷오버 상태)
07 문서의 §2 불변 규칙(I1~I7), §5 원자 프로토콜, §6 게이트, §8 완료 정의를 이번 작업의
구속 조건으로 삼는다. 충돌 시 07 문서가 우선한다.

=== 1. 핵심 좌표 (현재 구조) ===
authoring source:
  - Data/Gameplay/ChampionGameData/champions.json            (챔피언/스킬/visual 1차 source)
  - Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json
generator/audit/verify:
  - Tools/LoLData/Build-LoLDefinitionPack.py
  - Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
  - Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
  - Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1
generated (손으로 편집 금지):
  - Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp
  - Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp
runtime accessor/query:
  - ServerData::GetLoLGameplayDefinitionPack(), ServerData::GetLoLSpawnObjectDefinitionPack()
  - Shared/GameSim/Definitions/GameplayDefinitionQuery.{h,cpp}  (TickContext::pDefinitions)
identity:
  - Shared/GameSim/Definitions/DefinitionIds.h
legacy(삭제 후보, reader 0 전엔 건드리지 말 것):
  - Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.{h,cpp}
  - ChampionStatsRegistry, ChampionRuntimeDefaults, MinionCombatDef 하드코딩,
    ChampionAIPolicy 하드코딩, ServerMinionTuning balance 상수

작업 시작 시 라인 번호를 추측하지 말고 rg/audit으로 현재 위치를 재확인하라.
하드코딩 인벤토리는 다음으로 새로 측정한다:
  rg "constexpr f32_t k" Shared/GameSim/Champions
  rg "ChampionGameDataDB::" Client Server Shared
  powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1

=== 2. 절대 규칙 (위반 시 즉시 중단) ===
- I1 의존 방향: Shared는 ServerPrivate/ClientPublic/Engine/Client/Renderer/DX11 include 금지.
  ServerPrivate 값은 Client 바이너리에, ClientPublic 값은 Server gameplay 권위에 들어가지 않는다.
- I2 값 소유 단일화: 런타임 코드는 값을 소유하지 않는다. Shared struct는 모양만.
- I3 결정성: 순수 컷오버는 동작을 안 바꾼다. 값은 byte-identical 이관.
  timing은 tick 기반 유지, 새 float 경로 도입 금지.
- I4 무회귀 순서: 추출 -> pack 패리티 -> 패리티 증명 -> reader 전환 -> 게이트 -> (reader 0 후) 삭제.
- I5 클라는 gameplay truth 새로 생성 금지(명시된 local smoke 예외만, 그것도 격리).
- I6 generated는 산출물. 손편집 금지, generator로만 변경. audit 제외 유지.
- I7 Engine public header에 LoL/Server/GameSim/DX11 concrete 노출 금지. 바꾸면 UpdateLib.bat 남김.
- 밸런스 수치 자체를 바꾸지 마라(연출/밸런스 변경은 이 작업 범위 밖).
- gameplay/balance 값 -> ServerPrivate. visual/연출 값 -> ClientPublic. 절대 섞지 마라.

=== 3. 작업 루프 (이번 slice를 이 순서로) ===
S-1 추출: 이번 slice의 legacy 리터럴을 식별하고 byte-identical 값을 authoring JSON에 기입.
          (gameplay -> Data/LoL/ServerPrivate/**, visual -> Data/LoL/ClientPublic/**)
S-2 생성: Build-LoLDefinitionPack.py를 확장(필요 시)해 값 정규화/검증/build hash 포함 후 emit.
          generator가 누락/중복키/범위위반을 에러로 막게 한다. 그다음 generator를 실제 실행한다.
S-3 패리티: pack 값 == legacy 값(byte-unit) 증명. 필요하면 parity export 추가.
S-4 전환: 소비 reader만 pack/GameplayDefinitionQuery 로 바꾼다. 값/동작은 그대로.
          여기까지 SimLab same-seed 해시가 변하면 STOP 하고 원인 분석(값 parity 실패 의심).
S-5 검증: 아래 명령 전부 PASS.
S-6 삭제: 이번 slice가 마지막 reader였다면(audit/rg로 reader 0 증명) 해당 legacy 삭제.
          확신 없으면 삭제하지 말고 "reader 잔존" 으로 리포트에 남긴다.
S-7 보고: 아래 리포트 형식으로 .md/TODO/<오늘날짜>/ 에 1건 작성.

=== 4. 검증 명령 (S-5) ===
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
이 스크립트가 다음을 순서대로 본다:
  Freshness(--check) / Legacy audit / Visual timing parity(mismatch 0) /
  GameSim+Server+Client+SimLab Debug x64 build / SimLab same-seed & seed+1 / git diff --check
추가 기대값:
  - SimLab same-seed 해시: 순수 컷오버에서 직전 값과 동일해야 한다.
  - SimLab seed+1 해시: same-seed와 달라야 한다(결정성 살아있음).
  - 이번 도메인 audit 하드코딩 카운트: 직전보다 감소(또는 0).
빌드 산출물 꼬임(vc143.pdb lock, WintersEngine.pch invalid)은 코드 실패로 오판하지 말 것.
그 경우 Engine/GameSim/Server/Client/SimLab을 MSBuild Clean 후 파이프라인 재실행.
Engine public header를 바꿨다면 UpdateLib.bat 실행 후 EngineSDK/inc 동기화 확인.

=== 5. 리포트 형식 (S-7) ===
경로: .md/TODO/<YYYY-MM-DD>/WINTERS_DATA_DRIVEN_<PHASE>_<SLICE>_REPORT.md (한국어)
포함:
  - 결과 한 줄 (이번 slice가 무엇을 데이터화했는가)
  - 원자 흐름 (data value -> JSON -> generated pack -> query -> component)
  - 코드 변경 (추가/수정/삭제 파일 목록)
  - byte-parity 증거 (legacy 값 == pack 값)
  - 검증 결과 (위 게이트 출력: build hash, SimLab same-seed/seed+1, audit 카운트, parity mismatch, git diff)
  - 남은 의심 지점 (이 도메인에서 아직 하드코딩으로 남은 reader, 다음 slice 후보)

=== 6. 출력 기대 ===
- 이번 실행은 [이번 slice 범위] 한 묶음만 끝낸다. 범위를 임의로 넓히지 마라.
- 불확실하면 추측 말고 "확인 필요"로 표시하고 멈춘다.
- 끝에는 반드시 §4 검증을 돌린 실제 결과와 §5 리포트 경로를 보고한다.
- 다음에 실행하면 좋은 slice 1~3개를 우선순위와 함께 제안한다.
```

---

## Phase별 [이번 slice 범위] 예시

```text
P3 (스킬 밸런스):
  - "Annie Q/W/R damage + stun/shield constexpr -> ServerPrivate SkillEffect 데이터, AnnieGameSim 훅이 pack에서 읽기"
  - "클라 Annie_Registration.cpp cooldownSec/rangeMax/manaCost -> pack 또는 서버 replicated 로 대체"
P4 (timing contract 분리):
  - "visualYawOffset reader(CommandExecutor/MoveSystem/Zed/Irelia) -> ClientPublic visual query 로 분리"
  - "action-lock ticks / stage window 권위 timing -> ServerPrivate gameplay pack 단일화"
P5 (봇 AI):
  - "ChampionAIPolicy Ashe/Jax 프로필+콤보 -> Data/LoL/ServerPrivate/AI/ChampionAIProfiles.json"
  - "AssignDefaultBotSkillRanks progression -> AI pack"
P6 (미니언/웨이브/맵):
  - "Client local ResolveMinionCombatDef() fallback 제거/격리"
  - "ServerMinionTuning kWaveIntervalTicks/kInitialWaveDelayTicks/scan -> ServerPrivate wave 데이터"
P7 (네트워크 식별자):
  - "Snapshot.fbs championId(ubyte) 옆에 DefinitionKey(uint) 추가, 송수신 변환, codegen"
P8 (legacy 삭제):
  - "ChampionGameDataDB reader 0 증명 후 ChampionGameDataDB.{h,cpp} + 구 generated 삭제"
P9 (Tier 2):
  - "EntityInspectorPanel 읽기전용 + 서버 override command" / "CDataHotReload dev-only JSON overlay"
```

---

## 운영 메모

```text
- 한 slice = 한 도메인의 한 reader 묶음. 크게 잡으면 결정성 게이트 디버깅이 불가능해진다.
- P4를 P8(ChampionGameDataDB 삭제) 앞에 둔다. visual/timing contract가 안 갈리면 reader가 0이 안 된다.
- P7을 P8 앞에 둔다. 네트워크가 안정 식별자로 가야 legacy enum 의존을 끊는다.
- 매 slice 후 git 커밋 1개 + 리포트 1개. 부분 롤백 가능하게 유지.
- .md 문서/리포트는 한국어. 코드 식별자/경로/명령어는 원문 유지.
```
