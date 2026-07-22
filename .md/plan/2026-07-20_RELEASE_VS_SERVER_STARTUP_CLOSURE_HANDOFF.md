Session - 현재 촬영 실행을 보존하면서 Visual Studio Release Server 직접 실행 계약을 다음 작업에서 영구 종결한다.
좌표: 없음 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-19_RELEASE_AUTHENTICATED_ACCOUNT_REPLAY_E2E_PLAN.md / RESULT.md

# 1. 사용자 목표

사용자는 Docker Backend를 켠 뒤 Visual Studio에서 `Release|x64`, Server 시작 프로젝트를 실행하고, 별도로 Release Client를 필요한 수만큼 실행하는 기존 작업 흐름을 원한다. 매번 UDP 인자·Backend 환경변수·game-session ID를 기억하거나 Codex의 숨은 프로세스를 찾으면 안 된다.

# 2. 확정 원인

1. `ServerRuntimeOptions::networkMode` 기본값은 TCP다.
2. Visual Studio의 `Server.vcxproj.user`는 비어 있어 직접 실행 시 `WintersServer.exe`에 인자와 Backend 환경이 전달되지 않았다.
3. Docker MinIO가 Replay object storage로 TCP 9000을 점유한다.
4. 따라서 인자 없는 Release Server는 TCP 9000을 열려다 종료했고, Backend가 UDP 9000 endpoint를 준 Release Client는 `Server Required`를 표시했다.
5. Backend가 게임 서버에 합쳐진 것이 아니다. Auth/Profile/Matchmaking/Replay/PostgreSQL/Redis/MinIO는 Docker 서비스이고, `WintersServer.exe`는 별도 authoritative game server다.
6. `UpdateLib.bat`의 `EngineSDK/inc/*_Manager.h` 삭제는 의도된 SDK 은닉 정책이며 이번 문제와 무관하다. 이전 세션에서 이를 빌드 위험으로 판단한 것은 오판이었다.

# 3. 현재 실행 상태 — 건드리지 말 것

2026-07-20 현재 사용자의 즉시 촬영을 위해 기존 launcher로 실행했다.

```text
Backend game session: f27544c6-0b5e-4c95-80a9-31a236d45501
Visible CMD PID: 14948
Release Server PID: 86476
Server command: Server\Bin\Release\WintersServer.exe --net-transport=udp --smoke-seconds=21600
UDP 9000 owner: PID 86476
Release Client PID: 67540
Release Client PID: 83568
Backend health: 8081/8083/8084/8087 모두 HTTP 200
```

새 작업은 위 PID를 종료하거나, 사용자가 촬영을 끝내기 전에 Server/Client Release 링크 빌드를 실행하지 않는다. 실행 파일이 잠긴 상태에서 빌드하면 과거의 LNK1104/PDB write 오류가 재현된다. 현재 launcher는 smoke 모드라 `q + Enter`가 아니라 CMD 창 닫기 또는 Ctrl+C로 종료해야 한다.

# 4. 현재 파일 상태와 소유권

- 작업 트리는 대규모 dirty 상태다. 다른 세션의 gameplay/UI/data 변경을 blanket revert하지 않는다.
- 컨텍스트 압축 후 `Server/Private/main.cpp`와 Replay worker 쪽 P1 보정이 Release binary에 빌드됐지만, 당시 전체 Replay 재비평/E2E는 끝나지 않았다.
- 이번 작업에서 제품 소스는 아직 수정하지 않았다. 기존 Replay PLAN 끝에 `# 2026-07-20 Visual Studio Release 서버 실행 계약 종결` 초안과 두 차례 비평 disposition만 추가했다.
- 현재 계획의 마지막 수정은 helper 삽입 위치를 `UsesUdp()` 정의 아래로 옮겼다. 새 작업이 3차 독립 델타 비평을 수행해 `P0/P1 0`을 확인하기 전에는 구현하지 않는다.
- definition generator와 generated pack은 이번 실행 설정 범위가 아니다. 다른 데이터 세션 산출물을 재생성하거나 덮어쓰지 않는다.

# 5. 계획 문서의 확정 방향

정본은 아래 문서 맨 끝 절이다.

- `.md/plan/2026-07-19_RELEASE_AUTHENTICATED_ACCOUNT_REPLAY_E2E_PLAN.md`
- 절: `2026-07-20 Visual Studio Release 서버 실행 계약 종결`

반영 예정 범위:

1. `Server/Private/main.cpp`
   - authenticated UDP desktop-session named mutex를 `ReplayUploadQueue::StartFromEnvironment()` 및 destructive fresh-server reconciliation보다 먼저 획득한다.
   - 중복 서버는 exit 9와 명확한 오류를 남기고 Backend capacity를 건드리지 않는다.
2. `Server/Include/Server.vcxproj`
   - Release debugger 인자 `--net-transport=udp`.
   - stable local session `winters-local-game-session`과 Matchmaking/Replay 개발 환경.
   - environment merge를 명시한다.
3. `Winters.slnLaunch.user`
   - 현재 Server+Client 동시 시작 프로필을 `Release Server`, `Release Client`로 분리한다.
   - 사용자별 파일이므로 현재 workspace UX 보정이며 다른 checkout의 portable 계약으로 과장하지 않는다.
4. `Tools/Harness/StartVisibleReleaseServer.cmd`
   - interactive 실행에서는 `--smoke-seconds`를 제거해 `q + Enter` 정상 종료를 복구한다.
5. `Tools/Harness/StartReleaseAccountReplayCapture.ps1`
   - game server identity는 `winters-local-game-session`으로 고정한다.
   - 증거 디렉터리는 별도 GUID `capture_run_id`로 구분한다.
6. `.claude/gotchas.md`
   - Build 성공과 authenticated Release launch 성공은 별개이며, UDP/session/env/단일 인스턴스가 하나의 실행 계약이라는 재발 방지 규칙을 기록한다.

# 6. 독립 비평 이력

1차: `P0 0 / P1 2 / P2 3 — FAIL`

- 중복 서버가 bind 실패 전에 기존 capacity를 abort할 수 있음.
- XML parse와 동등 프로세스 실행만으로 실제 VS F5를 증명할 수 없음.

2차: `P0 0 / P1 1 / P2 2 — FAIL`

- mutex helper 초안이 아직 선언되지 않은 `UsesUdp()`를 호출하는 위치라 C3861 가능.
- 계획의 helper anchor를 `UsesUdp()` 정의 바로 아래로 수정했다.
- 3차 델타 재비평은 아직 수행하지 않았다.

# 7. 새 작업 실행 순서

1. `AGENTS.md`, `.claude/gotchas.md`, codebase compass, `CLAUDE_Legacy.md`, 계획서 작성 규칙을 읽는다.
2. 본 문서와 정본 PLAN의 마지막 절, 관련 실제 파일을 대조한다.
3. 독립 sub-agent에게 마지막 수정분 read-only 델타 비평을 맡긴다. `P0/P1 0` 전에는 제품 소스를 수정하지 않는다.
4. 사용자의 현재 Server/Client PID가 살아 있으면 구현은 가능해도 Release 링크 빌드와 Docker 재기동은 보류한다. 프로세스를 임의 종료하지 않는다.
5. 계획 통과 후 정본 PLAN의 정확한 블록만 surgical edit한다.
6. 사용자가 촬영 종료 후 `WintersServer`/`WintersGame` 0개와 UDP 9000 free를 확인한다.
7. Docker를 override 없는 stable session으로 재생성한다.
8. Server Release와 Client Release를 `/m:1`로 직렬 빌드한다.
9. VS debugger property XML/evaluated value, Backend container session, launcher session이 모두 `winters-local-game-session`인지 검사한다.
10. 첫 서버를 실행하고 active match/generation을 기록한다. 두 번째 서버 실행이 exit 9이며 DB 값이 불변인지 검증한다.
11. TCP 9000은 MinIO, UDP 9000은 WintersServer가 동시에 소유하는지 확인한다.
12. 실제 Visual Studio에서 `Release Server` 프로필 F5 1회, 종료, Ctrl+F5 1회를 수동 확인한다. 둘 다 `transport=udp`, stable session, Replay queue enabled가 보여야 한다.
13. 같은 이름 RESULT 문서의 마지막에 예측 대 실측, 판결, 대가 갱신을 추가한다.

# 8. 합격 기준

```text
P0/P1 critique remainder = 0
Docker session = winters-local-game-session
VS Release Server transport = udp
TCP 9000 owner = MinIO
UDP 9000 owner = exactly one WintersServer
duplicate Server = exit 9 before Backend mutation
Release Server build = PASS
Release Client build = PASS
F5 restart = PASS
Ctrl+F5 restart = PASS
visible/manual server shutdown = q + Enter PASS
Client 정상 Custom Lobby 진입 = PASS
```

# 9. 금지 사항

- 현재 촬영 프로세스를 새 작업이 자동 종료하지 않는다.
- server default를 Release 전역 UDP/dev-secret hardcode로 바꾸지 않는다. debugger/local launcher metadata에서만 개발 계약을 제공한다.
- Replay 지급을 다시 Redis queue, assembly window, 고정 클라이언트 수와 결합하지 않는다.
- `UpdateLib.bat`의 Manager header purge를 회귀로 되돌리지 않는다.
- unrelated dirty files, generated gameplay pack, UI/champion 변경을 정리하거나 revert하지 않는다.

# 10. 인수인계 판정

`CURRENT_CAPTURE_RUNTIME_READY / PRODUCT_SOURCE_NOT_EDITED_FOR_VS_FIX / PLAN_CRITIQUE_P1_FIXED_IN_DOC / THIRD_CRITIQUE_AND_IMPLEMENTATION_PENDING`
