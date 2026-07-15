# 2026-07-15 인게임 밸런스 수치 전수 감사 — HP/데미지/전투/경제가 지금 어디에 사는가

- 목적: "빌드 1회 → JSON 수정 → 인게임 반영 → 플레이 → 크로노 되감기" 기획자 루프를 기준으로, 게임플레이 수치의 소재지를 저장 계층별로 전수 지도화한다.
- 방법: 7영역 병렬 스윕(데이터 파일/챔피언 A-K/L-Z/공용 시스템/서버+아이템/핫어플라이·크로노/갭 스캔) + 완결성 크리틱 3렌즈(부재 도메인/앵커 재검증/클라 패리티). 전 항목 rg/Read 검증 앵커 포함, 핵심 앵커 12건 재검증 **전부 CONFIRMED**.
- 기준 트리: 2026-07-15 working tree (팩 해시 0x57A21F98, 레거시 해시 0x3A6CDBF9, SimLab 골든 85A270CA375932B7).
- 실행 로드맵: [2026-07-15_FULL_DATA_DRIVEN_BALANCE_MASTER.md](2026-07-15_FULL_DATA_DRIVEN_BALANCE_MASTER.md)

## 0. 한 줄 결론

**오늘 기준, 서버 권위 밸런스 수치 중 리빌드 없이 바꿀 수 있는 것은 practice 오버라이드 채널(Debug 전용·엔티티 단위·32개 캡)이 유일하다.** 진실 JSON은 존재하지만 전부 파이썬 코드젠으로 C++에 소성(baking)되며, 서버는 런타임에 .json을 한 번도 열지 않는다(`rg '\.json' Server/Private` = generated/주석뿐). 경제(골드/XP)·아이템·미니언 튜닝은 JSON 표현 자체가 없다.

## 1. 저장 계층 분류 (이 문서의 어휘)

| 계층 | 의미 | 핫어플라이 |
|---|---|---|
| `json-runtime` | 런타임에 파일을 읽음 | 가능 (일부 재시작/재클릭 필요) |
| `json-codegen-baked` | JSON이 진실이지만 코드젠→C++ 소성 | 불가 — 코드젠+리빌드 |
| `generated-cpp` | 코드젠 산출물 자체 | 불가 |
| `cpp-header-table` | 헤더의 리터럴 표/기본값 초기화 | 불가 — 광역 리빌드 |
| `cpp-hardcode` | .cpp 본문 리터럴/constexpr | 불가 |
| `cpp-fallback-constant` | 데이터 미스 시에만 발화하는 그림자 상수 | 불가 + 미스를 밸런스 버그로 위장 |

## 2. 현재 데이터 흐름 (권위 경로)

```text
[진실 JSON 4종 — 기획자가 편집해야 하는 파일]
  Data/Gameplay/ChampionGameData/champions.json            (17챔피언 스탯+슬롯 쿨다운/마나/사거리/락)
  Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json   (52 스킬 효과: 데미지/CC/대시/실드/소환)
  Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json   (구조물/정글/미니언/시작 로드아웃)
  Data/LoL/ServerPrivate/Gameplay/SummonerSpellGameplayDefs.json (Flash 4.25/300s — 유일 항목)
        |
        |  python 코드젠 2종 (빌드 전 오프라인)
        |   Tools/ChampionData/build_champion_game_data.py  -> Shared/GameSim/Generated/ChampionGameData.generated.cpp (0x3A6CDBF9)
        |   Tools/LoLData/Build-LoLDefinitionPack.py        -> Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp (0x57A21F98)
        v
  ServerData::GetLoLGameplayDefinitionPack()  — 컴파일된 정적 배열, 런타임 불변
        |
        v  GameRoomTick.cpp:124-133  tc.pDefinitions = &definitions;   <-- 매 틱 포인터 주입 = 핫스왑 자연 이음새
        v
  GameplayDefinitionQuery::ResolveSkillEffectParam / ResolveSkillCooldown / ...
    해석 순서: PracticeOverride(엔티티) -> 팩(tc.pDefinitions) -> 레거시 generated DB -> cpp fallback 상수
```

- 출력 미러(편집 금지): `ChampionGameplayDefs.json`, `SkillGameplayDefs.json` 은 코드젠이 재생성하는 산출물. 직접 편집하면 다음 빌드에서 소리 없이 되돌아간다. 디렉터리 안에 입력/출력 구분 표식 없음.
- 신선도 게이트: 두 코드젠 모두 `--check` byte-비교. `Verify-LoLDataDrivenPipeline.ps1` 이 check→레거시 감사→goal status→MSBuild 4프로젝트→SimLab 결정론까지 체인. 즉 **공인 파이프라인 자체가 "JSON 수정 = 4개 vcxproj 리빌드"를 강제**한다.
- 런타임 해시 핸드셰이크: Hello 에 레거시 해시만 실림(`GameRoomLobby.cpp:326-334` ↔ `SnapshotApplier.cpp:639-655`), WARN-ONLY(최대 8로그, raw OutputDebugStringA), 팩 해시(0x57A21F98)는 와이어 검증 없음.
- 챔피언 스탯이 **두 개의 독립 generated 테이블에 중복 소성**(레거시 DB + 팩). BA 윈드업 타이밍은 팩을 우회해 레거시 DB 직행(`CommandExecutor.cpp:3020-3021`).

## 3. 오늘 존재하는 핫어플라이 & 크로노 인프라 (S014/S015/S032 유산)

유일한 JSON→라이브 경로:
```text
practice_balance_overrides.json (클라 로컬, .gitignore)
  -> ChampionTuner.cpp:424 LoadOverrides (nlohmann 파싱, '9'키 패널, 수동 Load 버튼)
  -> SendPracticeControl 커맨드 (서버는 JSON을 모름 — 권위 유지)
  -> GameRoomCommands.cpp 핸들러 (#if defined(_DEBUG) 전용, host 전용, practice 모드 전용)
  -> Practice*OverrideComponent 3종 (GameplayComponents.h:410/420/426, 32개 캡)
  -> GameplayDefinitionQuery.cpp:321-355 / StatSystem.cpp:32-133 이 매 재계산에서 오버레이
```

| op | 대상 | 한계 |
|---|---|---|
| 10/11 SkillEffectOverride | 스킬 효과 파라미터 ~40종 허용 | 엔티티 단위. **쿨다운/마나/캐스트타임은 데드존**: `ResolveSkillCooldown/ManaCost`(GameplayDefinitionQuery.cpp:270-319)가 effect 전용 오버로드를 호출해 오버라이드 컴포넌트를 안 봄 — 서버가 커맨드를 수락해놓고 조용히 무시 |
| 17/18 ChampionStatOverride | 기본 스탯 16종 + EffectiveAttackSpeed | 엔티티 단위, 1e6 캡 |
| 19/20 ItemStatOverride | 가격+13필드 | 엔티티 단위 — 같은 아이템이 챔피언마다 다른 스탯 가능 |
| 23/24 StructureStatOverride | 구조물 HP/포탑 AD | kind 단위 즉시 적용, 복원은 소성 팩 기본값 |

- **"밸런스 패치" 시맨틱(정의 레벨/전 엔티티) 부재** — 전부 per-entity 오버레이.
- 크로노: 키프레임 30틱 간격×링 90개(90초), 되감기 1틱~60초, RESTORE-only(저널 재시뮬 없음), 되감기 후 일시정지 착지+타임라인 포크. **Practice 오버라이드 3종 전부 WKF1 키프레임 등록 확인**(WorldKeyframe.cpp:309-311) — 키프레임 시점 이전 적용분은 되감기 생존, 되감긴 구간 안에서 적용한 것은 소실.
- WRPL v2 저널: 오버라이드=AuthoringMutation, 시간 제어=ControlPlane 도메인 분류(GameRoomCommands.cpp:80-101). **faithful re-sim 러너는 미구현**(클라 ReplayPlayer 는 스냅샷 재생 전용) — "밸런스 바꿔서 리플레이" 계약만 있고 실행기 없음.
- 파일 워처 없음(Load+Apply 수동 클릭). 전체 루프가 `#if defined(_DEBUG)`.

## 4. 도메인별 전수표

### 4.1 진실 JSON이 이미 커버하는 것 (json-codegen-baked — 리빌드 필요)

- 챔피언 기본 스탯 20필드×17종 + 슬롯별 cooldownSec/manaCost/rangeMax/stageWindowSec/lockDurationSec — champions.json
- 스킬 효과 52키: 데미지/퍼랭크/CC 지속/대시/실드/소환정책(Tibbers, Kalista 센티널) — SkillEffectGameplayDefs.json
- 구조물 HP 3000/4000/5500, 포탑 AI(사거리 7.75/쿨 1.0/AD 150·넥서스 180/투사체 18), 정글 캠프 11종 HP/AD/어그로/리쉬, 미니언 5역할 스탯, 시작 로드아웃(startGold 10000/startLevel 6/respawnDelaySec 3.0) — SpawnObjectGameplayDefs.json
- Flash rangeMax 4.25 / cooldownSec 300 — SummonerSpellGameplayDefs.json

### 4.2 현재 이미 런타임 JSON (참고 — 밸런스 아님 or 특수)

- `practice_balance_overrides.json` / `attack_speed_tuning.json` — §3의 유일 핫 경로
- `Data/Account/AccountEconomyPolicy.json` — Go auth 시작 시 로드 (RP 1000, 서비스 재시작 필요)
- 상점 챔피언 가격 — Postgres `shop_items` 행 (migration 000008 시드, DB-hot)
- `Data/GameModes/gameMode.json` — 클라 메뉴 진입 시 재로드 (수치 없음)
- `ChampionSoundMap.json` — 런타임 로드 (수치 없음)

### 4.3 챔피언 스킬 JSON 커버리지 갭 (SkillEffectGameplayDefs.json 에 항목 자체가 없는 스킬)

| 챔피언 | 누락 슬롯 | 지금 수치가 사는 곳 |
|---|---|---|
| Ashe | Q(패시브)/E/BA | `AsheSimComponent.h:10-19` (4스택/4s/+20dmg/슬로우 1.5s), BA 투사체 speed 18/radius 0.35 (`AsheGameSim.cpp:405-407`) |
| Fiora | E | `FioraGameSim.cpp:289-293` (5s 윈도우/2회), `FioraSimComponent.h:14` (+30dmg), R 타이머 8s (:313-314) |
| Jax | W/R | `JaxSimComponent.h:12-30` (W 5s/+45, R 8s/+70 3타주기 — 3타 카운터 `JaxGameSim.cpp:479-485`) |
| Kalista | Q | **최악 사례**: `KalistaGameSim.cpp:258-273` — speed 27/radius 0.6/**damage 70 생 리터럴**, 어떤 데이터/오버라이드로도 변경 불가 |
| Kindred | Q(미구현) + E 3타 임계 | 3타 하드코딩 `KindredGameSim.cpp:587-591` |
| Yasuo | W(윈드월 전체) | `YasuoGameSim.cpp:571-584,990-1015` — halfLength 1.6+0.35/rank, 두께 0.5, 형성 0.25s, 수명 4.0s |
| Sylas | R | `SylasGameSim.cpp:388-427` — 강탈 지속 45s, range 폴백 10 이 라이브 경로 |
| MasterYi | Q/W/E | R만 JSON 존재 |
| Annie | 패시브 | 스턴 4스택 constexpr (`AnnieGameSim.cpp:33`) |
| Yasuo | 패시브 | flow 100/실드 100/지속 3s — **공용 DamagePipeline.cpp:282 에 거주** |

### 4.4 챔피언별 잔여 cpp 하드코딩 (JSON 커버 스킬 안에서도 남은 것)

- 공통 패턴: 대시 벽 클램프 반경 `0.5f` 가 6파일 복붙(LeeSin:549, Sylas:514, Viego:991, Yasuo:1052+380, Yone:203/706, Irelia:719, Fiora:449, Jax:526) — 대시 필 전역 튜닝 불가
- 공통 패턴: 컴포넌트 기본값 = 사실상 밸런스 표 (Annie Tibbers 45s, Viego 소울 5s/빙의 0.72s×2곳 중복(`CommandExecutor.cpp:1662`), Kalista 의식 1.5s/당김 0.25s/아크 2.1, LeeSin 대시 0.18s)
- Ezreal: E 블링크 4.75 폴백, R non-epic 150+75 하드 폴백(:503-512), 착지 탐색 32스텝
- Kalista R: **cpp 폴백이 JSON에서 드리프트** — cpp 0.35/2.25/0.75 vs json 0.45/2.5/1.0 (팩이 이기므로 라이브는 json 값)
- Riven: R 폴백 번들(15s/20AD/10.5/100+50/cos45), Q 3콤보 구조
- Zed: R range 폴백 6.25, 스폰 높이 1.15, FX 500/800ms
- Yone: R 아크 높이 2.1, E FX 700ms
- Yasuo: R 탐색 반경 14, Q 2스택 캡
- 랭크 공식 불일치: Irelia `perRank*rank` vs Ezreal/Garen/Kindred `perRank*(rank-1)` vs Annie `perRank*max(rank,1)` — JSON 마이그레이션 시 정규화 필요 판단 대상
- 캐스트 이동 정책: 챔피언×슬롯 거대 switch (`GameplayDefinitionQuery.cpp:411-511`) + `kMaximumCastInputLockTicks=8`

### 4.5 공용 전투 시스템 (cpp-hardcode — JSON 표현 없음)

| 항목 | 값 | 위치 |
|---|---|---|
| 레벨 성장 곡선 | `n*(0.7025+0.0175n)`, 저항 100/(100+R), AS 클램프 [0.2,3.003] | `CombatFormula.cpp:6-98` |
| 치명타 배율 | 1.75 (StatComponent.h:42 기본값, **어떤 것도 안 씀** = 전역 고정) | DamagePipeline.cpp:154-182 |
| CDR 캡 | 0.4 — **2곳 중복** | CommandExecutor.cpp:684-694, StatSystem.cpp:340 |
| 어시스트 윈도우 | 10s | DamageQueueSystem.cpp:30 |
| 스킬 랭크 캡/레벨 게이트 | R=3(6/11/16), QWE=5(1+rank*2) — 2파일 일치 필수 | SkillRankSystem.cpp:13-20, CommandExecutor.cpp:3146-3148 |
| 게임플레이 반경 폴백 | 챔피언 1.2/미니언 0.5/구조물 1.5/정글 1.25 | GameplayStateQuery.cpp:53-70 |
| 아이템 MS 스케일 | 0.01 | StatSystem.cpp:27 |
| **포탑 투사체 데미지** | **raw HP 차감 — DamagePipeline 완전 우회**(방어력/실드/킬 보상/킬 피드 없음) | StructureProjectileSystem.cpp:99-124 |

### 4.6 경제/보상/XP — JSON 표현 전무 (전부 cpp)

- `RewardRegistry.cpp:67-98` ctor 하드코딩: XP 곡선 L1-17 = 280..1880(+100), 챔피언 킬 300g/어시 150g/퍼블 +100g, victimNextLevelXPFactor 0.50, 미니언 근접 61.75/80.60xp/21g·원거리 30.40/39.68/14g·공성+슈퍼 95/124xp/60g, 포탑 250g, 공유 반경 20. `Add/SetExperienceCurve` API는 있으나 **호출자 0**.
- 죽은 필드: teamXP/killerXP/goldGrowth(+3/90s)/maxKillerGold(90)는 세팅만 되고 **읽는 곳 0** (`ExperienceSystem.cpp:256-311`) — 공성 골드 성장·XP 분배는 조용히 미작동.
- 정글 킬은 `GrantKillRewards` 분기 없음 — 캠프/에픽 골드·XP 미지급 상태.
- 패시브 골드: 2g/1s, 110초 시작 — `GoldIncomeSystem.h:10-12` (WKF1 보존 위해 의도적 stateless).
- **AI 그림자 사본**: `ChampionAIValuation.h:22-27` 이 킬 300/미니언 21·14/포탑 250 을 수동 중복 — RewardRegistry 를 튜닝하면 봇 가치판단과 조용히 갈라짐(컴파일 체크 없음, 주석 경고만).

### 4.7 아이템 — JSON/코드젠 입력 자체가 없음

- `ItemDef.h:43-90`: `CItemRegistry::Find` 안 `static const ItemDef kItems[]` 34종(가격+스탯, Data Dragon 16.13.1 수기 전사). 소비자: StatSystem 재계산, BuyItem 가격(:3190), 봇 빌드, 상점 UI.
- 인벤토리 6칸, 와드 트린켓(3340)은 별도 `WardDefinitions.h`(90s/사거리 6/시야 10), Kalista 서약 아이템 3599 constexpr.
- S033이 "아이템 팩 편입"을 명시 이연.

### 4.8 미니언/웨이브 — 진실 3분할

- 기본 스탯: SpawnObjectGameplayDefs.json (소성) ✔
- 전투 필/캐던스: `ServerMinionTuning.h:11-43` constexpr — 윈드업 0.22/0.6, 스캔 0.15s, 웨이브 900틱(30s)/첫 300틱/스태거 10틱, 분리력 0.65/0.35/0.18
- 성장/구성: `GameRoomSpawn.cpp:508` `+2.5%/min` 30분 캡(JSON 값을 코드가 곱함), 웨이브 구성 3+3+공성(3웨이브마다) `ServerMinionWaveRuntime.cpp:403-416`
- **클라 이중 진실**: `MinionCombatDef.h:21-37` 헤더 표를 클라만 읽음(SnapshotApplier.cpp:261, Minion_Manager.cpp:1198) — 서버는 소성 팩. 동기 강제 장치 없음.
- 시체 타이머 드리프트: 서버 1.2f(`GameRoomUnitAI.cpp:651`) vs 공용 1.5f(`StructureProjectileSystem.cpp:120`)
- 원거리 미니언 투사체 speed 14/radius 0.45 (`GameRoomUnitAI.cpp:214-218`)

### 4.9 서버 스폰 잔여 리터럴

- 정글 시야 10 / collider (`GameRoomSpawn.cpp:390-398`), 챔피언 스폰 폴백 0.75/19 (:720-758), 봇 스킬 순서 18레벨 표 (:58-93), 스모크 로스터(Sylas 600HP/더미 100000HP), 폴백 구조물 배치(WorldBootstrap.cpp:85-104)

### 4.10 소환사 주문/룬/와드/리콜/리스폰

- 기본 로드아웃 {4 Flash, 14 Ignite} — `ChampionScore.h:7-18` 구조체 기본값
- 룬 잔재(S033 이후 죽음): LethalTempo 상수 소비자 0, `CRuneSystem::OnBasicAttackHitChampion` 호출자 0 — 그러나 스폰 부착+WKF1 직렬화는 유지(`WorldKeyframe.cpp:372-373`)
- 리콜 2s (`RecallComponent.h:7`), 리스폰 3s 평면(레벨 스케일 없음, JSON respawnDelaySec)

### 4.11 권위 폴백 상수 (데이터 미스 → 가짜 밸런스)

| 폴백 | 위치 |
|---|---|
| BA 데미지 55 / 사거리 5.5 (**5.5는 3곳 중복**) | CommandExecutor.cpp:2954-2968, AttackChaseSystem.cpp:47, ChampionStatsDef.h:25 |
| 훅 없는 스킬 45+25*rank magic (bounded 트레이스 후 침묵) | CommandExecutor.cpp:1264-1293 |
| 서버 투사체 55+25*(rank-1)/speed 24/radius 0.55 | CommandExecutor.cpp:1499-1522 |
| ChampionStatsDef 기본 600HP/60AD ↔ ChampionGameDataDB 폴백 600HP/**55AD** — 기본값끼리도 드리프트 | ChampionStatsDef.h:10-29, ChampionGameDataDB.cpp:17-34 |

### 4.12 SkillScalingTable — 배선만 있고 데이터 0 (S033 이연 확정)

- `RegisterDefaultChampionSkillScalingTables()` **빈 몸체**(ChampionRuntimeDefaults.cpp:20-22), 팩 전 스킬 `scalingTableId=0`, 유일 등록자는 클라 스모크(skillId 1201).
- DamagePipeline.cpp:295-332: 명시 flat 존재 시 표 데미지 스킵, **ratio 열은 항상 가산** — 17챔피언 훅 전체(~15+ 사이트)가 `request.flatAmount`/`projectile.damage` 로 flat 주입 중이므로, flat 사이트 전환 전에 표를 채우면 데미지는 그림자화·ratio는 이중 가산. **활성화는 flat→ratio 전환 감사 선행이 전제** (S033 결정 유지).
- 참고: 레지스트리는 런타임 가변 싱글턴이라 이음새 후보지만, 커버리지가 데미지 표뿐(쿨다운/마나/사거리 불가) — 핫리로드 본선은 팩 이음새가 맞다.

### 4.13 클라이언트 패리티 (크리틱 검증 — 서버 핫리로드 시 거짓말할 표면)

**핵심 판정: 클라는 예상보다 훨씬 서버-피드형.** HP/최대HP/전투 스탯/이동속도/공격사거리/XP/골드/쿨다운(잔여+총량)이 전부 스냅샷 리플리케이션 → 서버측 밸런스 핫스왑은 **클라 수정 없이 1틱 내 HUD 전파**.

리로드 슬라이스가 반드시 커버해야 할 3곳:
1. **상점 UI**: 가격/스탯 라인이 씬 초기화 때 클라 컴파일 ItemDef.h 에서 소성(LoLUIContentRegistry.cpp) — 매치 내내 거짓말. 해법 = 카탈로그 revision 통지 + 기존 `ReapplyLoLShopItems` 재호출 (M3).
2. **Hello 핸드셰이크**: 레거시 해시(0x3A6CDBF9)만 1회 비교·Debug 로그만. 런타임 리로드는 컴파일 해시가 계속 "일치"하므로 드리프트가 비가시 — 활성 팩 revision 을 와이어에 실어야 함 (M6).
3. **Kalista 패시브 대시 예측**: 클라 소성 코드젠 값(distance/duration/grace) — 유일한 실제 클라 이동 예측. 서버만 바꾸면 러버밴딩 (M6).

권장 커버: 액션 락 애니 페이싱(Scene_InGameNetwork.cpp:332, 클라 소성 lockDurationSec), 미니언 fog 시야(SnapshotApplier.cpp:261). 안전하게 지연 가능: 리플리케이션 스탯 전부, 공격 사거리 링(리플리케이션 우선), 클라 *_Registration.cpp 스킬 리터럴(네트워크 모드에서 게이트 안 함), 튜너 기준선 컬럼.

**닫힌 질문**: 클라 로컬 스킬 데미지 리터럴(Garen 250 등, applyTargetDamage)은 **네트워크 플레이에서 도달 불가** — ApplyLocalPrediction 이 네트워크 권위에서 조기 반환(Scene_InGameLocalSkills.cpp:2300), Scene_InGame.cpp:1008 이 액티브 스킬 런타임 클리어. 오프라인 전용 잔재로 분류(M5 에서 명시 게이트만).

### 4.14 부재 기능 (크리틱 확인 — 데이터화 대상이 아니라 설계 결정 대상)

| 도메인 | 상태 | 증거 |
|---|---|---|
| HP/마나 리젠 | **미구현** — 리젠 스탯 자체가 없음. 마나는 소비 전용, 회복은 리콜/리스폰 풀리필뿐 | StatComponent.h(필드 없음), RecallSystem.cpp:33-41, GameRoom.cpp:357-361 |
| Ignite | **미구현** — 로드아웃 id 14는 죽은 데이터. 캐스트 커맨드 없음(eCommandKind::Flash=10 뿐) | ChampionScore.h:9, CommandExecutor.cpp:3389-3399 |
| 정글 보상 | **미구현** — 캠프/에픽 킬 = 0골드 0XP. eRewardSourceKind::Jungle 선언만 있고 미등록·미조회 | ExperienceSystem.cpp:240-312, RewardRegistry.cpp:92-97 |
| 바론/드래곤 팀 버프 | **미구현** — MatchScore 카운터 증가(HUD)뿐, 버프 부여 0건 | DamageQueueSystem.cpp:254-262 |
| 분수 레이저/스폰 무적/부쉬 은신(서버) | **미구현** — 분수=스폰 지오메트리, 리스폰 즉시 타겟 가능, 부쉬=클라 렌더 데이터 | GameRoom.cpp:371-372, MapDataFormats.h |
| 죽음 타이머 스케일링 | 평면 3.0s 고정, 스케일 코드 없음 | SpawnObjectGameplayDefs.json:10 |
| 실드/버프 정책 | 수치는 없음(순수 메커니즘) — 단일 슬롯 덮어쓰기, kMaxBuffs=16 침묵 드랍이 구조 사실 | ShieldSystem.cpp:87-159, BuffComponent.h |

크리틱이 추가 발굴한 수치 포켓: BA 55 폴백 **2번째 사이트**(CombatActionSystem.cpp:73), XP 공유 반경 20 2파일 중복, 챔피언 히트박스 = 전 챔피언 공용 1.8/0.9 단일 프로파일(spatialRadius 만 챔피언별), 정글 AI 잔여(복귀 반경 0.5/리쉬 풀피 리셋/시야 10/collider 2.0·1.0/JungleComponent 기본 1000HP), Tibbers 펫 AI 거리(0.65/2.5/0.8, GameRoomUnitAI.cpp:934-1009), 시작 로드아웃 startGold 10000/startLevel 6은 dev 값(LoL 500g/Lv1 아님).

## 5. 이중 진실/드리프트/데드 코드 요약 (청산 대상 목록)

1. 챔피언 스탯 2중 소성 (레거시 DB 0x3A6CDBF9 ↔ 팩 0x57A21F98) + BA 윈드업의 레거시 직행
2. Kalista R 폴백 드리프트 (0.35/2.25/0.75 vs 0.45/2.5/1.0)
3. MinionCombatDef.h(클라) ↔ 소성 팩(서버) 이중 진실
4. 미니언 시체 타이머 1.2 vs 1.5
5. ChampionAIValuation.h 경제 그림자 상수
6. RewardRegistry 죽은 필드(teamXP/goldGrowth/maxKillerGold) + 정글 보상 부재
7. LethalTempo 죽은 배선(스택만 쌓임)
8. 쿨다운/마나 오버라이드 데드존(수락 후 무시)
9. 클라 로컬 데미지 리터럴 경로
10. 폴백 기본값끼리 드리프트(AD 60 vs 55)
11. 출력 미러 JSON 2종의 입력/출력 무표식

## 6. 기획자 루프 관점 격차 요약

| 루프 단계 | 현재 | 격차 |
|---|---|---|
| JSON 수정 | 진실 JSON 4종 존재 | 경제/아이템/미니언튜닝/공용전투는 JSON 없음 |
| 반영 | 코드젠+4프로젝트 리빌드 | **런타임 로더 부재가 유일 최대 병목** — tc.pDefinitions 가 이음새 |
| 플레이 | practice 모드 완비 | Debug 빌드 전용, per-entity 캡 |
| 되감기 | 키프레임/되감기/저널 완비 | 오버라이드 키프레임 등록 확인됨. re-sim 러너만 부재 |

## 7. 미해결 질문 (계획서에서 CONFIRM_NEEDED 로 승계)

1. Release 서버에서 디자이너 루프를 열 것인가("designer build" 구성) — 현재 전부 _DEBUG.
2. ~~practice 오버라이드 확장 vs 팩 리로드~~ → **팩 리로드로 결정** (마스터 문서 D1).
3. 되감기 시 `m_keyframes.clear()` 경로(GameRoomCommands.cpp:2536/2649)가 practice 토글 시 히스토리를 지움 — 디자이너 UX 고지 필요.
4. 랭크 공식 불일치 정규화 여부, Yasuo 패시브 1회성(재생성 writer 없음) 의도 여부.
5. ~~정글 버프/Ignite 구현 여부~~ → **미구현 확정** (§4.14) — 기능 백로그로 분리(마스터 문서 D6).
6. 부재 기능 백로그(리젠/Ignite/정글 보상·버프/스폰 무적/죽음 타이머 스케일)의 우선순위 — 정글 보상만 M2 편입, 나머지 별도 결정.
7. 공용 전투 상수(치명타 1.75/성장 곡선) 데이터화 vs 코어 룰 잔류 — M5 에서 결정.
