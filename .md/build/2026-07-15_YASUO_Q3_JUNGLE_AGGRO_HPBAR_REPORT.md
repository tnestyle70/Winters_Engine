# 2026-07-15 야스오 Q3 꺼짐 + 정글 체력바/어그로/HP 튜닝 보고서

Bot/Jungle AI는 GameCommand 생산자 원칙 유지 — 정글 공격은 `BasicAttack` 명령 emit → CommandExecutor 검증 경로 그대로. 리쉬 풀피 리셋만 in-sim 규칙(DeathSystem류)으로 직접 수행.

## 1. 야스오 Q3 땅 꺼짐 (에셋 결함, 코드 무변경)

- 원인: `Client/Bin/Resource/Texture/Character/Yasuo/anims/skinned_mesh_yasuo_spell1_wind.wanim`의 Root/Root_Upper/Root_Lower/Pelvis 위치 트랙이 전부 (0,0,0). 엔진 `CAnimation::Evaluate`는 채널이 있으면 rest 로컬을 통째로 대체하므로 기립 높이(Root_Upper rest Z −125.534)가 붕괴 → Q3 재생 동안만 몸이 지면 아래로 침몰. 스테이지 선택(EventApplier stage 3→spell1_wind)·클립 매칭·SnapshotApplier Y는 전부 정상 확인.
- 조치: 신규 `Tools/Anim/patch_wanim_root_track.py` — 제로화된 루트 계열 채널을 `.wskel` rest 병진으로 재기록(.bak 백업, idempotent, `--dry-run`/`--audit` 지원). 패치 후 재검사 `[ok]`.
- 오딧 결과(전 챔피언): 동일 잠복 결함 8채널 — `Irelia/skinned_mesh_irelia_turn_0.wanim`(Pelvis/Root), `Yone/skinned_mesh_yone_walk1_turn_-90|_0|_90.wanim`(Pelvis/Root). 이번 범위 외, 동일 명령으로 패치 가능(작업 칩 등록됨).

## 2. 정글 체력바

- `Client/Private/Scene/Scene_InGame.cpp` `SyncWorldHealthBarsToEngineUI`에 Jungle ForEach 추가(hp/maxHp는 이미 스냅샷 리플리케이션 완동 상태였음 — 등록 루프만 공백).
  - 에픽 4종(Baron=0/Dragon=1/Blue=2/Red=3): `Character` kind + 중립팀 → 챔피언형 바가 자동 적색 필(IM_COL32(218,52,48)), maxMana=0이라 마나 스트립 자동 숨김.
  - 소형 캠프(subKind≥4, 칼날부리 포함): `Unit` kind + `fWidthScale=3` → 미니언 바 가로 3배(43.088→129.26px).
- Engine: `UIWorldHealthBarDesc.fWidthScale = 1.f` 필드 신설(`Engine/Public/Manager/UI/WorldHealthBarState.h`), `DrawUnitHealthBars`/`DrawUnitHealthBarsRHI` 폭 계산을 루프 내부 per-bar로 이동. EngineSDK는 `UpdateLib.bat` 산출물로 동기(수기 편집 없음).

## 3. 정글 근접 어그로 + 리쉬 복귀 (서버 권위)

- `JungleAIComponent` 확장: `bHasAnchor/bReturning/aggroRange/leashRange/anchorX/anchorZ` (trivially_copyable 유지, 키프레임 자동 편승 — 기존 WKF1 캡처는 크기 변화로 폐기 대상).
- `JungleAISystem` 재구성(결정론: DeterministicEntityIterator, 동률=낮은 EntityID):
  1. 복귀 중이면 전투 무시하고 캠프로 보행(TryBuildMovePath, 실패 시 직선), 도착(0.5) 시 해제
  2. 리쉬 이탈(leashRange) 시 어그로 해제+풀피 리셋+복귀 시작
  3. 비어그로 상태에서 aggroRange 내 최근접 챔피언 획득(기존 피해 보복 어그로는 그대로 유지)
  4. 기존 BasicAttack 명령 emit 로직 유지
- `GameRoomSpawn::SpawnServerJungleFromStageEntry`: def의 aggro/leash + 스폰 위치 앵커를 컴포넌트에 초기화. 하네스/레거시 스폰은 첫 틱 lazy 앵커.

## 4. HP/데이터 실측화 (`SpawnObjectGameplayDefs.json` → 코드젠 0x57A21F98)

| subKind | 캠프 | HP (구→신) | 비고 |
| --- | --- | --- | --- |
| 0 바론 | 8000 유지 | aggro 2.5 / leash 8 |
| 1 드래곤 | 5000 유지 | aggro 2.5 / leash 8 |
| 2/3 블루/레드 | 1500→2300 | aggro 3 / leash 9 |
| 4 돌거북 | **신규 행** 1400 | 구 default 폴백 1500/45AD 해소 |
| 5 두꺼비 | 1500→2000 | |
| 6 늑대 | **신규 행** 1600 | |
| 7 칼날부리 | **신규 행** 1200 | |
| 8/9/10 미니 | 1500→400/350/350 | AD 25 유지 |

- `JungleCampGameDef`/`JUNGLE_FIELDS`에 `aggroRange`/`leashRange` 추가, `Build-LoLDefinitionPack.py` 재실행으로 `LoLGameplayDefinitions.generated.cpp` 재생성.

## 검증

- 빌드: Engine→GameSim→Server→Client→SimLab Debug x64 전부 exit 0. 1차 시도에서 Server 빌드 중 `run_codegen.bat`(flatc) 1회 실패 = 사운드 세션 동시 빌드와 Generated/cpp 출력 충돌로 판정(수동 재실행 정상, 재시도 통과). `git diff --check` 통과.
- SimLab: `SimLab.exe 1800 42` PASS — 동일 시드 리플레이 해시 일치 85A270CA375932B7(결정론 유지), seed+1 상이(1C930208430B1685). **골든 해시 의도 갱신**: 구 E4AAEB3B6AB1FA60 → 신 85A270CA375932B7 (정글 어그로/데이터가 궤적에 반영). 구 학습 코퍼스는 rules/definition 해시로 구분.
- 미검증(사용자 인게임 게이트): ① 야스오 Q3 시전 시 지면 유지 ② 에픽 4종 적색 챔피언바/소형몹 3배폭 미니언바 표시 ③ 캠프 접근 시 어그로 → 리쉬 밖으로 끌면 풀피 복귀 ④ 신규 HP 체감.

## 롤백

- wanim: `.bak` 원복. 코드: 파일 단위 revert (JungleAIComponent.h / JungleAISystem.cpp / JungleCampGameDef.h / GameRoomSpawn.cpp / WorldHealthBarState.h / UI_Manager.cpp / Scene_InGame.cpp / Build-LoLDefinitionPack.py / SpawnObjectGameplayDefs.json). 생성 cpp는 스크립트 재실행. EngineSDK는 UpdateLib.bat 재실행.

## 다음 슬라이스 후보

- 정글 사망 정리/리스폰 스케줄러 + `eRewardSourceKind::Jungle` 골드/XP 보상(현재 시체 영구 잔존, 보상 없음)
- Irelia/Yone 잠복 루트 트랙 패치(칩 등록됨), 캠프 단위 연동 어그로(campId), 바론/드래곤 바 Y 오프셋 전용화
