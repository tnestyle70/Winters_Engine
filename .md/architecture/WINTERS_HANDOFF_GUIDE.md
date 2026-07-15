# Winters Handoff Guide (인수인계 / 온보딩 / 패치 체크리스트)

작성일: 2026-07-09 (커밋 f9d4d5c 기준). 새 작업자(사람/에이전트)가 이 코드베이스에서 패치·업데이트를 안전하게 수행하기 위한 문서. 구조 상세는 `WINTERS_DEPENDENCY_MAP.md`, 실패 처리 규약은 `WINTERS_ERROR_HANDLING_POLICY.md`, 원칙은 `WINTERS_DESIGN_PHILOSOPHY.md`.

## 1. 읽기 순서 (신규 진입)

1. `AGENTS.md` → `.claude/gotchas.md` → `.md/architecture/WINTERS_CODEBASE_COMPASS.md`
2. 이 문서 + `WINTERS_DEPENDENCY_MAP.md` (구조/빌드 그래프)
3. 도메인 진입 시: 게임플레이·서버권위 = `CLAUDE_Legacy.md`, C++ 컨벤션 = `WINTERS_ENGINE_CONVENTIONS.md`, UI = `WINTERS_UI_PIPELINE_ARCHITECTURE.md`
4. 문서 신뢰 규칙: **code wins over docs**. 특히 `WINTERS_ENGINE_INTEGRATION_REVIEW.md`는 세션 로그라 일부 스테일 (Engine→GameSim UI 위반 주장, "루트 CMake 없음" 주장은 이미 해소/변경됨).

## 2. 빌드 파이프라인 (처음 빌드하기 전에)

- 표준 빌드: `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m` (Debug|x64 / Release|x64만 존재).
- 빌드 순서는 sln이 관리한다. **개별 vcxproj 단독 빌드 금지** — Client/EldenRingClient는 Engine ProjectReference가 없어 stale EngineSDK lib에 링크된다.
- `UpdateLib.bat` = Engine → EngineSDK 배포 허브. Engine public 헤더를 고치면 SDK sync가 필요하지만 Engine PostBuild/각 PreBuild가 자동 수행. **EngineSDK/inc를 손으로 고치지 말 것** (생성물).
- `Shared/Schemas/run_codegen.bat` = flatc 코드젠, 세 프로젝트가 매 빌드 실행. `.fbs`가 계약 원본이고 Generated는 손대지 않는다.
- 알려진 빌드 함정: 병렬 빌드 시 flatc/UpdateLib 파일 레이스 가능(재빌드로 해소), EngineSDK/inc가 반쯤 빈 상태로 깨지면 `WINTERS_SDK_PURGE=1` 후 Engine 리빌드.
- 리소스는 `Client/Bin/Resource`에서만 해석 (config별 복사본 금지 — AGENTS.md 규칙).

## 3. 런타임 구동 검증 (패치마다)

- 클라 단독: F5 (Scene_Login → MainMenu → InGame 스모크). 서버 없으면 "Server not reachable ... local-only mode" 로그 후 오프라인 흐름 — 정상.
- 서버+클라: `WintersServer.exe`(포트 9000, 룸 1개 하드코딩) 먼저, 클라 접속. 서버 콘솔에서 `hp <netId> <value>` 디버그 명령 가능, `q` 종료.
- 결정성: `Tools/Bin의 SimLab.exe [tickCount] [seed]` — exit 0=결정적, 1=발산.
- 리플레이: 룸 종료 시 `Replay/room*_tick*.wrpl` 자동 기록 (메모리 누적 → 종료 시 저장; 장시간 매치는 메모리 주의).
- 판정 규칙: **서버 로그만으로 클라 비주얼 성공을 판정하지 않는다.** 비주얼 작업은 Client Debug x64 실행 확인, 권위 작업은 서버+클라 왕복 확인 (CLAUDE_Legacy 검증 기준).

## 4. 패치/업데이트 전 체크리스트

compass의 "작업 전 체크" 7문항에 더해:

1. 같은 문제를 푸는 기존 인프라가 있는가? (`rg`로 카탈로그/레지스트리/매니저 확인 — 제2의 렌더러/캐시/루프 금지)
2. 이 변경의 실패 분기에 P1 진단이 있는가? (`WINTERS_ERROR_HANDLING_POLICY.md` §1~2)
3. gameplay truth를 만드는가? → Shared/Server로. Client는 presentation만. Bot AI는 GameCommand 생산자이며 truth 컴포넌트를 직접 mutate하지 않는다.
4. Engine public 헤더를 바꾸는가? → dllexport 시그니처 변화면 Client/Server 리빌드 파급, SDK sync 확인. dllexport 클래스에 unique_ptr 멤버 추가 시 copy ctor/assign 명시 delete (gotcha 2026-04-23).
5. 컴포넌트 레이아웃을 바꾸는가? → trivially-copyable static_assert 유지, 스냅샷 직렬화(SnapshotBuilder) 필드 대응 확인.
6. `.fbs`를 바꾸는가? → 클라·서버 동시 배포 필요. verify 실패는 이제 bounded 로그로 보인다(`invalid Snapshot buffer`) — 스키마 drift 증상은 "월드 프리즈 + 이 로그".
7. 새 파일 추가 → 소유 vcxproj + .filters에 등록 (CMake는 CMake 소유 타깃만 — gotcha 2026-05-28). Client.vcxproj의 ExcludedFromBuild 패턴(InputSystem/SkillDispatchSystem) 근처는 특히 주의.
8. Shared/GameSim에서 Engine ECS 타입이 필요하면 **직접 `ECS/*`를 include하지 말고** `Shared/GameSim/Core/Ecs/` 어댑터를 경유한다 — GameSim PreBuild의 `Check-SharedBoundary.ps1`이 직접 include를 빌드 실패로 만든다. sim 진단 출력은 `Core/Debug/SimDebugOutput.h`의 `WintersOutputAIDebugStringA`.
8. 빌드 후 `git diff --check` + 해당 흐름 실제 구동.

## 5. 알려진 지뢰밭 (건드리기 전에 반드시 읽기)

- **CHttpClient async 계약 (2026-07-09 재설계)**: AsyncGet/AsyncPost는 이제 진짜 비동기다 — future를 `m_PendingRequests`가 소유하고 worker는 호출 시점 `RequestSnapshot` 복사본만 읽는다. **파괴 시점에는 진행 중인 요청이 끝날 때까지 블로킹**하므로(수명 안전의 대가) 소유자(AuthClient 등)를 리셋하는 코드는 최악의 경우 WinHTTP 타임아웃만큼 대기할 수 있다. 이 드레인 계약을 깨면(future detach 등) use-after-free가 열린다.
- **Backend 폴더의 동명 클래스 ODR 함정**: `Client/Public/Network/Backend/SnapshotApplier.h`와 `CommandSerializer.h`는 살아 있는 `Network/Client/` 버전과 같은 클래스명의 죽은 스텁. .cpp를 vcxproj에 다시 넣거나 잘못된 헤더를 include하면 조용히 ODR 위반.
- **Shared/Network/PacketDef.h는 죽은 계약**: includer 0, TPS=20으로 실제(30Hz)와 모순. 참조 금지.
- **CServerEntry는 스텁**: Initialize가 false 반환, Get_JobSystem이 nullptr — JobSystem 서버 통합은 미완. 호출자 없음.
- **suspicion 시스템은 미집행**: CSession::IsSuspicious 호출자 0 — 악성 패킷은 드랍만 되고 킥 없음.
- **Scene_InGame은 8,844줄 갓클래스** (9개 TU 분할, 단일 클래스): 헤더 수정 시 클라 절반 리컴파일. 챔피언별 상태가 헤더에 하드코딩 — 새 챔피언 추가 시 갓헤더 수정을 피하는 방향으로.
- **SnapshotApplier의 yaw 계측 블록 2곳은 포맷-후-폐기 상태로 보존됨** (commit 1813b00이 출력만 제거): yaw 디버깅 시 OutputDebugStringA를 로컬로 재무장해서 쓰고, 작업 후 상태 결정 필요.
- **EldenRingEditor는 CMake 전용** — sln 워크플로에서 절대 빌드되지 않아 Engine API 변경 시 조용히 썩는다. Elden 자산이 `Client/Bin/Resource/EldenRing/`에 있는 소프트 커플링도 인지.
- **미니언과 챔피언의 facing 컨벤션은 다르다** (gotchas 2026-05-20 클러스터): 전역 yaw 변경 전 CommandExecutor/MoveSystem/SnapshotApplier/FaceMoveDirection 비교.
- **`.claude/worktrees/`에 vcxproj/배치 복사본 9벌** — repo-wide glob으로 "그" 파일을 찾는 도구는 워크트리 복사본을 잘못 집을 수 있다. 항상 메인 레포 기준.
- **profiler.json / Profiles/ / Replay/ / EngineSDK/ / out/ 은 Derived** — 손으로 편집하지 않고 커밋 대상 아님 (ROOT_TRUTH_BASELINE).

## 6. 협업 원칙 (요약)

- 문서 역할 분담: 반복 실수 = gotchas, 구조 결정 = compass/architecture 문서, 일회성 작업 = `.md/plan/YYYY-MM-DD_*.md` (Session 헤더 형식).
- 계획서는 `.md/계획서작성규칙.md` 형식 (기존 코드 앵커 + 교체/추가 블록, 새 파일은 전문).
- 커밋 전: 요청과 1:1로 추적되는 diff인지(Surgical Changes), 실패 분기 진단이 있는지, 빌드+구동 검증했는지.
- 트레이스 로그 규율: 실패 진단은 bounded로 상시, 루틴 트레이스는 게이트/제거 (POLICY §1.5).
