# Winters 엔진 도메인 & 포트폴리오 스토리텔링 — 기술면접 대비

> 작성일: 2026-07-10. 근거: `.md/architecture/WINTERS_CODEBASE_COMPASS.md`, `WINTERS_DEPENDENCY_MAP.md`, `WINTERS_DESIGN_PHILOSOPHY.md`, `WINTERS_ERROR_HANDLING_POLICY.md`, `.claude/gotchas.md`, 프로젝트 메모리(2026-04 ~ 2026-07 세션 기록). 인용한 파일 경로는 전부 레포에서 실존 확인함.

## 이 카테고리의 출제 경향

자체 엔진 포트폴리오를 들고 가면 면접관의 질문은 거의 항상 세 갈래로 온다.

1. **검증 공격**: "정말 네가 만들었나? 얼마나 깊이 이해하나?" — 아키텍처 경계, 스레드 모델, 버그 사례를 파고든다. 특정 결정의 **이유**와 **트레이드오프**를 즉답할 수 있어야 한다.
2. **현실 감각 공격**: "왜 상용 엔진 안 썼나? 회사 오면 언리얼 쓸 텐데?" — 자체 엔진 자체가 목적이 아니라 **엔진/네트워크 내부를 체득하기 위한 수단**이었음을 증명해야 한다.
3. **성숙도 공격**: "혼자 만든 코드는 보통 지저분한데, 협업 가능한 코드인가?" — 의존성 강제 장치, 에러 처리 정책, 문서/컨벤션 체계, 스스로 아는 약점 목록으로 반격한다.

핵심 전략: **모든 답변을 "결정 → 이유 → 트레이드오프 → 사고(버그) → 재발 방지 장치" 서사로 끝낸다.** 기능 나열은 감점, 사고와 그로부터 만든 시스템이 득점 포인트다.

---

## 핵심 개념 정리

### 1. 전체 아키텍처: 5계층과 의존성 방향

**정의**: Winters는 "하나의 엔진 DLL 위에 여러 게임 클라이언트"를 목표로 하는 자체 C++ 엔진이다. `WintersEngine.dll` 위에 MOBA 클라이언트(`WintersGame.exe`, LoL 구조 복각)와 액션 RPG 클라이언트(`WintersElden.exe`)를 별도 제품으로 올린다 (`.md/architecture/WINTERS_CODEBASE_COMPASS.md` "제품 방향").

**계층과 소유권** (Compass "계층 책임" 기준):

| 계층 | 산출물 | 소유 |
|---|---|---|
| **Engine** | WintersEngine.dll | 창/프레임 루프, RHI, 렌더러, 리소스, ECS primitive, UI 렌더링, JobSystem — 제품 코드를 모른다 |
| **Shared/GameSim** | WintersGameSim.lib (static) | 서버 권위 gameplay truth의 데이터와 결정론 시뮬레이션 계약 — GameCommand, Snapshot/Event 스키마, 챔피언 GameSim. Engine/Renderer/UI/DX를 모른다 |
| **Server** | WintersServer.exe | 위치/HP/쿨타임/피해/투사체/승패의 **유일한 권위**. GameCommand를 받아 GameSim을 실행하고 Snapshot/Event를 송신 |
| **Client** | WintersGame.exe | Snapshot/Event를 visual state로 소비. 보간/약한 예측/애니/FX. authoritative truth를 새로 만들지 않는다 |
| **EngineSDK** | inc/lib/bin | Engine의 배포 허브. Engine PostBuild의 `UpdateLib.bat`이 동기화 — Client는 Engine 소스가 아니라 SDK의 lib에 링크한다 |

여기에 Tools(SimLab 결정성 하네스, AssetConverter), Services(Go, 계정/HTTP)가 붙는다 (`WINTERS_DEPENDENCY_MAP.md` §1 빌드 그래프).

**의존성 방향 (한 줄 암기)**: `Client → Engine(SDK) + Shared`, `Server → Engine + Shared`, `Shared → 아무것도(수학/타입 제외)`, `Engine → 아무 제품 코드도 모름`.

**왜 이 방향인가**: 실패 오염 격리가 목적이다. `WINTERS_DESIGN_PHILOSOPHY.md` P2 — "계층 간 오염 방지의 최전선은 의존성 방향이다. Engine은 제품 코드를 모르고, Shared/GameSim은 Engine/Renderer/UI/DX를 모르고, Server는 Client 비주얼을 모른다." Shared가 렌더러를 모르면 서버는 헤드리스로 돌고, 시뮬레이션은 SimLab에서 렌더 없이 결정론 검증이 된다.

**규칙이 아니라 장치로 강제한다** — 면접 최대 득점 포인트:
- 경계는 **vcxproj include-path 레벨**에서 본다. "코드에 include가 없다"가 아니라 "include가 컴파일될 수 없다"가 목표. gotcha(2026-07-09): "EngineSDK/inc가 비(非)Engine 프로젝트 include path에 있으면 계층 규칙은 컴파일로-강제-불가가 된다."
- `Tools/Harness/Check-SharedBoundary.ps1` — Shared 전체를 스캔해 `#include "ECS/…"`, `Engine_Defines.h`, `Client/`, `Server/`, `d3d11`, `imgui` 직접 include를 잡아 **GameSim PreBuild에서 빌드를 실패시키는 lint**. Phase 7F 어댑터(`Shared/GameSim/Core/Ecs/*`, `Core/World/World.h`)만 예외 화이트리스트.
- **Phase 7F 어댑터**: 원래 Shared 80개 파일이 Engine ECS 헤더를 직접 include하고 있었다(역사적 위반). 2026-07-09에 `Shared/GameSim/Core/Ecs/` 어댑터 9종(Entity/ISystem/SpatialIndex/TransformComponent/CoreComponents/NavAgentComponent 등)을 신설해 78+1개 파일을 재라우팅, 직접 include 잔존 0건. 단 마지막 잔존은 정직하게 말한다: `Shared/GameSim/Core/World/World.h`가 아직 `using World = ::CWorld;`로 Engine 타입을 가리키고, GameSim.vcxproj의 EngineSDK/inc 경로도 남아 있다. Shared 소유 결정론 ECS 백엔드를 만들어 어댑터를 repoint하는 것이 마지막 슬라이스다 (`WINTERS_DEPENDENCY_MAP.md` §3).
- 실측 상태는 `WINTERS_DEPENDENCY_MAP.md` §2에 표로 관리: Engine→제품 include 위반 0건(626파일 grep), Client/Public ID3D11 노출 0건(164파일), Server→Client 0건.

**면접 연결**: "의존성 규칙은 문서에 쓰는 것만으로는 지켜지지 않는다는 걸 위반 80파일을 직접 걷어내며 배웠고, 그래서 lint를 빌드에 물렸다"가 서사의 핵심. UE5.7 소스 비교 감사(2026-07-10)에서 스스로 내린 결론 — "Winters는 아키텍처 생각은 현업급인데 강제가 사람/문서 의존이었다. UE는 같은 규칙을 빌드시스템/매크로/CI가 기계 강제한다" — 를 약점 자인+로드맵으로 쓰면 성숙도 어필이 된다.

### 2. 서버 권위(Server-Authoritative) 구조

**정의**: 게임플레이 진실(위치, HP, 마나, 쿨타임, 피해, 투사체, 승패)은 서버만 만든다. 클라이언트는 입력을 **GameCommand**로 보내고, 서버가 실행한 결과를 **Snapshot/Event**로 받아 그린다.

**런타임 데이터 흐름** (`WINTERS_DEPENDENCY_MAP.md` §4, 실측):

```text
[Client 입력] → CCommandSerializer → TCP(PacketEnvelope + FlatBuffers CommandBatch)
  → Server IOCP worker → CFrameParser → CPacketDispatcher → CGameRoom::OnCommandBatch
  → CCommandIngress (seq 게이트, Move 병합, ingress mutex)      ← 네트워크 스레드는 여기까지
  → [30Hz tick 스레드, m_stateMutex] DrainCommands
  → ServerBotAI (GameCommand "생산"만) → ICommandExecutor::ExecuteCommand
  → Shared 시스템들 + 15개 챔피언 GameSim::Tick
  → 서버 전용 페이즈(미니언/터렛/투사체/사망·리스폰) → LagComp 기록
  → Snapshot/Event FlatBuffers → 전 세션 broadcast (+CReplayRecorder)
  → Client CSnapshotApplier/CEventApplier → ECS visual 적용 → 보간/예측/yaw 보호
```

관련 실물: `Server/Private/Game/CommandIngress.cpp`, `GameRoomTick.cpp`, `ServerAICommandProducer.cpp`, `SnapshotBuilder.cpp`, `ReplayRecorder.cpp`, `LobbyAuthority.cpp`; 클라 측 `Client/Private/Network/Client/SnapshotApplier.cpp`, `EventApplier.cpp`.

**GameCommand 프로듀서 패턴**: 봇 AI조차 truth 컴포넌트(Transform/HP/cooldown)를 직접 고치지 않고 GameCommand를 **생산**해서 사람 입력과 같은 파이프(ICommandExecutor)로 넣는다 (Compass "Server 금지" 항목). 효과 3가지: ① 검증/치팅 방어 로직이 한 초크포인트에 모인다 ② 봇과 사람이 같은 코드 경로를 타므로 봇이 곧 통합 테스트 트래픽이다 ③ 리플레이(`ReplayRecorder.cpp`)가 command 스트림 기록만으로 성립한다. 2026-07-10 UE 비교 감사에서 외부 검증 결과 "gameplay-net 약점 0건, 봇=GameCommand 생산자 실제 준수"로 확인된, 포트폴리오에서 가장 강한 부분.

**스레드 격리**: IOCP worker는 파싱과 ingress mutex push까지만. truth는 30Hz tick 스레드가 단독 소유. `WINTERS_DESIGN_PHILOSOPHY.md` P2 — "스레드 경계도 오염 경계다. 네트워크 실패는 세션 단위로 격리(단절)하고 룸 전체 상태를 건드리지 않는다." 보낼 수 없는 세션은 죽은 세션으로 간주하고 단절한다 (`Session::OnSendComplete`, WSA 코드 로그 후 단절 — `WINTERS_ERROR_HANDLING_POLICY.md` §2).

**Move 병합(coalescing)**: 우클릭 연타 시 pending 상태의 옛 Move를 같은 세션의 최신 Move로 교체한다 — 스테일 Move가 일일이 실행되며 보이는 조향 턴을 만들지 않도록 (gotcha 2026-05-20). 비-이동 커맨드는 권위 유지.

**클라 예측과 yaw 보호 스토리** (면접 "가장 어려웠던 버그" 1순위 소재, gotchas 2026-05-20 ~ 05-22에 7건으로 남아 있는 연대기):
1. **증상**: 우클릭 이동 시 챔피언이 순간적으로 반대를 봤다가 돌아오는 잔떨림.
2. **원인 1 — 에셋 이질성**: 자체 변환 `.wmesh`와 Riot 원본 FBX의 body forward 축이 달랐다(+Z vs -Z). 흩어진 per-call `+PI` 보정이 챔피언마다 다르게 적용됨 → `ResolveChampionVisualYawFromDirection` / `GetDefaultChampionVisualYawOffset`으로 챔피언별 오프셋을 한 곳에 수렴 (`Shared/GameSim/Definitions/ChampionRuntimeDefaults.h`).
3. **원인 2 — 정규화 위치**: 서버가 매 틱 yaw를 [-PI, PI]로 정규화하면 빠른 우클릭에서 ±PI 경계를 재교차하며 한 바퀴 도는 보간이 발생 → "Transform의 body yaw는 연속 상태, wire 값이 canonical이 아니다"라는 원칙 수립. 저장은 `ResolveChampionVisualYawNear`(현재 yaw에 가장 가까운 등가각), 정규화는 wire/로그 비교에서만.
4. **원인 3 — 예측 덮어쓰기**: `lastAckedCommandSeq`가 진행됐다는 이유만으로 서버 스냅샷이 로컬 클릭 예측 yaw를 덮어씀 → SnapshotApplier가 Hello의 로컬 netId를 기억하고, 서버 yaw가 실제로 따라잡거나 액션 락이 걸릴 때까지 예측 yaw를 보호.
5. **원인 4 — 클라 로컬 시스템 경합**: 클라이언트 `CNavigationSystem`이 SnapshotApply와 SyncFromECS 사이에서 복제된 챔피언 yaw를 덮어씀 → 서버 권위 플레이에서는 복제 챔피언에 대해 클라 NavAgent/Velocity 이동 시스템을 아예 돌리지 않는다. 이후 이동이 계단식이 되면 보간/예측을 고치지, 로컬 내비를 되살리지 않는다.

교훈으로 답하기: "서버 권위는 '서버 값으로 덮으면 끝'이 아니었다. **어느 값이 언제 canonical인지**(wire는 정규화, 로컬은 연속), **예측을 언제까지 보호하는지**(ack 진행 ≠ 서버가 따라잡음)를 계약으로 정의해야 했다."

**스냅샷 vs 이벤트**: 상태(위치/HP처럼 최신값만 의미)는 스냅샷으로 멱등하게 덮고, 발생 사실(시전, 피격, FX cue)은 이벤트로 보낸다. FlatBuffers 채택 — verify로 경계 검증이 가능하고 zero-copy 접근. **verify 실패는 절대 bare return으로 버리지 않는다** — "스키마 드리프트가 네트워크 정지와 똑같이 보이는(조용한 월드 프리즈)" 사고 방지. `SnapshotApplier OnHello/OnSnapshot`, `EventApplier OnEvent`에 bounded 로그 + drop (gotcha 2026-07-09, `WINTERS_ERROR_HANDLING_POLICY.md` §2).

**데이터 버전 핸드셰이크**: Hello 패킷에 `dataBuildHash`(하위호환 끝필드)를 실어 서버(`GameRoomLobby.cpp`)가 송신, 클라 SnapshotApplier가 비교해 불일치 시 bounded 로그. "클라와 서버가 다른 밸런스 데이터로 계산하는" 침묵 드리프트를 관측 가능하게 만든 것. (현재는 로그만 하고 진행 — 접속 거부 게이트는 로드맵, 이것도 정직하게 말할 것.)

### 3. ECS 설계와 시스템 Phase — "Phase는 데이터 흐름이다"

**정의**: Winters ECS는 컴포넌트 = POD 데이터, 시스템 = 로직, `CSystemScheduler`(`Engine/Public/ECS/SystemScheduler.h`)가 시스템을 **Phase 번호 순서**로 실행한다. Phase는 실행 우선순위가 아니라 **데이터 의존성 그래프의 위상 정렬**이다: 어떤 시스템이 쓴 컴포넌트를 다른 시스템이 같은 프레임에 소비하려면, 생산자 Phase < 소비자 Phase여야 한다.

**대표 사고 — NavSystem/AISystem Phase swap** (2026-04-28 세션 기록):
- 증상: 첫 미니언 라인 클래시에서 한쪽 팀 근접 미니언이 제자리에서 run/attack 애니만 재생 (비대칭 stuck).
- 원인 1: 순서가 Transform(0) → **Nav(1) → AI(2)** 였다. AI가 Phase 2에서 Chase 결정 + nav 타깃 갱신 → Nav는 이미 끝남 → 경로 재계산은 다음 프레임 → 첫 Chase 프레임은 LaneMove의 **stale velocity**로 엉뚱한 방향 이동. 수정: **AI(1) → Nav(2)로 swap** — "결정을 쓰는 시스템이 결정을 소비하는 시스템보다 먼저". 같은 프레임 안 set→consume 보장.
- 원인 2: `Find_Path`가 도달 불가 시 빈 경로를 **조용히** 반환 → NavSystem이 속도 0만 세팅 → Chase 상태 + 제자리 애니 (silent fail). 수정: Chase 한정 직선 fallback + Profiler counter(`Nav::PathEmpty`, `Nav::DirectFallback`) 노출.
- 이 사고에서 gotcha 2건("ECS Phase 순서 = data dependency 순서", "path silent fail 금지")과, 이후 Pathfinder에 `ePathFindResult` enum(NullGrid/StartBlocked/GoalBlocked/NoRoute/BrokenPath) out-param을 넣는 정책(`Engine/Public/Manager/Navigation/Pathfinder.h`, 2026-07-09 적용)이 나왔다.
- 디버깅 방법론 교훈: 추측 분석 3회가 전부 빗나갔고, **profiler counter 5분 작업이 추측 1시간을 이겼다** → "증상 튜닝 전에 관측 장치부터"(P4)가 코딩 규칙이 됨.

**병렬화와의 결합**: 같은 Phase 안의 시스템/엔티티는 데이터 경합이 없다는 계약이므로 JobSystem으로 fan-out 가능. 미니언 AI는 race 없이 병렬화하기 위해 **Decision pass(읽기 전용 판단) / Apply pass(쓰기)** 2-pass로 분리했다 (2026-04-28 worker-safety 세션 — Profiler thread_local race를 main=slot 0 / worker=idx+1 슬롯 분리로 해결).

**Shared용 ECS 어댑터**: 시뮬레이션 코드는 Engine ECS 헤더를 직접 보지 않고 `Shared/GameSim/Core/Ecs/*` 어댑터를 통해서만 접근 (§1 참조). 결정론이 필요한 순회는 `Shared/GameSim/Systems/DeterministicEntityIterator/`로 순서를 고정한다.

### 4. JobSystem / Fiber 병렬화

**구조** (`Engine/Public/Core/JobSystem.h`, `JobCounter.h`, `Engine/Private/Core/JobSystem.cpp`):
- worker별 **Chase-Lev work-stealing deque** (owner는 bottom에서 push/pop, thief는 top에서 steal — owner pop과 thief steal이 마지막 1개에서만 CAS 경합).
- `CJobCounter`: atomic 카운터, condition variable 없음. `WaitForCounter`는 잠들지 않고 **help-stealing** — 기다리는 동안 자기도 남의 잡을 훔쳐 실행 (idle 낭비 제거 + 데드락 회피).

**대표 사고 — Main-push race** (2026-04-23, Phase 5-A): 처음엔 Main 스레드가 worker의 deque에 직접 push했다. Chase-Lev는 **owner만 push**할 수 있는 자료구조다 — 규약 위반으로 memory ordering race가 생겨 worker pop이 빈 상태로 오인 → counter가 영원히 0이 안 됨 → `WaitForCounter` 무한 대기 → Main hang. 당일은 병렬 경로를 끄고 싱글스레드 fallback으로 기능 회귀 없이 유지, 이후 **`t_iWorkerIdx == -1`(main) 판별 → 전용 global queue로 우회**하는 정식 수정. 면접 포인트: "lock-free 자료구조는 알고리즘의 소유권 규약까지가 스펙이다. 코드가 컴파일되고 대충 돌아가는 것과 규약 준수는 다르다."

**Fiber 방향** (2026-05-11 박제, `.md/plan/engine/FIBER_MASTERY_MASTER.md`, `FIBER_SERVER_INTEGRATION.md`):
- 왜 fiber인가: OS 스레드 context switch ~1μs(커널 진입, 스케줄러) vs fiber switch ~수십 ns(유저 모드 레지스터 교환). "잡이 다른 잡을 기다리는" 그래프에서 스레드를 블록하지 않고 fiber를 suspend/resume — Naughty Dog GDC 모델.
- **Server IOCP × Fiber 통합의 핵심 결정**: IOCP worker는 fiber화하지 않는다. completion → mutex queue push까지만 하고, **tick fiber가 소비**한다. 이유: IOCP completion 콜백 문맥에서 fiber yield하면 IOCP 스레드 바인딩/락 소유가 꼬인다. 락 잡은 채 yield 금지 규약과 한 세트.
- 검증 기준을 먼저 정의: 병렬화 후에도 same input → **byte-exact snapshot** (결정성 검증이 최우선 게이트).

**결정론 검증 인프라**: `Tools/SimLab` — 렌더 없이 GameSim만 300틱 돌려 상태 해시를 뽑는다. 2026-07-09 대규모 리팩터(108파일) 후 해시가 리팩터 전과 동일(당시 해시 BB6A67502987351F)함으로 "동작 무변경"을 **증명**하고 머지했다. 부동소수점 결정론을 위해 Server에 `/fp:precise` 적용, `Shared/GameSim/Core/Determinism/DeterministicRng.h`/`DeterministicTime.h`로 난수/시간 소스 고정.

### 5. RHI 추상화

**정의**: 렌더링 백엔드(DX11 → DX12/Vulkan/콘솔)를 교체 가능하게 하는 얇은 인터페이스 계층. `IRHIDevice` + 핸들 타입(`IsValid()` 패턴, 생성 실패는 빈 핸들 `{}`) + backend-neutral renderer(`Engine/Public/Renderer/RHISceneRenderer.h`).

**격리 규칙** (Compass "RHI 방향" + 실측):
- DX11 concrete 타입(`ID3D11Device`, `ID3D11ShaderResourceView` 등)은 `Engine/Private/RHI/DX11/`(예: `CDX11Device.cpp`)에 가둔다. Engine/Public과 Client/Public에 ID3D11 노출 **실측 0건** (`WINTERS_DEPENDENCY_MAP.md` §2). 네이티브 핸들이 불가피하게 넘어갈 땐 void*로만.
- "빌드 통과를 위해 RHI 경로를 legacy DX11 concrete로 되돌리지 않는다" — 이관 중 가장 흔한 퇴행을 금지 규칙으로 박음.

**멀티 제품 전략**: LoL과 Elden이 renderer 클래스 계층을 복제하지 않는다. 같은 RHI renderer에 **서로 다른 RenderWorldSnapshot**(월드 → 렌더 제출용 스냅샷 데이터)을 공급하는 구조. "렌더러를 새로 만드는 게 아니라 스냅샷 작성이 제품별 일"이라는 한 줄이 설계 의도.

**실패 처리 연결**: RHI 리소스 생성 실패는 HRESULT + debugName 로그 후 빈 핸들 (`CDX11Device::CreateBuffer/CreateTexture`의 `[CDX11Device] FAIL:` 패턴), 텍스처 로드 실패는 스테이지+경로+HRESULT (`Engine/Private/RHI/RHITextureLoader.cpp`) — 에러 정책 §2 초크포인트 표의 기준 구현.

**알고 있는 약점(정직 카드)**: device-removed(TDR) 미처리, 릴리즈에서 통과되는 assert 전용 사전조건 — UE 감사에서 스스로 확정한 HIGH 항목이며 로드맵 Phase 0에 있음.

### 6. FX 시스템

**구조** (2026-04-25~26 구축, 이후 13챔피언 확장):
- **셰이더 2계열 분리**: `FxSprite.hlsl`(빌보드 — atlas 시퀀스, UV 스크롤, fade in/out, blend preset) / `FxMesh.hlsl`(3D 메쉬 — **erode**(noise 기반 소멸) + alpha clip + tint). 공용 `CBFxParams`(b2) cbuffer로 파라미터 전달.
- **컴포넌트**: `FxBillboardComponent`(vColor/fFadeIn·Out/iAtlasCols·Rows·FrameCount/fAtlasFps/fUVScrollU·V/blendMode — 기존 `eBlendPreset` 재사용, 신규 enum 만들지 않음), `FxMeshComponent`(3축 회전 + 재질 필드). POD로 유지해 DLL 경계 안전.
- **챔피언별 프리셋 함수 카탈로그**: `Client/Public/GameObject/Champion/<Name>/<Name>FxPresets.h` — Irelia/Yasuo/Kalista/Riven/Ezreal/Garen/Zed/Viego/Yone/Fiora/Ashe/Annie/Jax 13종 실존. 씬 코드에 인라인으로 흩던 스폰 코드를 이름 있는 함수(SpawnQTrail, SpawnEBeam, SpawnRBladeFan…)로 수렴 — gotcha 2026-05-26 "흩어진 FX 라우팅은 리뷰 불가, 이름 있는 카탈로그로 모아라"의 실행.
- **서버 주도 cue**: 서버가 FX cue 이벤트를 보내고 클라 `Client/Private/GameObject/FX/FxCuePlayer.cpp`가 cue 이름 → 프리셋으로 라우팅. **unknown cue/emitter는 침묵 no-op 금지, bounded 로그**(LogMissingCue) — 에러 정책 초크포인트.
- **튜닝 루프**: EffectTuner ImGui 패널 — 프리셋 드롭다운 + 슬라이더 + Spawn Test 버튼. "아티스트 없이 프로그래머가 수치를 코드-빌드 루프 없이 튜닝"하는 최소 도구.

**대표 사고 — 이렐리아 E sword 안 보임** (2026-04-26, 디버깅 방법론 이야기로 최적):
- 스폰/렌더 호출 전부 정상인데 화면 0픽셀. CPU 쪽 가설(transform, scale, 단위) 4개 전부 틀림.
- 정답은 셰이더+데이터: `clip(texColor.a - 0.05f)`에 **sprite 캡처용 PNG**(render/*.png — mesh diffuse가 아님)를 물렸고, 그 PNG의 알파 분포가 mesh UV 영역 밖 → 전 픽셀 clip. `.wmesh` UV bbox와 PNG 알파 bbox를 **직접 계측**해 30분에 확정, 본체 머티리얼 텍스처(`*_texture.png`)로 교체.
- 교훈: "CPU 디버거로 못 잡는 계열이 있다. 셰이더를 먼저 읽고, 데이터(UV/alpha bbox)를 계측하고, CPU/GPU 경계에서 분기하라" — 디버깅 파이프라인 스킬로 박제.
- 부수 함정 2개도 언급 가능: 런타임 컴파일 셰이더는 빌드 검증을 안 거친다(오타가 F5에서야 터짐) + PostBuild xcopy가 timestamp만 비교해 OutDir에 옛 .hlsl이 남는다.

**로드맵**: 노드 그래프 기반 이펙트 툴(Phase G 계획서, `.md/plan/EffectTool/`) — Niagara 등가물(WFX)로 확장. UE 감사 결론대로 에디터 코어(트랜잭션/원자적 저장/검증)를 먼저 세우고 툴을 올리는 순서.

### 7. 150챔피언 스케일 아키텍처

**문제 정의**: 챔피언 9체까지는 중앙 switch/테이블로 버텼지만, 50~150체 스케일에서는 "챔피언 1체 추가 = 중앙 파일 N곳 수정"이 병목이자 충돌 지점이 된다. 목표: **챔피언 1체 = 폴더 1개 + 파일 6개 + vcxproj 등록, 중앙 테이블 수정 0** (달성 시 15~20분/체).

**장치 3종** (B-11d v3.1 설계, 현재 Shared에 구현):
1. **Registry self-register**: `Client/Public/GamePlay/ChampionRegistry.h` + 챔피언별 `<Name>_Registration.cpp`(예: `Client/Private/GameObject/Champion/Yone/Yone_Registration.cpp`). ChampionRegistry.ForEach로 밴픽 UI가 자동 생성 — 챔피언 추가가 UI 코드를 건드리지 않는다. `eChampion`은 명시값 enum(추가는 빈 ID, 삭제/재사용 절대 금지 — 네트워크/DB 호환).
2. **GameplayHookRegistry — hookId 4분할** (`Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h`): `MakeGameplayHookId(champ, variant) = (champ << 16) | variant`. variant는 스킬 키(BA/Q/W/E/R) × 시점 4분할 — **KeySwap / OnCastAccepted / CastFrame / Recovery**. 시전 수락 시점 로직(스택 갱신)과 판정 프레임 로직(투사체 생성)이 한 hook에 섞이는 것을 타입 수준에서 방지. `Dispatch`는 bool 반환 — 미등록이면 legacy fallback 가드. 등록 테이블은 champ 256 × variant 256 함수 포인터 배열(정적, 캐시 친화).
3. **GenericPendingHit + 공통 데미지 경로** (`Shared/GameSim/Systems/PendingHit/PendingHitSystem.h`, `CommandExecutor/ICommandExecutor.h`): "N프레임 뒤 판정" 패턴(windup 후 hit)을 챔피언 공통 구조로 일반화하고, 피해는 `ApplyDamage(world, src, srcTeam, tgt, amount)` 공통 함수로 수렴 — Scene 의존 0, 챔피언 코드가 씬을 캐스팅하는 안티패턴 제거.

**현재 실측**: `Shared/GameSim/Champions/`에 15체(Annie, Ashe, Fiora, Irelia, Jax, Kalista, Kindred, LeeSin, MasterYi, Riven, Sylas, Viego, Yasuo, Yone, Zed)가 폴더 패턴으로 서버 GameSim에 존재. 클라 FX 프리셋도 같은 폴더 패턴 13종.

**면접 프레임**: "이건 Ezreal 1체 추가 작업이 아니라 **한계비용을 낮추는 아키텍처 사이클**이었다. 첫 시민에게 안티패턴을 박으면 150체가 전부 물려받으므로, 설계 리뷰(외부 AI 교차 리뷰 9건 보정)를 거쳐 hookId 분할과 공통 데미지 경로를 먼저 확정했다."

### 8. 에러 처리 정책 — silent fail 금지

**철학의 기원** (`WINTERS_DESIGN_PHILOSOPHY.md`): NYPC 봇 대회 회고 — "디버깅이 수월한 구조가 이긴다 / 예외 처리가 다른 구조를 건드리면 안 된다 / 1등 봇의 코드가 길었던 이유는 모든 특수상황을 코드로 명시했기 때문". 이를 P1~P4로 고정:
- **P1** 실패는 발생 지점에서 즉시, 가시적으로 (bare return 금지, dead diagnostics 금지, 성공 로그는 결과 확인 후에만)
- **P2** 한 구조의 실패가 다른 구조를 오염시키지 않는다 (폴백 사다리는 명시적+로그, 의존성 방향 = 오염 방어선)
- **P3** 모든 특수상황은 코드에 명시 (외부 입력=분기, 내부 불변식=assert; 실패 원인이 여럿이면 enum으로 구분)
- **P4** 디버깅이 수월한 구조를 먼저 (관측 장치 우선; "패킷이 안 옴"과 "패킷이 거부됨"이 구분돼야 한다)

**실행 규약** (`WINTERS_ERROR_HANDLING_POLICY.md`):
- **예외는 쓰지 않는다.** 반환값 기반(bool/HRESULT/nullptr/핸들 IsValid). 유일한 throw 경계는 cooked asset 파싱의 CBinaryReader(std::runtime_error)이고 로더가 catch해 false로 변환. 이유: DLL 경계+게임 루프에서 예외 전파 경로는 상태 오염 통로이고, 게임은 "그 프레임을 포기"가 아니라 "degraded로 계속"이 필요한 도메인.
- **bounded 로깅**: 함수-로컬 static 카운터로 상한(실패류 8 / 컨텐츠 miss류 64 / 계측류 512). 성공과 실패가 카운터를 공유하면 성공이 예산을 소진해 실패가 안 보인다(실제 사고 사례 있음).
- **dead diagnostics 금지**: sprintf_s로 포맷만 하고 출력하지 않는 코드는 "로그가 없는 것보다 나쁘다 — 디버깅하는 사람이 트레이스가 있다고 믿게 만든다". 2026-07-09 감사에서 SnapshotApplier 1,652줄에 OutputDebugString 호출 0개, sprintf_s 8곳 전부 미출력을 발견하고 일괄 정리.
- **루틴 트레이스 vs 실패 진단 구분**: 틱/스냅샷/yaw 정상 흐름 로그는 리팩터 중 추가 금지·사용 후 제거, **실패 진단은 항상 살아 있어야 하며 제거 대상이 아니다**. 이 구분이 "로그 없애기"와 "silent fail 금지"가 충돌하지 않게 하는 운영 규칙.
- **경계별 초크포인트 표** 운영: RHI 생성/텍스처 로드/씬 전환/네트워크 verify/send 실패/스폰 실패/FX cue/이동 stuck — 각 경계의 규약과 기준 구현 파일을 표로 고정 (§2).
- **미해소 침묵 지점도 문서화** (§4): "고쳐야 할 것"과 "따라 해도 되는 패턴"을 구분해 남긴다. 예: CommandExecutor의 unknown command 침묵 drop, suspicion 카운터 기록만 되고 enforcement 없음.

**대표 적용 사례 요약**: FlatBuffers verify 실패 bounded trace+drop / Pathfinder `ePathFindResult` 원인 enum / `std::async` future 폐기가 소멸자 대기로 **동기 블로킹**이 되는 함정(CHttpClient를 future 소유 + 요청 스냅샷 복사 + 소멸자 드레인으로 재설계, `Client/Private/Network/Backend/CHttpClient.cpp`) / 서버 내비 폴백 사다리(authored grid → bake → structures-only → all-walkable, 각 단계 `[ServerNav]` 로그, `Server/Private/Game/GameRoomNav.cpp`).

### 9. (보조) 데이터 아키텍처와 개발사(史) 타임라인

**데이터 경계** (Compass 최상단, 2026-06-22 결정): JSON은 authoring/cook 입력이고 런타임 프레임 코드는 validated immutable pack만 읽는다. `DefinitionKey`= 네트워크/세이브용 안정 식별자, `ChampionDefId` 등 dense ID는 pack-local, `EntityHandle`은 프로세스-로컬이며 어떤 직렬화 경계도 넘지 않는다. 폴백 데이터 진입은 bounded 로그로 가시화(16사이트) — "폴백이 제2의 truth가 되면 안 된다". 분리 진행률을 스코어카드(18개 도메인, 종합 28%)로 **정량 관리**하는 것 자체가 어필 포인트.

**타임라인 한 장** (스토리텔링 뼈대):

| 시기 | 마일스톤 |
|---|---|
| 2026-04 초중순 | DX11 렌더러/맵 로딩(B-4) → ImGui+씬 시스템(B-5) → 타겟팅/전투 입력(B-6) → 맵 에디터+바이너리 스테이지 포맷 |
| 2026-04 하순 | JobSystem/Chase-Lev(5-A, main-push race 사고) → 프로파일러로 17.8ms→9ms + `.wmesh` 자체 포맷 전환 → 이렐리아 FX 파이프라인 → 미니언 전투 2-pass 병렬 + Phase swap 사고 |
| 2026-04 말~05 | 150챔피언 스케일 설계(B-11d) → 네트워크 계획(TCP MVP + UDP 마스터) → Fiber mastery + IOCP×Fiber 통합 설계 |
| 2026-05 중하순 | 서버 권위 실전: yaw 연대기 7 gotchas, Move coalescing, 예측 보호 |
| 2026-06 | Codebase Compass 제정, DataDriven 경계 결정 |
| 2026-07 | 설계철학 P1~P4 박제 → 전수 감사(65건/high 18) → 에러 경계 리팩터(108파일, SimLab 해시 동일 증명) → Phase 7F 어댑터+경계 lint → UE5.7 소스 비교 감사(7차원) |

서사 요지: **"그리는 것 → 병렬로 돌리는 것 → 스케일하는 것 → 온라인으로 만드는 것 → 무너지지 않게 만드는 것(정책/강제 장치)"** 순서로 관심사가 진화했고, 마지막 단계가 곧 실무 협업 역량이다.

---

## 발표 시나리오

### 3분 요약 버전

> "Winters는 제가 만든 자체 DX11 C++ 엔진이고, 그 위에 LoL 구조를 복각한 **서버 권위 온라인 MOBA**가 실제로 돌아갑니다. 구조는 5계층 — Engine DLL, 결정론 시뮬레이션을 담는 Shared/GameSim static lib, 그걸 실행하는 IOCP 서버, 스냅샷을 소비하는 클라이언트, 그리고 배포 허브 EngineSDK입니다. 핵심 규칙은 세 줄입니다. **Shared는 렌더러를 모른다, 서버만 진실을 만든다, 클라는 그린다.**
>
> 세 가지만 꼽겠습니다. 첫째, **서버 권위**: 봇 AI조차 상태를 직접 못 고치고 사람과 같은 GameCommand 파이프로 명령을 '생산'만 합니다. 그래서 검증이 한 초크포인트에 모이고, 커맨드 스트림 기록만으로 리플레이가 됩니다. 둘째, **결정론 검증**: 렌더 없는 SimLab 하네스로 300틱 상태 해시를 뽑아서, 108개 파일 리팩터 후 해시가 바이트 단위로 동일함을 증명하고 머지한 적이 있습니다. 셋째, **경계의 기계 강제**: Shared가 Engine 헤더를 직접 include하면 PreBuild lint가 빌드를 실패시킵니다. 규칙을 문서로 두면 안 지켜진다는 걸 위반 80파일을 직접 걷어내며 배웠기 때문입니다.
>
> 이 프로젝트로 증명하고 싶은 건 엔진을 만들 수 있다가 아니라, **엔진/네트워크/게임플레이가 만나는 지점의 사고를 원인 enum과 카운터, 정책 문서, 빌드 게이트로 바꿔온 과정**입니다. 상용 엔진 밑에서 무슨 일이 일어나는지 알고 쓰는 프로그래머가 되기 위한 수단이었습니다."

(타임박스 팁: 30초 개요 → 각 40초 × 3포인트 → 30초 클로징. 데모 영상이 있다면 서버 권위 파트에서 우클릭 이동+스킬+스냅샷 오버레이 10초.)

### 10분 상세 버전 (섹션당 예상 시간)

1. **(1분) 제품과 계층**: WintersEngine.dll 위 두 클라이언트 그림 → 5계층 표 → 의존성 한 줄 암기 버전.
2. **(2분) 서버 권위 파이프라인**: 위 데이터 흐름 다이어그램을 그대로 화이트보드에. 강조 3점 — IOCP 스레드는 ingress mutex까지(진실은 30Hz tick 스레드 단독 소유), 봇=GameCommand 생산자, verify 실패는 bounded 로그+drop("스키마 드리프트가 네트워크 정지처럼 보이는 사고" 방지).
3. **(2분) 가장 어려웠던 버그 — yaw 연대기**: 증상 → 4개의 겹친 원인(에셋 forward 축 / ±PI 정규화 경계 / 예측 덮어쓰기 / 클라 로컬 시스템 경합) → "wire 값과 연속 상태의 구분"이라는 계약으로 종결. gotcha 7건으로 남긴 것까지.
4. **(1.5분) ECS Phase와 병렬화**: Phase=데이터 흐름 → NavSystem/AISystem swap 사고 → Chase-Lev JobSystem과 main-push race → Decision/Apply 2-pass. "profiler counter 5분이 추측 1시간을 이겼다."
5. **(1.5분) 스케일 아키텍처**: 150챔피언 목표 → Registry self-register / hookId 4분할 / GenericPendingHit+ApplyDamage → 현재 서버 GameSim 15체 실증.
6. **(1분) 에러 처리 정책**: P1~P4 → dead diagnostics 금지 → bounded 카운터 → 초크포인트 표 운영. "로그를 지우는 규칙과 silent fail 금지가 충돌하지 않는 이유(루틴 vs 실패 구분)"까지 말하면 깊이가 전달됨.
7. **(1분) 스스로 아는 약점과 로드맵**: UE5.7 소스 비교 감사 — 강점(네트워크 권위 약점 0건)과 HIGH 약점(minidump/CI/버전 게이트/include 화이트리스트)을 그대로 공개 → Phase 0~2 로드맵. "약점 목록을 스스로 만들 수 있다"가 클로징 메시지.

---

## 예상 질문 & 모범답변

### Q1. Winters가 뭔지 한 문장으로 설명해 보세요.
**답**: "자체 DX11 C++ 엔진과 그 위에서 실제로 돌아가는 서버 권위 온라인 MOBA입니다 — 엔진(DLL), 결정론 시뮬레이션 라이브러리, IOCP 서버, 클라이언트가 계층 분리돼 있고, 챔피언 15체가 서버 GameSim에서 시뮬레이션됩니다."
**꼬리 대비**: "실제로 돌아간다"의 근거를 바로 댈 것 — 서버+클라 왕복 스모크, SimLab 300틱 결정성 하네스, 리플레이 레코더.

### Q2. 왜 상용 엔진을 안 쓰고 직접 만들었나요?
**답**: "목적이 '게임 출시'가 아니라 '엔진과 온라인 게임의 내부를 체득'하는 것이었기 때문입니다. 언리얼을 쓰면 리플리케이션, 렌더 파이프라인, 잡 스케줄링이 이미 풀려 있어서 그 결정의 **이유와 비용**을 배울 수 없습니다. 저는 Chase-Lev deque의 소유권 규약을 어겨 메인 스레드가 행에 걸리는 사고, 스냅샷이 클라 예측을 덮어쓰는 사고를 직접 겪고 고치면서, 상용 엔진이 왜 그렇게 설계됐는지를 역방향으로 이해하게 됐습니다. 실제로 마지막엔 UE5.7 소스를 받아 제 엔진과 7개 차원으로 전수 비교 감사를 했고, '내 엔진은 규칙은 현업급인데 기계 강제가 약하다'는 결론과 보완 로드맵까지 뽑았습니다. 상용 엔진을 부정한 게 아니라, 상용 엔진을 **더 잘 쓰기 위한** 훈련이었습니다."
**함정**: "상용 엔진은 배울 게 없다"는 뉘앙스 금지. UE 감사 이야기가 최고의 방어.

### Q3. 입사하면 유니티/언리얼을 쓰라고 할 텐데 괜찮나요?
**답**: "네, 오히려 유리하다고 생각합니다. 제가 만든 것들은 전부 상용 엔진의 등가물이 있습니다 — 제 GameplayHookRegistry는 GAS의 어빌리티 훅, RenderWorldSnapshot은 렌더 프록시, WFX 방향은 Niagara, 정의 팩은 DataTable/DataAsset에 대응합니다. 실제로 UE 툴↔Winters 등가물 매핑 문서(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md`)를 만들어 이식 설계까지 해봤습니다. 밑단을 만들어 본 사람은 엔진이 숨겨놓은 비용 — 예를 들어 Tick 순서 의존성이나 리플리케이션 대역폭 — 을 예측하며 씁니다. 새 엔진 온보딩은 API 학습이지 개념 학습이 아니게 됩니다."

### Q4. 5계층 구조와 의존성 방향을 설명해 보세요.
**답**: 핵심 개념 §1의 표+한 줄 암기. 반드시 덧붙일 것: "중요한 건 방향 자체보다 **왜** — 실패 오염 격리입니다. Shared가 렌더러를 모르기 때문에 서버가 헤드리스로 돌고 SimLab이 렌더 없이 결정성을 검증합니다. 그리고 이 규칙은 문서가 아니라 PreBuild lint(`Tools/Harness/Check-SharedBoundary.ps1`)가 빌드 실패로 강제합니다."
**꼬리 대비**: "EngineSDK는 왜 있나?" → Client가 Engine 소스가 아닌 SDK lib에 링크하는 배포 허브. stale lib ABI 크래시 위험을 스스로 알고 있고(감사 HIGH 항목) sln 경유 빌드 규칙+로드맵으로 관리 중이라고 답한다.

### Q5. 의존성 규칙을 어떻게 '강제'하나요? 문서만으론 안 지켜질 텐데요.
**답**: "정확히 그 문제를 겪었습니다. Shared 80개 파일이 Engine ECS 헤더를 직접 include하는 상태가 역사적으로 누적돼 있었습니다. 해소는 3단계였습니다 — ① 오염 절단: Engine_Defines.h 체인이 dinput.h와 using namespace를 시뮬레이션 코드에 주입하던 것을 컴포넌트 헤더에서 끊음 ② 어댑터: `Shared/GameSim/Core/Ecs/` 9종을 만들어 79개 파일을 재라우팅 ③ 강제: 직접 include를 발견하면 GameSim 빌드를 실패시키는 lint를 PreBuild에 연결. 그리고 리팩터 전후 SimLab 해시가 동일함으로 동작 무변경을 증명했습니다. 남은 마지막 단계도 압니다 — World가 아직 `using World = ::CWorld`로 Engine 타입이라, Shared 소유 ECS 백엔드로 repoint해야 EngineSDK include 경로까지 제거됩니다."
**포인트**: 완결이 아니라 "잔존을 정확히 알고 있음"이 신뢰를 만든다.

### Q6. 서버 권위가 뭐고, 왜 그렇게 만들었나요?
**답**: "게임플레이 진실을 서버만 만들 수 있게 하는 구조입니다. 클라이언트는 입력을 GameCommand로 보내고 결과 Snapshot/Event를 그리기만 합니다. 이유는 치팅 방어가 표면적 이유고, 더 근본적으로는 **진실의 소유자를 하나로** 만들어 상태 분기 버그를 없애기 위해서입니다. 클라가 로컬에서 상태를 만들면 '서버와 클라가 다른 세계를 믿는' 계열의 버그가 생기는데, 저는 이걸 yaw 예측 사고로 직접 겪었습니다."
**꼬리 대비**: "격투 게임도 서버 권위인가?" → 롤백 넷코드(P2P 결정론 lockstep)와의 비교 — 인원수/입력 지연 허용치/치팅 모델이 다르면 답이 다르다. MOBA는 10인+관전+리플레이라 서버 권위가 표준.

### Q7. 봇 AI가 왜 상태를 직접 못 고치게 했나요? 성능 낭비 아닌가요?
**답**: "봇도 사람과 같은 GameCommand를 '생산'만 하고 같은 ICommandExecutor 파이프를 탑니다. 세 가지를 얻습니다 — ① 검증/사거리/쿨타임 체크가 한 초크포인트에 모여 봇이라고 규칙을 우회하는 버그가 원천 차단됩니다 ② 봇이 곧 통합 테스트 트래픽이 됩니다. 서버 스모크는 봇 로스터로 풀 파이프라인을 돌립니다 ③ 커맨드 스트림 녹화만으로 리플레이가 성립합니다(`Server/Private/Game/ReplayRecorder.cpp`). 성능 비용은 커맨드 구조체 생성 정도로 미미하고, 미니언/터렛처럼 수가 많고 규칙이 단순한 유닛은 의도적으로 서버 권위 코드가 직접 mutate하는 **명시된 예외**로 뒀습니다 — 예외는 두되 문서화한다는 원칙입니다."

### Q8. 스냅샷과 이벤트를 왜 나눴나요?
**답**: "의미가 다릅니다. 위치/HP처럼 **최신값만 의미 있는 상태**는 스냅샷으로 멱등하게 덮습니다 — 하나 유실돼도 다음 것이 복구합니다. 시전/피격/FX cue처럼 **발생 사실 자체가 의미인 것**은 이벤트로 보냅니다 — 유실되면 안 되고 두 번 적용돼도 안 됩니다. 이 구분이 없으면 'HP를 이벤트 델타로 보내다 하나 유실되어 영구 어긋남' 같은 사고가 납니다. 클라에서도 소비자가 분리돼 있습니다(SnapshotApplier / EventApplier)."
**꼬리 대비**: 대역폭 최적화 질문 → 델타 스냅샷/관심 영역(AOI)은 UDP 마스터플랜(M1-M6) 로드맵에 있고 현재 TCP MVP 단계임을 정직하게.

### Q9. 클라이언트 예측은 어디까지 하고, 서버와 충돌하면 어떻게 하나요?
**답**: "약한 예측만 합니다 — 우클릭 즉시 이동 시작과 몸 방향(yaw) 회전. 충돌 해소가 어려웠던 게 yaw인데, 처음엔 'ack가 진행되면 서버 값으로 덮는다'로 구현했다가 잔떨림이 났습니다. 원인은 **ack 진행 ≠ 서버가 내 예측을 따라잡음**이라는 것. 지금은 SnapshotApplier가 Hello의 로컬 netId를 기억하고, 서버 yaw가 실제로 예측치에 수렴하거나 액션 락(스킬 시전)이 걸릴 때까지 예측 yaw를 보호합니다. 위치는 서버 값 보간이 기본이고, 로컬 내비게이션 시스템은 복제 챔피언에 대해 아예 돌리지 않습니다 — 스냅샷 적용과 로컬 시스템이 같은 컴포넌트를 두고 경합하던 사고를 겪은 뒤의 결정입니다."

### Q10. 가장 어려웠던 버그를 하나 얘기해 주세요.
**답(주력 — yaw 연대기)**: 핵심 개념 §2의 5단계 스토리를 2분 압축. 구조: 증상(우클릭 시 순간 반대 보기) → 원인이 4겹(에셋 forward 축 이질성 / ±PI 정규화 재교차 / 예측 덮어쓰기 / 클라 로컬 시스템 경합) → 각각의 수정 → 최종 계약("Transform yaw는 연속 상태, 정규화는 wire/로그에서만") → gotcha 7건으로 박제. 마무리: "버그 하나가 에셋 파이프라인, 수학, 네트워크 예측, ECS 실행 순서 네 도메인에 걸쳐 있었고, 고치는 것보다 **다시 안 나게 만드는 것**(헬퍼 함수로 쓰기 지점 수렴 + 규칙 문서화)이 진짜 작업이었습니다."
**대안 카드**: ① 미니언 stuck(Phase swap + silent fail — ECS/디버깅 방법론 질문일 때) ② 이렐리아 E sword 0픽셀(GPU 디버깅 방법론 질문일 때) ③ Chase-Lev main-push race(동시성 질문일 때). 면접관의 직군에 맞춰 선택.

### Q11. ECS를 왜 썼나요? OOP가 더 단순하지 않나요?
**답**: "세 가지 실익 때문입니다. ① 서버/클라 이중 실행: 컴포넌트가 POD라 같은 게임플레이 컴포넌트를 서버 truth와 클라 visual 양쪽에서 쓰고, 스냅샷 직렬화가 자연스럽습니다 ② 병렬화 단위: 시스템×Phase가 데이터 의존성을 명시하므로 JobSystem fan-out 지점이 명확합니다 ③ 캐시 지역성: 미니언 수십 기 AI/이동/전투를 컴포넌트 배열 순회로 돌립니다. 다만 ECS가 만능이라고 생각하지 않습니다 — 씬/UI 같은 저빈도 로직은 여전히 클래스 기반이고, ECS는 '수가 많고 매 틱 도는 것'에만 씁니다."
**꼬리 대비**: "Phase가 뭔가?" → Q12로 연결.

### Q12. 시스템 실행 순서(Phase)는 어떻게 관리하나요?
**답**: "Phase 번호가 곧 데이터 의존성 위상 정렬입니다. 이걸 말로만 알다가 사고로 체득했습니다 — 미니언 AI가 Phase 2에서 Chase를 결정하는데 Nav가 Phase 1이라 이미 끝난 상태여서, 첫 Chase 프레임이 이전 상태의 stale velocity로 움직였습니다. 비대칭 stuck이라 재현도 까다로웠죠. 수정은 AI(1)→Nav(2)로 swap해서 '결정 생산자가 소비자보다 먼저'를 보장한 것이고, 이때 'Phase 순서 = data dependency 순서'를 gotcha로 박제했습니다. 언리얼의 TickGroup, 유니티 DOTS의 SystemGroup ordering이 정확히 같은 문제의 해법입니다."

### Q13. silent fail 금지 정책을 구체적으로 설명해 주세요.
**답**: "실패 경로에 관측 수단(로그/카운터/상태 문자열) 없이 bare return만 있는 코드를 금지합니다. 계기는 두 사고입니다 — Pathfinder가 도달 불가 시 빈 경로를 조용히 반환해 미니언이 제자리 애니로 굳은 사고, 그리고 FlatBuffers verify 실패를 bare return으로 버리면 스키마 드리프트가 네트워크 정지와 똑같이 보인다는 발견. 정책화한 게 세 가지입니다: ① 실패 원인이 여럿이면 enum으로 구분(`ePathFindResult` — NullGrid/StartBlocked/GoalBlocked/NoRoute/BrokenPath) ② 로그는 bounded(static 카운터 상한 8/64/512 — 로그 폭주로 프레임을 죽이지 않기) ③ 포맷만 하고 출력 안 하는 dead diagnostics는 리뷰 반려 — 있다고 믿게 만드는 가짜 트레이스가 no log보다 나쁩니다. 실제로 1,652줄짜리 SnapshotApplier에서 sprintf_s 8곳 전부 미출력인 걸 감사로 잡아냈습니다."

### Q14. 예외(exception)를 안 쓴다고요? 왜죠?
**답**: "게임 루프+DLL 경계 조합에서는 반환값 기반이 낫다고 판단했습니다. 이유: ① DLL 경계를 넘는 예외는 ABI/런타임 조합에 따라 미정의 동작 위험 ② 게임은 '이 프레임을 포기하고 unwinding'이 아니라 '진단 남기고 degraded로 계속'이 필요한 도메인 ③ 실패 경로를 명시적 분기로 쓰게 되어 P3(특수상황 명시)와 정합. 단 절대주의는 아닙니다 — cooked asset 파서(CBinaryReader)는 깊은 중첩 파싱이라 std::runtime_error를 쓰되, **로더가 catch해서 bool로 변환하는 좁은 경계**로 한정했고, 새 throw 경계 추가는 아키텍처 결정 사항으로 못박았습니다."
**꼬리 대비**: "그럼 에러 코드 지옥 아닌가?" → 계층별 컨벤션 통일(Engine 팩토리=nullptr, RHI=IsValid 핸들, Server=bool+cerr)로 호출부 패턴을 한 가지로 유지.

### Q15. JobSystem 구조를 설명해 보세요.
**답**: "worker별 Chase-Lev work-stealing deque입니다. owner는 자기 deque bottom에서 push/pop, 다른 worker는 top에서 steal — 마지막 1개를 두고만 CAS 경합이 나는 구조라 락 없이 확장됩니다. 대기는 `WaitForCounter`가 잠들지 않고 help-stealing — 기다리는 동안 자기도 남의 잡을 훔쳐 실행해서 idle 낭비와 데드락을 같이 없앱니다. 그리고 이 구조에서 제일 값진 사고를 겪었습니다 — Q16."

### Q16. 그 JobSystem 사고(race)를 자세히 말해 보세요.
**답**: "Main 스레드가 worker의 deque에 직접 push하게 만들었습니다. Chase-Lev는 **owner만 push할 수 있다**는 소유권 규약이 알고리즘의 일부인데 그걸 어긴 거죠. 결과는 memory ordering race — worker의 pop이 빈 deque로 오인 → 잡이 실행 안 됨 → counter가 영원히 0이 안 됨 → WaitForCounter 무한 대기 → 메인 스레드 행. 엔티티 36개 이상에서만 병렬 경로에 진입해서 발현 조건도 까다로웠습니다. 당일은 병렬 경로만 꺼서 싱글스레드 fallback으로 기능 회귀 없이 유지했고 — 이게 P2 격리의 실천이기도 합니다 — 정식 수정은 `t_iWorkerIdx == -1`로 main을 판별해 전용 global queue로 우회시키는 것이었습니다. 교훈: lock-free는 '코드가 돈다'와 '규약을 지킨다'가 다르며, 자료구조의 논문상 전제조건까지가 스펙입니다."

### Q17. Fiber를 왜 도입하려 했고, 스레드와 뭐가 다른가요?
**답**: "스레드 context switch는 커널 진입 포함 ~1μs, fiber는 유저 모드 레지스터 교환이라 수십 ns 수준입니다. 잡이 잡을 기다리는 그래프(A가 B의 완료를 기다림)에서 스레드를 블록하면 코어가 놀지만, fiber면 그 자리에서 suspend하고 다른 잡을 실행합니다 — Naughty Dog GDC 모델입니다. 제 설계에서 중요했던 결정은 오히려 '어디에 fiber를 **안 쓰나**'입니다: IOCP worker는 fiber화하지 않고 completion을 큐에 push까지만, tick fiber가 소비합니다. IOCP 콜백 문맥에서 yield하면 스레드 바인딩과 락 소유가 꼬이기 때문입니다. 그리고 병렬화의 합격 기준을 성능이 아니라 **결정성**(same input → byte-exact snapshot)으로 먼저 정의했습니다."

### Q18. IOCP 서버의 스레드 모델을 설명해 보세요.
**답**: "IOCP worker 스레드들이 recv completion을 받아 파싱(CFrameParser→CPacketDispatcher)하고, 커맨드를 ingress mutex 아래 큐에 넣는 데까지만 합니다. 게임 상태는 30Hz tick 스레드가 단독 소유하고, 틱 시작에 DrainCommands로 가져갑니다. 네트워크 스레드가 게임 락을 잡지 않는 게 원칙입니다 — 스레드 경계를 실패 오염 경계와 일치시킨 겁니다. 세션 실패는 세션 단위 격리: send 하드 실패면 WSA 코드를 로그하고 그 세션만 단절하지, 룸 truth는 건드리지 않습니다. 알고 있는 트레이드오프도 있습니다 — OnSessionJoin/Leave가 state mutex를 잡아서 느린 틱이 accept를 지연시킬 수 있다는 것까지 의존성 지도에 적어뒀습니다."

### Q19. 결정론(determinism)은 왜 필요하고 어떻게 보장하나요?
**답**: "세 소비자가 있습니다 — ① 리팩터 검증: 렌더 없는 SimLab이 300틱 상태 해시를 뽑는데, 108파일 리팩터 후 해시가 이전과 동일함으로 '동작 무변경'을 증명하고 머지했습니다. 테스트 커버리지가 부족한 개인 프로젝트에서 가장 강력한 회귀 게이트였습니다 ② 리플레이: 커맨드 스트림 재생 = 같은 결과 ③ 장래 lockstep/AI 학습. 보장 수단은 부동소수점(`/fp:precise`, 서버 vcxproj에 명시), 난수/시간(`Shared/GameSim/Core/Determinism/DeterministicRng.h`/`DeterministicTime.h`), 순회 순서(DeterministicEntityIterator), 그리고 병렬화 시에도 byte-exact 기준 유지입니다."
**꼬리 대비**: "float 결정론은 크로스 플랫폼에서 깨질 텐데?" → 맞다, 현재는 단일 플랫폼(MSVC x64) 전제. 크로스 플랫폼이면 고정소수점/soft float 또는 lockstep 포기가 선택지라고 트레이드오프로 답한다.

### Q20. RHI 추상화는 어떻게 설계했나요?
**답**: "IRHIDevice 인터페이스 + 핸들 타입(IsValid 패턴) + backend-neutral renderer(RHISceneRenderer)입니다. 설계 규칙이 셋: ① DX11 concrete 타입은 `Engine/Private/RHI/DX11/`에 격리 — Engine/Public과 Client/Public에 ID3D11 노출이 실측 0건입니다 ② 생성 실패는 예외가 아니라 빈 핸들 + HRESULT/debugName 로그 ③ '빌드 통과를 위해 RHI 경로를 DX11 concrete로 되돌리지 않는다'는 퇴행 금지 규칙. 그리고 멀티 제품 관점이 핵심인데, LoL과 Elden이 렌더러를 복제하지 않고 같은 RHI renderer에 서로 다른 RenderWorldSnapshot을 공급합니다 — 렌더러가 아니라 스냅샷 작성이 제품별 일이 되도록요."
**꼬리 대비**: "DX12로 가면 뭐가 제일 아픈가?" → 리소스 상태 전이/배리어와 descriptor 관리가 DX11의 암묵 관리에서 명시 관리로 바뀌는 것. 핸들+스냅샷 구조라 renderer 상위는 유지되고 backend 구현만 추가된다고 답변. device-removed 미처리가 현재 약점인 것도 선제 언급 가능.

### Q21. FX 시스템에서 기술적으로 자랑할 부분은?
**답**: "화려함보다 구조입니다. ① 셰이더를 sprite(atlas/fade/scroll)와 mesh(erode/alpha clip/tint) 2계열로 분리하고 파라미터를 공용 cbuffer(b2) 하나로 통일 ② 챔피언별 프리셋 함수 카탈로그(13챔피언, `<Name>FxPresets.h`)로 씬 코드의 인라인 스폰을 수렴 — '흩어진 FX 라우팅은 리뷰 불가'라는 협업 교훈의 실행입니다 ③ 서버가 FX cue 이벤트를 보내고 클라 FxCuePlayer가 라우팅하는데, unknown cue는 침묵 no-op이 아니라 bounded 로그입니다 — 컨텐츠 추가 시 오타가 즉시 보이도록. ④ EffectTuner ImGui 패널로 튜닝 루프에서 리빌드를 제거했습니다."
**보너스 카드**: E sword 0픽셀 디버깅(Q10 대안 ③)을 여기 붙이면 GPU 디버깅 역량까지 전달.

### Q22. 챔피언 150체를 목표로 한 아키텍처를 설명해 보세요.
**답**: "목표를 '챔피언 1체 추가 = 폴더 1개, 중앙 파일 수정 0'으로 잡았습니다. 장치는 셋 — ① Registry self-register: 챔피언별 등록 cpp가 스스로 등록하고, 밴픽 UI는 Registry.ForEach로 자동 생성. eChampion은 명시값 enum으로 박제(삭제/재사용 금지 — 네트워크/DB 호환) ② hookId 4분할: `(champ << 16) | variant`로 스킬 키(BA/Q/W/E/R) × 시점(KeySwap/OnCastAccepted/CastFrame/Recovery)을 분리 — 시전 수락 로직과 판정 프레임 로직이 한 훅에 섞이지 않게. Dispatch는 bool 반환으로 legacy fallback 가드 ③ GenericPendingHit + ApplyDamage 공통 함수: 'N프레임 뒤 판정'과 피해 적용을 공통화해 챔피언 코드의 Scene 의존을 0으로. 현재 서버 GameSim에 15체가 이 폴더 패턴으로 실존합니다. 핵심 사고방식은 '첫 시민에게 안티패턴을 박으면 150체가 전부 물려받는다'여서, 구현 전에 설계 리뷰로 9건을 보정하고 들어갔습니다."

### Q23. 데이터 드리븐은 어떻게 하고 있나요?
**답**: "경계를 먼저 정의했습니다 — JSON은 authoring/cook 입력이고, 런타임 프레임 코드는 validated immutable pack만 읽습니다. 식별자도 3종을 구분합니다: DefinitionKey(네트워크/세이브용 안정 ID), dense ID(pack-local 인덱스), EntityHandle(프로세스-로컬, 어떤 직렬화 경계도 못 넘음). 서버 전용 밸런스 수치는 서버에만, 클라 비주얼 값은 클라에만 컴파일됩니다. 그리고 이행을 정량 관리합니다 — 18개 도메인 분리 스코어카드(현재 종합 28%), 폴백 데이터 진입 시 bounded 로그 16사이트로 '폴백이 제2의 진실이 되는' 사고를 감시하고, Hello 패킷의 dataBuildHash로 클라-서버 데이터 버전 드리프트를 관측합니다."

### Q24. 협업 경험이 없는 것 아닌가요? 혼자 만든 프로젝트인데.
**답**: "사람 팀은 아니지만, 협업의 본질인 '내가 아닌 존재가 내 코드를 수정해도 깨지지 않게 하는 장치'를 집중적으로 훈련했습니다. 복수의 AI 에이전트(Claude, Codex)를 실제 작업자로 붙여 개발했는데, 이게 신입 협업과 놀랍도록 비슷합니다 — 컨텍스트가 없는 작업자가 규칙을 모르고 어기죠. 그래서 만든 게 ① 행동 규칙 문서 체계(CLAUDE.md/AGENTS.md/Compass — 역할 분리: 규칙 vs 실측 vs 사고 로그) ② 반복 실수 로그 `.claude/gotchas.md` — 사고를 '날짜-영역-실수-예방규칙' 포맷으로 30건 이상 박제 ③ 사람이 안 지켜도 되는 기계 강제(경계 lint, PreBuild 게이트) ④ 계획서 선리뷰 워크플로 — 구현 전에 anchor 기반 계획서를 쓰고 교차 리뷰(예: Ezreal 사이클에서 9건 보정)를 받는 프로세스입니다. '코드 리뷰 받을 줄 아는가'는 이 워크플로가 그대로 증거가 됩니다."

### Q25. 설계에서 후회하는 결정이 있나요?
**답**: "셋 꼽겠습니다. ① 경계 강제를 늦게 넣은 것 — Shared→Engine 직접 include 80파일이 쌓인 뒤에 어댑터+lint로 걷어냈습니다. 처음부터 include 경로를 화이트리스트로 잠갔으면 애초에 안 쌓였습니다. ② EngineSDK를 xcopy 동기화로 만든 것 — stale lib에 링크되는 ABI 드리프트 위험이 구조적으로 남았고, ProjectReference 체계였으면 없었을 문제입니다. ③ 관측(에러 진단)을 '나중에'로 미룬 것 — silent fail 사고를 두 번 겪고 나서야 P1~P4 정책으로 정리했는데, 첫 사고 때 정책화했어야 합니다. 공통 교훈은 '규칙은 어긴 코드가 쌓이기 전에 기계로 강제하라'입니다 — 그래서 UE 비교 감사의 로드맵 Phase 1이 전부 강제 장치(include 화이트리스트, CI 게이트)입니다."
**포인트**: 후회 질문은 겸손 테스트가 아니라 메타인지 테스트. "없다"는 최악, 사소한 것도 감점. 구조적 후회+교훈+로드맵 3단이 정답 형태.

### Q26. 이 프로젝트로 무엇을 증명하고 싶나요?
**답**: "세 가지입니다. ① **경계를 설계하고 지키게 만드는 능력** — 서버 권위, 계층 의존성, 스레드 소유권 같은 경계를 정의하고, 문서가 아니라 lint/게이트/해시 검증으로 강제한 것 ② **사고를 시스템으로 바꾸는 능력** — yaw 잔떨림, 미니언 stuck, lock-free race 같은 사고를 고치는 데서 끝내지 않고 원인 enum, 카운터, gotcha 로그, 정책 문서로 재발 방지 장치를 만든 것 ③ **자기 코드의 약점을 스스로 감사하는 능력** — UE5.7 소스와 7차원 비교해 HIGH 약점 목록(minidump 부재, CI 부재, 버전 게이트)과 로드맵을 뽑았습니다. 신입이 '무엇을 만들었나'로 평가받는다면 저는 '만든 것이 무너졌을 때 어떻게 대응했나'까지 보여드릴 수 있습니다."

### Q27. 성능 최적화 경험을 말해 보세요.
**답**: "프레임 17.8ms를 9ms로 복구한 사이클이 있습니다. 순서가 중요한데 — 먼저 자체 Profiler(계층 scope + counter)를 만들어 **측정부터** 했고, 병목이 런타임 FBX 파싱과 개별 로드에 있음을 확인한 뒤 자체 바이너리 포맷 `.wmesh`로 27개 에셋을 오프라인 변환(AssetConverter)해 로드 경로를 바꿨습니다. 이후 규칙으로 박은 게 '최적화는 JSON scope/counter와 frame budget으로 증명한다 — Profiler 표시를 늘리는 것만으로는 최적화로 보지 않는다'(Compass)입니다. 렌더를 끄거나 로스터를 줄여서 빨라 보이게 하는 건 최적화가 아니라 실험 격리이고, 그건 명시적 lab path로 분리해 측정에 표기합니다."

### Q28. 네트워크 프로토콜은 왜 TCP인가요? 게임이면 UDP 아닌가요?
**답**: "단계 전략입니다. 1단계는 TCP로 '서버 권위 파이프라인 전체'(커맨드 직렬화→IOCP→틱→스냅샷→적용)를 세우는 것 — 여기서 순서/재전송을 TCP에 맡기면 게임 로직 검증에 집중할 수 있습니다. UDP 마스터플랜(M1-M6)은 별도로 박제돼 있습니다 — ack 비트필드, 프래그먼트, 스냅샷 델타, 그리고 암호화는 별도 마일스톤. 30Hz 스냅샷+보간 구조라 TCP HOL 블로킹의 체감 비용이 트위치 슈터보다 낮다는 판단도 있었습니다. '패킷 로스에 민감한 상태는 스냅샷 멱등 덮어쓰기 구조라 UDP 전환 시에도 재사용된다'는 게 설계상 포인트입니다."

### Q29. 리플레이/관전은 어떻게 구현되나요?
**답**: "서버 권위+결정론의 부산물로 거의 공짜입니다. 진실 변경이 전부 GameCommand 초크포인트를 지나므로, ReplayRecorder가 커맨드 스트림(+시드)을 기록하면 결정론 시뮬레이션 재실행으로 리플레이가 됩니다. 관전은 스냅샷 브로드캐스트의 구독자 추가입니다. 여기에 버전 문제가 얽히는데 — 다른 데이터/코드 버전으로 재생하면 다른 결과가 나오므로, dataBuildHash 같은 버전 스탬프를 리플레이 헤더에 넣는 것이 로드맵에 있습니다."

### Q30. 본인 엔진의 가장 큰 약점이 뭐라고 생각하나요?
**답**: "감사로 확정한 목록이 있습니다. 최상위 셋: ① **크래시 가시성 0** — minidump 핸들러(SetUnhandledExceptionFilter)가 없어서 필드 크래시가 재현 불가 미제가 됩니다 ② **CI 부재** — 빌드/lint/SimLab 게이트가 전부 수동입니다. 스크립트들이 exit code 규약은 이미 지켜서 wiring만 남은 상태입니다 ③ **버전 핸드셰이크 불완전** — 프로토콜 버전 상수가 죽어 있고, dataBuildHash 불일치도 로그만 하고 진행합니다. 셋 다 '아키텍처 문제'가 아니라 '강제/배선 문제'라는 게 스스로의 진단이고, 그래서 로드맵 Phase 0이 전부 배선 작업입니다. 약점을 모르는 게 약점이라고 생각해서, UE5.7 소스와의 비교 감사로 이 목록을 만들었습니다."
**포인트**: 약점 질문에 준비된 목록+분류(구조 문제 vs 배선 문제)+로드맵으로 답하면 오히려 최고 득점 문항이 된다.

### Q31. 디버깅을 어떻게 하는 편인가요? 프로세스가 있나요?
**답**: "규칙 두 개로 요약됩니다. ① **추측이 2회 빗나가면 즉시 계측으로 전환** — 미니언 stuck 때 정황 분석 3회가 전부 틀렸고, profiler counter 5분이 정답을 줬습니다. 이후 '증상 튜닝 전에 관측 장치부터'(debug UI 오버레이, bounded 트레이스, 이동 버그면 현재 셀/다음 웨이포인트/stuck 이유 노출)가 코딩 규칙이 됐습니다. ② **CPU/GPU 경계에서 분기** — '호출은 되는데 안 보임' 계열은 CPU 디버거로 못 잡습니다. 셰이더를 먼저 읽고 데이터(UV/알파 bbox)를 직접 계측해서, sprite 캡처 PNG를 mesh diffuse로 잘못 쓴 사고를 잡았습니다. 이 프로세스 자체를 스킬 문서로 박제해서 다음 사고 때 재사용합니다."

### Q32. 멀티스레드 버그는 어떻게 예방하나요?
**답**: "구조로 예방하고, 잡을 땐 결정론으로 잡습니다. 예방: ① 소유권 규칙 — truth는 tick 스레드 단독 소유, IOCP는 ingress mutex까지, Chase-Lev는 owner-push 규약 준수 ② 읽기/쓰기 분리 — 미니언 AI를 Decision(읽기)/Apply(쓰기) 2-pass로 나눠 같은 Phase 내 경합 제거 ③ thread_local 슬롯 분리(main=0/worker=idx+1)로 프로파일러 race 해소. 검증: 병렬화 전후 same input → byte-exact snapshot을 게이트로 — 스케줄링에 따라 결과가 흔들리면 해시가 즉시 잡아냅니다. race를 '재수 없으면 나는 버그'가 아니라 '해시 불일치라는 재현 가능한 실패'로 바꾸는 게 전략입니다."

### Q33. 툴/에디터 쪽 경험은요?
**답**: "런타임보다 늦게 시작했지만 순서에 대한 교훈이 있습니다. 맵 에디터(구조물/정글 배치 + 바이너리 스테이지 포맷), EffectTuner(FX 튜닝 패널), 자체 에셋 컨버터(.wmesh/.wtex 계열 cook)를 만들었고, Elden 에디터 쪽에는 트랜잭션 기반 Undo(CEditorTransaction)가 동작합니다. UE 감사에서 얻은 결론이 중요한데 — flagship 툴(어빌리티 타임라인 에디터)을 먼저 만들면 undo/원자적 저장/검증 부재를 전부 상속하므로, **에디터 코어(트랜잭션 공용화, AtomicWriteFile, validator 배선)를 선행**하고 그 위에 툴을 올리는 순서로 로드맵을 세웠습니다. '툴은 기능이 아니라 데이터 안전이 본체'라는 게 결론입니다."

### Q34. AI(에이전트) 도구로 개발했다면, 본인 실력은 어떻게 증명하나요?
**답**: "도구가 코드를 쳐도 설계 결정, 사고 분석, 검증 게이트는 사람 몫이고 그 증거가 문서로 남아 있습니다. ① 에이전트의 잘못된 분석을 제가 계측으로 반증한 기록(미니언 stuck 때 외부 분석 3건이 stale이었고 profiler counter로 제가 확정) ② 에이전트 간 교차 리뷰를 제가 중재해 9건 보정을 확정한 설계 사이클 ③ 에이전트가 규칙을 어기지 못하게 만든 기계 강제(lint/게이트) — 이건 도구를 쓰는 능력이 아니라 팀을 통제하는 능력에 가깝습니다. 그리고 yaw 수학, Chase-Lev 규약, IOCP 스레드 모델 같은 건 지금 이 자리에서 화이트보드로 설명할 수 있습니다 — 어느 부분이든 파고들어 주십시오."

### Q35. 우리 회사에서 이 경험을 어디에 쓸 수 있죠?
**답(클라 직군 버전)**: "스냅샷 소비/보간/예측 보호, RHI 경계 감각(백엔드 격리, 핸들 계약), GPU 디버깅 파이프라인, FX/애니 프레젠테이션 계층 경험이 즉시 이식됩니다. 특히 '서버 권위 아래에서 클라가 무엇을 예측하고 언제 양보하는가'를 사고로 배운 게, 온라인 게임 클라 포지션의 핵심 감각이라고 생각합니다."
**답(서버 직군 버전)**: "IOCP 스레드 모델, 커맨드 초크포인트 설계, 30Hz 틱 권위 시뮬레이션, 결정론 검증, FlatBuffers 경계 검증, 세션 격리 정책이 즉시 이식됩니다. 특히 '봇=커맨드 생산자' 구조는 부하 테스트 트래픽 생성과 안티치트 초크포인트 설계로 바로 연결됩니다."

---

## 내 프로젝트 연결 포인트

면접 중 어떤 주제가 나와도 아래 문장으로 Winters에 착지시킨다.

- **아키텍처/설계 질문** → "저는 그 규칙을 문서가 아니라 빌드 게이트로 강제해 본 경험이 있습니다 — Shared 경계 lint가 PreBuild에서 빌드를 실패시킵니다."
- **네트워크 질문** → "제 서버에서는 봇조차 상태를 직접 못 고칩니다. 사람과 같은 GameCommand 파이프를 타고, 그래서 리플레이가 커맨드 녹화만으로 성립합니다."
- **동시성 질문** → "Chase-Lev의 owner-push 규약을 어겨 메인 스레드 행을 만들어 본 적이 있습니다. lock-free는 알고리즘의 전제조건까지가 스펙이라는 걸 그때 체득했습니다."
- **디버깅 질문** → "추측 2회 빗나가면 계측 전환이 제 규칙입니다. profiler counter 5분이 추측 1시간을 이긴 사고가 계기였습니다."
- **품질/유지보수 질문** → "silent fail 금지, dead diagnostics 금지, bounded 로깅 상한(8/64/512)을 정책 문서로 박고 감사로 집행해 봤습니다."
- **리팩터링 질문** → "108파일 리팩터를 SimLab 결정성 해시 동일로 '동작 무변경'을 증명하고 머지했습니다. 커버리지가 부족할 때 결정론이 최강의 회귀 테스트입니다."
- **협업/프로세스 질문** → "반복 실수를 gotchas 로그로 30건 이상 박제하고, 구현 전 계획서 교차 리뷰로 9건을 보정받아 본 워크플로가 있습니다."
- **성장 가능성 질문** → "UE5.7 소스를 받아 제 엔진과 7차원 비교 감사를 하고 약점 로드맵을 만들었습니다. 배우는 방법을 시스템화하는 게 제 방식입니다."
- **겸손 체크 질문** → "제 엔진의 HIGH 약점 목록을 제가 먼저 말씀드릴 수 있습니다 — minidump 부재, CI 부재, 버전 게이트 불완전. 전부 로드맵에 있습니다."

---

## 마지막 점검 체크리스트

면접 전날 이 목록만 훑는다. 각 줄을 소리 내어 30초 안에 설명할 수 있으면 통과.

- [ ] 5계층 한 줄: Client→Engine(SDK)+Shared, Server→Engine+Shared, Shared→무(無), Engine→제품 모름. 이유 = 실패 오염 격리.
- [ ] 경계 강제 3종: vcxproj include-path 레벨 관점 / Phase 7F 어댑터 9종(`Shared/GameSim/Core/Ecs/`) / `Check-SharedBoundary.ps1` PreBuild lint. 잔존 = `using World = ::CWorld` + EngineSDK/inc 경로.
- [ ] 서버 권위 흐름 암송: 입력 → CommandBatch(FlatBuffers) → IOCP → ingress mutex → 30Hz tick → BotAI(생산만) → ICommandExecutor → GameSim 15체 → Snapshot/Event broadcast → SnapshotApplier/EventApplier.
- [ ] yaw 연대기 4겹: 에셋 forward 축 / ±PI 정규화 재교차 / ack≠따라잡음(예측 보호) / 클라 로컬 Nav 경합. 종결 계약 = "yaw는 연속 상태, 정규화는 wire에서만".
- [ ] Phase swap 사고: AI(2)→Nav(1) 역순이 stale velocity 유발 → swap. + Pathfinder silent fail → `ePathFindResult` 5종 enum.
- [ ] JobSystem: Chase-Lev owner-push 규약, main-push race → 행, `t_iWorkerIdx==-1` global queue 우회. WaitForCounter = help-stealing.
- [ ] Fiber 결정: IOCP worker는 fiber화 안 함(completion→queue→tick fiber 소비). 합격 기준 = byte-exact snapshot.
- [ ] 결정론: SimLab 300틱 해시, 리팩터 전후 동일 증명(108파일). /fp:precise + DeterministicRng/Time + 결정적 순회.
- [ ] RHI: IRHIDevice + IsValid 핸들 + RHISceneRenderer. DX11 concrete는 Private 격리(공개 노출 실측 0건). LoL/Elden = 같은 renderer, 다른 RenderWorldSnapshot.
- [ ] FX: sprite/mesh 셰이더 2계열 + b2 cbuffer, atlas/fade/erode, 프리셋 카탈로그 13챔피언, unknown cue = bounded 로그. E sword 0픽셀 = sprite PNG vs mesh diffuse + UV/알파 bbox 계측.
- [ ] 150챔프 3장치: Registry self-register(+eChampion 명시값 불변) / hookId = (champ<<16)|variant × 4시점(KeySwap/OnCastAccepted/CastFrame/Recovery) / GenericPendingHit + ApplyDamage(Scene 의존 0).
- [ ] 에러 정책: 예외 안 씀(CBinaryReader 한 곳 예외), bare return 금지, dead diagnostics 금지, bounded 8/64/512, 루틴 트레이스 vs 실패 진단 구분, verify 실패 = 로그+drop.
- [ ] 성능 스토리: Profiler 먼저 → 17.8ms→9ms → .wmesh 27개 변환. "최적화는 counter와 frame budget으로 증명".
- [ ] 약점 목록(선공용): minidump 0 / CI 부재 / 버전 게이트 불완전 / EngineSDK xcopy ABI 드리프트 / ensure 부재 / device-removed 미처리. 전부 "구조가 아니라 배선 문제" + Phase 0~2 로드맵.
- [ ] 후회 3종: 강제 장치 늦음 / EngineSDK xcopy / 관측 후행. 교훈 = "규칙은 어긴 코드가 쌓이기 전에 기계로 강제".
- [ ] 협업 증거: gotchas 30+건 박제 / 계획서 교차 리뷰 9건 보정 / 규칙 문서 3층(행동/실측/사고) / 기계 강제.
- [ ] 3분 발표 뼈대: 5계층 → 서버 권위/결정론/기계 강제 3포인트 → "사고를 시스템으로 바꾼 과정" 클로징.
- [ ] 금지 사항: 기능 나열형 답변 / "약점 없다" / 상용 엔진 폄하 / 검증 안 한 수치 즉흥 인용.
