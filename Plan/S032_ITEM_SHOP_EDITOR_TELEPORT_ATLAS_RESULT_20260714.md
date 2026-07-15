# S032 결과 보고 — 아이템 상점 34종 + 시트형 라이브 튜닝 + 클릭 텔레포트 + SR 아틀라스 (2026-07-14)

Session - '9' 키 연습 패널에서 챔피언 스탯/아이템 수치를 시트로 나열·수정·Apply하고, 클릭 텔레포트·상점 배치·골드까지 인게임에서 즉시 반영한다.

Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다 — 본 슬라이스의 오버라이드도 전부 PracticeControl 명령 → 서버 검증 → 스냅샷 회귀 경로이며, 봇 로직은 무접촉이다.

## 1. 반영된 것 (as-built)

### 1-1. 아이템 데이터 확장 (15 → 34종, 전부 현행 협곡 검증)
- `Shared/GameSim/Definitions/ItemDef.h` — 컴포넌트 7(1011/1018/1026/1027/1031/1054/1057), 신발 5(3006/3020/3047/3111/3158), 완성템 7(3031/3065/3072/3078/3089/3157/3742) 추가. 검증 출처: Riot Data Dragon 16.13.1 `maps["11"]` 구매가능 + 공식 위키 (3047=현행 Plated Steelcaps ID 확인).
- `Client/Private/GamePlay/LoLUIContentRegistry.cpp` — 카탈로그 34종(시작템→신발→재료→완성템 배치), 툴팁 스탯 라인에 AbilityHaste/MagicPen/Lethality 추가.
- `Client/Public/GamePlay/LoLUIContentRegistry.h` — `WintersTypes.h` include + 상점 편집 API 5종 공개.

### 1-2. 상점 배치 런타임 편집
- 카탈로그를 런타임 벡터로 승격 (`GetRuntimeShopCatalog`), `iOrder` = 벡터 위치.
- API: `GetLoLShopEditorEntryCount/GetLoLShopEditorEntry/SetLoLShopEditorEntryEnabled/MoveLoLShopEditorEntry/ReapplyLoLShopItems` — Reapply가 `UI_Register_InGameShopItems` + `UI_Reload_Lua` 재호출로 P 상점에 즉시 반영.

### 1-3. Practice 라이브 오버라이드 (서버 권위)
- `ICommandExecutor.h` — `ePracticeOperation` 17~20 (Apply/ClearChampionStatOverride, Apply/ClearItemStatOverride) + `eChampionStatOverrideId` 16종 + `eItemStatOverrideField` 14종.
- `GameplayComponents.h` — `PracticeChampionStatOverrideComponent`/`PracticeItemStatOverrideComponent` (각 32엔트리, trivially copyable).
- `WorldKeyframe.cpp` — 신규 컴포넌트 2종 레지스트리 등록 (되감기/크로노 왕복 보존). 기존 WKF1 blob은 store-count 프리플라이트로 자동 무효화(폐기+재채집 정책).
- `GameRoomCommands.cpp` — op 핸들러 4종 (기존 스킬 오버라이드 핸들러 미러: Debug 빌드+호스트+practice enable 게이트, targetNet 지원, 값 검증, 적용 시 `StatComponent.bDirty`).
- `StatSystem.cpp` — 챔피언 오버레이(`BuildBaseStats` 입력 사전 변형: 팩/레거시/폴백 3경로 전부) + 아이템 오버레이(`ApplyRuntimeModifiers` 아이템 루프 사본 변형).
- `CommandExecutor.cpp` — `HandleBuyItem` 가격 오버라이드 (fieldId=Price).

### 1-4. ChampionTuner (F10 / '9' 키)
- **Champion Stats (Live)**: 스탯 16종 전체 시트 나열 (baseline = champions.json 테이블 `ChampionGameDataDB::ResolveStats`), 수정 시 변경분만 오버라이드 행으로 upsert(베이스라인 복귀 시 자동 제거), Apply = Clear+전송, `(override, base x)` 표시.
- **Item Balance (Live)**: 아이템 콤보 선택 → Price+13필드 전체 시트 (BotRK: FlatAd 40 / BonusAS 0.25 / FlatMoveSpeed 0처럼 +0 필드도 나열), 동일 upsert 방식, 여러 아이템 변경분 누적 Apply.
- **Shop Layout**: ▲▼ 순서 이동 + 노출 체크박스 + Apply Shop Layout.
- **클릭 텔레포트**: "Teleport (Click Map)" 암 모드 → 지면 클릭 즉시 이동 (ESC 해제, ImGui 클릭 미소비, 서버 nav-reject가 벽 클릭 방어). 원거리는 미니맵 클릭(카메라 점프) 후 지면 클릭 2단계.
- **Set Gold**: 목표 골드 입력 → 현재값과의 델타를 AddGold로 전송 (서버 op가 양수 전용 — 감소는 후속 SetGold op 필요).
- **JSON**: `practice_balance_overrides.json`에 `championStats`/`itemStats` 배열 추가 (버전 1 유지, 구파일 호환). 파일 수정 → Load JSON → Apply 2클릭 = 핫 적용.
- `Scene_InGameImGui.cpp` — '9' 키 = 연습 패널 토글 (`WantCaptureKeyboard` 가드).

### 1-5. SR 아틀라스 필터 재생성
- 신규 `Tools/UIAtlas/build_itemshop_atlas.py` (Pillow, `--check` 드라이런 지원) — 특수모드/아레나/Ornn(90+), 삭제 레거시(49+, 2033 부패물약 포함), 테스트/중복(kiwi/strawberry 등) 제외.
- 결과: `item_icons_atlas.png` 1024×2048→**1024×1024**, 스프라이트 454→**211**, "shop" 텍스처/스프라이트 18종 보존. 원본 `.bak` 백업 + `Items_excluded_from_atlas.txt` 제외 로그 179건.
- 원본 PNG는 삭제하지 않음 — 아틀라스에서 제외만. 실삭제는 사용자 그룹 결정(①~⑦) 대기.
- `Tools/generate_itemshop_catalog_from_reference.py` — 기준 목록 34종 동기.

## 2. 검증

통과:
- 카탈로그 4자 검증: `entries=34 uniqueIds=34 authoritativeDefs=34 atlasSprites=34` PASS
- 빌드: GameSim/Server/SimLab + Client 전부 exit 0 (경고만, 에러 0)
- SimLab 골든: `SimLab.exe 1800 42` PASS — 프로브 19종, same-seed 해시 `DB0DC85E451999AD` **변경 전과 동일**(기본 궤적 무영향 확인), 키프레임 신규 등록 2종 포함 원자성/복원 결정론 PASS
- `RunValidation.ps1`: `[AIResearch] PASS` (Python 계약 + Shared 경계)

미검증 (사용자 인게임 F5):
- '9' 키 패널 → 시트 수정 → Apply → HUD/전투 반영, 클릭 텔레포트, Shop Layout 재배치, P 상점 34종 아이콘/툴팁

주의사항:
- 오버라이드 전 채널 Debug 빌드 + 호스트 + practice enable 한정 (Release 무시) — 학습 코퍼스 수집 시 사용 금지(측정 규율), 확정값은 champions.json/ItemDef.h 소성 후 재수집.
- 애니메이션 재생 속도(공속↔BA 애니 배속)는 이번 범위 아님 — Slice D(BA 윈드업 분리+미니언 배속 패턴 이식) 예정.
- 핸드오프 가드가 본 세션에서 `.md` 편집 1회를 오탐 차단 → 템플릿 텍스트 출력 후 동일 편집 재시도로 통과 (내용 무변경, gotcha 2026-07-10 패턴).

## 3. Next slice

1. 사용자 인게임 눈검증 → 이상 시 본 패킷 재오픈.
2. **Slice B (경제 루프)**: 기승인 계획서(패시브 골드/포탑 보상/봇 자동 구매/치명타·흡혈 활성) 반영.
3. **Slice D**: BA 윈드업 분리 + 클라 BA 애니 공속 배속(미니언 패턴) + 리콜 취소 3종.
4. 아이콘 실삭제(그룹 ①~⑦) 사용자 결정 대기, 후속 SetGold(감소) op.
