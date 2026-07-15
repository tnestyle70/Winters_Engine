# S030 Session — 계정 DB 로그인/챔피언 상점 + 나의 정보(전적/리플레이) 씬/설정창/저장 트리거 + 야스오 FX 리워크

Session - 백엔드 인프라(아이디 로그인 + PostgreSQL 영속 RP/챔피언 소유권 + MainMenu 상점), 나의 정보 씬(전적/리플레이 재생/로비 복귀) + 설정창(기어 아이콘) + 리플레이·봇 trace 저장 트리거 3종(정상 종료/수동 저장/ESC 강제 종료), 야스오 FX 3건(피격 보호막/회오리 반투명 화이트/Q=요네 이펙트) + Q 스택 로직 검토를 한 세션에서 구현·빌드·보고한다.

작성: Claude 레인 / 2026-07-14 / 선행: S014~S016(Claude), S017~S029(Codex, 전부 Handoff). 조사: 병렬 매핑 에이전트 6종(scene/persistence/replay/yasuo-fx/ui-atlas/build), 전 앵커 rg 검증.

---

## 0. 상태 동기화 (세션 문서 기반)

- **Claude 레인**: S014(Pause/Step/TimeScale+명령저널) → S015(크로노 브레이크: 비트정확 키프레임 90종+되감기+SimLab 골든 PASS 81KB) → S016(F5 Model & Anim Lab). 전부 빌드/SimLab PASS, **인게임 게이트 대기**.
- **Codex 레인**: S018(Ezreal 투사체 권위) → S021(AI 크로노 결정 trace) → S022(PyTorch BC 섀도 정책) → S023(UDP+Chase-Lev+FiberFull) → S024(5v5 봇 30분 soak) → S025(DAgger canary) → S029(액터 스케일링 preflight). 전부 Handoff.
- **로컬 상태**: main 브랜치, 미커밋 변경 582파일(위 Handoff 세션들의 as-built). 마지막 커밋 8676251(이력서 문서).
- **백엔드 사전 조사**: Go 마이크로서비스(auth/profile/shop/match, chi+pgx+PostgreSQL+Redis)와 C++ CHttpClient/CAuthClient/CShopClient/ClientShellSession 스캐폴딩이 이미 존재. `.md/plan/2026-07-14_NUMERIC_ACCOUNT_LOGIN_IMGUI_STORE_IMPLEMENTATION_PREFLIGHT.md`(숫자 슬롯 1~5, RP 10,000 안)가 잠금 문서로 있으나 **사용자 최신 요구(임의 아이디 문자열, RP 1,000, 챔피언 일괄 50RP)가 우선** — identity 모델(`user_identities (provider, provider_subject) -> users.id UUID`)과 transaction 원칙은 프리플라이트를 그대로 계승한다.
- **환경**: Docker Desktop 기동 + `Services/docker-compose.yml`(postgres:5433/redis/kafka, `services_pgdata` 볼륨 보존). 네이티브 PostgreSQL 18(5432)은 winters 롤이 없어 사용하지 않음. Go 1.26.2, `godotenv`로 `Services/.env` 자동 로드(DB_PORT=5433).

## 1. Goal / Non-goals / 순서

**Goal 1 — 계정 DB 로그인 + 챔피언 상점 (영속)**
- 로그인 씬 ImGui 아이디 입력 → 로그인: DB에 존재하면 해당 플레이어로 로그인, 없으면 "회원 가입을 하세요" 문구 + 회원 가입 버튼 → 계정 생성(초기 RP 1,000).
- MainMenu 상점 버튼 → Scene_Shop: 좌상단 뒤로가기, 깨끗한 배경, 챔피언 직사각형 초상화+가격(전 챔피언 50RP), 미보유=클릭 구매, 보유=반투명 오버레이+"이미 구매한 챔피언입니다".
- RP/소유권은 PostgreSQL 영속, 재로그인 시 동기화.

**Goal 2 — 나의 정보 씬 + 설정창 + 저장 트리거**
- MainMenu 우상단 챔피언 초상화 버튼 → Scene_MyInfo: 전적(프로필 요약+로컬 경기 기록) + 리플레이 목록 → 재생 버튼으로 리플레이 진입, 리플레이 내부에서 로비 복귀 버튼.
- 리플레이 + 봇 AI decision trace(JSONL) 저장 트리거: ①정상 종료(넥서스 파괴 = 서버 game-end 감지 신설) ②AI Debug/리플레이 창 수동 저장 버튼 ③ESC 강제 종료(클라 OnExit flush + 서버 세션 전원 이탈 시 finalize).
- 인게임 clarity_hudatlas 기어 아이콘 클릭 → 설정창(다크 패널) → "메인 메뉴" 버튼으로 MainMenu 복귀.

**Goal 3 — 야스오 FX + Q 로직**
- 피격(패시브 보호막) 시 리신 W 스타일 반투명 보호막(장막 fbx `haga_sphere_geo.fbx` + `color_yasuo_passive_shield.png`).
- Q3/EQ 회오리: 메쉬 반투명 화이트 + 하단 파티클.
- 야스오 Q = 요네 `q_mortal_steel` 이펙트 세트를 야스오 wfx로 이식.
- Q1/Q2 스택, Q3 회오리, E-Q 원형 큐 로직 검토(클라=시전 시 스택 vs 서버=적중 시 스택 divergence 해소).

**Non-goals**: 실제 소셜 로그인(S028), 경기 종료 서버→Services 보상 파이프(S028), 인게임 골드 아이템 상점과 RP 상점 통합 금지(프리플라이트 §1-35 유지), 밴픽 overlay(S027 레인), refresh token 자동 저장.

**순서 근거**: Goal 1 백엔드(Go)는 C++ 빌드와 독립이라 최선두(HTTP로 즉시 검증 가능) → Goal 1 클라(Scene_Login/Shop) → Goal 2(씬 등록 인프라 공유) → Goal 3(wfx 데이터 위주, 빌드 영향 최소) → 통합 빌드/SimLab/보고서.

## 2. 핵심 설계 결정 (rg 검증 근거)

1. **아이디 로그인 = identity adapter**: `(provider='local_id', provider_subject=lower(아이디)) -> users.id UUID`. `/auth/id/register`(존재 시 409), `/auth/id/login`(부재 시 404 → 클라가 "회원 가입을 하세요" 표시). 프리플라이트의 dev_slot을 임의 문자열로 일반화. TokenPair에 `user_id`,`display_name` 추가(`Services/pkg/auth/jwt.go:11`).
2. **초기 RP 1,000 / 챔피언 50RP**: `Data/Account/AccountEconomyPolicy.json` startingBalance=1000, 챔피언 상품 17종(selectable 로스터 전체) 각 50RP를 migration seed(`product_key` unique, `ON CONFLICT DO NOTHING`)로 주입. 가격/잔액의 권위는 DB, 클라 하드코딩 금지.
3. **Storefront 원자 스냅샷**: `GET /shop/storefront`(JWT) = balance+17상품+owned LEFT JOIN 한 응답 — 기존 items/inventory 3콜 조합의 race 제거(`ClientShellBackendService.cpp:73-130` 대체). 구매는 `POST /shop/purchase` transaction(wallet FOR UPDATE → owned면 무과금 `already_owned` → 차감+inventory+ledger). 기존 `ON CONFLICT quantity+1`(`shop/repository.go:87`)은 챔피언에 사용 금지.
4. **Scene_Shop/Scene_MyInfo 정식 씬 신설**: `eSceneID::Shop`은 예약만 된 미사용 id(`Client/Public/Defines.h:16`), `MyInfo`는 enum 추가. 프리플라이트의 "MainMenu 내 ImGui 모드" 경계는 당시 dirty vcxproj 회피용이었고, 본 세션은 어차피 vcxproj에 씬 2종을 등록하므로 정식 씬이 정도. 씬 패턴은 `Scene_MainMenu.h` 미러(static Create + 6 lifecycle).
5. **게임 종료 감지 신설**: 서버에 넥서스 파괴 감지가 전무(`NexusTag`는 스폰/스냅샷 전용, GameEnd 코드 0건) → GameRoom tick에서 넥서스 사망 감지 → `FinalizeReplayRecorder()`(기존 `GameRoomReplication.cpp:16-51` 재사용) + GameEnd 이벤트 브로드캐스트. 클라는 이벤트 수신 시 결과 표시+봇 trace 저장.
6. **ESC 저장 보강**: ESC는 `CWin32Window.cpp:199-206` `PostQuitMessage` 즉시 종료 → 셧다운 경로의 `Change_Scene(End)`(`GameApp.cpp:44`)가 `Scene_InGame::OnExit`을 부르므로 거기서 봇 trace flush. 서버는 `OnSessionLeave`에서 인간 세션 전원 이탈 시 replay finalize(현재 `LobbyAuthority.cpp:319-371`은 bStopReplay 미설정) — TCP disconnect가 ESC를 대리한다.
7. **봇 trace 클라 저장**: 라이브 클라에는 trace 파일 쓰기가 전무(SimLab 전용 `AiDecisionTraceCaptureWriter.h`) → AIDebugPanel의 `debugDecisionTrace`(스냅샷 스트리밍 데이터)를 JSONL로 덤프하는 저장 버튼 + 자동 트리거(게임 종료/OnExit) 신설. 서버 권위 원칙 침해 없음(표현 데이터 덤프).
8. **설정창 = HUD 아틀라스 패턴**: 기어 스프라이트를 `hud_atlas_manifest.json`에 등록, `UI_DrawManifestSprite` + `UI_PointInRect`+`IO.MouseClicked[0]`(기존 상점 버튼 패턴 `UI_Manager.cpp:2965-2968` 미러)로 클릭 → 다크 패널 + "메인 메뉴" 버튼. Engine(UI_Manager)은 요청 플래그만 노출, 씬 전환은 Client(Scene_InGame)가 폴링 — 기존 `m_pfnLevelSkill` 콜백 경계 유지. 인게임→MainMenu 전환은 신설(네트워크 정리 포함).
9. **야스오 Q 스택 divergence**: 클라 `Yasuo_Skills.cpp:104`(시전 시 스택) vs 서버 `YasuoGameSim.cpp:583-606`(적중 시 스택) — 서버 권위 유지, 클라는 복제 스택 값을 따르도록 수정. FX는 wfx 데이터(`q_slash/q_tornado/passive_shield.wfx`)와 프리셋 함수만 변경, 게임플레이 로직 비접점.
10. **Bot AI는 GameCommand 생산자**: 본 세션의 서버 변경(게임 종료 감지/replay finalize)은 AI가 게임플레이 진실을 직접 변이하는 경로를 만들지 않는다.

## 3. 터치 파일 (레이어별)

**Services (Go)**: `migrations/000008_create_account_identity_storefront.{up,down}.sql`(신규), `internal/auth/{model,service,repository,handler}.go`, `internal/shop/{model,service,repository,handler}.go`, `internal/profile/{model?,handler,repository}.go`(/me), `pkg/auth/jwt.go`, `pkg/config/config.go`(DevAuthEnabled), `cmd/auth/main.go`, `cmd/shop/main.go`(storefront 라우트), `.env`/`.env.example`, `Data/Account/AccountEconomyPolicy.json`(신규).

**Client (C++)**: `Public|Private/Network/Backend/{AuthClient,CShopClient,ProfileClient}.{h,cpp}`, `Public|Private/ClientShell/{ClientShellTypes.h,ClientShellDataStore,ClientShellBackendService}`, `Public|Private/Scene/{Scene_Login,Scene_MainMenu}`, 신규 `Scene_Shop.{h,cpp}`, 신규 `Scene_MyInfo.{h,cpp}`, `Public/Defines.h`(eSceneID::MyInfo), `Private/Scene/Scene_InGameImGui.cpp`(리플레이 exit/저장), `Private/UI/AIDebugPanel.cpp`(trace 저장), 신규 `Private/Replay/AiTraceJsonWriter.{h,cpp}`(또는 UI 내 구현), `Private/Scene/Scene_InGame*.cpp`(설정 요청 폴링/OnExit flush/GameEnd 처리), `Include/Client.vcxproj`+`.filters`.

**Engine (C++)**: `Private/Manager/UI/UI_Manager.cpp`+`Public/Manager/UI/UI_Manager.h`(기어 버튼/설정 패널/메인메뉴 요청 플래그), `Client/Bin/Resource/UI/hud_atlas_manifest.json`(기어/패널 스프라이트).

**Server (C++)**: `Private/Game/GameRoomTick.cpp` 또는 `GameRoomInternal.cpp`(넥서스 사망 감지→GameEnd), `Private/Game/GameRoomLobby.cpp`/`LobbyAuthority.cpp`(전원 이탈 시 finalize), 이벤트 스키마(기존 Event 페이로드 재사용 가능 여부 확인 후 최소 추가).

**FX 데이터**: `Data/LoL/FX/Champions/Yasuo/{q_slash,q_tornado}.wfx` 수정, `passive_shield.wfx` 신규, `Client/Private/GameObject/Champion/Yasuo/{YasuoFxPresets.cpp,Yasuo_Skills.cpp}`, `Client/Public/.../YasuoFxPresets.h`, 필요 시 `LoLVisualDefinitions.generated.cpp`(FxMeshPreload 항목).

## 4. 검증

- **Backend**: `cd Services; go vet ./...` + `go build ./...`; docker postgres에 migration 적용; curl E2E — ①미존재 아이디 login→404 ②register→201(RP 1000) ③재register→409 ④login→200 동일 UUID ⑤storefront→17상품/50RP/owned=false ⑥purchase→remaining 950/owned=true ⑦동일 아이템 재purchase→already_owned 무과금 ⑧재로그인 후 storefront→소유/잔액 복원.
- **C++ 빌드**: `MSBuild <proj> /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal` — Engine→GameSim→Server→Client→SimLab 순.
- **SimLab 회귀**: `.\Tools\Bin\Debug\SimLab.exe 1800 42` → `PASS full SimLab` + 키프레임 골든(blob=81027) + 해시 `DB0DC85E451999AD`/`57A9B2394575042A` 유지.
- **git diff --check** 클린.
- **인게임 게이트(사용자)**: RESULT §3 체크리스트로 이관 — 로그인/상점/재로그인 동기화, MyInfo→리플레이 재생→복귀, 기어→설정→메인메뉴, ESC 후 replay/trace 파일 존재, 야스오 Q/피격 FX 확인.

## 5. 롤백 범위

- Services는 forward-only migration(000008 down = 명시적 예외 발생) — 계정/ledger는 파괴적 rollback 금지(프리플라이트 계승).
- C++/wfx는 전부 이 세션 diff 단위로 원복 가능. 신규 씬 2종은 vcxproj 항목 제거로 격리 롤백.
