# 01. Winters 엔진 전체 개관 — "프로젝트 소개해주세요"의 완성 답안

> 이 챕터는 면접의 첫 질문 "프로젝트를 소개해주세요"에 대한 대본이자, 이후 모든 심화 질문의 진입 지도다.
> 코드 문법 관련 답변은 `.md/interview/cpp/` 세트가 담당한다. 여기서는 도메인 구조와 의사결정만 다룬다.

---

## 1. 무엇을 만들었나 — 한 줄 정의

**"League of Legends 스타일의 서버 권위(server-authoritative) 멀티플레이어 MOBA를, 상용 엔진 없이 DirectX 11 기반 자작 엔진 위에서 클라이언트·서버·공유 시뮬레이션까지 전부 직접 구현한 프로젝트입니다."**

구체적으로 지금 돌아가는 것:

- **자작 엔진 DLL** (`WintersEngine.dll`): 윈도우/프레임 루프, RHI(DX11 기본, DX12 병행 이관), ECS, JobSystem(work-stealing), 내비게이션(A*/flow-field), 자체 프로파일러, 자체 바이너리 에셋 포맷(`.wmesh`/`.wskel`/`.wanim`/`.wmat`/`.wfx` 등 — `.md/architecture/WINTERS_CODEBASE_COMPASS.md` Tools/Editor 절)
- **게임 서버** (`WintersServer.exe`): IOCP 기반 TCP 서버, 30Hz 고정 틱 결정론 시뮬레이션, 스냅샷/이벤트 복제, 로비/밴픽/재접속, 봇 AI
- **게임 클라이언트** (`WintersGame.exe`): 스냅샷 적용 + 보간/예측, HUD/FOW, FX, 리플레이
- **공유 시뮬레이션** (`WintersGameSim.lib`): 서버·클라·SimLab이 함께 쓰는 결정론 게임플레이 코어 — 챔피언 15종의 스킬 훅, 데미지 파이프라인, 챔피언 AI (`Server/Private/Game/GameRoomTick.cpp`의 챔피언 Tick 목록)
- **확장 트랙**: 같은 엔진 DLL 위에 Elden 스타일 액션 RPG 클라이언트(`WintersElden.exe`)를 별도 제품으로 올리는 멀티 제품 구조 진행 중

핵심 셀링 포인트를 한 문장으로 더 얹으면: **"게임이 돌아간다"가 아니라 "치트 불가능한 구조로 돌아간다"** — 클라이언트는 판정을 하나도 소유하지 않는다.

---

## 2. 왜 만들었나

세 층위로 답한다 (면접에서 "왜 상용 엔진 안 썼나"는 반드시 나온다).

1. **엔진 내부를 소유하기 위해.** Unity/Unreal을 쓰면 렌더 파이프라인, 메모리, 스레딩, 네트워크 복제가 블랙박스다. 나는 프레임이 17.8ms에서 9ms로 떨어지는 이유를 프로파일러 카운터로 직접 증명하고 싶었고, 그러려면 계측 지점을 내가 소유해야 했다.
2. **서버 권위 네트워크 게임의 전체 수직 슬라이스를 경험하기 위해.** MOBA는 입력→명령→서버 판정→복제→연출이라는 권위 분리 문제의 교과서다. 이 파이프라인을 한 번 전부 소유해 보면 어떤 온라인 게임 팀에 가도 경계 문제를 이해할 수 있다.
3. **설계 원칙을 실험할 검증장이 필요해서.** LoL 복각과 Elden 복각은 최종 제품이 아니라, "하나의 엔진이 장르가 다른 두 게임을 지탱할 수 있는가"를 검증하는 두 개의 시험장이다 (`.md/architecture/WINTERS_CODEBASE_COMPASS.md` 제품 방향 절).

---

## 3. 전체 레이어 지도 — 5개 산출물과 소유권

빌드 산출물 기준 (`.md/architecture/WINTERS_DEPENDENCY_MAP.md` §1, 실측):

```text
                    ┌────────────────────────────────────────────┐
                    │  Shared/GameSim  (WintersGameSim.lib)      │
                    │  gameplay truth의 타입과 결정론 시뮬 계약   │
                    │  GameCommand, Snapshot/Event 스키마,        │
                    │  챔피언 스킬 훅, 데미지 파이프라인          │
                    └───────────┬───────────────┬────────────────┘
                ProjectReference│               │ProjectReference
                                ▼               ▼
   ┌────────────────────┐   ┌──────────────────────┐
   │ Server              │   │ Client               │
   │ (WintersServer.exe) │   │ (WintersGame.exe)    │
   │ 30Hz 틱, IOCP,      │   │ 스냅샷 적용, 보간/예측│
   │ 판정의 유일한 권위  │   │ HUD/FOW/FX = 연출만  │
   └─────────┬──────────┘   └─────────┬────────────┘
   Engine 직접│참조              EngineSDK│lib 링크 (ProjectReference 없음)
             ▼                         ▼
   ┌─────────────────────────────────────────────┐
   │ Engine (WintersEngine.dll)                  │
   │ RHI/렌더러/ECS/JobSystem/내비/리소스/프로파일러│
   │ ── 제품 명사(LoL/Elden)를 모른다 ──          │
   └───────────────────┬─────────────────────────┘
            PostBuild: UpdateLib.bat
                       ▼
   ┌─────────────────────────────────────────────┐
   │ EngineSDK (inc/lib/bin 배포 허브)            │
   │ 헤더 미러링 + *_Manager.h purge              │
   │ = CGameInstance 게이트웨이 경계를 물리로 강제 │
   └─────────────────────────────────────────────┘
```

각 계층의 소유권 한 줄 요약 (`.md/architecture/WINTERS_CODEBASE_COMPASS.md` 계층 책임 절):

| 계층 | 소유하는 것 | 금지되는 것 |
|---|---|---|
| Shared/GameSim | gameplay truth 타입 + 결정론 시뮬 계약 | Engine/Renderer/UI/DX/ImGui include 금지 |
| Server | 위치·HP·쿨다운·피해·승패의 권위, 봇 명령 생산 | 봇 AI가 truth 컴포넌트 직접 수정 금지 (command만 생산) |
| Engine | 창/프레임/RHI/ECS/리소스 등 제품 중립 서비스 | 제품 코드 의존 금지 — LoL 명사를 모른다 |
| Client | 프레젠테이션: 보간, 애니/FX 재생, UI 상태 | authoritative truth를 새로 만들면 안 됨 |
| EngineSDK | Engine public 표면의 배포 사본 | 수작업 편집 금지 (UpdateLib.bat이 유일한 쓰기 주체) |

이 지도의 축은 **"gameplay truth vs presentation"** 이분법이다. 어떤 코드 조각이든 "이건 truth인가 연출인가"를 먼저 물으면 어느 계층에 놓을지가 결정된다.

그리고 이 경계는 문서가 아니라 **기계가 강제한다**: `Tools/Harness/Check-SharedBoundary.ps1`이 GameSim PreBuild에서 실행되어, Shared가 Engine/DX/ImGui를 직접 include하면 **빌드가 실패**한다 (Phase 7F 어댑터 `Shared/GameSim/Core/Ecs/*`만 화이트리스트). 서버 전용 수치는 Server 바이너리에만, 클라 비주얼 값은 Client 바이너리에만 컴파일해서 데이터 경계를 링크 경계로 만들었다 (`.md/architecture/WINTERS_DATA_ARCHITECTURE.md` §1).

---

## 4. 한 프레임의 여정 — 입력에서 픽셀까지

면접에서 화이트보드에 그릴 수 있어야 하는 그림. 실측 근거: `.md/architecture/WINTERS_DEPENDENCY_MAP.md` §4, `Server/Private/Game/GameRoomTick.cpp`, `Client/Private/Scene/Scene_InGame.cpp` OnUpdate.

```text
[클라: 우클릭/QWER]
  → CCommandSerializer: seq 부여, raw 클릭 의도 보존, 예측 기록
  → TCP (PacketEnvelope + FlatBuffers CommandBatch)
[서버: IOCP worker 스레드]
  → CFrameParser (길이 프리픽스, NeedMore/Complete/Invalid)
  → CPacketDispatcher (FlatBuffers verify 게이트)
  → CCommandIngress: seq 게이트 + Move 병합(연타=최신만) — ingress mutex까지만
[서버: 30Hz 틱 스레드 — truth의 유일한 소유자, m_stateMutex]
  → Phase_DrainCommands → Phase_ServerBotAI (봇도 command만 생산)
  → Phase_ExecuteCommands → Phase_SimulationSystems
     (스탯/버프/쿨다운/이동/전투 시스템 + 챔피언 15종 GameSim::Tick)
  → LagCompensation 이력 기록
  → Phase_BroadcastEvents → Phase_BroadcastSnapshot (+리플레이 기록)
[클라: 다음 프레임 OnUpdate]
  → 보간 시작점 캡처 → PumpNetwork (recv 스레드가 쌓은 것 드레인)
  → CSnapshotApplier / CEventApplier: verify → ensure → populate → reap
  → 스냅샷 tick 변경 시 보간 재시작, 로컬 yaw 예측 보호
  → ECS 스케줄러 실행 (클라 시뮬은 게이팅 — 프레젠테이션 시스템만)
  → FOW 텍스처 갱신 → 렌더 (DX11 legacy + RHI 스냅샷 병행 경로)
```

말로 풀 때의 포인트 세 가지:

1. **틱은 30Hz 고정, dt는 컴파일 타임 상수다.** `DeterministicTime::kFixedDt = 1/30` (`Shared/GameSim/Core/Determinism/DeterministicTime.h`). 가변 dt는 결정론과 리플레이를 깨기 때문에 시뮬레이션에서 배제했다.
2. **스레드 경계 = 오염 경계.** IOCP 워커는 ingress mutex까지만 들어오고, truth는 틱 스레드가 단독 소유한다. 네트워크 실패는 세션 단위로 격리되어 룸 상태를 오염시키지 않는다 (`.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md` P2).
3. **클라이언트 순서 의존성.** 보간 시작점을 스냅샷 적용 **전에** 캡처해야 튐 없는 lerp가 된다 — `Scene_InGame.cpp::OnUpdate`의 첫 줄들이 그 순서를 고정한다.

---

## 5. 규모와 개발 연대기

### 규모 (실측 수치만)

- 빌드 산출물: Engine DLL + Client/Server EXE + GameSim static lib + SimLab/AssetConverter/EldenClient 툴 트랙 + Go 매치메이킹 서비스 (`WINTERS_DEPENDENCY_MAP.md` §1)
- Engine 소스 약 626개 파일 (2026-07-09 의존성 전수 감사의 grep 모수)
- 플레이 가능 챔피언 15종 — 각각 Shared GameSim 훅 + 서버 판정 + 클라 FX 풀 사이클
- 에러 감사에서 적발된 대형 핵심 경로 파일: 클라 `SnapshotApplier.cpp` — 감사 시점(f9d4d5c) 기준 1,652줄 (이 수치는 뒤의 dead diagnostics 사고 이야기와 세트). 단일 파일 최대는 `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp` 3,025줄
- 데이터 주도 분리율: 18개 도메인 스코어카드 기준 28% — 과장하지 않고 이 숫자 그대로 말한다. 단 밸런싱 빈도가 가장 높은 스탯/스킬 수치는 이미 JSON→cook으로 분리 완료 (`WINTERS_DATA_ARCHITECTURE.md` §4)

### 연대기 — "계층 상승" 서사 (개발 세션 기록 기준)

각 단계가 왜 그 순서여야 했는지(의존성)로 설명하는 것이 포인트다.

| 시기 | 단계 | 왜 이 순서인가 |
|---|---|---|
| 2026-04 초중순 | 렌더 기반: 맵/오브젝트 로딩, 씬 시스템, ImGui | 보이는 것이 없으면 아무것도 검증 못 한다 |
| 2026-04 중순 | 입력/전투: 타겟팅, 스킬 디스패치, 쿨다운, FMOD | 렌더 위에서만 입력 검증 가능 |
| 2026-04 하순 | 인프라: 프로파일러, `.wmesh` 자체 포맷 + AssetConverter, JobSystem | 콘텐츠가 늘기 전에 계측·에셋 파이프라인 확보 (17.8ms→9ms 복구가 이 시기) |
| 2026-04 말 | 게임플레이 스케일: 미니언 전투 병렬화, 챔피언 FX 사이클, 150챔프 Registry 설계 | 단일 챔피언 완성 → 확장 구조로 승격 |
| 2026-04-30~05 | 네트워크: TCP MVP, 서버 권위 전환, yaw 동기화 대장정 | 로컬 시뮬이 완성된 뒤에야 무엇을 서버로 옮길지 보인다 |
| 2026-06~07 | 아키텍처 정제: 데이터 소유권 감사, 에러 경계 리팩터링, Phase 7F 의존성 절단, 경계 lint | 기능이 쌓인 뒤의 부채를 감사→슬라이스→기계 강제로 상환 |

한 줄 요약: **렌더 → 게임플레이 → 병렬화 → 네트워크 → 아키텍처 정제.** "만들고 → 측정하고 → 원칙으로 박제한다"의 반복이었다.

---

## 6. 핵심 설계 철학 5개

면접에서 "설계 원칙이 있나"에 바로 이 다섯을 꺼낸다. P1~P4의 기원은 NYPC 봇 대회 회고다 — 디버깅이 수월한 구조가 이기고, 예외 처리가 다른 구조를 건드리면 안 되고, 1등 봇이 길었던 이유는 모든 특수상황을 코드로 명시했기 때문이라는 실전 교훈을 엔진 전 계층 원칙으로 박제했다 (`.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md`).

### ① Truth와 Presentation의 분리가 모든 경계의 축
서버(+Shared)가 gameplay truth를 소유하고, 클라이언트는 연출만 한다. 봇 AI조차 truth를 직접 못 고치고 사람과 같은 GameCommand를 생산해 같은 검증 경로를 탄다. 이 이분법 하나로 "이 코드는 어디에 두나"라는 질문 대부분이 즉답된다.

### ② 실패는 발생 지점에서 즉시, 가시적으로 (P1)
`return nullptr;`만 있는 실패 경로 금지. 포맷만 하고 출력하지 않는 dead diagnostics는 로그가 없는 것보다 나쁘다 — 실제로 1,652줄짜리 SnapshotApplier에서 sprintf_s 8곳이 전부 미출력인 것을 감사로 적발하고 정책화했다. 폴백은 침묵하지 않는다: 서버 내비게이션은 authored grid→bake→structures-only→all-walkable 4단 사다리를 각 단계 `[ServerNav]` 로그와 함께 내려간다 (`Server/Private/Game/GameRoomNav.cpp`).

### ③ 결정론이 네트워크와 리플레이의 기반
고정 dt + 파생 시드 RNG + 정렬 순회. 예외(exception)도 결정론 틱을 위해 배제하고 반환값 모델로 통일했다 — 유일한 throw 경계는 cooked asset 파서 `CBinaryReader`뿐이고 로더가 흡수한다 (`.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md` §0). 리플레이는 "방송 바이트=기록 바이트" 재사용이라 별도 직렬화가 없다.

### ④ 아키텍처 규칙은 문서가 아니라 기계가 강제한다
Shared→Engine 위반 78개 파일을 빅뱅이 아니라 어댑터 간접층(Phase 7F)으로 먼저 절단하고, 재발은 PreBuild lint(`Check-SharedBoundary.ps1`)가 빌드 실패로 차단한다. 데이터 drift는 접속 시 빌드 해시 핸드셰이크로 로그에 드러난다. 레거시 테이블은 폴백 카운터가 0에 수렴해야만 삭제할 수 있다(zero-reader 규칙).

### ⑤ 디버깅이 수월한 구조를 먼저 만든다 (P4)
증상 튜닝 전에 관측 장치부터. 빈 경로 하나로 뭉개지던 4가지 실패 원인을 `ePathFindResult` enum으로 분해했고(`Engine/Public/Manager/Navigation/Pathfinder.h` — 과거 미니언 stuck 사고의 대책), 서버 봇 AI의 결정 트레이스를 스냅샷에 실어 클라 ImGui로 시각화한다. "패킷이 안 옴"과 "패킷이 거부됨"이 구분되어야 한다는 규칙도 여기서 나온다.

---

## 7. 포트폴리오 소개 스크립트

### 1분 버전 (엘리베이터)

> "LoL 스타일 멀티플레이어 MOBA를 상용 엔진 없이 밑바닥부터 만들었습니다. DX11 기반 자작 엔진 DLL 위에 클라이언트와 IOCP 게임 서버가 올라가고, 서버가 30Hz 고정 틱으로 모든 판정을 소유하는 완전한 서버 권위 구조입니다. 클라이언트는 스냅샷을 보간해서 연출만 하고, 판정 값은 클라 바이너리에 컴파일조차 되지 않습니다. 챔피언 15종이 스킬·데미지·봇 AI까지 풀 사이클로 돌아가고, ECS, work-stealing JobSystem, A* 내비게이션, 자체 에셋 포맷과 프로파일러까지 엔진 계층을 직접 구현했습니다. 가장 자랑하고 싶은 건 기능 개수가 아니라, 계층 경계를 빌드 단계 lint로 기계 강제하고 실패 경로를 전수 감사하는 방식으로 프로젝트를 운영했다는 점입니다."

### 3분 버전 (구조 중심)

1분 버전에 이어서:

> "구조는 다섯 조각입니다. 제품 중립 Engine DLL, 그 public 표면만 미러링한 EngineSDK, 결정론 게임플레이 코어인 Shared GameSim 정적 라이브러리, 그리고 이를 소비하는 Server와 Client입니다. 설계의 축은 'gameplay truth vs presentation' 이분법입니다 — Engine은 LoL이라는 제품 명사 자체를 모르고, Shared는 렌더러와 DX를 include할 수 없고, 클라이언트는 truth를 새로 만들 수 없습니다.
>
> 한 프레임의 여정으로 요약하면: 클라가 우클릭을 raw 의도 그대로 명령으로 직렬화해 보내면, 서버 IOCP 워커가 파싱·verify 후 ingress 큐에 넣고, 30Hz 틱 스레드가 명령 실행→시뮬레이션→스냅샷 방송을 돌립니다. 봇 AI도 사람과 똑같이 명령만 생산합니다. 클라는 스냅샷을 verify→적용→보간하고 로컬 예측과 충돌하면 보호 상태기계로 조정합니다.
>
> 이 구조가 문서로만 있으면 무너지기 때문에, Shared의 경계 위반은 PreBuild lint가 빌드를 실패시키고, 서버·클라 데이터 버전 불일치는 접속 시 빌드 해시 핸드셰이크로 즉시 드러나게 했습니다. 렌더는 현재 DX11이 기본 경로이고, IRHIDevice 추상화 위에서 DX12 경로를 병행 검증하며 점진 이관 중입니다. 같은 엔진 DLL 위에 Elden 스타일 액션 RPG 클라이언트를 두 번째 제품으로 올리는 작업도 진행 중인데, 렌더러를 복제하지 않고 RenderWorldSnapshot이라는 데이터 계약만 제품별로 다르게 채우는 방식입니다."

### 10분 버전 (스토리 + 깊이)

3분 버전 골격에 아래 네 블록을 얹는다. 각 블록은 독립적으로 빼고 넣을 수 있다.

**블록 A — 성장 서사 (2분).**
> "개발 순서 자체가 의존성 그래프였습니다. 먼저 렌더와 씬을 세워야 눈으로 검증할 수 있었고, 그 위에 타겟팅과 스킬을 올렸고, 콘텐츠가 늘기 전에 프로파일러와 자체 에셋 포맷, JobSystem 같은 인프라를 깔았습니다. 프레임이 17.8ms까지 밀렸을 때 추측 대신 프로파일러로 병목(스키닝 갱신)을 특정한 뒤, 같은 세션에서 정적 엔티티 bAnimated 스킵과 27개 모델 .wmesh 쿠킹 통합을 함께 적용해 9ms로 복구한 게 이 시기입니다. 로컬 시뮬이 완성된 뒤에야 서버 권위 전환을 시작했는데, 이때 '무엇이 truth이고 무엇이 연출인가'가 실제 버그(클라 내비가 서버 yaw를 덮어쓰는 사고)로 드러나면서 클라 시뮬 시스템을 전부 게이팅하고 순수 프레젠테이션으로 강등시켰습니다. 마지막 단계가 아키텍처 정제입니다 — 의존성 전수 감사, 에러 경계 리팩터링, 경계 lint 도입."

**블록 B — 설계 철학의 기원 (2분).**
> "설계 원칙 4개는 NYPC 봇 대회 회고에서 나왔습니다. 디버깅이 수월한 구조가 이기고, 실패 처리가 다른 구조를 오염시키면 안 되고, 모든 특수상황은 코드로 명시해야 한다는 교훈을 문서로 박제하고 코드 감사로 근거를 달았습니다. 예를 들어 '실패의 가시화' 원칙은 1,652줄짜리 스냅샷 적용기에 실패 로그가 0개였던 걸 적발하면서 정책이 됐고, '특수상황 명시' 원칙은 빈 경로 반환 하나로 뭉개지던 4가지 길찾기 실패 원인을 enum으로 분해하는 실코드로 내려왔습니다."

**블록 C — 가장 어려웠던 문제 하나 (2분, 챕터 심화로 연결).**
> "클라-서버 동기화에서 가장 오래 잡은 건 챔피언 몸통 yaw였습니다. 각도의 ±PI 경계 재교차, 챔피언마다 다른 메시 forward axis, 클라 예측과 서버 권위의 충돌, 클라 내비게이션의 덮어쓰기까지 네 종류의 사고가 겹쳐 있었습니다. 결론은 'yaw를 연속 상태로 볼 것인가 wire 값으로 볼 것인가'라는 소유권 관점 정리였고, Transform에 쓸 때는 항상 현재 값 근방으로 해석하고 wire/로그 비교에서만 정규화하는 규칙으로 수렴시켰습니다. 이 과정에서 배운 게 '증상 튜닝 전에 계측부터'라는 디버깅 규율입니다."

**블록 D — 정직한 현재 한계와 로드맵 (2분).**
> "미완성인 부분도 정확히 알고 있습니다. 전송은 TCP 단일이고 UDP 데이터면과 스냅샷 델타 압축은 다음 단계입니다. 랙 보상은 200ms 이력 링버퍼까지 구현했지만 실제 되감기 적용은 공정성 모델을 더 고민하려고 유보 중입니다. 데이터 주도 분리율은 스코어카드로 28%인데, 밸런싱 빈도가 높은 스탯·스킬 수치는 이미 JSON→cook으로 분리했고 나머지는 폴백 카운터가 0이 되는 도메인부터 슬라이스로 옮기고 있습니다. 스키닝 메시는 아직 RHI 스냅샷 경로에 못 올라갔습니다. 이렇게 '구현된 것'과 '설계 의도'를 구분해서 말하는 게 제 문서 운영 원칙이기도 합니다."

---

## 8. 첫 소개 직후 예상 꼬리질문 6개

**Q1. 왜 Unity/Unreal을 안 썼나요? 실무에선 상용 엔진을 쓸 텐데.**
- 답변 골격: 목적이 게임 출시가 아니라 엔진·네트워크 계층의 소유였다 → 상용 엔진에서도 결국 부딪히는 문제(프레임 버짓, 복제 경계, 에셋 파이프라인)를 블랙박스 없이 경험 → UE를 배척하지 않는다는 증거로, UE 툴 구조를 분석해 '검증된 런타임 계약' 규율만 가져오고 Blueprint 인터프리터는 결정론 해시 검증을 깨서 의도적으로 거부한 판단(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md`)을 든다.
- 꼬리 대비: "그래서 UE는 얼마나 아나" → 툴 이식 감사 문서 기반으로 디스크립터/에셋 쿠킹 대응 관계를 말한다.

**Q2. 혼자 만들었다는데 규모 관리가 됐다는 걸 어떻게 믿죠?**
- 답변 골격: 규칙을 문서가 아니라 기계에 넣었다 — PreBuild 경계 lint, 빌드 해시 핸드셰이크, 폴백 카운터 게이트, EngineSDK 자동 미러링(*_Manager.h purge로 게이트웨이 경계를 물리 강제, `UpdateLib.bat`) → 의존성 지도는 '의도'와 '실측'을 분리해 file:line 근거로 관리(`WINTERS_DEPENDENCY_MAP.md`).
- 꼬리 대비: "lint가 못 잡는 위반은?" → 남은 링크 의존(Shared의 Engine CWorld 백엔드)을 §3에 위반으로 기록해 두고 마지막 슬라이스로 남겨둔 상태임을 정직하게 말한다.

**Q3. 서버 권위인데 클라이언트 반응성은 어떻게 확보했나요?**
- 답변 골격: 이동은 raw 클릭 의도를 보내고 로컬 yaw 예측 + 보호 상태기계, 액터는 고정시간 보간 → ack만 믿지 않고 "서버가 실제로 예측을 따라잡을 때까지" 보호하는 게 핵심 → 스냅샷 적용과 보간 시작 캡처의 프레임 내 순서 의존성까지 설명하면 깊이가 산다.
- 꼬리 대비: "롤백 넣을 건가?" → 랙 보상 링버퍼는 있고 적용은 공정성 tradeoff로 유보 중.

**Q4. 결정론은 어떻게 보장하고, 어떻게 검증하나요?**
- 답변 골격: 고정 dt 상수 + 파생 시드 RNG + 정렬 순회의 3축, 컴포넌트 trivially_copyable을 static_assert로 강제 → 검증은 SimLab same-seed 해시 게이트 — 동작 무변경 슬라이스는 해시 동일이 통과 조건(`WINTERS_DATA_ARCHITECTURE.md` §6).
- 꼬리 대비: "부동소수점은?" → 서버 `/fp:precise` 적용을 말한다.

**Q5. 가장 후회하는 설계 결정은?**
- 답변 골격: HP의 이중 진실(HealthComponent와 레거시 hp 필드)이 낳은 미러링 코드 — 단일 진실 원칙을 초기에 어긴 대가 → 즉시 통합 대신 미러링으로 버틴 이유(호출부 광범위, 리스크)와 폴백 카운터 기반 상환 계획까지 말하면 '실수를 시스템으로 갚는' 태도가 전달된다.
- 꼬리 대비: "지금 다시 시작하면?" → 데이터 소유권 매트릭스를 먼저 세우고 시작한다.

**Q6. 이 프로젝트에서 회사에 가져올 수 있는 능력은 뭔가요?**
- 답변 골격: (1) 경계 설계 — truth/presentation 분리와 그것의 기계 강제 (2) 계측 우선 디버깅 — 추측 대신 프로파일러/트레이스/오버레이 (3) 부채의 정량 관리 — 28% 스코어카드처럼 완성도를 숫자로 정직하게 말하고 슬라이스로 상환하는 운영.
- 꼬리 대비: "팀 협업 경험은?" → 소유권 매트릭스/락 파일/워크 패킷으로 다중 장비를 팀처럼 운영한 사례, AI 협업에서 gotchas 로그로 반복 실수를 박제한 사례.

---

## 다른 챕터와의 연결

- 이 챕터에서 "판정은 서버가 소유한다"고 말한 것의 코드 수준 근거 → 서버 도메인/네트워크 챕터(틱 파이프라인, IOCP, 스냅샷 빌더)에서 심화
- 레이어 경계와 DLL 관련 문법 질문(dllexport, ODR, 템플릿 경계) → `.md/interview/cpp/02_compile_link_dll.md`
- ECS/JobSystem 구조 질문 → `.md/interview/cpp/11_architecture_ecs.md`, `.md/interview/cpp/09_concurrency.md`
- 예외 없는 에러 모델의 문법적 배경 → `.md/interview/cpp/10_error_handling.md`
- 네트워크 직렬화(FlatBuffers, 프레이밍) → `.md/interview/cpp/12_network_serialization.md`
- 설계 철학·에러 정책 원문: `.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md`, `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md`
- 계층 소유권과 실측 위반 목록: `.md/architecture/WINTERS_CODEBASE_COMPASS.md`, `.md/architecture/WINTERS_DEPENDENCY_MAP.md`
