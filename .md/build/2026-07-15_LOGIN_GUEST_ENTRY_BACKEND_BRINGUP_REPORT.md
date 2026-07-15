# 2026-07-15 로그인 게스트 입장 + 백엔드 기동 보고서

## 증상
- 로그인 화면 ImGui `계정 로그인` 창에 회원가입 버튼이 없고, docker를 띄워도 로그인 실패.

## 원인 (3중)
1. 실행 중이던 `Client/Bin/Debug/WintersGame.exe`(00:32 빌드)가 S030 이후 코드(`회원 가입` 버튼, Scene_Login.cpp 00:36 수정분)보다 오래됨 — 회원가입 버튼은 코드에 이미 존재.
2. docker 컨테이너(winters-postgres/redis/kafka)가 `Exited (255)` 상태 — Docker Desktop 재시작 후 컨테이너 미기동.
3. auth HTTP 서비스(:8081)는 docker가 아니라 별도 Go 프로세스(`go run ./cmd/auth`)이며, `/auth/id/*` 엔드포인트는 `WINTERS_DEV_AUTH_ENABLED=true` 환경변수 필요(기본 false). profile(:8084)/shop(:8086)도 메인메뉴 초기 sync에 필요.

## 반영
- `Client/Private/Scene/Scene_Login.cpp`: `회원 가입` 버튼 아래에 `비회원으로 시작` 버튼 추가 — 기존 `RequestOfflineLogin()`(오프라인 계정 SetOfflineAccount) 재사용, `m_bLoginInFlight` 동안 비활성.
- 신규 `Services/StartBackend.ps1`: docker compose up -d → postgres healthy 대기 → auth(WINTERS_DEV_AUTH_ENABLED=true)/profile/shop 3창 기동.

## 검증
- MSBuild Debug x64 순차: GameSim → Server → Client 전부 exit 0 (`ALL BUILDS PASSED`, exe 00:44 갱신).
- `git diff --check` Scene_Login.cpp 통과.
- 백엔드 E2E: 미등록 ID 로그인 404 → `/auth/id/register` 성공(user_id/토큰 발급) → 재로그인 200. 포트 8081/8084/8086 LISTENING.
- 마이그레이션 9테이블(pgdata 볼륨) 기존 적용 확인 — 신규 적용 불필요.

## 남은 확인
- 사용자 인게임 눈검증: 새 exe에서 `회원 가입`/`비회원으로 시작` 버튼 노출, 실제 가입→로그인→메인메뉴 진입.
