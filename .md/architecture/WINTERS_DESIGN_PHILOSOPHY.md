# Winters Design Philosophy

작성일: 2026-07-09
기원: NYPC 봇 대회 회고 — (1) 디버깅이 수월한 구조가 이긴다, (2) 예외 처리가 다른 구조를 건드리면 안 된다, (3) 1등 봇의 코드가 길었던 이유는 모든 예외/특수상황을 전부 코드로 명시했기 때문이다.

이 문서는 Winters 전 계층(Engine/Client/Server/Shared/Services/Tools)에 적용되는 설계 원칙 4개를 고정한다. 세부 규칙은 도메인 문서로 위임한다:
- 예외/실패 처리 규약: `WINTERS_ERROR_HANDLING_POLICY.md`
- 의존성 방향과 현재 실측 상태: `WINTERS_DEPENDENCY_MAP.md`
- 온보딩/패치 체크리스트: `WINTERS_HANDOFF_GUIDE.md`
- 계층 소유권 compass: `WINTERS_CODEBASE_COMPASS.md`

## P1. 실패는 발생 지점에서 즉시, 가시적으로 처리한다

- 실패 경로는 반드시 관측 가능해야 한다: bounded `OutputDebugStringA`(또는 Winters 래퍼), 카운터, 상태 문자열 중 최소 하나.
- `return {};` / `return nullptr;` / bare `return;` 만 있는 실패 경로는 금지. 무엇이, 어디서, 왜(HRESULT/WSA 코드/경로) 실패했는지 남긴다.
- 진단을 포맷만 하고 출력하지 않는 코드(dead diagnostics)는 로그가 없는 것보다 나쁘다 — 디버깅하는 사람이 "트레이스가 있다"고 믿게 만든다. 출력하거나 삭제한다.
- 성공 로그는 결과를 확인한 뒤에만 출력한다. 반환값을 버리고 "loaded" 를 찍는 코드는 거짓말이다.
- 근거 사례: 2026-07-09 감사에서 SnapshotApplier(1,652줄)에 OutputDebugString 호출 0개, sprintf_s 8곳 전부 미출력이었고, 과거 미니언 프리즈 사고(gotcha 2026-04-28)도 빈 경로 silent fail이 원인.

## P2. 한 구조의 실패가 다른 구조를 오염시키지 않는다

- 부분 초기화 후 실패하면, 실패를 상위에 알리고(반환값) 이미 만든 것을 정리하거나 명시적 degraded 상태로 전환한다. "실패했지만 계속 진행"은 반드시 로그와 함께 의도된 폴백이어야 한다.
- 폴백 사다리는 명시적이고 로그된다. 모범: 서버 내비게이션 (authored grid 실패 → bake → structures-only → all-walkable, 각 단계 `[ServerNav]` 로그, `GameRoomNav.cpp`).
- 계층 간 오염 방지의 최전선은 의존성 방향이다: Engine은 제품 코드를 모르고, Shared/GameSim은 Engine/Renderer/UI/DX를 모르고, Server는 Client 비주얼을 모른다. 위반은 `WINTERS_DEPENDENCY_MAP.md`의 위반 목록에 근거와 함께 기록하고 슬라이스로 해소한다.
- 스레드 경계도 오염 경계다: IOCP 스레드는 ingress mutex까지만, tick 스레드가 truth를 소유한다. 네트워크 실패는 세션 단위로 격리(단절)하고 룸 전체 상태를 건드리지 않는다.

## P3. 모든 특수상황은 코드에 명시한다

- "일어나지 않을 것"이라는 가정 대신 분기를 쓴다. 단, Karpathy 가드레일의 "불가능한 시나리오에 대한 에러 처리 금지"와의 균형: 입력이 외부(네트워크, 파일, 유저, 에셋)에서 오면 특수상황이고, 내부 불변식이면 assert/검증 게이트다.
- 실패가 여러 원인을 가질 수 있으면 원인을 구분한다 (예: Pathfinder의 빈 경로는 null-grid / start-blocked / goal-blocked / no-route 4가지를 합쳐버림 — reason enum 도입이 로드맵에 있음).
- 열거형 switch에는 default에 로그를 두어 "새 케이스 추가 후 처리 누락"을 가시화한다 (unknown packet kind, unknown cue name 등).
- 특수상황 처리 코드는 흩어놓지 않고 이름 있는 카탈로그/헬퍼로 모은다 (gotcha 2026-05-26).

## P4. 디버깅이 수월한 구조를 먼저 만든다

- 증상 튜닝 전에 관측 장치부터: debug UI/overlay, bounded 트레이스, 시각 캡처 (CLAUDE.md 디버깅 파이프라인 규칙과 동일).
- 이동/경로: 현재 셀, 다음 웨이포인트, 해석된 경로, 보정 방향, stuck/resolve 이유를 노출한다.
- 복제 경계: "패킷이 안 옴"과 "패킷이 거부됨"이 구분되어야 한다 — verify 실패 카운터/트레이스 필수.
- 로그 규율은 gotcha 2026-05-28과 양립한다: **루틴 트레이스**(틱/스냅샷/yaw 정상 흐름)는 게이트 뒤에 두거나 사용 후 제거하고, **실패 진단**은 bounded로 항상 살아 있어야 한다. 이 구분이 이 철학의 핵심 운영 규칙이다.

## 회사 코드베이스 작업 원칙 (기존 프레임워크 안에서의 패치/업데이트)

1. 기존 구조를 먼저 읽는다: 같은 문제를 푸는 기존 인프라(카탈로그, 레지스트리, 매니저)를 grep으로 확인 후 재사용/확장한다. 두 번째 렌더러/캐시/업데이트 루프를 만들지 않는다.
2. 변경은 요청에 1:1로 추적 가능해야 한다 (Karpathy Surgical Changes).
3. 모든 변경은 실제 실행 환경에서 검증한다: 빌드 통과는 최소선이고, 해당 흐름(F5 스모크, 서버+클라 왕복, SimLab 결정성)을 실제로 구동한다. 서버 로그만으로 클라 비주얼 성공을 판정하지 않는다 (CLAUDE_Legacy 규칙).
4. 실패를 관측할 수 없는 변경은 완성이 아니다: 새 기능의 실패 분기에 P1 진단이 없으면 리뷰에서 반려한다.
5. 반복된 실수는 `.claude/gotchas.md`에, 구조 결정은 compass/이 문서 계열에 기록한다.
