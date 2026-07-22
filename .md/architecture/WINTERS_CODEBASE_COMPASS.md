# Winters Codebase Compass

## DataDriven Definition Boundary (2026-06-22)

- JSON is authoring/cook input. Runtime frame code reads validated immutable packs, never JSON strings.
- `DefinitionKey` is the stable SharedContract identity for network/save/manifest use.
- `ChampionDefId`, `SkillDefId`, and other dense IDs are pack-local only.
- `EntityHandle` is process-local entity lifetime identity and never crosses JSON/network/save boundaries.
- Shared/GameSim owns definition types and deterministic lookup behavior, not LoL data values.
- ServerPrivate gameplay values compile only into Server; ClientPublic visual values compile only into Client.
- Legacy tables are deleted only after reader count reaches zero and build/SimLab/client-server smoke gates pass.
- Hot Reload and Perforce integration start after the DataDriven ownership and cutover gates are complete.
- Practice/balance tools stay on the normal in-game network path: Client UI sends typed commands, Server policy and GameSim own every gameplay mutation, and Snapshot/Event reports the result.
- A practice balance overlay is temporary bounded session data, never canonical authoring data. Approved values return to JSON/sheet -> validate -> cook -> SimLab/build before release.
- Canonical skill damage follows `ServerPrivate SkillEffectGameplayDefs.json -> codegen -> DamageFormulaDef -> DamageRequest -> DamagePipeline`; champion hooks may choose targets or variants but must not own a second copy of the same rank/scaling values.
- Variant-only flat damage may use ranked `SkillEffectParam` values only through an exact generator/runtime allowlist; ordinary flat damage and ratios remain owned by `DamageFormulaDef`.
- Canonical item on-hit damage follows `ServerPrivate ItemGameplayDefs.json -> codegen -> ItemDef -> DamageQueueSystem -> DamagePipeline`; mixed damage types require separate requests rather than silently folding into the basic attack type.
- Canonical animation playback/cast/recovery timing follows `ClientPublic ChampionVisualDefs.json -> codegen -> SkillRegistry`. Runtime timing panels may produce a review draft, but a draft is not release truth until merged and regenerated.
- In F4 canonical authoring, the current saved source JSON has highest value authority. Dated PLAN/RESULT numbers are historical baselines and generated files are derivatives. Generators and tests prove current-source parity; they must not restore an editable exact value unless the user explicitly froze that value.

## Runtime Concurrency / Network Boundary

- Transport adapters own sockets, connection generations, framing, ACK/retry/order, fragmentation/reassembly, and transport backpressure. GameRoom consumes a logical session id, packet type/sequence, and owned payload bytes; it must not branch on socket handles or UDP endpoints.
- IOCP completion callbacks do not mutate GameRoom, ECS, lobby, or authority state. They publish bounded owned ingress; the 30 Hz room owner drains and revalidates it before world mutation.
- TCP and UDP may coexist only behind the same logical-session/send boundary. Adding a transport must not duplicate command validation, GameSim execution, Snapshot/Event construction, or client visual application.
- Engine owns generic JobSystem/Chase-Lev/Fiber primitives. Do not expose Engine jobs, Win32 fiber handles, TLS/FLS, IOCP, or socket types to `Shared/GameSim`; Shared describes deterministic work/data dependencies only.
- IOCP waits remain OS-thread completion waits. FiberFull is for CPU continuations waiting on JobCounter dependencies, not for wrapping `GetQueuedCompletionStatus`, overlapped socket lifetime, or arbitrary blocking I/O.
- A FiberFull `WaitForCounter` boundary must not be crossed while holding a thread-affine lock, an in-flight socket/OVERLAPPED ownership assumption, or mutable TLS scratch that must survive as fiber-local state. Use stack/job-owned state or FLS for continuation-owned context.
- Job publication is a lifetime contract: fully construct immutable work before pointer-token publication, increment counters before publication, and keep Submit/Wait admitted until publish/inline completion. Shutdown destroys deque/fiber state only after that admission boundary is quiescent.

작성일: 2026-06-04

이 문서는 새 작업자가 처음 방향을 잡기 위한 활성 Compass다. 코드 목록을 전부 복사하지 않는다. "어느 계층이 무엇을 소유하고, 어디까지 의존해도 되는가"를 정리한다.

## 읽는 순서

1. `AGENTS.md`
2. `.claude/gotchas.md`
3. 이 문서
4. 작업 도메인 문서

도메인 문서:
- 서버 권위, GameSim, 네트워크, 스냅샷, 챔피언 스킬, AI, FX cue: `CLAUDE_Legacy.md`
- 서버 상태 전이, 챔피언 예외, 충돌 우선순위, 사람형 bot 확장 원칙: `.md/architecture/WINTERS_SERVER_SIMULATION_EXCEPTION_DOCTRINE.md`
- 한 실행 내 Practice/AI/asset authoring, Undo/Redo, linked clock, deterministic rewind/branch 원칙: `.md/architecture/WINTERS_LIVE_AUTHORING_CHRONOBREAK_ARCHITECTURE.md`
- NYPC Perception/회고 자산, Influence layer, Chrono AI A/B, imitation/RL artifact bridge 원칙: `.md/architecture/WINTERS_NYPC_HUMANLIKE_AI_RESEARCH_ARCHITECTURE.md`
- 엔진 C++ 컨벤션과 `CGameInstance` 경계: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`
- LoL/Elden 공용 RHI 방향: `.md/plan/rhi/sessions/S13_LOL_TO_ELDEN_SHARED_RHI_RENDER_PIPELINE.md`
- Unreal 5.7.4 기준 backend/feature profile/renderer/platform 분리와 Console Restricted 경계: `.md/architecture/WINTERS_UNREAL_STYLE_MULTI_BACKEND_RHI_ARCHITECTURE.md`
- 렌더링 필터와 DX11 생존 경로: `.md/plan/rhi/sessions/S15_ENGINE_RENDERING_FILTER_AUDIT.md`
- Elden 클라이언트 분리 방향: `.md/EldenRing/00_ELDENRING_INDEX.md`
- Elden editor 세션: `.md/plan/EldenRingEditor/01_DX12_IMGUI_EDITOR_BOOTSTRAP.md`부터 `07_BOSS_BLACKBOARD_HFSM_BT_TUNING.md`
- LoL/Elden 검증장을 Class & Servant 출시/운영 기반으로 연결하는 전체 폴더 방향: `.md/architecture/CLASS_SERVANT_FOUNDATION_DIRECTION.md`
- LoL/Elden 공용 UI 파이프라인: `.md/architecture/WINTERS_UI_PIPELINE_ARCHITECTURE.md`
- ImGui 튜너·디버그 패널·워크플로 편집기 제품 설계와 완료 게이트: `.md/architecture/WINTERS_IMGUI_TOOL_DESIGN_GUIDE.md`
- 장기 AAA 엔진 챕터: `.md/문서/00_INDEX.md`

## 제품 방향

Winters는 하나의 엔진으로 여러 게임 클라이언트를 띄우는 구조를 목표로 한다.

```text
WintersEngine.dll
├── WintersLOL.exe      // MOBA client
└── WintersElden.exe    // Action RPG client
```

현재 LoL 클라이언트는 DX11 런타임이 기본 경로다. DX12/Vulkan/console 방향은 RHI 이관을 통해 점진 도입한다. Elden 계열 클라이언트는 LoL 코드 안에 섞지 않고, 같은 엔진 DLL과 같은 RHI/asset/runtime contract 위에 별도 제품 클라이언트로 올린다.

## 계층 책임

### Shared / GameSim

역할:
- 서버 권위 gameplay truth의 데이터와 deterministic simulation contract
- `GameCommand`, Snapshot/Event schema, gameplay component, champion data, server-side skill validation

의존 규칙:
- `Shared/GameSim`은 `Engine`, `Client`, `Renderer`, `UI`, `ImGui`, `DX11`을 include하지 않는다.
- gameplay 결과는 Shared/Server에서 만들어지고 Client는 presentation으로 소비한다.

### Server

역할:
- 클라이언트 입력을 `GameCommand`로 받아 GameSim을 실행
- 위치, HP, 마나, 쿨타임, 피해, 투사체, 구조물, 미니언, 승패, bot command generation의 권위
- Snapshot/Event/FX cue 송신

금지:
- 서버 로그만으로 client visual 성공을 판정하지 않는다.
- bot AI가 Transform/HP/cooldown 같은 truth component를 직접 고치지 않는다. command를 생산한다.

LoL bot decision contract:

```text
Server World Fact
-> ChampionAIPerception (30 Hz read-only target/status/economy facts)
-> ChampionAIValuation (deterministic utility scores)
-> IChampionAIBrain (intent + hysteresis)
-> champion profile/combo executor
-> GameCommand
-> CDefaultCommandExecutor
```

- 30 Hz는 perception/utility 및 emergency interrupt 갱신 주기다. 일반 deliberation은 기본 5 Hz이고 동일 command를 매 tick 재전송하지 않는다.
- `Q/W/E/R/BA/Combo` 선택은 profile/combo 계층에 두고 damage, status, movement 결과는 기존 executor와 champion GameSim이 만든다.
- 선택 우선순위는 `hard safety -> active combo/dive commitment -> new utility`다. emit 전 `CanMove/CanAttack/CanCast`를 검사해 서버가 거절할 명령으로 combo step을 진행시키지 않는다.
- 포탑 파괴 같은 macro 전환은 home lane을 덮어쓰지 않고 active objective/lane으로 분리한다. macro는 hard safety와 이미 승인된 combo/dive 뒤에서만 전환하며, 모든 bot 결과는 기존 `GameCommand` 경로로만 적용한다.
- 상태 충돌은 마지막 if가 이기는 방식으로 숨기지 않는다. `ActionState` sequence, 명시적 우선순위, 결정론 probe로 stale impact와 중복 commit을 차단한다.
- 공용 Perception에는 champion 이름을 포함한 필드를 추가하지 않는다. 특수 대상은 generic ability/mobility target으로 전달하고 champion tactics registry가 해석한다.

### Engine

역할:
- 창, frame loop, RHI, renderer, resource, ECS primitive, UI rendering, editor/runtime 공통 서비스
- `CGameInstance`는 낮은 빈도의 gateway API를 제공한다.

의존 규칙:
- Engine은 Client 제품 코드에 의존하지 않는다.
- Engine UI panel은 `CWorld`, `Shared/GameSim`, 제품별 manager를 직접 조회하지 않는다. 필요한 데이터는 view state로 받는다.
- Engine public header 변경 시 `EngineSDK/inc` 동기화가 필요할 수 있으므로 검증 항목에 `UpdateLib.bat` 또는 빌드 전 SDK sync를 남긴다.

### Client

역할:
- LoL/Elden 같은 제품별 scene, input, camera, presentation bridge, UI state build, weak prediction, interpolation, animation/FX playback
- 서버 Snapshot/Event를 visual state로 적용

의존 규칙:
- Client는 Engine/EngineSDK와 Shared schema/component를 읽을 수 있다.
- Client가 authoritative gameplay truth를 새로 만들면 안 된다. 예외는 명시된 local-only smoke path뿐이다.
- Client/Public에 새 `ID3D11*` 의존을 넓히지 않는다. DX11 concrete handle은 Engine backend 또는 좁은 bridge/adapter에 가둔다.

### Tools / Editor

역할:
- asset import, content browser, material resolver, effect graph, sequencer, world partition editing, validation/cook pipeline

의존 규칙:
- Editor 전용 기능은 normal F5 runtime을 숨기거나 우회하지 않는다.
- Editor는 runtime asset contract를 생산하고 검증한다. Runtime은 최종적으로 `.wmesh`, `.wskel`, `.wanim`, `.wtex`, `.wmat`, `.wmap`, `.wfx`, `.wseq` 같은 Winters binary를 우선한다.

## LoL DX11 현재 기준

- 기본 클라이언트 visual smoke는 DX11이다.
- UI는 현재 ImGui/DX11 SRV 경로가 살아 있다. 새 UI texture loading은 먼저 `CUI_Manager::Load_TextureSRV`와 `CUIAtlasManifest` 재사용을 검토한다.
- 한 UI feature 때문에 별도 asset registry나 중복 DX11 device accessor를 만들지 않는다. 공용화가 필요하면 Engine UI/resource 계층에서 기존 흐름을 확장한다.
- UI panel은 render만 한다. 월드, FOW, 스냅샷, 생사, 팀 시야 판정은 Client bridge가 view data로 만든다.
- Runtime resource는 `Client/Bin/Resource`에서 해석한다. config별 `Client/Bin/Debug*/Resource` 복사는 기준 경로가 아니다.

## 성능 최적화 경계

- 최적화는 JSON scope/counter와 frame budget으로 증명한다. Profiler 표시를 늘리는 것만으로는 최적화로 보지 않는다.
- 새 renderer, cache, update loop를 추가하기 전에 기존 경로의 중복을 제거하거나 같은 owner 안에서 확장한다.
- Map/static mesh batching, material grouping, visibility mask, draw submission 최적화는 Engine의 generic render/resource 계층 소유다. Client는 LoL scene/view data를 만들고 generic Engine API를 호출한다.
- Client 성능 문제를 해결하려고 Engine public header에 LoL Client 타입, Server 타입, GameSim truth owner, DX11 concrete type을 새로 노출하지 않는다.
- normal F5 runtime에서 roster, map, minion, champion, snapshot, UI, FX를 숨기는 것은 최적화가 아니라 실험 격리다. 격리가 필요하면 명시적인 lab path로 분리하고 측정 결과에 표시한다.

## RHI 방향

목표:
- DX11 기본 경로를 유지하면서 `IRHIDevice`, RHI handle, render snapshot, backend-neutral renderer로 이관한다.
- LoL과 Elden은 renderer class hierarchy를 복제하지 않고 같은 RHI renderer에 서로 다른 world/render snapshot을 공급한다.

금지:
- build 통과를 위해 DX12/RHI path를 legacy DX11 concrete type으로 되돌리지 않는다.
- Client/Public 또는 Shared에 `ID3D11Device`, `ID3D11ShaderResourceView`, `DX11Pipeline`, `DX11Shader` 노출을 늘리지 않는다.
- Engine filter나 `.vcxproj.filters`의 가상 폴더만 믿고 소유권을 판단하지 않는다. 실제 경로와 include 방향을 확인한다.

## Elden Client 방향

목표:
- `WintersElden.exe`는 `WintersEngine.dll` 위에서 별도 action RPG client로 선다.
- 첫 세로 슬라이스는 Elden 전체 복각이 아니라, 작은 field에서 skinned character load, idle/run/attack/dodge, lock-on camera, streaming boundary를 증명하는 것이다.
- 원본 추출 에셋은 로컬 검증용이다. 공개 빌드/포트폴리오 경계는 대체 가능 에셋과 자체 pipeline code 중심으로 유지한다.

구조:
- LoL과 Elden의 scene/camera/input/combat/world module은 분리한다.
- Asset runtime은 최종적으로 Winters binary를 우선한다.
- Elden renderer를 새로 복제하지 않는다. 필요한 것은 Elden식 `RenderWorldSnapshot` 작성이다.

## Editor 방향

Elden editor와 공용 editor는 다음 계층으로 본다.

```text
Editor shell
├── Content Browser / Asset Catalog
├── Importer / Converter / Validator
├── Material Resolver
├── WFX / FX Graph
├── Sequencer
└── World Partition / Streaming Cell Tool
```

Editor는 디자이너 workflow를 만들되, runtime gameplay truth와 renderer backend 경계를 흐리지 않는다. Editor에서 만든 데이터가 runtime에서 로드될 때는 같은 resource/path/validation contract를 탄다.

## 협업 문서 구조

- `AGENTS.md`: cross-agent 행동 규칙과 반드시 읽을 문서 포인터
- `CLAUDE.md`: Claude Code용 행동 규칙과 hook
- `.claude/gotchas.md`: 반복 실수 방지 로그
- 이 문서: active architecture compass
- `.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md`: 설계 원칙 P1~P4 (실패 즉시 가시화, 실패 격리, 특수상황 명시, 디버깅 우선)
- `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md`: 예외/실패 처리 규약과 경계별 초크포인트
- `.md/architecture/WINTERS_DEPENDENCY_MAP.md`: 실측 의존성 지도 (빌드 그래프, 위반 목록, 해소 로드맵)
- `.md/architecture/WINTERS_HANDOFF_GUIDE.md`: 온보딩/패치 체크리스트/지뢰밭 목록
- `.md/architecture/WINTERS_DATA_ARCHITECTURE.md`: 게임 데이터 소유권 매트릭스, 폴백 가시화 규칙, D-1~D-6 마이그레이션 슬라이스
- `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md`: UE 툴/Fab 자산의 Winters 이식 설계 (ingestion→cook→validator 경로)
- `.md/계획서작성규칙.md`: `/plan-rules` 출력 형식
- `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`: C++ 컨벤션과 Engine boundary
- `CLAUDE_Legacy.md`: 서버 권위 GameSim 작업 compact brief
- `_MODULE.md`: 향후 모듈별 Compass manifest. 아직 없으면 이 문서와 `rg`로 보강한다.

## 작업 전 체크

cross-module 작업이면 먼저 아래를 답한다.

1. gameplay truth인가, presentation인가?
2. 새 의존이 어느 방향으로 생기는가?
3. Engine public header를 바꾸는가?
4. DX11 concrete type을 public/API로 밀어 올리는가?
5. normal F5 runtime을 debug/lab path로 우회하고 있지는 않은가?
6. LoL 전용 코드가 Elden/공용 엔진 경계로 새는가?
7. 문서 갱신이 필요한 architectural decision인가, gotcha인가, 일회성 plan인가?

## Boundary enforcement pointers

- Job/Fiber ownership: `Engine/Public/Core/JobSystem.h`, `Engine/Private/Core/JobSystem.cpp`, `Engine/Public/Core/JobSystem/WorkStealingDeque.h`
- Transport semantics: `Shared/Network/PacketSemantics.h`, `UdpPacketHeader.h`, `UdpFragmentHeader.h`
- Socket/ACK/reassembly adapters: `Server/Private/Network/UdpIocpCore.cpp`, `Client/Private/Network/Client/UdpClient.cpp`
- Logical session/tick handoff: `Server/Public/Network/ServerSessionHub.h`, `Server/Private/Network/ServerSessionHub.cpp`, `Server/Private/Game/GameRoomTick.cpp`
- Shared boundary gate: `Tools/Harness/Check-SharedBoundary.ps1`
