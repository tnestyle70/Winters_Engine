# S030 Result — 계정 DB 로그인/챔피언 상점 + 게임종료·저장 인프라 (부분 완료, 잔여는 S031 계획서)

작성: Claude 레인 / 2026-07-14. 계획: `Plan/S030_..._SESSION_20260714.md`. 잔여 구현 지시서: **`Plan/S031_INGAME_WIRING_SETTINGS_YASUO_FX_SESSION_20260714.md`** (다른 세션에서 이어받는 진입점).

## 1. As-built (반영 완료)

### Goal 1 — 계정 DB 로그인 + 챔피언 상점 (완료)

Services (Go + PostgreSQL, docker `winters-postgres`:5433):
- `migrations/000008_create_account_identity_storefront.up.sql` — `user_identities`(provider, provider_subject→users.id UUID), users email/password NULL 허용, `wallets.currency_code='RP'`, coin_transactions tx_type 확장(initial_grant 등), shop_items product/content key + **챔피언 상품 17종 × 50RP 시드**. down은 forward-only 예외.
- `Data/Account/AccountEconomyPolicy.json` — startingBalance **1000** (Auth 서비스가 기동 시 로드, 실패 시 미기동).
- auth: `/auth/id/register`(409=중복) · `/auth/id/login`(**404=미가입** → 클라 회원가입 유도), `CreateIdentityAccount` 단일 트랜잭션(advisory lock + users/identity/wallet/stats/initial_grant), `WINTERS_DEV_AUTH_ENABLED` 게이트, TokenPair에 user_id/display_name 추가, FindByID/FindByEmail COALESCE.
- shop: `GET /shop/storefront`(JWT, 잔액+17상품+owned LEFT JOIN 원자 스냅샷), `PurchaseChampion` 트랜잭션(wallet FOR UPDATE → **보유 시 무과금 already_owned** → 차감+inventory+ledger; quantity+1 경로 제거).
- profile: `/profile/me`, `/profile/me/history` (JWT claims 전용).

Client (C++):
- `AuthClient` LoginByID/RegisterByID + statusCode/userID/displayName 파싱, `CShopClient` GetStorefront + status/owned 파싱, `ProfileClient` GetMyProfile/GetMyHistory.
- `ClientShellDataStore` ApplyStorefront(원자 교체)+FindStoreItemByContentKey+IsInitialSyncReady+MatchHistory, `ClientShellBackendService` 초기 sync=/profile/me+storefront(3콜 race 제거), RequestStorefrontSync/RequestMatchHistory/IsPurchaseInFlight.
- `Scene_Login` ImGui 아이디 패널(로그인→404 시 "존재하지 않는 회원입니다. 회원 가입을 하세요"+가입 버튼).
- 신규 `Scene_Shop`(eSceneID::Shop): MatchLoadingBackground 배경, 좌상단 뒤로가기, 17종 초상화 그리드+가격 바, 보유=검정 55% 오버레이+"보유 중", 클릭 구매/보유 클릭 시 "이미 구매한 챔피언입니다".
- 신규 `Scene_MyInfo`(eSceneID::MyInfo 추가): 프로필/전적(백엔드+로컬 기록)/리플레이 목록→재생(MatchLoading→CScene_InGame(path)).
- `Scene_MainMenu`: 상점 버튼(47,92,236,138) + 우상단 초상화 나의 정보 버튼(1405,24,1505,124) + 표시명/RP 표기, 오프라인 seed는 오프라인 계정만.
- 신규 `Replay/LocalMatchRecord`(Replay/LocalMatchHistory.jsonl append/로드).
- `Client.vcxproj`/`.filters` 등록: Scene_Shop, Scene_MyInfo, LocalMatchRecord, AiTraceExport.

### Goal 2 — 부분 반영 (배선은 S031)

- Server: `Phase_CheckGameEnd`(넥서스 HealthComponent 사망 감지→`kGlobalGameEndEffect` EffectTrigger 브로드캐스트, flags=승리팀) + 같은 틱 브로드캐스트 후 `FinalizeReplayRecorder()`; `OnSessionLeave`에서 **세션 전원 이탈 시 리플레이 즉시 발행**(ESC 보증 서버측). `GameRoom.h` m_bGameEnded.
- Shared: `kGlobalGameEndEffect = 0xF1A50002u` (`ReplicatedEventComponent.h`).
- Client: `CEventApplier` GameEnd latch + `ConsumeGameEndEvent`; 신규 `UI/AiTraceExport`(모든 ChampionAIDebugComponent trace 링→`Replay/AITrace/AITrace_<UTC>.jsonl`); `Scene_InGame.h` S030 멤버/메서드 선언(구현은 S031).
- 설정창 리소스 좌표 확정: clarity_hudatlas 기어 (631,269,32,32) / 골드·틸 패널 프레임 (4,90,341,311) — 사용자 제공 이미지 2종과 일치 확인.

### 협업 문서

- `Plan/S030_..._SESSION_20260714.md`(계획), 본 RESULT, `Plan/S031_..._SESSION_20260714.md`(잔여 지시서), `ACTIVE_WORK_PACKETS.md` S030 패킷.

## 2. 검증 결과 (2026-07-14 실행)

- Backend E2E **14/14 PASS**: 미가입 로그인 404 → 가입 201(RP 1000) → 재가입 409 → 로그인 200 동일 UUID → storefront 17상품/50RP/owned=false → 구매 950/owned → **재구매 already_owned 무과금** → storefront 소유·잔액 동기화 → /profile/me → 계정 격리(신규 계정 RP 1000/미보유) → 잘못된 아이디 400 → 무토큰 401. (서비스 8081/8084/8086 + docker postgres 5433/redis)
- `go build ./...` + `go vet ./...` PASS.
- MSBuild Debug x64 `/m:1 /nr:false`: **GameSim → Server → Client 전부 PASS** (14:10, 에러 0). Engine/SimLab은 이번 세션 비접점(Engine 헤더 변경 없음).
- 미실행: SimLab 회귀(서버 tick에 Phase_CheckGameEnd 추가됨 — S031에서 골든 해시 유지 확인 필요), 인게임 게이트 전부.

## 3. 인게임 게이트 (사용자)

S031 계획서 §2 "인게임 게이트" 1~7 체크리스트로 이관. Goal 1(로그인/상점/재로그인 동기화)은 현재 반영분만으로 검증 가능 — 백엔드 기동 명령은 S031 §2 참조.

## 4. 알려진 리스크 / 주의

- 넥서스 파괴 감지는 반영되었으나 **클라 배선(S031) 전까지 클라는 GameEnd 이벤트를 무시** — 서버 리플레이 발행은 동작한다.
- 미커밋 working tree에 Codex S018~S029 Handoff 변경과 본 세션 변경이 공존. 커밋 분리 시 S030 소유 경로는 ACTIVE_WORK_PACKETS 패킷 기준.
- 첫 Client 빌드에서 관찰된 UdpClient.cpp C2660은 재빌드에서 소멸(스테일 빌드 상태로 판정) — 재발 시 `Shared/Network/UdpReliabilityChannel.h` OnAck 3인자 시그니처와 대조.
