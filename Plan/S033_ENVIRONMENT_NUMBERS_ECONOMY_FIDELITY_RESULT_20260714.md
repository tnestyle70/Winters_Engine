# S033 결과 보고 — 경제 루프 + 수치 실측화 + 전투 충실도 + SimLab 팩 통일 + 룬 제거 (2026-07-14)

Session - 봇 self-play가 의미를 갖도록 환경 수치(경제/스탯/쿨다운/타이밍)를 완결하고 학습 파이프의 수치 이원화를 봉합한다.

Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다 — 신규 봇 구매도 `BuyItem` 명령 emit → `HandleBuyItem` 검증 경로다.

## 1. 반영된 것 (as-built)

### 1-1. 룬 시스템 제거 (치명적 속도 포함, 사용자 지시)
- `StatSystem.cpp` 룬 공속 가산 제거, `CombatActionSystem.cpp`/`EzrealGameSim.cpp` 스택 적립 호출 제거, `GameRoomTick.cpp`+SimLab 4곳 `CRuneSystem::Execute` 배선 제거, SimLab `RuneLoadout/RuneRuntime` 부착 제거, `SpawnObjectGameplayDefs.json` `startRuneCount: 0`.
- 타입/키프레임 등록은 비활성 잔존(레이아웃 안정) — 완전 삭제는 후속 정리 항목.

### 1-2. 경제 루프 (기승인 계획서 1-1~1-13 전체)
- 시작 골드 **500 / 시작 레벨 1** (`spawnLoadout` — 연습 편의는 F10/9키 패널 AddGold/SetLevel로 대체).
- **패시브 골드**: 신규 `Shared/GameSim/Systems/Gold/GoldIncomeSystem.h/.cpp` — 1:50부터 +2g/s, tickIndex 파생 무상태(키프레임 안전). `GameSim.vcxproj` 등록, 서버 틱+SimLab 미러 4곳 배선.
- **포탑 파괴 골드**: `RewardRegistry`에 Turret 250g 등록(`ChampionAIValuation::kTurretGold`와 동치) + `GrantKillRewards` Turret 분기(챔피언 킬러만).
- **어시스트/퍼스트블러드 골드**: 기존 `ChampionAssistCreditComponent` 크레딧에 assistGold 150 지급, 양팀 통산 킬 0에서의 첫 킬에 firstBlood 100 지급 (`DamageQueueSystem::ApplyScoreForKill`). `CExperienceSystem::GrantGold` public 승격.
- **치명타/흡혈 활성**: BA 요청에 `DamageFlag_CanCrit|CanLifesteal` 부여, `DamagePipeline`에 실적용 데미지 기준 흡혈 회복(시전자 생존 게이트, MirrorHealth 동기).
- **봇 자동 구매**: 신규 `ChampionAIItemBuild.h/.cpp`(아키타입 4종 빌드 오더: AD캐스터/공속/AP/브루저) + `TryEmitItemPurchase`(베이스 반경 5 내, 빌드 오더의 다음 미보유 아이템, 골드/슬롯 게이트) — Kalista 계약 emit 다음 순위로 배선. `AiEpisodeSchema.py` KnownCommandKinds에 BuyItem=5 추가.

### 1-3. 수치 실측화 (champions.json — 282개 값)
- 17챔프 기본 스탯 LoL 실측 근사 차등(HP 560~690/성장 98~124, AD 50~69, AS/성장 챔프별, 방어/마저 원거리·근접 차등), 63개 스킬 실쿨다운(Q 4~18/W/E/R 70~180) + 마나 코스트. 스키마 필드 무변경(기계검증). 백업: 스크래치패드 `champions_backup.json`.
- 재생성: legacy 해시 `0x8DD2E61D`, 팩 buildHash `0x453D9E8B` — definition_hash 체인 자동 갱신.

### 1-4. 전투 충실도
- **BA 윈드업 분리**: `uImpactTick = start + windup/공속스케일`, `uEndTick = start + 전체길이` — dead API `fWindupSec` 소비 시작 (`CommandExecutor.cpp`).
- **클라 BA 애니 공속 배속**: `EventApplier::PlayReplicatedActionVisual`에서 `attackSpeed/base` 비율(clamp 0.2~4)을 배속에 곱함 — 공속 상승 시 모션-서버 desync 해소.
- **리콜 취소 3종**: 피격(`DamagePipeline`), 평타 시작, 스킬 시전(마나 소비 직후) — LoL 파리티, "싸우며 리콜" exploit 봉쇄.
- **죽은 포탑 정지**: `TurretAISystem` 재활성/발사 양쪽에 자기 HP 게이트.
- **웨이브 시간 스케일링**: 미니언 HP/AD +2.5%/분(30분 상한, tickIndex 파생) + **매 3웨이브 공성 미니언 1기**(보상 테이블 기등록분 활성).

### 1-5. SimLab 학습 파이프 수치 통일
- `SpawnChampion` 팩 전환: 정의 팩 스탯 + `ChampionDefinitionComponent`/`SkillLoadoutComponent` 부착 + spawnLoadout 골드/부활 + 팩 시야/반경. 하드코딩 5건(gold 10000/respawn 5s/sight 12/LethalTempo/legacy 스탯) 제거. 레벨만 프로브 호환 위해 6 고정(주석 명시).
- `CStatSystem::Execute(world)` → `Execute(world, pack)` **16곳 전부 통일**.
- 신선도 게이트: `build_champion_game_data.py --check` 신설(생성물 stale 시 exit 1).

## 2. 검증

| 게이트 | 결과 |
|---|---|
| `build_champion_game_data.py --check` (신설) | PASS (hash 0x8DD2E61D) |
| 빌드 (Winters.sln Debug x64 전체) | **exit 0** (수정 2회: TickContext include, min 템플릿 표기) |
| SimLab 골든 `1800 42` | **PASS** — 프로브 19종 전부, 이중실행 해시 일치 `E4AAEB3B6AB1FA60`(실측화로 의도된 갱신), seed+1 상이 |
| RunValidation (학습 계약 + Shared 경계) | **PASS** |
| 카탈로그 4자 검증 | PASS (34종) |

수동 확인(사용자 F5): 봇이 베이스에서 도란 구매→라인 복귀, 1:50부터 골드 자동 증가, 포탑 파괴 +250g, 리콜 중 피격 시 취소, 3웨이브째 공성 미니언, 킬 골드/어시 골드, 실측 쿨다운(스킬 난사 소멸).

## 3. 이연 항목 (사유 명시)

- `SkillScalingTable` 전 챔피언 실등록: 챔피언 훅이 이미 flat 데미지를 주입 중이라 **더블 카운트 위험** — 챔피언별 훅 감사 후 별도 슬라이스.
- `SkillEffectGameplayDefs` 미커버 36엔트리: 서버 훅 자체가 없는 스킬이라 데이터만 넣어도 무의미 — 훅 신설과 함께.
- rank vs rank-1 공식 통일(Irelia↔Ezreal), `Scene_InGameNetwork` BA 종료시간 보정, 미니언 call-for-help, 포탑 heat ramp, 억제기 효과, 아이템 데이터팩 편입, 보상 테이블 라이브 오버라이드, 룬 타입 완전 삭제.

## 4. Next slice

인게임 눈검증 → 9키 시트로 실측값 미세 튜닝 → 확정치 champions.json 소성 → (봇 학습 재개 시) frozen SimLab.exe 아카이브 후 코퍼스 재수집 — 구 코퍼스는 rules/definition 해시로 자동 구분됨.
