Session - Release 3클라 인증 매치를 계정별 즉시 다시보기와 독립 재생까지 구현·검증한다
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-19_RELEASE_AUTHENTICATED_ACCOUNT_REPLAY_E2E_PLAN.md

# 예측 vs 실측

| 항목 | 예측 | 실측 |
|---|---|---|
| 3인 인증 매칭 | 동일 match/game session의 signed UDP ticket 3개 | `WINTERS_MATCH_SIZE=3`과 고정 `WINTERS_GAME_SESSION_ID`를 matchmaking container에 주입하고 Release UDP server가 같은 session으로 기동되는 preflight PASS |
| 서버 replay 업로드 | canonical match WRPL enqueue 후 ready | authenticated match identity·complete roster가 없으면 명시적 reason으로 거부하고, 정상 artifact는 `queued match=<id> participants=<n>`으로 enqueue하도록 구현. 실제 넥서스 종료 upload는 3계정 실측 필요 |
| 프로필 노출 | 진입 즉시 처리 중 표시, ready 자동 polling | 게임 종료 overlay의 `프로필 / 다시보기`, post-match refresh, expected match 1초 polling/60초 timeout 구현 및 Release Client build PASS |
| 계정별 재생 | 세 계정 cache에 동일 checksum, 프로세스별 독립 player | cloud `다시보기` 클릭 시 계정 cache download 완료 후 해당 client의 기존 `CReplayPlayer` scene으로 전환. 실제 세 계정 checksum/동시 독립 조작은 3계정 실측 필요 |
| camera | 기존 replay camera 유지, 동일 시점 미보장 | camera track/pose/DynamicCamera source 변경 0건. 각 client는 기존 replay camera를 독립 사용 |
| capture launcher | 계정·replay 영속 데이터 보존, stale matchmaking 제거 | PostgreSQL/MinIO named volume 보존, Redis `matchmaking:*`만 선택 삭제, 기존 server process와 UDP 9000 이중 점유 검사, 6시간 기본 server lifetime 구현 |

# 원인 확정

기존 Release 실행의 `[Replay] saved Replay/room1_tick1_16309.wrpl ...`은 파일 기록 자체는 성공했다는 뜻이지만 계정 replay 등록 성공을 뜻하지 않는다. `room1_tick...` fallback 이름은 server room에 backend의 canonical `match_id`, `game_session_id`, authenticated participant roster가 들어오지 않은 local/anonymous 경로였다는 증거다. 이 상태에서는 `FinalizeReplayRecorder()`가 account ACL을 만들 수 없어 replay service enqueue를 생략하고, 따라서 `/replay/me`와 프로필 새로고침에는 아무것도 나타나지 않는다.

`Debug/Release` 자체가 replay 노출을 가르는 본질은 아니다. 본질은 `signed matchmaking assignment -> authenticated server roster -> canonical match WRPL -> replay service ready -> account ACL 조회`의 연속성이다. 이번 변경은 영상 촬영용 Release 경로를 이 한 경로로 고정한다.

# 구현 결과

## Services / launcher

- `GameSessionConfig`에 고정 capture session과 2~10인 match size를 추가하고 기본 실행은 기존 2인/dynamic session을 유지했다.
- matchmaking은 capture launcher가 지정한 하나의 `game_session_id`로 그룹 내 서로 다른 signed player ticket을 발급한다.
- `StartReleaseAccountReplayCapture.ps1`은 backend를 새 환경으로 재기동하고 health, Redis state, 기존 server/UDP 점유, signed session marker, UDP ready marker를 모두 확인한 뒤에만 ready를 선언한다.
- `docker compose down`에는 `-v`를 사용하지 않는다. 실제 Redis image의 anonymous volume 발견 후 `FLUSHDB` 대신 `matchmaking:*`만 선택 삭제·재검증하도록 보정했다.
- redirected background server의 stdin EOF 종료를 피하려고 기존 `--smoke-seconds` 모드를 사용하며 기본 lifetime은 21,600초다.

## Server

- signed session, replay upload queue 상태, UDP transport-ready 로그를 즉시 flush한다.
- replay finalize 실패를 `missing_authenticated_match_identity`, `incomplete_authenticated_roster`, `file_size_failed`, queue failure로 분리했다.
- 정상 enqueue는 canonical match ID와 participant 수를 남기므로 Release stdout만으로 upload 경로를 판별할 수 있다.

## Client

- 넥서스 종료 overlay에 `프로필 / 다시보기` 진입을 추가하고 shared game network를 끊은 뒤 backend post-match refresh를 시작한다.
- MyInfo는 방금 매치 ID의 replay가 아직 없으면 처리 중 문구를 표시하고 1초마다 최대 60초 replay library를 갱신한다.
- cloud replay의 `다시보기`는 account cache download가 성공한 뒤 기존 replay scene을 연다. 다운로드 intent는 scene exit/generation 변경 시 폐기해 늦은 callback의 오동작을 막는다.
- 세 client는 같은 backend artifact를 받더라도 각 프로세스의 cache와 `CReplayPlayer`를 사용하므로 pause/seek/speed 상태를 공유하지 않는다.
- camera 관련 source는 수정하지 않았다.

# 검증 증거

## 자동 PASS

- `gofmt` 대상 5개 파일
- `go test ./...`
- `go vet ./...`
- `docker compose config --quiet`
- PowerShell parser: `StartReleaseAccountReplayCapture.ps1`
- Docker backend image rebuild 및 health 8081/8083/8084/8087
- Release capture preflight:
  - session `00a68575-3d69-47c6-b22a-41c72a7c3f6b`
  - server PID와 UDP 9000 owning PID 일치
  - matchmaking container `WINTERS_MATCH_SIZE=3`
  - container session과 server signed session 일치
  - stale `matchmaking:*` key 0건
  - 증거: `.md/build/release-replay-capture/00a68575-3d69-47c6-b22a-41c72a7c3f6b/capture.json`, `server.stdout.log`
- `Server/Include/Server.vcxproj`, Release x64, `/m:1`
- `Client/Include/Client.vcxproj`, Release x64, `/m:1`
- `Tools/Harness/ReplayClientSmoke.vcxproj`, Release x64, `/m:1`
- `RunReplayCommandContractProbe.ps1`: v1/v2, 60-byte ABI, journal/resim, paused snapshot, sealed publish retry PASS
- relevant tracked diff 및 신규 PLAN/RESULT/launcher trailing whitespace 검사 PASS

## 수동 실측 필요

자동화 환경에는 서로 다른 세 계정의 로그인 자격과 실제 3개 Release client 화면 입력이 없으므로 아래는 거짓 PASS로 처리하지 않는다.

1. launcher를 실행하고 서로 다른 세 Release 계정으로 matchmaking에 들어간다.
2. 세 assignment의 `match_id/game_session_id` 동일성과 signed UDP room 입장을 확인한다.
3. 넥서스를 파괴하고 server stdout의 `queued match=<id> participants=3`, worker `status=ready`를 확인한다.
4. 세 client에서 `프로필 / 다시보기`로 들어가 같은 match replay가 자동 polling으로 나타나는지 확인한다.
5. 각 client에서 `다시보기`를 실행해 tick 범위/checksum이 같고 pause/seek/speed 조작이 다른 두 client에 전파되지 않는지 확인한다.

# 병렬 세션 경계

- Yasuo source, champion data, generated definition pack은 이 replay 세션에서 수정하지 않았다.
- dirty worktree의 다른 gameplay/UI 변경은 보존했다. Release build는 그 변경들이 합쳐진 현재 snapshot에서 통과했다.

# 판결

`IMPLEMENTED / AUTOMATED_PASS / LIVE_3_ACCOUNT_CONFIRM_NEEDED`

코드·서비스·Release 빌드·signed UDP capture preflight까지는 닫혔다. 남은 것은 코드 결함이 아니라 실제 세 로그인 계정과 세 화면이 필요한 최종 acceptance다. 해당 실측 전에는 `FULL_E2E_PASS`라고 주장하지 않는다.

# ⑤ 갱신

- 2026-07-19: PLAN의 독립 비평 gate P0 0 / P1 0 이후 구현 시작.
- 2026-07-19: Redis anonymous volume과 background stdin EOF를 실기동에서 발견해 launcher/PLAN을 보정.
- 2026-07-19: Release capture preflight, Services tests, Server/Client/ReplayClientSmoke Release builds, replay contract PASS.
- 2026-07-19: 테스트용 Release server는 종료했고 backend containers만 실행 상태로 남겼다.

# 실플레이 실패 보정 진행

사용자 실플레이에서 아래 로그가 재현되어 기존 `IMPLEMENTED / AUTOMATED_PASS` 판결을 폐기했다.

```text
[Replay] saved Replay/room1_tick1_3665.wrpl records=8619 snapshots=3665 events=4707
[ReplayUpload] skipped reason=missing_authenticated_match_identity path=Replay/room1_tick1_3665.wrpl
```

추가 코드 조사 결과 MainMenu `RequestPlay()`가 matchmaking join/poll을 호출하지 않고 product scene을 즉시 열어, 온라인 계정도 assignment 없는 기본 TCP로 접속하고 있었다. 기존 launcher preflight는 backend/server 준비만 증명했으며 실제 client 진입 경로를 검증하지 않았다.

보정 구현은 PLAN §6의 최종 P0 0 / P1 0 비평 gate를 통과했다. 적용 범위는 MainMenu의 authenticated join/poll/assignment gate, DELETE queue cancel, matcher/leave Redis atomic claim, Release 3계정 identity·upload·library·독립 playback 검증이다. 적용·재검증이 끝날 때까지 판결은 `LIVE_FAILURE_CONFIRMED / FIX_IN_PROGRESS`다.
## 2026-07-19 최종 구현·검증 갱신

### 원인 확정

- Docker나 replay 파일 저장 자체가 원인이 아니었다. 서버는 `.wrpl`을 정상 저장했지만 Release MainMenu의 `RequestPlay()`가 계정 matchmaking join/poll을 거치지 않고 게임으로 직행했다.
- 그 결과 세 TCP/UDP 세션에 backend match/account participant identity가 없었고, 종료 시 upload queue가 정확히 `missing_authenticated_match_identity`로 중단했다.
- 프로필의 Cloud/account replay 목록은 계정 participant가 연결된 업로드만 보여 주므로 로컬 파일이 있어도 목록이 비었던 것이 정상적인 실패 결과였다.

### 반영

- 인증된 Release 계정은 Game Start에서 matchmaking join → poll → complete assignment → signed UDP ticket 경로로만 게임에 진입한다.
- Redis Lua claim으로 동시 join의 중복 매치를 막고, matched/claiming 상태의 leave가 배정을 지우지 못하게 했다.
- Client 취소는 backend DELETE 성공 뒤에만 로컬 assignment/lobby state를 비운다.
- 촬영 launcher는 두 generated data freshness check가 PASS하지 않으면 backend/server를 띄우지 않는다.
- 카메라 트랙은 사용자 결정대로 추가하지 않았다. 기존 replay snapshot/event 재생만 사용한다.

### 자동 검증 PASS

- `go test ./...` PASS.
- Docker 재빌드 뒤 서로 다른 3계정이 같은 match `ca6e14ac-ced4-4a27-ae72-183c1de402dc`와 game session `52d19ebe-179e-4cb4-a60e-dda479209cc8`을 받았다.
- 세 계정 모두 서로 다른 signed UDP ticket으로 동시 handshake PASS.
- DB match participant 3명 확인.
- matched leave는 거부되고 assignment가 유지되며, queued 계정 cancel/rejoin은 `none → queued`로 정상 전이했다.
- launcher PowerShell parser PASS.
- Release Server 및 `Client/Bin/Release/WintersGame.exe` 최종 링크 PASS.
- `ReplayClientSmoke.exe` Release 재빌드 PASS(실행 파일 생성 확인). 이 smoke의 실제 list/download 실행은 계정에 ready replay가 생긴 뒤 수행하는 수동 완료선이다.

### 최종 판정

`IMPLEMENTED / RELEASE_BUILD_PASS / 3-ACCOUNT_IDENTITY_E2E_PASS / LIVE_NEXUS_UPLOAD_PLAYBACK_CONFIRM_NEEDED`

실제 세 GUI 클라이언트로 넥서스를 파괴한 뒤 세 프로필에 replay가 즉시 나타나고 각각 독립 재생되는 마지막 육안 수용 검사는 자동화 환경에서 수행하지 않았다. 따라서 그 실플레이까지 했다고 과장하지 않는다. 다만 이전 실패를 만든 익명 Release 진입 경로는 제거했고, 업로드에 필요한 동일 match의 세 account identity와 ticket/participant 연결은 자동 E2E로 증명했다.

# 2026-07-20 Release 3클라 실동작 종결

## 추가 원인과 보정

계정 replay 업로드와 Cloud 목록 노출이 성공한 뒤에도 첫 실동작에서는 세 클라이언트가 replay `MatchLoading`에 머물렀다. live match에서 남은 `CLoLMatchContextRuntime::Context().bUseNetworkRoster`가 replay 부트스트랩에도 적용되어, 이미 연결을 끊은 네트워크 roster를 무한 대기한 것이 원인이었다.

- MyInfo와 CustomMode의 cloud replay 진입 전에 live match context를 `Reset()`한다.
- CustomMode replay 진입은 남아 있을 수 있는 live `CGameSessionClient` 연결도 먼저 끊는다.
- replay scene은 practice fallback champion roster를 생성하지 않고 WRPL snapshot roster만 복원한다. 이로써 net 1/net 1005 ghost actor 경로를 제거했다.
- canonical WRPL의 `yourNetId=0`을 카메라 트랙으로 확장하지 않았다. snapshot에 존재하는 생존 Champion 중 가장 작은 NetId를 deterministic spectator focus로 사용하고, 최초 update와 seek 성공 뒤 다시 획득한다.
- replay 완료 시 해당 match와 정확히 일치하는 Redis `matched` 상태만 Lua로 원자 삭제한다. queued 상태나 다른 새 match 배정은 보존한다.

독립 비평은 위 보정 계획에 대해 마지막으로 `P0 0 / P1 0`을 판정했고, 그 뒤에만 구현했다.

## 실제 Release E2E 결과

실행 조건:

- Release client 3개: PID 37980, 46116, 78160
- 서로 다른 계정: `replaye2e0720002528c1`, `replaye2e0720002528c2`, `replaye2e0720002528c3`
- signed game session: `4cb0387b-ede3-483a-8390-642db7681dd2`
- Release UDP server와 Docker auth/matchmaking/profile/replay/PostgreSQL/Redis/MinIO 실기동

핵심 종결 매치 `1c7678dd-7cd2-4383-bca2-1c6773a9f6ca`:

- 넥서스 종료 대체 서버 권위 명령 직후 `[Replay] saved`, `[ReplayUpload] queued ... participants=3`, `status=ready`를 확인했다.
- PostgreSQL 실측: match `completed`, participant 3명, replay `ready`, size 40,465,936 bytes, snapshots 1,475, events 783.
- `replay_user_library`가 세 계정 모두 생성됐고 `last_downloaded_at`도 세 계정 모두 기록됐다.
- 세 프로필의 Cloud/account 탭에 동일 match가 즉시 나타났고 각 클라이언트가 직접 다운로드·재생했다.

독립 재생 실측:

- client 1만 8.27초에서 일시정지해 버튼이 `Play`로 바뀐 동안 client 2와 3은 각각 33.83초, 34.43초까지 계속 진행했다.
- client 1의 paused seek를 27.47초로 이동한 뒤 `Replay Chrono seek complete`와 champion/HUD focus 복원을 확인했다.
- 따라서 replay 파일은 계정별로 접근 권한을 가지되 player state, pause, seek는 프로세스별로 독립이다.
- camera track은 추가하지 않았다. 세 클라이언트는 같은 deterministic 초기 spectator actor를 잡지만 이후 카메라 입력은 각 프로세스 로컬 상태다.

메인 메뉴와 즉시 재매치 실측:

- client 1, 2, 3 모두 replay panel의 `메인 메뉴로`를 직접 눌러 MainMenu로 복귀했다.
- 복귀 직후 세 계정의 Redis matchmaking status가 모두 없는 상태임을 확인했다.
- 세 클라이언트가 곧바로 다시 Game Start를 눌러 새 match `d87f7e3d-cc21-4305-adc5-61438ab66384`와 서로 다른 signed ticket 3개를 받았다.
- 세 클라이언트 모두 밴픽 완료 후 같은 새 match의 인게임에 재진입했다.
- 검증 종료를 위해 이 두 번째 매치도 정상 종료했다. 최종 DB는 match `completed`, participant 3명, replay `ready`, library account 3명이며 세 Redis status key는 다시 모두 삭제됐다.

## 증거 파일

증거 루트: `.md/build/release-replay-capture/4cb0387b-ede3-483a-8390-642db7681dd2/`

- 계정별 Cloud 목록: `rerun-client1-profile-ready-final.png`, `rerun-client2-profile-ready-final.png`, `rerun-client3-profile-ready-final.png`
- 세 클라 replay 화면: `rerun-client1-long-replay-active.png`, `rerun-client2-long-replay-active.png`, `rerun-client3-long-replay-active.png`
- 독립 pause: `rerun-client1-independent-pause-proof.png`, `rerun-client2-independent-pause-proof.png`, `rerun-client3-independent-pause-proof.png`
- paused seek/focus: `rerun-client1-seek-focus-proof.png`
- 수동 MainMenu 복귀: `rerun-client1-manual-mainmenu-return.png`, `rerun-client2-manual-mainmenu-return.png`, `rerun-client3-manual-mainmenu-return.png`
- 즉시 재매치 인게임: `rerun-client1-manual-return-immediate-rematch-ingame.png`, `rerun-client2-manual-return-immediate-rematch-ingame.png`, `rerun-client3-manual-return-immediate-rematch-ingame.png`

## 최종 재검증

- `go test ./...`: PASS
- auth/matchmaking/profile/replay health: 모두 HTTP 200
- replay Docker image rebuild 및 재기동: PASS
- Release `Client/Include/Client.vcxproj` x64 `/m:1`: PASS
- 실제 Redis same-match delete / different-match preserve / queued preserve / missing-key retry 계약: PASS
- 관련 source/PLAN/RESULT `git diff --check`: whitespace error 0건. 출력된 내용은 LF→CRLF working-copy warning뿐이다.

## 최종 판정

`FULL_E2E_PASS / RELEASE_3_ACCOUNT / ACCOUNT_LIBRARY_READY / INDEPENDENT_PLAYBACK / MANUAL_MAINMENU_RETURN / IMMEDIATE_REMATCH_PASS`

이전의 `LIVE_NEXUS_UPLOAD_PLAYBACK_CONFIRM_NEEDED` 판정은 폐기한다. Release 세 계정에서 종료→업로드→프로필 노출→세 클라 독립 재생→수동 MainMenu 복귀→즉시 새 게임 인게임 진입→재종료 및 상태 정리까지 실동작으로 닫혔다.

# 2026-07-20 프로필 Replay 목록·Chrono 상단 배치

## 반영

- MyInfo 첫 진입 시 프로필/전적은 왼쪽 열, Replay는 남은 오른쪽 영역 전체를 사용하도록 1080×759 기준 약 696×639px로 재배치했다.
- 과거 `imgui.ini`에 작은 창 크기가 저장돼 있어도 scene 진입 시 기본 배치를 다시 적용하며, 같은 scene 안의 수동 resize는 유지한다.
- Cloud/account 목록은 `생성 시각 | match 앞 8자 | 용량`을 표시한다. hover하면 전체 match/replay ID와 format을 확인할 수 있다.
- 계정 cache 및 로컬 replay 목록은 filename 앞 8자와 용량을 표시하고, hover하면 전체 filename을 확인할 수 있다.
- 모든 Replay 행은 고정 action 열을 사용하므로 긴 UUID 때문에 `다시보기`/`재생` 버튼이 창 밖으로 밀리지 않는다.
- Replay Chrono Break는 하단 HUD 위가 아니라 화면 상단으로 이동했다. 좌측 성능 overlay의 최대 int32 폭과 우측 match HUD 영역을 동적으로 비우며, 1080×759에서는 첫 kill-feed 영역 위에서 끝난다.
- Chrono의 긴 status/error는 공간 안에 들어오면 전문을 표시하고, 넘으면 짧은 안내와 hover tooltip으로 전문을 제공한다.

## 코드·빌드 검증

- 독립 계획 비평: `P0 0 / P1 0 — PASS`.
- `git diff --check` 관련 파일 whitespace error 0건. LF→CRLF working-copy warning만 존재한다.
- `Client/Include/Client.vcxproj`, Release x64, `/m:1`: PASS, 오류 0개. 기존 C4275 warning 4개만 존재한다.
- 산출물: `Client/Bin/Release/WintersGame.exe`.
- 직접 실행한 500-tick replay는 캡처 전에 정상 종료되어 MainMenu로 자동 복귀했다. 이후 프로필 진입 자동 조작 전에 client process가 종료됐으므로 이번 UI delta의 육안 PASS는 주장하지 않는다.

## 사용자 육안 확인선

1. Release 로그인 후 프로필에 처음 들어갔을 때 Replay 창이 화면 오른쪽의 넓은 영역으로 바로 열리는지 확인한다.
2. Cloud/account 네 행이 서로 다른 생성 시각과 match 앞 8자로 구별되고, 각 행 우측 `다시보기`가 모두 보이는지 확인한다.
3. `내 리플레이`와 `로컬/디버그`에서도 각 행 우측 `재생`이 보이고 hover 시 전체 filename이 나오는지 확인한다.
4. replay 재생 중 Chrono가 화면 상단에 있고 좌측 성능 overlay, 우측 match HUD, kill-feed, 하단 HUD와 겹치지 않는지 확인한다.
5. Restart, Play/Pause, 속도 slider, 프로필 복귀, MainMenu 복귀가 모두 클릭되는지 확인한다.

## 판정

`IMPLEMENTED / RELEASE_BUILD_PASS / MANUAL_LAYOUT_VISUAL_CONFIRM_NEEDED`

# 2026-07-20 유동 인원 매칭·계정 Replay 권한 종결

## 확정 원인

- Replay 저장·배포 자체에는 3클라이언트 고정 조건이 없었다. 서버는 인증된 실제 참가자 목록을 기록하고, replay backend는 `match_participants`를 기준으로 계정별 library 접근 권한을 만든다.
- 고정되어 있던 것은 matchmaking의 `WINTERS_MATCH_SIZE=3`이었다. 이 값이 큐에서 정확히 3명을 기다리게 만들어 1명·2명은 게임에 들어가지 못하고, 4명은 한 명이 다음 큐에 남는 병목을 만들었다.
- 따라서 “3클라이언트 검증용 실행 수”와 “제품 매칭 성립 조건”을 분리하는 것이 근본 수정이다.

## 반영 구조

- matchmaking은 가장 오래 기다린 사용자를 기준으로 5초 assembly window를 연다. 그 구간에 들어온 호환 사용자를 1명부터 최대 10명까지 한 match로 원자적으로 claim한다.
- `StartReleaseAccountReplayCapture.ps1 -HumanPlayers N`의 `N`은 이제 실행할 클라이언트 수일 뿐, backend의 필수 매칭 인원수가 아니다.
- 한 match가 고정 게임 서버를 사용 중일 때는 PostgreSQL `game_server_capacities`가 다음 match 생성을 막는다. 마지막 세션 정리 후 서버가 정확한 `match_id`로 ready를 통지해야만 다음 match를 받는다.
- ready 통지는 sidecar에 먼저 내구성 있게 기록한 뒤 전송한다. matchmaking 장애·서버 재시작 시 재전송하며, 오래된 ready가 새로운 match를 해제하지 못하도록 현재 `active_match_id`와 비교한다.
- Redis 상태가 유실돼도 PostgreSQL의 active allocation과 participant를 근거로 참가자의 matched 상태와 signed ticket을 복구한다. 제삼자 계정은 복구하거나 replay를 조회할 수 없다.
- Replay artifact는 경기당 canonical WRPL 한 개다. backend가 파일을 계정 수만큼 복제하는 것이 아니라 실제 참가 계정 각각에 동일 경기의 library/ACL 행을 만든다. 따라서 1인 판이면 1계정, 2인 판이면 2계정, 4인 판이면 4계정, 최대 10인 판이면 10계정의 Cloud/account 탭에 경기당 한 행이 생긴다.

## 독립 계획 비평

- 최종 재비평: `P0 0 / P1 0 / P2 1 — PASS`.
- 남은 P2는 Redis 전체 유실 회귀에서 match row가 이미 `completed`여도 capacity reference를 기준으로 참가자 복구·재가입 거부를 확인하도록 수용해 검증했다.

## 자동·통합 검증

- `go test ./...`: PASS.
- 동적 matcher 순수 계약: 1·2·3·4명 cohort, 10명 상한, window 직전·정각·직후 경계 PASS.
- 실제 Docker auth/matchmaking/PostgreSQL/Redis HTTP 흐름:
  - 1명: 같은 match participant 1명 PASS.
  - 2명: 같은 match participant 2명 PASS.
  - 3명: 같은 match participant 3명 PASS.
  - 4명: 같은 match participant 4명 PASS.
  - 10명: 같은 match participant 10명 PASS.
- Replay service/DB ACL: 참가자 1·2·3·4·10명 완료 처리 PASS. 1·2·3·4명 통합 테스트에서 모든 참가자의 library 조회·다운로드 권한과 outsider 거부 PASS.
- 서버 capacity 순서 보장: 첫 match가 `completed`여도 ready 전에는 다음 사용자가 queued 유지, 정확한 ready 후 다음 match 생성 PASS.
- Redis 전체 유실: active participant의 같은 match/ticket 복구, 재가입 거부, outsider `none`, ready 후 capacity 해제 PASS.
- stale ready: conflict 반환 및 현재 capacity 보존 PASS. active match DB 삭제는 FK가 거부 PASS.
- fresh Release server startup: 남은 completed capacity를 UDP bind 전에 조회·해제 PASS.
- migration up/down을 transaction 안에서 적용·롤백 PASS.
- Docker compose 설정 검사와 launcher PowerShell parser PASS.
- Release x64 `Server/Include/Server.vcxproj`: PASS, 산출물 `Server/Bin/Release/WintersServer.exe`.
- Release x64 `Client/Include/Client.vcxproj`: PASS, 산출물 `Client/Bin/Release/WintersGame.exe`.
- 관련 변경 파일 `git diff --check`: whitespace error 0건. 전체 저장소 검사는 이번 범위 밖 기존 `Client/Private/Scene/Scene_InGameInput.cpp:244` 공백 1건만 보고했다.

## 최종 실행 상태와 경계

- Docker backend는 갱신된 동적 matcher로 실행 중이며 `max_players=10`, `assembly_window=5s`, `game_session_id=winters-local-game-session`이다.
- matchmaking queue는 0명, game-server capacity는 비어 있다.
- 검증용 `WintersServer`/`WintersGame` 프로세스는 남기지 않았다. 사용자가 볼 수 없는 숨은 서버 프로세스도 없다.
- 기존 Release 3계정 실동작 검증으로 넥서스 종료→Cloud/account 노출→세 클라이언트 독립 재생→MainMenu 복귀→즉시 재매치가 PASS였다. 이번 변경에서는 1·2·4·10명 backend 매칭·participant·Replay ACL을 자동/통합 검증했고, 각 인원수로 GUI에서 넥서스를 직접 파괴하는 반복 촬영은 수행하지 않았다.
- 4명이 같은 판을 원하면 첫 사용자가 큐에 들어온 뒤 5초 안에 네 계정이 모두 join해야 한다. 늦게 들어온 계정은 다음 cohort로 들어가며, 이는 인원수 고정이 아니라 매치 조립 시간 경계다.

## 최종 판정

`DYNAMIC_1_TO_10_IMPLEMENTED / RELEASE_SERVER_CLIENT_BUILD_PASS / DOCKER_MATCH_MATRIX_PASS / PARTICIPANT_REPLAY_ACL_PASS / CAPACITY_RECOVERY_PASS`

2명이어도 Replay를 볼 수 있고, 4명이면 네 참가 계정 모두 같은 경기 Replay 한 건을 각자의 Cloud/account 탭에서 볼 수 있다. 3명은 더 이상 필수값이 아니다.

# 2026-07-20 Custom Lobby 즉시 진입 복구 결과

## 폐기한 잘못된 설계

- Redis `matchmaking:queue/status/jointime/claim`
- `WINTERS_MATCH_ASSEMBLY_WINDOW=5s`
- 첫 참가자 이후 일정 시간 동안 cohort를 조립하는 matcher worker
- MainMenu의 반복 status polling, 재클릭 cancel UX
- “3클라이언트 Replay”를 “3명이 모여야 시작 가능한 match”로 결합한 전제

이 결합은 Replay 지급에 필요하지 않았다. Replay는 경기당 WRPL 1개를 저장하고 실제 참가 계정마다 DB ACL 한 행을 추가하면 되므로, 로비 진입 방식과 독립적이다.

## 최종 반영

- Backend join은 PostgreSQL의 현재 open Custom Lobby admission을 transaction으로 확보하고 첫 요청부터 즉시 `matched` ticket을 반환한다.
- 1~10명은 host Start 전까지 같은 match ID를 받으며 대기 시간창이 없다.
- UDP ticket에 capacity generation을 포함했고 실제 UUID session에서 길이가 256바이트를 넘는 문제를 발견해 Go/Shared/Server 최대 크기를 384바이트로 일치시켰다.
- Server는 Loading 시작 시 실제 human slot에 연결된 인증 계정만 roster로 남기고, 새 계정의 mid-game 합류를 거부한다.
- Replay worker는 `CompleteMatch`를 upload reserve보다 먼저 실행한다. Backend는 completion artifact의 계정이 lobby admission에 있었는지 검증한 뒤 그 계정만 `match_participants`로 확정한다.
- `MarkReady`는 확정 참가자만 `replay_user_library`에 넣는다. admission-only 계정은 Replay를 볼 수 없다.
- Replay repository와 matchmaking service에서 Redis 의존성을 제거했다. Redis는 인증/프로필 등 다른 서비스 용도로만 남고 이 흐름에는 `matchmaking:*` key가 없다.
- 완료 match가 DB에 확정되기 전에 capacity ready가 앞서지 않도록 upload artifact를 ready notification보다 우선 처리하고, Backend ready는 completed match만 해제한다.
- 촬영용 launcher에서 assembly-window와 Redis queue 정리/검증 코드를 제거하고 서버 창을 숨기지 않도록 변경했다.
- 별도 `StartVisibleReleaseServer.cmd`를 추가해 서버 PID가 실제 보이는 CMD의 자식으로 남도록 했다.

## 검증

- `go test ./...`: PASS.
- Docker PostgreSQL integration, 즉시 admission: 1·2·3·4·10명 PASS.
- Docker PostgreSQL Replay final roster/ACL: 실제 플레이 1·2·3·4·10명 PASS, admission-only outsider 접근 거부 PASS.
- 기존 dynamic replay ACL 1·2·3·4명 PASS.
- Redis `matchmaking:*`: 0개.
- matchmaking/replay Docker image rebuild 및 health HTTP 200 PASS.
- Release x64 Server build PASS: `Server/Bin/Release/WintersServer.exe`.
- Release x64 Client build PASS: `Client/Bin/Release/WintersGame.exe`.
- 현재 실행: visible CMD PID 64200 -> Release Server PID 36008, UDP 9000; Release Client PID 26156/73732 두 개.
- 현재 backend game session: `f27544c6-0b5e-4c95-80a9-31a236d45501`.
- 현재 두 Release 계정은 같은 match `e6f7cd51-7838-4e76-a93d-c503373307c4`에 admission 2명으로 들어왔다. 실제 `/matchmaking/join` 응답은 각각 14ms, 5ms였고 Redis `matchmaking:*` key는 생성되지 않았다.

## 사용자 실플레이 확인 범위

두 Release Client에서 서로 다른 계정 로그인 후 Game Start 즉시 동일 Custom Lobby 입장, Nexus 종료 후 각 계정 Cloud/account에 Replay 1건 노출, 각 창 독립 재생은 현재 열린 실행 환경에서 최종 수동 확인한다.

## 최종 판정

`REDIS_MATCH_QUEUE_REMOVED / IMMEDIATE_CUSTOM_LOBBY_ADMISSION_PASS / ACTUAL_PARTICIPANT_REPLAY_ACL_PASS / RELEASE_BUILD_PASS / LIVE_2_CLIENT_NEXUS_CONFIRM_IN_PROGRESS`

# 2026-07-20 Visual Studio Release 서버 실행 계약 종결 결과

## 1. 예측 vs 실측

- 적중: 3차 독립 델타 비평은 `P0 0 / P1 0 / P2 0 — PASS`였다. helper는 실제 `UsesUdp()` 아래에 들어갔고 duplicate guard는 `ReplayUploadQueue::StartFromEnvironment()`보다 먼저 실행된다.
- 적중: Release debugger 평가값은 `--net-transport=udp`, `winters-local-game-session`, Backend/Replay 환경 6개, environment merge=true였다. compose matchmaking과 capture launcher도 같은 session ID로 일치했다.
- 적중: Server/Client Release `/m:1` 빌드는 모두 exit 0이었다. XML·JSON·PowerShell parse와 관련 tracked diff의 `git diff --check`도 통과했다.
- 빗나감: Docker volume에 migration 12 이전 active capacity가 `generation=0`으로 남아 첫 실행은 UDP 인증 로그 뒤 `Replay upload environment is invalid`, exit 8이었다. 계획의 “stable session 재생성 직후 fresh reconciliation 성공” 예측이 이 legacy 데이터 상태를 빠뜨렸다.
- 수정 실측: 다른 세션의 migration/source는 건드리지 않고 해당 로컬 capacity와 연결 match만 transaction으로 `generation 0→1` 정합화했다. 다음 첫 서버가 원래 reconciliation 경로로 stale allocated match를 `aborted` 처리하고 capacity를 해제했다.
- 적중: 자동 런타임에서 첫 서버 PID 40928이 UDP 9000을 단독 소유하는 동안 MinIO TCP 9000 listener가 함께 유지됐다. duplicate는 명시 오류와 exit 9를 남겼고 capacity는 전후 `winters-local-game-session|<none>|1`로 불변이었다. 첫 서버는 exit 0이었다.
- 적중: `StartVisibleReleaseServer.cmd`는 smoke timeout 없이 `Press 'q' + Enter`를 표시했고 실제 `q` 입력으로 exit 0했다.
- 적중: 사용자 실제 Visual Studio `Release|x64` / `Release Server` 프로필에서 F5와 Ctrl+F5를 각각 실행했다. 두 실행 모두 JobSystem probe PASS 뒤 `UDP signed match-ticket authentication enabled gameSession=winters-local-game-session`, `Replay upload queue enabled`, `transport=udp ... endpoint=0.0.0.0:9000`, `Press 'q' + Enter to quit`까지 동일하게 도달했다.

## 2. 판결

`수정 반영 / AUTOMATED_RELEASE_STARTUP_PASS / DUPLICATE_EXIT9_DB_INVARIANT_PASS / Q_ENTER_PASS / MANUAL_VS_F5_CTRL_F5_PASS`

제품 소스 방향은 유지한다. 자동화 가능한 빌드·설정·Docker·포트·중복 guard·정상 종료 계약과 실제 Visual Studio UI의 F5/Ctrl+F5 workspace-local launch profile acceptance가 모두 통과했다. Visual Studio Release 서버 실행 계약에는 남은 확인 항목이 없다.

## 3. ⑤ 갱신

- `.slnLaunch.user`는 현재 workspace UX이며 다른 checkout의 portable 설정이 아니다. 다른 checkout은 최초 1회 Server-only profile 선택이 필요하다.
- migration 12 적용 시점에 이미 active였던 개발 DB volume은 generation 0 residue를 가질 수 있다. 신규 admission은 1부터 증가하지만 이 legacy 상태는 fresh-server abort API가 받지 않으므로 일회성 데이터 정합화가 필요했다.
- 실제 F5/Ctrl+F5는 XML/evaluated property와 동등 프로세스 실행으로 대체할 수 없는 마지막 수동 검증 비용이었고, 2026-07-20 사용자 실측으로 지불·종결됐다.

# 2026-07-20 계정별 Replay 관점 identity 종결

## 1. 예측 대 실측

- 예측: zxcv3·zxcv4가 같은 canonical WRPL을 내려받았기 때문에 야스오 snapshot이 빠졌거나 두 계정 cache가 잘못 덮어써졌을 수 있다.
- 실측: 문제 match `64fbeee5-b1e7-46b9-99e8-4bab5423cc4a`, replay `4a7c95bc-3460-4d48-81bc-cdaa1a3e6843`의 두 account cache WRPL은 checksum까지 같은 정상 canonical 파일이었다. snapshot에는 `netId=1 / champion=17(LeeSin)`과 `netId=2 / champion=2(Yasuo)`가 모두 존재했다.
- 실측: canonical replay snapshot의 `yourNetId`는 0이고, 기존 `ApplyReplaySpectatorFocus()`는 계정 identity가 없을 때 가장 작은 champion NetId를 골랐다. 따라서 두 계정 모두 `netId=1`, 즉 리신을 보는 것이 코드상 필연이었다.
- 판정: replay 데이터 손실이나 다중 client 파일 충돌이 아니라 `account user_id -> authoritative lobby slot.netId` 관점 identity가 Server→DB→API→Client cache→scene 사이에서 유실된 것이 근본 원인이다.

## 2. 반영 계약

- Server는 Loading 시 실제 human lobby slot의 `slot.netId`를 authenticated participant에 봉인하고, 완전한 roster만 replay upload artifact의 `perspective_net_id`로 직렬화한다. legacy pending artifact는 0으로 읽되 신규 artifact는 0이면 enqueue하지 않는다.
- migration `000013_replay_participant_perspective`는 `match_participants.replay_net_id`와 match 내 non-null unique index를 추가했다.
- replay service는 완료·재시도 시 user/result/perspective를 함께 검증한다. legacy 0→non-zero backfill과 동일 값 retry는 허용하고, non-zero mismatch는 conflict, 이후 0 retry는 기존 값을 지우지 않는다.
- `/replay/me`와 authorized download metadata는 로그인 계정의 `perspective_net_id`를 반환한다. 1·2·3·4·10명 모두 한 canonical WRPL을 공유하되 각 계정은 서로 다른 perspective를 갖는다.
- Client는 Cloud list→download result→account cache `.perspective` sidecar→`CScene_InGame`까지 perspective를 전달한다. sidecar final을 WRPL final보다 먼저 원자 게시하고, account replay의 missing·zero·malformed·overflow metadata는 재생을 막는다. 최소 NetId fallback은 명시적 local/debug replay에만 남겼다.
- replay update와 seek 뒤에는 지정된 NetId만 local player/HUD/camera focus로 다시 적용한다. 해당 NetId가 없으면 다른 champion으로 대체하지 않고 fail-closed 한다.

## 3. 독립 비평 gate

- helper를 `UsesUdp()` 아래로 이동한 VS startup 계획 수정분의 독립 3차 read-only 비평: `P0 0 / P1 0 / P2 0 — PASS`.
- replay perspective 보정 계획의 최종 독립 read-only 비평: `P0 0 / P1 0 / P2 0 — PASS`.
- accepted/held P0/P1가 없는 상태에서만 source edit를 진행했다.

## 4. 검증 실측

- 보호 대상 visible CMD PID 14948, Release Server PID 86476, Release Client PID 67540/83568은 종료 명령을 보내지 않았다. build·migration·replay service 교체 직전 재조회에서 네 PID와 `WintersServer`/`WintersGame` process가 모두 없고 UDP 9000도 비어 있음을 확인했다.
- 사용자 제공 Visual Studio `Release|x64 / Release Server` F5와 Ctrl+F5 로그는 모두 JobSystem probe PASS, signed UDP session, replay upload queue enabled, `0.0.0.0:9000`, `Press 'q' + Enter to quit`까지 도달했다. 기존 판정 `MANUAL_VS_F5_CTRL_F5_PASS`를 유지한다.
- `Server/Include/Server.vcxproj`, Release x64 `/m:1`: PASS, 오류 0, 기존 경고 204.
- `Client/Include/Client.vcxproj`, Release x64 `/m:1`: PASS, 오류 0, 기존 DLL 경계 경고 135.
- `Tools/Harness/ReplayClientSmoke.vcxproj`, Release x64 `/m:1`: PASS, 오류·경고 0.
- `ReplayClientSmoke.exe --perspective-contract-only`: `replay_perspective_contract=pass`. valid/missing/malformed/overflow/listing과 sidecar publish 실패 시 WRPL 미게시 계약을 확인했다.
- `go test ./internal/replay -count=1 -v` with PostgreSQL: PASS. 1·2·3·4·10명 `GetAuthorized`/`ListAuthorized` distinct perspective, outsider 거부, duplicate perspective 거부, legacy retry matrix를 모두 확인했다.
- `go test ./... -count=1`: PASS.
- migration 13은 실행 중인 PostgreSQL에 원자 적용되었고 ledger, BIGINT nullable column, `(match_id, replay_net_id)` partial unique index를 실측했다.
- replay Docker image만 새 source로 rebuild/recreate했다. `services-replay-1`은 새 image digest로 running이고 `http://localhost:8087/health`는 HTTP 200 `{"status":"ok"}`이다.
- 관련 tracked source의 `git diff --check`: whitespace error 0. 다른 session의 dirty source/generated pack은 revert·clean·임의 수정하지 않았다.

## 5. 기존 문제 match 복구 판정과 대가

- zxcv3(`a68fe0c8-f3c1-4d2f-aba5-a9d223e6275c`)와 zxcv4(`ca62814d-c507-4df9-bffb-e27b87c4d507`)의 과거 DB participant에는 `champion_key`, `replay_net_id`가 없고, account `MatchHistory.jsonl`에도 match/result만 있어 누가 리신 net 1이고 누가 야스오 net 2였는지 증명할 수 없다.
- 과거 DB `slot=0/1`은 당시 completion artifact의 unordered participant 순서이므로 lobby NetId 증거로 사용하지 않았다. 추측 backfill은 두 계정 관점을 뒤바꿀 수 있어 수행하지 않았다.
- 기존 sidecar 없는 account cache는 fail-closed 한다. 과거 match는 계정↔champion 대응이 확인된 뒤 DB를 backfill하고 각 계정 Cloud에서 다시 받아야 복구된다. 신규 match는 이 수동 복구 비용 없이 자동으로 perspective가 기록된다.
- canonical WRPL은 client 수와 무관하게 경기당 1개를 유지한다. 추가 비용은 participant당 DB BIGINT 1개와 account cache sidecar 1개뿐이며 2명·3명·최대 10명까지 동일 경로를 사용한다.

## 6. 최종 판정

`VS_RELEASE_STARTUP_CLOSED / REPLAY_ROOT_CAUSE_CONFIRMED / ACCOUNT_PERSPECTIVE_IMPLEMENTED / RELEASE_SERVER_CLIENT_BUILD_PASS / POSTGRES_1_2_3_4_10_CONTRACT_PASS / CLIENT_FAIL_CLOSED_PASS / REPLAY_SERVICE_HEALTH_PASS`

신규 경기의 다중 client replay 관점 계약은 source·DB·API·cache·scene 전체에서 종결되었다. 기존 문제 match는 identity 증거가 애초에 저장되지 않아 안전한 자동 backfill 대상이 아니며, 이를 숨기거나 임의 추정하지 않은 것이 최종 데이터 판정이다.
