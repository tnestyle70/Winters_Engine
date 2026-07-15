# Winters Data Architecture (게임 데이터 북극성)

작성일: 2026-07-09. GameSim/GameRoom/애니메이션 수치/레벨 스케일링/캐릭터 스탯 데이터의 소유권·흐름·마이그레이션을 고정하는 문서. 원칙은 `WINTERS_DESIGN_PHILOSOPHY.md`(P1~P4)와 compass의 DataDriven Definition Boundary. 2026-07-09 데이터 전수 감사(5개 영역, 위반 73건) 실측이 근거다. 감사 원본: 세션 산출물 `data_{shared,server,client,anim,hardcoded}.json`.

## 1. 데이터 소유권 매트릭스 (목표 = 현재의 방향)

```text
[Authoring]  Data/Gameplay/ChampionGameData/champions.json + Data/LoL/ServerPrivate/Gameplay/*.json
             └ Tools/LoLData/Build-LoLDefinitionPack.py, Tools/ChampionData/build_champion_game_data.py (cook)
                     │
        ┌────────────┼──────────────────────┐
        ▼            ▼                      ▼
[SharedContract]  [ServerPrivate]        [ClientPublic]
Shared/GameSim/   Server/Private/Data/   Client/Private/Data/
Definitions/*     LoLGameplayDefinitions LoLVisualDefinitions
(타입+결정론 조회,  .generated.cpp         .generated.cpp
 값 없음)          (스탯/스킬 수치/성장)     (모델/텍스처/애님 키/비주얼 프레임)
        │            │                      │
        ▼            ▼                      ▼
   TickContext.pDefinitions (Server가 주입) → Shared 시스템/챔피언 sim이 조회
                     │
                     ▼
   Snapshot/Event (StatComponent 복제) ────→ Client는 복제 스탯 + 비주얼 팩만 소비
```

- **id 규칙**: `eChampion`(u8, 명시값)이 현재의 wire/save identity (Hello/Lobby/Snapshot의 championId:ubyte). `DefinitionKey`(u32)는 pack 식별용으로 존재하나 아직 wire 미사용. `ChampionDefId/SkillDefId`(dense u16)는 pack-local — 컴포넌트(ChampionDefinitionComponent)까지만 허용, 직렬화 금지. **결정 필요(로드맵 D-6)**: 150챔프 스케일에서 eChampion(255 cap)을 유지할지 DefinitionKey를 wire로 승격할지.
- **레벨 스케일링**: 성장 수치(hpPerLevel 등)는 ChampionStatBlock(ServerPrivate pack)이 소유, `CStatSystem::BuildBaseStats`가 레벨 적용, 결과는 StatComponent로 복제. 클라는 성장 공식을 재계산하지 않는다.
- **애니메이션 수치의 이원화 (규칙)**: *게임플레이 타이밍*(lock duration, windup, 스테이지 윈도)은 ServerPrivate pack → 서버 tick이 판정. *비주얼 재생*(animKey, visualCastFrame, visualPlaySpeed)은 ClientPublic — 서버가 준 action 시작/종료 tick 안에서 클라가 연출. 클라 비주얼 프레임이 서버 판정을 만들면 안 되고, 서버 타이밍이 클라 팩에 복사되면 drift 위험(§3-4).

## 2. 2026-07-09 적용분 (이 문서와 함께 반영됨)

- **폴백 가시화 (P1)** — legacy 삭제의 게이트 계측. "pack miss → legacy/fabricated 값" 지점 16곳에 bounded 트레이스:
  - `GameplayDefinitionQuery.cpp` 10개 resolve 함수의 legacy DB 폴백 (`[Data] pack miss -> legacy ...`, LogPackFallback/ResolveLegacyFallbackChampion 헬퍼)
  - `ChampionGameDataDB::ResolveStats`의 fabricated 600HP/55AD 블록, `StatSystem` 재계산 폴백, `GameRoomSpawn` 스폰 폴백
  - `ExperienceSystem` 보상 정의 미스 3곳 (`[Data] reward miss ...` — Turret/Structure/Jungle 보상은 미등록 상태임이 이제 보인다)
  - `CommandExecutor::EnqueueFallbackSkillDamage` 범용 폴백 데미지 사용 (`[Command] fallback skill damage ...`)
  - **운영 규칙**: 정상 로스터 스모크에서 이 로그가 나오면 데이터 팩 회귀다. 카운터가 0에 수렴한 조회 경로부터 legacy DB 리더를 제거한다.
- **ChampionDef Shared→Client 이동** — 비주얼 정의(애님 키/fbx/셰이더/텍스처/스폰 스케일)가 `Client/Public/GameObject/ChampionDef.h`로 이동. Shared/Server 리더 0 확인 후 이동(경계 규칙 "ClientPublic visual values compile only into Client" 충족). 잔여: `basicAttackRange` 필드는 gameplay 중복 — 복제 StatComponent.attackRange로 대체 후 삭제(D-3).
- **SkinRegistry/SkinDef 삭제** — 리더 0 (compass의 zero-reader 삭제 규칙). 스킨 기능 재개 시 ClientPublic 팩으로 신설.

## 3. 확정 위반 클러스터와 마이그레이션 슬라이스 (우선순위순)

### D-1. 훅으로 이동 (P3): CommandExecutor의 챔피언 특수케이스
`CommandExecutor.cpp`에 Kalista 패시브 대시(:819), Viego 소울(:1547, 2.25/0.72/5.0 하드코딩), Ezreal E(:1439), Yasuo/Yone/Annie/Sylas/Jax/Kindred 분기(:1994), hookId 스위치(:1065)가 잔존. 방향: `CGameplayHookRegistry`에 pre-cast 훅 변형(StageResolve/TargetResolve/CooldownPolicy) 추가 후 챔피언당 1슬라이스로 각자의 `*GameSim.cpp`로 이동, 수치는 skill effect param으로 (Yasuo/Zed가 이미 이 패턴 — `ResolveSkillEffectParam`). Bot AI는 GameCommand 생산자 원칙 유지 — 훅은 command 실행 내부의 분기이지 truth 직접 조작 경로가 아니다.

### D-2. 팩으로 이동: Shared에 하드코딩된 LoL 값
- `RewardRegistry.cpp:57` XP 커브/킬 골드/미니언 보상 (Client에도 컴파일됨!) → ServerPrivate pack에 RewardDefs 섹션 신설, 룸 시작 시 로드. 타입은 Shared 유지.
- `ItemDef.h:45` 아이템 카탈로그 (골드/스탯) → 동일 경로. `generate_itemshop_catalog_from_reference.py`를 Build-LoLDefinitionPack에 편입.
- `MinionCombatDef.h`, ward 수치(CommandExecutor:2540), 스킬 레벨링 규칙(:2483), CDR cap(:568) → SpawnObject/GameRules pack 섹션.
- `ChampionGameDataDB.cpp:84` windup 0.35 휴리스틱 → champions.json windupSec 필드로 cook.

### D-3. Shared 생성 게임플레이 팩의 Client 노출 해소
`ChampionGameData.generated.cpp`(Shared, 쿨다운/사거리/스탯)가 Client에 링크됨 — ServerPrivate 값이 클라 바이너리에 들어가는 상태. 경로: (a) 클라 HUD가 필요한 값(사거리 링, 쿨다운 표시)은 "HUD 힌트" ClientPublic 팩으로 별도 cook 또는 복제 확장, (b) §2의 폴백 카운터를 0으로 만든 뒤 Shared sim 리더를 TickContext.pDefinitions로 통일, (c) legacy DB와 `SkillTable.cpp`(클라 수기 쿨다운/마나 — 3중 소스) 삭제. `SkillTimingPanel.cpp`의 const_cast 테이블 변조는 튜너 스크래치 사본으로 교체.

### D-4. 애니메이션 타이밍 정합 (감사 anim 클러스터의 2대 발견)
- **공속-애니 미반영**: 서버는 BA 윈도를 attackSpeed로 스케일하는데 클라 BA 애님 재생속도는 복제 attackSpeed를 무시(EventApplier:1188) — 레벨업/아이템 공속 증가 시 애니와 판정이 어긋난다. 수정: PlayReplicatedActionVisual에서 playSpeed × (복제 attackSpeed / baseAttackSpeed).
- **windup 사장(死藏)**: 서버 BA 임팩트가 windup이 아니라 풀 윈도 끝에 발생(CommandExecutor:2427) — cook된 windup 데이터가 죽어 있고 클라 히트 비주얼(castFrame)과 어긋남. 수정: uImpactTick=start+windupTicks(공속 스케일), uEndTick=start+actionTicks 분리. **게임플레이 동작 변경이므로 단독 슬라이스 + 서버/클라 왕복 검증 필수.**
- SnapshotBuilder의 lock 산출 3중 경로(:95)는 GameplayDefinitionQuery 경유로 단일화. 복제 actionStartTick 기반 애님 시킹(Scene_InGameNetwork:963)은 중도 관전/재접속 개선용 후속.

### D-5. 클라 잔여
- 챔피언 등록 중복(Viego/Sylas 이중 authoring — 팩이 이김, 등록 블록 삭제), ZedFxPresets의 FindChampionDef 결합, 스폰 화이트리스트 if-체인(Scene_InGameLifecycle:494)과 상태 컴포넌트 if-체인(ChampionSpawnService:134) → 등록 콜백 레지스트리로. Structure_Manager의 터렛 수치 수기 사본(:436) → 스냅샷/팩 단일화. Yasuo_Tuning.h의 클라 데미지 값 → 네트워크 모드에서 죽은 코드인지 확인 후 local-sim 게이트.
- `SkillDef` 타입 자체의 Client 이동은 **보류**: GameplayHookContext.pDef(Shared)와 CommandExecutor 로컬 생성이 타입을 사용 — 훅 컨텍스트 계약 재설계(D-1)와 함께.

### D-6. id 체계 결정
Snapshot/Hello의 championId:ubyte(eChampion) vs DefinitionKey u32 — 결정 대기.
- ~~D-6a 팩 해시 핸드셰이크~~ **완료 (2026-07-09)**: Hello에 `dataBuildHash` 추가(끝 필드 추가라 하위 호환, 0=구버전 검사 생략). 서버는 `ChampionGameDataDB::GetBuildHash()`를 송신(GameRoomLobby::SendHelloToSessionLocked), 클라는 자기 해시와 비교해 불일치 시 `[Data] build hash mismatch` bounded 로그(SnapshotApplier::OnHello). 서로 다른 생성 데이터로 빌드된 클라/서버 조합이 "미묘하게 다른 수치"가 아니라 접속 로그로 보인다.

## 4. 데이터 분리 스코어카드 (2026-07-09 실측)

"기획자가 코드 수정 없이 값을 바꿀 수 있는가"를 기준으로 한 도메인별 현황. 채점: **1.0** = authored 데이터가 유일 소스(cook 경유, 코드 이중화 없음) / **0.5** = 팩·데이터 파일은 있으나 코드 이중화 또는 필드 갭 / **0** = 로직 코드 하드코딩.

| # | 도메인 | 현재 소스 | 점수 |
|---|---|---|---|
| 1 | 챔피언 기본 스탯+레벨 성장 | champions.json→ServerPrivate pack이 primary이나 legacy dual(ChampionGameData in Shared, StatsRegistry, fallback) 잔존 | 0.5 |
| 2 | 스킬 게임플레이(쿨다운/사거리/코스트/스테이지/이펙트 파람) | JSON→pack 완결(스모크 폴백 0) + legacy dual 잔존 | 0.5 |
| 3 | 소환사 주문 | pack 단일 | 1.0 |
| 4 | 스폰 오브젝트(구조물/정글/미니언 스탯) | SpawnObjectGameplayDefs.json→pack, 일부 필드 갭(정글 sightRange, 미니언 투사체) | 0.5 |
| 5 | 미니언 웨이브 구성/포메이션 | ServerMinionWaveRuntime 코드 | 0 |
| 6 | 미니언 전투 세부(투사체/타이머) | 코드 + MinionCombatDef 인라인 | 0 |
| 7 | 보상/XP 커브 | RewardRegistry 코드 (Shared, 클라에도 컴파일) | 0 |
| 8 | 아이템 카탈로그 | ItemDef.h 코드 | 0 |
| 9 | 와드/트링킷 | CommandExecutor+WardDefinitions 코드 | 0 |
| 10 | 스킬 레벨링 규칙(maxRank/reqLevel) | CommandExecutor 코드 | 0 |
| 11 | 게임 규칙(CDR cap/리스폰/데스타이머) | 코드 산재 | 0 |
| 12 | 챔피언 특수 스킬 수치(Viego 소울 등 미이관분) | CommandExecutor 하드코딩 | 0 |
| 13 | 챔피언 AI 튜닝 | ChampionAIPolicy 코드 스위치+프로필 | 0 |
| 14 | 챔피언 비주얼(모델/텍스처/애님 키) | ClientPublic pack + 등록파일 + 레거시 테이블 3중 | 0.5 |
| 15 | 스킬 비주얼 타이밍(castFrame/playSpeed) | 등록 .cpp + s_SkillTable 수기 우세, 팩 부분 | 0.5 |
| 16 | 맵/스테이지(스폰 포인트/내비) | Stage1.dat (에디터 authoring) | 1.0 |
| 17 | FX 정의 | .wfx 데이터 + FxPresets 코드 혼재 | 0.5 |
| 18 | UI/상점 로스터 | LoLUIContentRegistry 코드 | 0 |

**종합: 5.0 / 18 ≈ 28%** (팩/데이터 파일이 존재하는 도메인 8/18=44%, 유일 소스 완결 2/18=11%, 완전 코드 소유 10/18=56%). 단 가중치 관점으로는 밸런싱 빈도가 가장 높은 도메인(1·2번: 스탯/스킬 수치)이 이미 JSON 편집→재cook으로 기획자 수정 가능 상태다 — "가장 자주 바뀌는 값" 기준으로는 절반 이상이 분리되어 있다. 갱신 규칙: D-슬라이스가 랜딩할 때마다 이 표의 점수를 갱신하고 종합 %를 다시 계산한다.

## 5. 역할 분리 협업 구조 (목표 상태)

| 역할 | 편집하는 것 | 도구 | 코드 접근 |
|---|---|---|---|
| 기획자 | `Data/Gameplay/**.json`, `Data/LoL/ServerPrivate/**.json` (스탯/스킬/보상/아이템/웨이브/게임규칙) | JSON 편집 → `Build-LoLDefinitionPack.py`/`build_champion_game_data.py` 재cook (중기: 에디터 데이터 패널, UE_FAB_TOOL_ADOPTION §4) | 불필요 |
| 디자이너(아트) | 모델/텍스처/애님/FX 원본 → `.w*` cook, `.wfx` 그래프, `ChampionVisualDefs` (애님 키/프레임) | AssetConverter + WFX 툴 + (중기) Content Browser/Validator | 불필요 |
| 개발자 | 타입/시스템/훅 (`*GameSim.cpp`, Definitions 구조체, 시스템) — **값은 소유하지 않음** | C++ | 전담 |
| 공통 게이트 | 재cook 시 build hash 갱신 → Hello 핸드셰이크가 클라/서버 정합 검증, 폴백 카운터 0 유지, SimLab PASS | — | — |

분리를 막는 현재 병목(=D-슬라이스가 곧 협업 구조 작업인 이유): 코드 소유 도메인 10개는 기획자가 값을 바꾸려면 개발자가 필요하고, 이중 소스 도메인 6개는 기획자가 JSON을 바꿔도 코드 사본이 이기는 경로가 남아 있다.

## 6. 게이트 (모든 D-슬라이스 공통)

1. `msbuild Winters.sln Debug|x64` 에러 0 + `Check-SharedBoundary.ps1` PASS
2. SimLab same-seed 해시 — 동작 무변경 슬라이스는 해시 동일 필수, 동작 변경 슬라이스(D-4)는 변경 사유를 플랜 문서에 명시
3. 서버+클라 스모크에서 `[Data]`/`[Command] fallback` 로그 0줄 (정상 로스터 기준)
4. legacy 테이블 삭제는 해당 폴백 카운터가 스모크 전 구간 0일 때만 (compass zero-reader 규칙)
