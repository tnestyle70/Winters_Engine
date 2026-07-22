# 2026-07-17 매치 플로우 예외처리 전수 감사 보고서

감사 방식: 5축 병렬 read-only 감사(로비/밴픽/로딩/인게임 네트워크/서버 검증) + 교차 검증. Run ID: wf_8d6874e2-9a3.
코드 수정 0 — 이 문서는 판결과 우선순위만 담는다. 수정은 별도 슬라이스.

## 0. 헤드라인 답 — "밴픽에서 미선택 상태로 시작을 누르면?"

**게임은 시작되지 않는다 — 서버가 확실히 막는다.** `CLobbyAuthority::TryStartGame`(Server/Private/Game/LobbyAuthority.cpp:1082-1106)이 점유 슬롯 전체를 순회하며 champion이 NONE/END인 슬롯이 있으면 reject하고 phase는 ChampionSelect에 머문다.

**그러나 유저 체감은 "클릭해도 아무 일 없음"이다.** 3중 누락:
1. 클라 버튼이 픽 상태와 무관하게 항상 활성 — 무조건 StartGame 송신 (Scene_BanPick.cpp:576-581)
2. 게이트용으로 만들어 둔 `IsLocalPlayerChampionPicked`(Scene_BanPick.cpp:917)는 **호출자 0명 — 사어코드**
3. 서버 reject 사유는 `LobbyState.debugMessage`로 클라까지 도착하는데(GameSessionClient.cpp:507-508) **화면에 렌더하는 코드가 없다**

→ 원칙 위반의 전형: 서버 게이트(보안)는 있는데 클라 게이트(UX)와 피드백 루프가 없다.

부속 사실 (갭 종합 확정): ①빈 슬롯은 검사에서 스킵되므로 1인+빈 슬롯 시작은 허용(의도된 솔로 플레이 경로) ②"host만 start" 게이트는 `m_bAllPlayersCanEditBots` 기본 true + SetEditPolicy 미구현으로 사실상 사망 — 아무 착석자나 start 가능 ③로컬 오프라인 경로도 `ValidateRosterForStart`가 reason 버퍼(160B)를 채우고 그대로 버린다 — dead diagnostic.

## 1. 심각도 순위표

| # | 심각도 | 결함 | 현재 동작 | 앵커 |
|---|---|---|---|---|
| 1 | **HANG(전방)** | Loading phase 고착 3종 — ①한 클라 에셋 실패 시 SetReady 미송신 ②로딩 중 이탈 시 서버가 all-ready 재평가 안 함(OnSessionLeave가 슬롯만 정리) ③SetReady fire-and-forget(재시도 없음). 타임아웃/데드라인 자체가 없음 | 나머지 전원이 InGame 경계에서 **무기한 대기** — 이탈자가 재접속해 ready를 다시 보내야만 풀림 | LobbyAuthority.cpp:1143-1145 (TrySetReady에서만 전이 평가) |
| 2 | **보안(서버권위 구멍)** | 비구매 챔피언 픽 우회 — 클라는 어두운 오버레이만(클릭 차단 없음), 서버 TryPickChampion에 소유권 검증 전무 | 미구매 챔피언 자유 픽 — **RP 상점 시스템이 밴픽에서 무력화** | Server/Private/Game (bOwned 검증 0건) |
| 3 | **보안(잠복)** | 재접속 attach가 신원 무관 — "첫 번째 끊긴 인간 슬롯" 반환이라 임의 세션이 남의 챔피언 조종권 획득 가능. TCP 중복 로그인 미검증(identity 미인증 스킵), suspicionCount 임계(5) 소비자 없음 | 무결성 침해가 조용히 성공 / 위조 패킷 무한 전송에도 세션 유지 | TryFindDisconnectedHumanSlot; ServerSessionHub |
| 4 | **HANG(무통지)** | 인게임 서버 사망/네트워크 단절 — 감지는 되나(TCP recv, UDP 15s idle) 팝업·재접속·씬 전환 전무. TCP half-open은 감지 자체가 없음(keepalive 0) | 월드 동결 + 로컬만 비권위 이동으로 조용히 전환 — 유저는 원인 인지 불가 | Scene_InGameNetwork; GameSessionClient |
| 5 | **자원/소급 실행** | 로비 phase 동안 커맨드 무한 축적(무상한 vector) → 개전 첫 틱에 일괄 재생 | 픽창에서 보낸 이동/스킬이 개전 순간 소급 실행 + 메모리 성장 벡터 | CommandBatch/m_pendingCommands |
| 6 | **무음 진단(전반)** | ①reject 사유 debugMessage 렌더 0곳 ②DevSmoke::Log가 컴파일 no-op(네트워크 진단 전멸) ③Release에서 verify 실패 bounded trace가 OutputDebugString 게이트로 무음 ④GameSessionClient verify 실패는 bare return | "왜 안 되는지"를 알 방법이 어떤 빌드에도 없음 — gotchas의 dead diagnostics 패턴 그대로 | GetLastLobbyMessage 호출 0; SmokeLog.cpp:5-12 |
| 7 | 룰 미구현 | 중복 챔피언 유일성 검사 없음, 밴픽 타이머/밴 단계 없음(픽 무기한), **범위 밖 champion id(18~254) 수용** — NONE/END만 거절 | 동일 챔피언 다중 시작 / 방 영구 대기 / 미등록 id로 fallback 챔피언 스폰 | TryPickChampion |
| 7b | **무결성(허위 표시)** | 종료 산출물 저장 실패 무시 — SaveEndOfMatchArtifacts가 쓰기 전 성공 래치, 반환값 폐기, 오버레이는 무조건 "리플레이와 AI trace가 저장되었습니다" 고정 | 디스크 풀/권한 오류 시 전적·trace 무음 유실 + 유저에게 성공 표시 | SaveEndOfMatchArtifacts |
| 8 | 정리 누락 | mid-game 전원 이탈 시 룸 잔존(m_bGameEnded 조건) — 다음 접속자가 반파된 월드에 합류 가능. MatchContext/MatchAssignment 미리셋(Reset 호출자 0) | 방치 시뮬 지속·스테일 티켓 접속 시도 | ResetMatchStateLocked 조건 |
| 9 | UX 소소 | ESC = 무조건 앱 종료(로딩 중 포함, 확인 없음), 로컬 서버 재시도 동기 블록(~2초 프리즈), 죽은 로비 UI 상호작용 유지 | 일시 프리즈·오조작 종료 | WM_KEYDOWN VK_ESCAPE 전역 |

## 2. 잘 돼 있는 것 (감사가 확인한 강점 — 이력서 재료)

- **서버 검증 깔때기는 실재하고 촘촘하다**: 프레임 파싱 → 패킷 verify → 세션별 시퀀스 윈도우 → 커맨드 인그레스(Move 코얼레싱) → 세션-엔티티 바인딩 → 실행기 게이트(alive/action-lock/CanMove/CanCast/쿨다운/마나) → 로비 권위(픽 완료 검사)
- 로비 슬롯 권위 견고: 전 변이 mutex 하 검증, 정원/점유/봇 중복 클릭 멱등, 중도 이탈 슬롯 재접속 예약
- 클라 에셋 실패는 클라 측에서는 잘 가드됨(필수 리소스 실패 시 진행 차단), 서버 사망 감지 자체는 존재
- 로딩은 하이브리드(메인 스레드 증분 LoadStep + 맵 서피스 WMesh 1개 백그라운드 잡) — "백그라운드에서 준비"는 부분 사실로 정정

## 3. 수정 우선순위 제안 (다음 슬라이스)

1. **P0 — Loading 고착 해소**: OnSessionLeave에서 all-ready 재평가 + Loading phase 데드라인(예: 60초) 후 미준비 슬롯 봇 전환 or 킥
2. **P0 — reject 사유 표면화**: debugMessage를 밴픽/로비 UI 토스트로 렌더(이미 데이터는 도착해 있음 — 렌더 1곳이면 끝) + `IsLocalPlayerChampionPicked`로 버튼 게이트(사어코드 부활)
3. **P1 — 소유권 서버 검증**: TryPickChampion에 계정 보유 챔피언 검사(백엔드 소유 목록을 매치 티켓에 동봉) + 클라 클릭 차단
4. **P1 — 단절 통지**: IsConnected 전이 감지 → 인게임 모달("연결 끊김") + TCP keepalive/idle 타임아웃
5. **P2 — 로비 커맨드 드롭**: 로비 phase에서 gameplay 커맨드 즉시 폐기(축적 금지)
6. **P2 — 재접속 신원 매칭**, suspicion 임계 집행, mid-game abandon 룸 리셋
7. **P3 — 중복 픽 유일성**, ESC 확인 다이얼로그, DevSmoke 재점화 정책

## 4. 이력서/포트폴리오 환전

이 감사 자체가 도메인 C의 증거다: "행복 경로가 아니라 어긋난 경로를 전수 감사해 HANG 2계열·보안 2계열·무음 진단 1계열을 특정하고 우선순위로 수복했다"(수복 후 문장). 특히 "서버는 막는데 클라가 침묵한다" 발견은 **클라 게이트(UX)와 서버 게이트(보안)는 둘 다 있어야 한다**는 원칙의 실증 사례로 면접 답변에 직결된다.
