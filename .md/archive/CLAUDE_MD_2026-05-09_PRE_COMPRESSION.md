# Winters Engine — 작업 운영 브리프

범용 멀티-RHI / 멀티-플랫폼 C++20 게임 엔진. 단일 엔진 DLL 로 LoL 모작 (`WintersLOL.exe`) + 엘든링 모작 (`WintersElden.exe`).

RHI 백엔드 비전:
- DX11 (현행 가동) → DX12 (Track 2 W7-9 Scaffold) → Vulkan (W14-17 박제 완료)

플랫폼 비전:
- Windows (현행 가동) → Xbox (DX12 공유) → PS5 (gnm/gnmx 박제 보류) → Mobile (Vulkan/Metal 공유)

현 상태:
- DX11 단일 백엔드 가동
- DX12 Bootstrap Scaffold (Engine Debug-DX12 config + clear/present 코드 경로)
- RH-7 Platform Surface/Capabilities public contract 1차 반영
- Phase 7D/7E InGameScene split 완료, S1 자동 smoke로 3클라 local binding/snapshot/Q skill hook 확인 + 1클라 full-map telemetry 확인 + direct-local selected/Sylas fallback telemetry 추가(직접 화면 시각 확인은 user-owned)
- Vulkan / 콘솔 / Mobile 은 RHI 추상화 완성 후 진입

이 문서는 운영 브리프다 (백과사전 X). 상세 설계는 `.md/architecture/`, `.md/plan/`, `.md/guide/` 에 있다.

마지막 갱신: 2026-05-07. Debug 전체 빌드, DX12SmokeHost 독립 빌드/8초 생존, BanPick `--banpick-smoke` 자동화, Server 1 + Client 3 hidden smoke(Yone/Fiora/Ezreal Join/Pick/StartGame/local binding/Snapshot/Q Dispatch/castFrame Hook) 통과. 1-client `--smoke-full-map` map init/InGameMap telemetry 통과. 다음 관문은 직접 화면에서 챔피언/맵 렌더 시각 확인.

세션 로그:
- 4월 누적: `.md/archive/SESSION_LOG_2026-04.md`
- 5월 누적: `.md/archive/SESSION_LOG_2026-05.md`
- 압축 전 원본: `.md/archive/CLAUDE_MD_2026-05-01_PRE_COMPRESSION.md`
- 본 갱신 직전 백업: `.md/archive/CLAUDE_MD_2026-05-06_PRE_UPDATE.md`
- 직전 codex 스타일 적용 전 백업: `.md/archive/CLAUDE_MD_2026-05-06_PRE_CODEX_STYLE.md`
- 의존성 그래프 박제: `.md/archive/DEPENDENCY_GRAPH_2026-05-06.md`

---

## 1. Current Focus (2026-05-07 갱신)

### A. 진행 트랙 4개

#### Track 1 (Graphics) — DX11 PBR + GGX + SSAO

W1~W4 완료 + W5 부분 + W6 partial. codex 정정: "caller 마이그 진행 중" 표현이 부정확. 실제는 IRHI facade 통과 시작 + DX11 native bridge 잔존.

```txt
W1   BRDF_GGX.hlsli + RH-0 §1 TODO marker
W2   PBR + cbuffer 4-light array + 7 챔프 PBR opt-in
W3   축소: Forward+ → cbuffer fixed 4-light
W4   NormalPass + SSAOPass + GTAO_CS + GTAO_Blur_CS + 9 leak consumer 7/9 마이그
W5   부분: Get_RHIDevice() -> IRHIDevice* 정식 통과. Public DX11 헤더 제거 / 6 챔프 차별화 보류
W6   partial:
       RH-3 7 보조 인터페이스 (BindGroup/Layout/CommandList/PipelineState/Queue/RenderPass/SwapChain) +
       RHIDescriptors + Tag template CRHIResourceTable + ShaderCompiler 통과
       CMaterialPBR.cpp:12, Texture.cpp:12, ModelRenderer.h:28 가 IRHI facade 받지만 내부에서 DX11 native handle 추출 (완전 마이그 X)
       셰이더 register(... space0) 명시 누락 (실측: Shaders/* + Shaders/SSAO/* + PBR 11개 전수, space0 0건)
```

#### Track 2 (RHI 멀티 백엔드) — DX12 Scaffold

W3, W5, W6 통과. codex 정정: IRHI 보조 인터페이스 7개 (BindGroup/Layout/CommandList/PipelineState/Queue/RenderPass/SwapChain) + Device 포함 총 8개. 구 표기 "4개" / "9 인터페이스" 둘 다 부정확.

W7-9 Scaffold 단계:

```txt
Engine/Private/RHI/DX12/ 28 파일 박제
  Device/Queue/SwapChain/CommandList/PipelineState/RenderPass/BindGroup/Layout/
  RootSignature/DescriptorHeap/MemoryAllocator/ResourceBarrier/Sampler/Shader/Texture/Buffer

Winters.sln 에 Debug-DX12|x64 / Release-DX12|x64 config 추가
  codex 정정: Engine 만 진짜 DX12 config, Server/Client/Tools 는 Debug|x64 매핑 (sln L34, L42, L50 = 솔루션 alias)

Engine.vcxproj Debug-DX12:
  OutDir Bin\Debug-DX12\
  AdditionalDependencies d3d12.lib;d3d11.lib;dxgi.lib;d3dcompiler.lib;dxguid.lib

ThirdPartyLib 에 D3D12MA 추가 (Engine/ThirdPartyLib/D3D12MA/Src/D3D12MemAlloc.cpp)

CDX12Device : public IRHIDevice 박제 (#if defined(WINTERS_RHI_BACKEND_DX12) 가드)
  IDXGIFactory6 + ID3D12Device + D3D12MA::Allocator + 8 메서드

Tools/DX12SmokeHost/ 추가
  WintersRun + IWintersApp 기반 DX12 smoke host. 단 Winters.sln 미등록 상태.

RH-7 public contract 1차 반영
  Engine/Public/RHI/{RHISurface,RHICapabilities}.h
  Engine/Public/Platform/{PlatformTypes,IPlatformWindow,IPlatformSurface}.h
  CWin32Window IPlatformWindow/IPlatformSurface bridge
```

합격 기준: Debug-DX12 Engine 빌드 통과 + DX12SmokeHost clear/present + Alt+F4 정상 종료. 시각 동일성은 W10-13 진입 (보류).

주의: `EngineConfig::rhiBackend`는 현재 의도 표기 필드에 가깝고, 실제 DX12 선택은 `WINTERS_RHI_BACKEND_DX12` compile macro가 결정한다. `DX12SmokeHost`는 파일이 있지만 아직 `Winters.sln` 프로젝트가 아니다.

W10-13 / W14-17 박제 완료, 진입 대기.

#### Track 3 (Fiber JobSystem) — Phase 5-B-pre / FiberShell M0

현 Phase 5-A MVP: Chase-Lev WorkStealingDeque + Global queue hybrid. `EnqueueJob` L122-152 의 race 1차 수정 완료 (Worker 는 자기 Deque, Main/외부/overflow 는 Global Queue).

5/5 신규 박제 (구 표기 "박제 0%" 부정확):

```txt
Engine/Public/Core/Fiber/{FiberTypes, Fiber, FiberPool}.h 3 헤더
  FiberPool.h 23 줄, 풀 본체 미박제 (m_vecFibers.clear() 만)

JobSystem.h:13 가 FiberTypes include
  eJobExecutionMode { ThreadOnly, FiberShell }
  SetExecutionMode / GetExecutionMode / m_eExecutionMode

JobSystem.cpp FiberShell 동작 본체:
  WorkerLoop:160-176 (FiberShell 모드면 ConvertThreadToFiber)
  TryExecuteItemOnFiber:253-273 (매 job CreateFiber/SwitchToFiber/DeleteFiber)
  FiberShellEntry:275-284
```

잔여 (안정화 계획서: `.md/plan/engine/FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`):
- CFiberPool 미사용. 매 job CreateFiber/DeleteFiber 비효율
- WaitForCounter Yield 없음. Fiber 컨텍스트에서 다른 fiber 로 switch 안 함 (busy-wait L287-)
- `Get_WorkerSlot()` Fiber resume 시 thread-local 인덱스 위험
- `InGameBootstrapBridge.cpp` NavSystem 생성 경로에 `Set_JobSystem(pJS)` 주입 완료. CFiberPool/WaitForCounter/Get_WorkerSlot 안정화는 잔여

#### Track 4 (Gameplay Stabilization) — 신설 + 5/6 Gameplay 4 Blocker

B-13 시스템 박제됨: SpatialHash / Vision / TurretAI / BT / MCTS (실제 트리). 다음 목표는 기능 추가가 아닌 phase 순서/race/행동 보존 검증. 특히 SpatialHash 가 MinionAI 라인전 행동 변경 안 했는지, Vision/Turret 이 같은 phase race 안 만드는지.

5/6 Gameplay 4 blocker (2026-05-07 기준: 코드 경로 정리됨, 런타임 smoke로 재검증):

```txt
G-1 BanPick → InGame champion identity smoke
    ChampionCatalog / RosterSpawner / ChampionSpawnService 경로로 정리됨.
    `--banpick-smoke` hidden 자동 smoke에서 Server 1 + Client 3 Yone/Fiora/Ezreal Join/Pick/StartGame/local binding/Snapshot 통과.
    렌더된 InGame champion identity는 직접 화면 smoke로 최종 확인 필요.

G-2 local ECS champion 기반 skill dispatch smoke
    GetPlayerChampionId()가 player entity의 ChampionComponent를 우선 읽는 구조로 정리됨.
    2026-05-07 hidden auto smoke에서 Yone/Fiora/Ezreal Q Dispatch/castFrame 통과.
    hook 결과: gameplay=1, visual=1, legacy=0.

G-3 Map submesh 256 visibility cap
    MeshGroupVisibilityComponent 는 cap 아님. 실측 `VisibilityMask = std::array<u64_t, 32>` = 2048 submesh, `CWMeshLoader::MAX_SUBMESHES = 2048`. 256 hit 는 ModelRenderer/CModel bone matrix cbuffer cap 후보. 맵 submesh 일부 안 보임은 Model/Material/.wmesh 로드 경로 별도 진단.

G-4 Stage1.dat Debug-only
    Client PostBuild에 `Client/Bin/Data/*` -> Debug/Release `Data/` 복사 추가.
    `Client/Bin/Release/Data/Stage1.dat` 확인 완료.
```

### B. 즉시 진입 명령

#### Option Stabilization-0 (codex 권장, 일회성 게이트)

```txt
1. Git/imgui imconfig.h dirty state 정리 (vendor 분기 vs reset 결정) — 남음
2. Debug|x64 전체 빌드 (Engine → Client → Server → AssetConverter) — 통과
3. EngineSDK/inc · /lib 동기화 정상 여부 확인 — 통과
4. Debug-DX12 Engine compile-only 재검증 — 통과
5. DX12SmokeHost 독립 build 명령 + 8초 생존 smoke — 통과
6. CLAUDE.md / TODO.md 현재 코드 기준 갱신 — 5/7 smoke 결과 반영
```

#### Option Stabilization-1 (5/6 신규, Phase 6C/6D/6D-2 후 안정화)

```txt
1. 전체 빌드 고정 (Engine + Client + Server + AssetConverter Debug 통과)
2. 3 클라 런타임 smoke
   Server 1 + Client 3 hidden 자동 smoke는 roster/start/local binding/snapshot/Q skill hook까지 통과.
   1-client `--smoke-full-map` map init/InGameMap telemetry 통과. 화면 기반 챔피언+맵 검증은 잔여.
3. Client filters 정리 (InGameRosterSpawner / InGameNetworkBridge / GameSessionClient + schema generated 포함)
4. Server / AssetConverter filters 정리
5. Phase 7 계속 진행 (7E bridge 완료 → 3-client smoke freeze)
```

#### Option A — Fiber JobSystem 안정화

진입 문서: `.md/plan/engine/FIBER_JOBSYSTEM_STABILIZATION_PLAN.md`

- CFiberPool 박제, 매 job CreateFiber/DeleteFiber 제거
- WaitForCounter Yield-aware
- `Get_WorkerSlot()` Fiber-safe
- `InGameBootstrapBridge.cpp:184` NavSystem `Set_JobSystem(pJS)` 적용 여부 검증

#### Option B — RHI/DX12 Bootstrap

진입 문서: `.md/plan/rhi/DX12_BOOTSTRAP_PLAN.md`

- Debug-DX12 Engine 빌드 통과
- CDX12Device::Create + SwapChain/Queue/CommandList 초기화
- Clear color present + Alt+F4 정상 종료

#### Option C — Track 1 W6 caller 정식 마이그

- `CMaterialPBR / CTexture / ModelRenderer` DX11 native bridge 제거
- W7 RH-5 진입 전 IRHI 완전 통과

#### Option D — Track 4 B-13 회귀 검증

- SpatialHash 와 MinionAI 행동 보존 확인
- Vision / TurretAI phase race 점검

#### Option E (5/6 신규) — Phase 7 Champion ECS / Scene Split 계속 진행

진입 문서: `.md/TODO/05-06/PHASE_7_CHAMPION_ECS_SCENE_SPLIT_MASTER_PLAN.md`

- 완료: 7A ChampionCatalog, 7B ChampionSpawnService, 7C BanPick/catalog/roster 흐름, 7D champion Skill/FX hook 이관
- Fiber 루트 후 반영: Irelia_Tuning, Irelia E/W/R accepted hook, Garen/Zed/Riven cast fallback hook, ChampionModuleBootstrap
- 완료: 7E InGameRender/PlayerControl/CombatInput/Debug/ChampionState/Lifecycle/SkillDispatch/PlayerTransform/Bootstrap bridge 분리
- Remaining validation: direct champion/map visual smoke is user-owned. Implemented code cleanup: 7F F-1/F-2/F-3, 7G filters, SkillDispatch/CombatInput bridge local runtime boundary cleanup, Scene local champion runtime bridge cleanup, and Client DX11 direct include cleanup.

### C. 미결 (5/6 누적)

1순위:
- Stabilization-1 status: Server 1 + Client 3 hidden smoke passed for Yone/Fiora/Ezreal Join/Pick/StartGame/local binding/Snapshot/Q skill hook. 1-client full-map telemetry passed. Direct champion/map visual smoke verification is user-owned.
- 7F cleanup F-1: Engine UI_Manager의 `Client/Bin/...` UI texture 경로를 `Resource/...` + `WintersResolveContentPath()` 로 전환. Engine Debug + Client Debug + 1-client fast smoke 통과.
- 7F F-2/F-3 complete: `Shared/GameSim/World.h` is the temporary Engine CWorld adapter, and `RenderComponent` is split into `ECS/Components/RenderComponent.h` with explicit Client includes.
- SkillDispatch/CombatInput bridge cleanup: champion module includes were removed from the bridges; local preconditions, dash/damage callbacks, passive dash queue/reset live behind `CScene_InGame` APIs; Riven Q animation key swap is now a `VisualHook`.
- Scene local champion runtime cleanup: Yasuo/Kalista/Irelia local runtime updates, passive dash queue/reset, target dash/R dash, and local damage application now live behind `CInGameChampionStateBridge`.
- Client DX11 include cleanup: `Scene_InGame.cpp` no longer includes `RHI/CDX11Device.h`; `FxBeamSystem.cpp` and `FxSystem.cpp` no longer include `RHI/DX11/BlendStateCache.h` directly.
- 7G complete: Engine/Client/Server/AssetConverter filters have MissingInFilters=0, FilterOnly=0, MissingFilter=0; stale filter backup files removed.
- DX12 핵심: `Tools/DX12SmokeHost` 독립 msbuild 명령과 8초 생존 smoke 통과. `Winters.sln` 등록은 선택 사항으로 남음.
- Fiber 안정화 전제: Phase 5-A → 5-B-pre 전환. `InGameBootstrapBridge.cpp` NavSystem `Set_JobSystem(pJS)` 주입 완료. 1만 job / Nav stress는 Track A M0-1에서 재확인.
- `Engine/External/imgui` `imconfig.h` 로컬 수정. 빌드 필수 여부 결정 + vendor 분기 또는 reset.

최근 로컬 산출물 참고:

```txt
2026-05-07 오전 기준
Engine/Bin/Debug/WintersEngine.dll
Engine/Bin/Debug-DX12/WintersEngine.dll
Client/Bin/Debug/WintersGame.exe
Server/Bin/Debug/WintersServer.exe
Tools/Bin/Debug/WintersAssetConverter.exe
```

산출물은 존재하지만 Stabilization-0에서는 같은 순서로 재빌드해 현재 working tree 기준을 다시 고정한다.

5/6 신규:
- `Scene_InGame.cpp` 일부 한글 주석 인코딩 깨짐 (CP949 → UTF-8 mojibake). 컴파일 영향 X, 정리 필요.
- filters 백업 파일 정리: `Client.vcxproj.filters.bak.2026-05-06`, `Engine.vcxproj.filters.bak.2026-05-06`. VS Solution Explorer 트리 확인 후 삭제.
- Client filters 추가 항목 매핑: 6D/6D-2 신규 4 파일 (`InGameRosterSpawner`, `InGameNetworkBridge`) + `GameSessionClient` + Lobby schema generated 파일 모두 적절한 Filter 자식 부착 (Stabilization-1 의 3 단계).

기타 누적:
- AnimUpdate 병렬화. Fiber 안정화 후 추가 +2~3ms 절감 기대.
- W5 Public DX11 헤더 제거 보류. `Engine/Public/RHI/DX11/` 9 파일 + `CDX11Device.h` 잔존. Public ID3D11* 노출 12 hit. W7 DX12 진입 전 정리 필요.
- Client → DX11 direct include leak 3건 정리 완료. `Scene_InGame.cpp` / `FxBeamSystem.cpp` / `FxSystem.cpp` 에서 해당 DX11 전용 include 제거. 내부 DX11 native bridge 제거는 Track 1 W6/W7 별도 축.
- 6 챔프 metallic/roughness 차별화. Yasuo/Sylas/Kalista/Garen/Zed/Riven 디폴트 동일.
- Track 1 W6 DX11 native bridge 제거. IRHI facade 통과만 함, 내부에서 native handle 추출.
- 셰이더 `register(... space0)` 명시 누락. 11 셰이더 전수 0건. DX12 / Vulkan SPIR-V 진입 전 필수.
- W4-W6 별도 빌드 산출물 부재. `WintersGame_Week4.exe ~ Week6.exe` 0개. 진입 전 빌드 통과 우선.

Architecture debt (Hygiene backlog, 즉시 작업 X):
- AI ↔ ECS 진성 순환 (`AI/{Blackboard, BehaviorTree, MCTS, RLBridge}.h` ↔ `ECS/Systems/{BehaviorTreeSystem, MCTSSystem}.h:5`). EntityID alias 분리로 끊기.
- GameInstance.h DLL 경계 4건. Sound/EntityBlueprint/ProfilerOverlay/CPUProfiler. codex 정정: 포인터 (`ProfilerOverlay/CPUProfiler`) = forward declare 가능, 값 전달 (`EntityBlueprint`) = 정의 필요. 제거 난이도 상이.
- Renderer/Resource → ECS (`MeshGroupVisibilityComponent`) 역참조. POD 분리.
- RHI/DX11 ↔ Renderer 양방향 (`BlendTypes/LightData`). RH-5 진입 전 RHI 중립 위치 이동.
- Shared → Engine.ECS 역의존 (deterministic 가정 위협). Sim-04a v2 진입 전 ECS POD 가드.

이전 누적 (보류):
- UI HP 팀색 바 PNG (`single_bar_blue/red.png` WIC 진단)
- 04a v2 D-0~D-3 sub-plan (TCP MVP)

---

## 2. Read First

```txt
1. .md/architecture/WINTERS_ENGINE_CONVENTIONS.md          네이밍 / DLL 경계 / 팩토리 / 타입 alias 확정 규칙
2. .md/process/PLAN_AUTHORING_PITFALLS.md                  계획서 박제 시 함정 + 8 단계 관문. 박제 진입 전 의무 통과
3. .md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md       11 섹션 / 150 챔프 타겟 / 7-레이어 구조
4. winters-skills/code/SKILL.md                            코드 작성 사이클
   winters-skills/code-scaffolding/SKILL.md                공통 함정
5. winters-skills/debug-pipeline/SKILL.md                  렌더/이펙트/스킬 버그 디버깅 (셰이더 우선 Read + 데이터 직접 계측)
6. 본 CLAUDE.md §5 (Critical Rules & Gotchas)              자주 발생하는 사고
7. 본 CLAUDE.md §8 문서 작성 스타일                         모든 .md 박제 시 의무 적용 (codex 패턴)
8. .md/roadmap/LOL_30DAY_MASTER_PLAN.md                    게임 콘텐츠 Phase 0~8
```

---

## 3. Repo Quick Map

```txt
Engine/                           WintersEngine.dll
  Public/{AssetFormat,Core,ECS,Editor,Framework,Manager,Platform,RHI,Renderer,Resource,Scene,Sound,AI,FX}
  Public/Core/{Fiber,JobSystem,Profiler}                    Fiber sub 신규 (Track 3)
  Private/                                                  대응 .cpp
    Private/RHI/DX11/                                       9 파일
    Private/RHI/DX12/                                       28 파일 (Track 2 Scaffold, Debug-DX12 config 한정)
  Include/                                                  공개 API (DLL 경계), 14 헤더
  ThirdPartyLib/{Assimp,DirectXTK,FMOD,D3D12MA}             D3D12MA 추가, ImGui 별도
  External/imgui/                                           ImGui 실위치 (.gitmodules 미등록 nested git, Stabilization-0 정리 대상)

Client/                           WintersGame.exe
  Private/{GameObject/Champion/{12체},GamePlay,Manager,Map,Network,Scene,UI}
    Champion 12체:
      Ezreal/Garen/Irelia/Kalista/Riven/Yasuo/Zed   (풀 박제)
      Annie/Ashe/Fiora/Jax                          (표준 3-file)
      Yone                                          (메쉬 그룹 가시성)
  Bin/Resource/{Shader,Font,Sound,Texture}                  PostBuild xcopy

Server/                           IOCP, GameRoom, ServerWorld, SnapshotBuilder
  Private/Network/{IOCPCore,Session,FrameParser}, Security/

Shared/                           deterministic sim + FlatBuffers schema
  GameSim/Components,Systems,Registry                       Health/Mana/SkillState/Move/Buff/Stat/...
  주의: Engine.ECS 역의존 (CoreComponents/GameplayComponents/TransformComponent), Architecture debt

Shaders/                          11 HLSL
  6 unlit:  Mesh3D / Skinned3D / Default3D / FxMesh / FxSprite / Triangle
  PBR:      Mesh3D_PBR / Skinned3D_PBR                      (Track 1 W2)
  Normal:   NormalOnly / SkinnedNormalOnly                  (Track 1 W4)
  SSAO/:    GTAO_CS / GTAO_Blur_CS                          (Track 1 W4)
  BRDF/:    BRDF_GGX.hlsli                                  (Track 1 W1)

Tools/WintersAssetConverter/      .wmesh / .wskel / .wanim 변환기 (Engine cpp 직컴파일, Assimp lib 링크)
Tools/DX12SmokeHost/              DX12 clear/present smoke host. 파일 존재, sln 등록은 미정
Services/                         Go 백엔드, auth/leaderboard/matchmaking/payment/profile/shop (Phase 0~7 통과)
EngineSDK/inc/                    Engine 공개 헤더 폴더 구조 유지 복사 (UpdateLib.bat 자동 동기화)
.md/                              architecture/plan/guide/roadmap/archive/process/TODO (5/7 기준 최근 RHI/Phase7/EffectTool 문서 추가)

Winters.sln                       4 config: Debug|x64, Debug-DX12|x64, Release|x64, Release-DX12|x64
                                  Debug-DX12 = Engine 만 진짜 DX12. Server/Client/Tools 는 Debug|x64 자동 매핑 (sln L34, L42, L50)
```

모듈 의존 방향 (Engine 필터 번호순, 낮은 번호는 높은 번호에 의존 X):

```txt
00.Manager (RHI Device/Pipeline/Shader/Buffer)
  → 01.Core (Timer/Input/Transform/JobSystem/Profiler)
  → 02.Structure (Framework/Entry)
  → 03.Renderer
  → 04.Editor (ImGui)
  → 05.ECS
  → 06.Resource
  → 07.Physics
  → 08.Audio
  → 09.Network
  → 10.JobSystem
  → 11.Scene
  → 12.Collision
  → 13.AI
```

향후 분리: `WintersLOL/` (Client+Server+Data MOBA 일체) / `WintersElden/` (액션 RPG, ER 모작).

---

## 4. Current Implemented State

### 4.1 렌더링 / RHI (DX11 + DX12 Scaffold)

CDX11Device (`Engine/Public/RHI/CDX11Device.h`):
- `ID3D11Device*` / `ID3D11DeviceContext*` 직접
- CGameInstance Tier-2 게터: `Get_RHIDevice() -> IRHIDevice*` 정식 (W5 통과)
- `Get_MeshShader / Pipeline / BlendStateCache` 등 legacy 게터 잔존 (`_Legacy` rename 보류)

CDX12Device (`Engine/Private/RHI/DX12/DX12Device.h`):
- `#if WINTERS_RHI_BACKEND_DX12` 가드
- IDXGIFactory6 + ID3D12Device + D3D12MA::Allocator + 8 메서드 (Initialize/CreateFactory/CreateAdapterAndDevice/CreateAllocator/CreateQueues/CreateFrameResources/CreateBackBufferViews/WaitIdle)
- `Engine/Private/RHI/DX12/` 28 파일 박제 (Track 2 W7-9 Scaffold)
- BeginFrame/EndFrame clear-present 코드 경로 있음. DX12 visual parity는 아직 아님.

RH-7 public surface/capability contract:
- `RHISurfaceDesc`, `RHICapabilities`, `PlatformNativeHandle`, `IPlatformWindow`, `IPlatformSurface` 추가
- `IRHIDevice::CreateSwapChain(const RHISurfaceDesc&)` shim 추가
- `CWin32Window`가 platform window/surface bridge 역할 시작
- EngineSDK/inc 동기화 파일 존재. 단 UpdateLib.bat Debug-DX12 배포 정책은 아직 고정 필요

DX12 smoke host:
- `Tools/DX12SmokeHost/main.cpp` 가 `EngineConfig::rhiBackend = DX12`로 `WintersRun` 호출
- `Tools/DX12SmokeHost/DX12SmokeHost.vcxproj` 존재
- `Winters.sln`에는 아직 프로젝트 미등록. 다음 작업에서 등록 또는 독립 build 명령 고정 필요

공유 셰이더/파이프라인 (`CEngineApp.h`): Mesh3D / Skinned3D / FxMesh / FxSprite, 엔진이 소유.

셰이더 11개:
- 6 unlit: Mesh3D / Skinned3D / Default3D / FxMesh / FxSprite / Triangle
- PBR: Mesh3D_PBR / Skinned3D_PBR (Track 1 W2)
- Normal: NormalOnly / SkinnedNormalOnly (Track 1 W4)
- SSAO/: GTAO_CS / GTAO_Blur_CS (Track 1 W4)
- 11개 전수 `register(... space0)` 명시 누락 (DX12 SM 5.1+ / Vulkan SPIR-V 진입 전 필수)

Renderer 6종: ModelRenderer / CubeRenderer / PlaneRenderer / FxStaticMeshRenderer / TriangleRenderer + FogOfWarRenderer (Track 4 B-13). Immediate-mode (RenderGraph 0).

Track 1 PBR 인프라: `Shaders/BRDF/BRDF_GGX.hlsli`, `Mesh3D_PBR.hlsl` cbuffer b3 PerMaterial, `CMaterialPBR` (`Renderer/CMaterialPBR.h`), `NormalPass` / `SSAOPass` (`Renderer/`).

IRHI 8 인터페이스 (codex 정정): IRHIDevice + IRHIBindGroup + IRHIBindGroupLayout + IRHICommandList + IRHIPipelineState + IRHIQueue + IRHIRenderPass + IRHISwapChain.

### 4.2 ECS / 게임플레이

Engine/Public/ECS: TransformComponent, GameplayComponents, NavAgentComponent, FxMeshComponent + ComponentStore + CWorld + ISystem + CSystemSchedular (현재 명칭, `Schedular` 오탈자 그대로, 정식 rename 별도 refactor 보류).

Phase B-7a (ModelRenderer 분해) 미진입. ModelRenderer 여전히 monolithic.

`Scene_InGame.cpp` 비대 축소 진행 (2026-05-07 기준 1185줄, S1 smoke telemetry 포함). 7E Render/PlayerControl/CombatInput/Debug/ChampionState/Lifecycle/SkillDispatch/PlayerTransform/Bootstrap bridge 분리와 7F local runtime cleanup 완료. 다음 큰 축은 직접 화면 smoke freeze와 남은 장기 architecture debt.

7E bridge 완료 파일:

```txt
Client/Public/Scene/InGameRosterSpawner.h
Client/Private/Scene/InGameRosterSpawner.cpp
Client/Public/Scene/InGameNetworkBridge.h
Client/Private/Scene/InGameNetworkBridge.cpp
Client/Public/Scene/InGameBootstrapBridge.h
Client/Private/Scene/InGameBootstrapBridge.cpp
Client/Public/Scene/InGameRenderBridge.h
Client/Private/Scene/InGameRenderBridge.cpp
Client/Public/Scene/InGamePlayerControlBridge.h
Client/Private/Scene/InGamePlayerControlBridge.cpp
Client/Public/Scene/InGameCombatInputBridge.h
Client/Private/Scene/InGameCombatInputBridge.cpp
Client/Public/Scene/InGameDebugBridge.h
Client/Private/Scene/InGameDebugBridge.cpp
Client/Public/Scene/InGameChampionStateBridge.h
Client/Private/Scene/InGameChampionStateBridge.cpp
Client/Public/Scene/InGameLifecycleBridge.h
Client/Private/Scene/InGameLifecycleBridge.cpp
Client/Public/Scene/InGameSkillDispatchBridge.h
Client/Private/Scene/InGameSkillDispatchBridge.cpp
Client/Public/Scene/InGamePlayerTransformBridge.h
Client/Private/Scene/InGamePlayerTransformBridge.cpp
```

BanPick/InGame champion identity:
- `CChampionCatalog`가 selectable/playable/bot champion read model 담당
- `CChampionSpawnService`가 roster/snapshot 신규 엔티티 스폰 경로 담당
- `CChampionModuleBootstrap`가 12 champion `KeepAlive` 호출을 단일화
- `Scene_InGame::GetPlayerChampionId()`는 player entity의 `ChampionComponent`를 우선 읽음

챔피언 12체 (`Client/Private/GameObject/Champion/`):

```txt
풀 박제 7체 (1~3 cpp 부가):
  Ezreal / Garen / Irelia / Kalista / Riven / Yasuo / Zed
표준 3-file 4체 (FxPresets + Skills + Registration):
  Annie / Ashe / Fiora / Jax
메쉬 그룹 가시성 1체 (Skills + Registration + Yone_MeshGroups + MeshGroupVisibilityComponent, FxPresets 없음):
  Yone (B-12 메쉬 분리 패턴)
```

B-13 시스템 박제됨 (Track 4): SpatialHash / Vision / TurretAI / BehaviorTree / MCTSPlanner. 회귀 검증 대기.

MeshGroupVisibilityComponent (W4 도입): `Renderer/ModelRenderer.h:5`, `Resource/Model.h:10` 가 ECS 컴포넌트 직접 의존 (역참조 = Architecture debt).

### 4.3 SharedSim / 서버 (작동하는 뼈대 존재)

Shared/GameSim:
- Components (Health/Mana/SkillState/Stat/MoveTarget/Buff)
- Systems (Move/SkillCooldown/DamageQueue/Death/Buff/Stat)
- GameplayHookRegistry

Engine.ECS 역의존: HealthComponent / ChampionComponent / ICommandExecutor / DeterministicEntityIterator 가 `ECS/Components/{CoreComponents, GameplayComponents, TransformComponent}.h`, `ECS/Entity.h`, `ECS/World.h` 직접 include. deterministic stand-alone 가정 위협.

Server/GameRoom.cpp: 30Hz tick + Phase_DrainCommands / Phase_ExecuteCommands / Phase_SimulationSystems / Phase_BroadcastSnapshot.

Transport boundary 통과: `Server/Private/{Game, Security}`, `Shared/` 모두 WSARecv/WSASend/recvfrom/sendto 0 hit. Session.cpp IOCP 격리.

BanPick room sync:
- `Shared/Schemas/LobbyTypes.fbs`, `LobbyState.fbs`, `LobbyCommand.fbs` 추가
- C++/Go generated lobby schema 파일 생성됨
- `CGameSessionClient`가 LobbyState/Hello/GameStart를 `GameContext`에 반영
- `CGameRoom`이 JoinSlot/LeaveSlot/PickChampion/SetBot/StartGame 처리 + LobbyState/GameStart/Hello broadcast
- 런타임 3-client hidden smoke는 `--banpick-smoke` 빠른 경로로 local binding/snapshot/Q skill hook까지 통과. 1-client `--smoke-full-map` map init/InGameMap telemetry 통과. full visual smoke는 잔여.

04a v2 sub-plan 박제 완료 (D-0~D-3). 현재는 BanPick/InGame smoke 이후 재개 권장.

### 4.4 자산 파이프라인

`.wmesh / .wskel / .wanim` 자체 포맷 (`Engine/Public/AssetFormat/`): Stage 3 박제 완료 (2026-04-28). 6 챔프 fast-path.

CModel::LoadModel: `.wmesh` fast-path / `.wmesh+.wskel` skinned fast-path / 실패 시 Assimp fallback.

WintersAssetConverter.exe: 31개 (구 27개 + Annie/Ashe/Fiora/Jax) FBX/GLB → `.wmesh` 검증. Irelia 60MB → 1.2MB = 50배 압축.

### 4.5 Go 백엔드 (Phase 0~7 완료, Phase 8 진행 중)

```txt
Phase 0    인프라 (Docker+pkg)                                                       완료
Phase 1~6  Auth / Leaderboard / Matchmaking / Profile / Payment / Shop  port 8081~8086  완료
Phase 7    Kafka E2E (MatchCompleted, SkinPurchased)                                완료
Phase 8    C++ Client SDK (WinHTTP)                                                 진행 중 (Client/Private/Network/Backend/ 박제 시작, winhttp.h include)
```

상세: `.md/plan/backend/00_BACKEND_PLAN_INDEX.md`.

### 4.6 JobSystem

CJobSystem (`Engine/Public/Core/JobSystem.h`):
- Worker N (hw_concurrency-2) + 각 Worker 가 CWorkStealingDeque 1개 소유
- Global queue (Main/외부/overflow fallback)

EnqueueJob race 1차 수정 (`JobSystem.cpp:122-152`): Worker 는 자기 Deque (Chase-Lev owner), Main/외부/overflow 는 Global Queue (lock).

eJobExecutionMode (`FiberTypes.h:5`): ThreadOnly / FiberShell. SetExecutionMode 로 런타임 전환.

FiberShell M0 박제 (`JobSystem.cpp:160-176, 253-284`):
- WorkerLoop ConvertThreadToFiber
- TryExecuteItemOnFiber 가 매 job CreateFiber/SwitchToFiber/DeleteFiber
- FiberShellEntry
- CFiberPool 미사용 + WaitForCounter Yield 없음 (Track 3 안정화 대기)

외부 사용: `InGameBootstrapBridge.cpp`에서 TransformSystem / NavigationSystem / MinionAISystem 모두 `Set_JobSystem(pJS)` 활성.

---

## 5. Critical Rules & Gotchas

평면 60+ 갓차를 6 카테고리로 압축. 4월 이전 누적 사고는 `.md/archive/SESSION_LOG_2026-04.md` 참조.

### 5.1 빌드 / 툴체인

- vcxproj `/utf-8` 필수. 한글 주석 CP949 해석 시 C4819 + 가짜 파서 오류 (C1075 '{').
- `.bat` 파일은 ASCII 전용. cmd.exe 가 ANSI (CP949) 로 읽음. UTF-8 한글 주석 = for 루프/xcopy 부분 실패. `chcp 65001` 도 첫 줄 이전엔 미적용.
- vc143.pdb lock. VS (`devenv.exe`) 켜져 있으면 cl.exe 가 pdb 잡힘 → C1041. 빌드 전 VS 종료 또는 `/p:MultiProcessorCompilation=false /maxcpucount:1`. mspdbsrv leak 시 kill.
- Engine 단독 빌드 → EngineSDK/inc 자동 동기화. `Engine.vcxproj` PostBuild Event. Client 빌드 전 Engine 1회 필수.
- EngineSDK/inc 직접 수정 금지 (위험). `xcopy /Y /D` 단방향 복사. Engine 원본 touch 시 SDK 수정 유실. 항상 Engine/Include + Engine/Public 원본 수정 후 UpdateLib.bat. F12 점프 시 SDK 경로 가능성. 탭 제목 + Solution Explorer 로 실제 경로 확인.
- Server.vcxproj 전제 (Sim-10 v2 M1): `<FloatingPointModel>Precise</FloatingPointModel>` (Debug+Release) + `Mswsock.lib` + Engine project reference.
- Transport boundary. `Server/Game/`, `Server/Security/`, `Shared/` 에서 `WSARecv` / `WSASend` / `recvfrom` / `sendto` 0 hit 강제 (grep). UDP 마이그 안전성 핵심.

### 5.2 코드 안전 (DLL / 타입 / Include)

- 공개 헤더 `std::` 명시 필수. Engine/Public + Engine/Include 의 `.h` 는 `using namespace std` 가정 X. Client TU 파싱 실패. 공개 헤더는 `std::vector` / `std::function`.
- 공개 헤더 `bool_t` / `i32_t` alias 사용 금지. `bool_t` 는 `namespace Engine` 안에만 존재. namespace 밖 공개 API 는 `bool` / `int32_t` 또는 `WintersTypes.h` 의 `f32_t` / `f64_t` (global alias).
- EngineSDK/inc 는 폴더 구조 유지 (`xcopy /S` 단방향 복사). 서브디렉토리 헤더는 항상 폴더 경로 포함 (`"ECS/Entity.h"` / `"Resource/Texture.h"` / `"RHI/CDX11Device.h"`). Engine 빌드만 통과하고 Client unqualified include 사용 시 C1083 실패. flat include 는 `Engine/Include` 루트 헤더 (`WintersAPI.h` / `WintersTypes.h` / `WintersMath.h` 등) 에만 허용.
- ComPtr FQN 필수 (SDK 노출 헤더). `#include <wrl/client.h>` + `Microsoft::WRL::ComPtr<...>` 완전 수식. `WintersPCH.h` 의 `using` alias 의존 시 Client C7568. 참고: `RHI/CDX11Device.h`, `RHI/DX11/BlendStateCache.h`.
- `WINTERS_ENGINE` dllexport 클래스 + `unique_ptr` 멤버. copy ctor / assign 명시 `= delete` 필수. 누락 시 MSVC `construct_at` SFINAE 실패 → vector::vector 무관 컴파일 에러 체인. 선례: `CWorld.h`, `CSystemSchedular.h`.
- `imgui.h` include + `_DEBUG` 빌드 + `DBG_NEW`. `Engine_Defines.h:41` `#define new DBG_NEW` 가 imgui placement new 와 충돌. include 를 `#ifdef _DEBUG / #undef new / ... / #define new DBG_NEW / #endif` 로 감싸기.

### 5.3 런타임 안전 (수명 / 이스케이프)

- `Scene_Manager::Change_Scene` 즉시 self-destruct (위험). 호출 직후 이전 Scene unique_ptr 가 그 자리에서 소멸. Scene 메서드 (OnUpdate/OnImGui) 안에서 `Change_Scene` 호출 후 코드 계속 실행 = use-after-free. 반드시 즉시 `return`. BanPick 류 ImGui 분기에서 `Change_Scene` → `ImGui::End()` → `return;` 3 줄 패턴 강제.
- BanPick → InGame roster 바인딩은 sessionId/netId 우선 (위험, 2026-05-06). 서버 lobby roster 를 InGame 에서 소비할 때 `slotId` 는 좌석 좌표일 뿐 로컬 플레이어 identity 가 아니다. `sessionId`/`netId` 가 있는데도 `(sessionId match) || (slotId match)` 로 판정하면 `MySlotId` stale/default 0 때문에 모든 클라이언트가 호스트 슬롯 (예: Yone) 으로 바인딩될 수 있다. 규칙: `MySessionId != 0` 이면 sessionId 만, 그 다음 netId, 둘 다 없을 때만 local fallback slotId. 로컬 슬롯 못 찾으면 first-human fallback 금지.
- `m_pActiveSkillDef` 댕글링 (Buffer Too Small 크래시). stage2 분기 `SkillDef s2 = *def;` 로컬 후 `&def` 저장 = 스택 해제. 해결: `SkillDef m_ActiveSkillDefStorage{}` 멤버 + 값 복사 후 포인터 저장.
- Wide string `\U` / `\u` / `\W` 이스케이프 충돌 (위험). `L"C:\Users\..."` 의 `\U` = universal-character-name 규칙 (8 hex). C4129/C4566 + 경로 깨짐. forward slash (`L"Client/Bin/Resource/..."`) 또는 raw literal (`LR"(C:\Users\...)"`). Windows API 모두 `/` 허용. CWD = `LocalDebuggerWorkingDirectory=$(SolutionDir)`.
- F12 = VS "Break into Debugger" 단축키. 게임 키 바인딩 사용 시 디버깅 중 ntdll break (힙 손상으로 오진 가능). M / Tab / F1~F11 사용.
- PlayAnimationByName 매칭 실패 silent. Model.cpp `FindAnimationIndex` -1 반환 시 조용히 early-return. 증상: "락 타이머 정상인데 애니만 idle/run 유지". L244 에 `if (idx<0) OutputDebugStringA(...)` 1줄 추가 권장. 이렐리아 W `spell2_1`→`spell2` 사고.
- 봇 챔프도 OnEnter idle 애니 명시 기동 필수. PlayAnimation 호출 전 `m_bPlaying=false` → 본 매트릭스 identity → bind pose 렌더. 무기 submesh 가진 챔프 (Kalista/Viego 등) 는 무기가 손 본 로컬 좌표 떠 있어 "안 보이는 것처럼".

### 5.4 데이터 / 포맷

- `.wmesh` skinned 정점 76B layout (위험). `WMeshFormat::VertexSkinned` POD 의 byte offset 을 셰이더 IL `D3D11_INPUT_ELEMENT_DESC AlignedByteOffset` 와 byte 단위 일치. 확정: pos[3] 0~11 / nrm[3] 12~23 / uv[2] 24~31 / tan[3] 32~43 / indices[4] 44~59 (uint32×4) / weights[4] 60~75. 임의 추가 필드 = GPU 가 다음 필드 byte 밀어 잘못 읽음 → 정점 NaN/0 collapse, 메시 사라짐 (애니/Transform 정상 = 진단 어려움). 가렌 사고 (2026-04-28).
- Assimp aiMatrix4x4 → XMFLOAT4X4 전치 필수. Assimp = column-vector / DX = row-vector. `ConvertMatrix()` 전치 누락 시 본 오프셋/Rest Pose/GlobalInverseRoot 전부 틀려 스키닝 폭발.
- Assimp 스키닝 표준: `Final = Offset × GlobalTransform × GlobalInverseRoot` (DX row-major). GlobalInverseRoot 누락 시 루트 노드 트랜스폼만큼 메시 틀어짐. 채널 없는 본 = Identity 가 아닌 Rest Pose 초기화.
- 스켈레톤 모델 stride 혼재 금지. 스켈레톤 있으면 본 없는 서브메시도 VTXANIM (76B). VTXMESH (44B) 와 섞이면 skinned IL 이 44B 정점 잘못 읽어 폭발. 본 없는 메시는 `weight[0]=1.0, index[0]=0`.
- LoL FBX 머티리얼 텍스처 경로 없을 수 있음. Diffuse 경로 없어 `LoadTextures()` 무동작. `SetOverrideTexture()` 대신 `LoadMeshTexture(meshIndex, path)` 수동.
- Blender FBX Export = glTF PBR 텍스처 경로 기록 불가. glTF/glb → Blender Import → FBX Export 시 텍스처 참조 0개. 해결: glb 를 Assimp 직접 로드 + `pScene->GetEmbeddedTexture()`.
- lol2gltf 1.0.0 `--flipX` silent ignore. `mapgeo2gltf` `-x false` 4 가지 형식 모두 무시 (CLI parser 버그, md5 검증 확정). 코드에서 맵 transform `SetScale({-0.01f, 0.01f, 0.01f})` X 미러 우회 + Mesh3D 가 `D3D11_CULL_NONE` 라 winding 안전.
- 변환 도구 결과 검증 필수. 옵션 silent ignore 가능. md5 / 정점 분포 / 시각 확인 후 다음 단계 진입.
- 바이너리 POD 포맷 변경 시 VERSION bump + 기존 .dat 삭제. 필드 추가 시 `sizeof` 변경 → 이전 포맷 호환 X. `STAGE_VERSION` 증가 + `static_assert(sizeof()==기대값)` + 기존 `.dat` 삭제. 버전 불일치 = count 쓰레기 → 거대 할당 → 힙 손상.

### 5.5 셰이더 / 렌더

- HLSL `row_major` 필수. DirectXMath = row-major / HLSL 기본 column-major. cbuffer 행렬에 `row_major matrix` / `row_major float4x4` 필수. 누락 = 카메라 이동 따른 비대칭 클리핑/왜곡.
- mul 순서. `mul(vector, matrix)` (행 벡터 × 행렬).
- 셰이더 수정 후 OutDir 강제 동기화 필수 (Phase FX-8). `D3DCompileFromFile` 가 OutDir (`Client/Bin/Debug/Shaders/`) 읽음. MSBuild incremental 이 .hlsl 변경 미감지 → PostBuild xcopy skip → 옛 .hlsl 그대로. 신호: Output 창 셰이더 컴파일 에러 메시지에 `Bin/Debug/Shaders/` 경로. 해결: `cp Shaders/*.hlsl Client/Bin/Debug/Shaders/` 직접 또는 Rebuild Solution 또는 PostBuild xcopy 에 `/Y` 강제.
- LoL FX FBX 의 `render/*.png` 는 mesh diffuse 아님. LoL 클라이언트 이펙트 카메라 캡처 스프라이트. FBX UV 가 알파 0 영역 가리켜 `clip(texColor.a-0.05f)` 가 모든 픽셀 버림 → 화면 출력 0 픽셀 ("스폰 안 됨" 처럼). CPU 디버거로 못 잡음. 해결: 진짜 머티리얼 텍스처 (`irelia_base_blades_passive_4_texture.png` 형태) 를 `SetMeshTexture` 바인딩. `render/*.png` 는 빌보드 (FxBillboardComponent) 전용.
- Additive blend + `vColor` RGB >= 1.0 = 흰색 포화. PNG 흰색 위주 (LoL FX glow_color: RGB=1, alpha 그라데이션) + tint G/B 1.0 초과 → clamp → 0.7/1.0/1.0. Additive `Src + Dst` alpha 무시 + 픽셀 중첩 → clamp 가속 → 흰색. 해결: vColor RGB <= 1.0 또는 AlphaBlend (글로우 약함).
- CPlaneRenderer 기본 CULL_BACK → 지면 퀘드 특정 각도 컬링. DX11 기본 RS 뒷면 컬링. `bBillboard=false` 지면 퀘드 + `fYaw` 회전 = 뒷면 노출 → 사라짐. 해결: `D3D11_CULL_NONE` TwoSided RS 백업/바인딩/복원.
- 모델 yaw `+XM_PI` 보정. 모델 뒷면=정면 컨벤션. `atan2(forward.x, forward.z) + XM_PI` 의 결과로 월드 forward 계산 시 부호 뒤집기 `Vec3 fwd{ -sinf(yaw), 0, -cosf(yaw) }`. 안 뒤집으면 마우스 반대 방향 버그. FX 스폰 (mis_spear/W stage2/R 투사체/dash 트레일) 모두 동일 부호. 90° (XM_PI/2) 와 180° (XM_PI) 헷갈리지 말 것.

### 5.6 게임 로직 / 스킬

- `SkillDef.lockDurationSec` 와 FBX 애니 길이 매칭 (위험). `m_fLastActionTimer = lockDurationSec` 0 도달 시 자동 idle/run 강제 전환. lockDurationSec < 애니 길이 = 중간 커트. AS=1.0 정속에서만 발현 (AS 올리면 우연히 우회 → 디버깅 어려움). 해결: SkillTable lockDurationSec = FBX 원본 길이. 애니별 배속은 `SkillDef.animPlaySpeed` 독립 필드. 이렐리아 실측: 평타=1.0 / Q=0.5 / W1=4.0 / E=1.0 / R=5.0. 부등식: `lockDuration × animPlaySpeed >= recoveryFrame / FBX_FPS` (위배 시 recoveryFrame hook 미발동).
- castFrame 감지 블록 분리 금지. 여러 블록 두면 첫 블록 끝 `m_fActivePrevFrame=curF` 로 두 번째 블록 `HasFramePassed(castFrame, prevFrame)` 항상 false. 단일 블록에 `bCastHit/bRecoveryHit` 플래그 캐싱 + 모든 반응 후 맨 마지막에 prevFrame 갱신.
- 애니 전환 시 castFrame 재발동. `PlayAnimationByName` 이 `m_dCurrentTime=0` 리셋 → 같은 castFrame 여러 애니 반복 통과. `m_bCastFrameFired` / `m_bRecoveryFrameFired` bool 멤버 + ApplyLocalPrediction 새 스킬 시 리셋. 1회만 보장.
- Pathfinder empty path silent fail 금지. NavGrid unwalkable 셀 + 도달 불가 = 빈 vector. `bHasGoal=false; vel.fSpeed=0` 만 처리하면 Chase/Pursuit 제자리 stuck. 추적 의도 분기에서 `target - selfPos` 직선 fallback + `bPathDirty=true` 다음 프레임 재시도.
- ECS Phase 순서 = data dependency. 같은 phase 내 시스템은 JobSystem Submit 으로 병렬 (race). Producer phase < Consumer phase 필수. AI(1) → Nav(2) 순서 지키지 않으면 첫 Chase 프레임 stale velocity. swap/추가 시 Producer/Consumer 의존 그래프 + 같은 phase race 검증.
- enum class 이름 충돌 across namespace. `Winters::Map::eTeam` (u32_t) vs `Engine::eTeam` (uint8_t) 동일 이름 다른 namespace 공존 = `using namespace` 만으로 해결 X. `Winters::Map::eTeam::Blue` fully qualify.
- `CInput` 마우스 LButton 없음. `m_Keys[256]` WndProc 에서 키보드만 기록. `IsKeyPressed(VK_LBUTTON)` 영원히 false. `GetAsyncKeyState(VK_LBUTTON) & 0x8000` 직접 + 멤버 bool 이전 상태 보관.

### 5.7 계획서 박제 함정 (2026-05-04 신규, 누적 사례)

`.md/plan/**/*.md` 박제 시 사고 19종. 상세 + 8 단계 관문 (A~H): `.md/process/PLAN_AUTHORING_PITFALLS.md`. 박제 진입 전 의무 통과. 누적 사례: 13_YONE v1 P1×2 + P2×2 / B-13 마스터 + sub P1×5 + P2×5.

- PIMPL 추측 의사코드 박제 금지 (P-2, 위험). PIMPL 클래스 신규 메서드 박제 시 헤더만 보고 내부 멤버/헬퍼 추측 금지. `.cpp` 의 `struct Impl { ... };` 정의 라인 Read 후 본문 인용 + 호출 helper 를 grep 으로 실재 검증. 사례: 13_YONE v1 §3.5.
- 신규 mask/visibility 추가 시 모든 render path 동시 박제 (P-3, 위험). `Render()` (main) + `RenderNormalPass(...)` (PBR G-Buffer) + shadow/depth 다중. 한 pass 만 박제 시 hidden submesh 가 SSAO/depth 에 ghost. `grep -rn "pRenderer->Render"` 호출자 식별 + 동시 박제. 사례: 13_YONE v1.
- `extern` 함수 / `static_cast<Scene_X*>` Scene 직접 의존 금지 (P-4, 위험). Champion skill 이 Scene 직접 호출 시 서버/멀티 Scene/엘든링 깨짐. ECS request 패턴: `XxxRequestComponent` 1-frame 부착 → 별도 system 다음 phase 소비. 사례: 13_YONE v1 vs v2.
- bitmask 폭은 본 계획서 자체 거론 미래 사례 + 50% (P-7). `u32_t mask = 32` 한도. 엘든링 보스 50~100 submesh 거론하면서 32-bit 박제 자기 모순. `std::array<u64_t, 4>` = 256 bit 또는 sparse. 사례: 13_YONE v1 §3.1 vs §6.
- §1 사전 결정 미박제 강제 (P-1+P-6). "필요" / "추정" / "TBD" 가 §1 표에 남으면 §3 본문 진입 금지. 데이터 결정이 변환/실행 결과 의존이면 박제 범위를 그 직전까지 한정. 사례: 13_YONE v1 §1.
- 인용 의미 반전 (P-8, 위험). 다른 .md / .h L## 인용 시 부사어 (`...의 룰`, `...준수`) 만 박제 + 직접 인용 블록 없으면 의미 정반대로 박제 가능. 사례: B-13 v1 00 마스터 §3 의 "CLAUDE.md L795 의 'flat 구조' 룰" 인용. 실제 AGENTS.md L437 은 "서브디렉토리 헤더는 반드시 폴더 경로 포함" 정반대. v1 그대로 박제 시 Client TU C1083. 회피: L## 인용 옆에 그 줄 직접 인용 블록 박제 + 의미 일치 검증.
- ECS Scheduler 동시성 모델 가정 (P-9, 위험). `ISystem::GetPhase()` `uint32_t`. 0.5/1.5 fractional 박제는 0/1 절삭 (의미 무효). 같은 phase 시스템 2개 이상 = JobSystem 병렬 (`Engine/Private/ECS/SystemScheduler.cpp:22-42` 등록 순서 무관). Producer→Consumer 의존 시스템은 다른 정수 phase. 같은 phase 는 read+write 0 인 시스템만 (write set 다른 시스템 묶기는 OK). 사례: B-13 v1 SpatialHash=0.5/TurretAI=1.5.
- Owner Scope 매트릭스 미결정 (P-10, 위험). Spatial/Vision/NavGrid 같은 World-단위 인덱스를 `CGameInstance` Tier-2 + `CEngineApp` 전역으로 박제 시 서버 멀티 GameRoom (Sim-10 v2) / 멀티 Scene / 엘든링 멀티 World 깨짐. 권장 매트릭스: Spatial/Vision/NavGrid → `CWorld` owned, RHI Device → `CEngineApp` 전역, Sound/Input/Resource → `CGameInstance` Tier-1, Scene-한정 → Scene 멤버. 사례: B-13 v1 04 §4.
- 도메인 상수 Engine 박제 (P-11). `CSpatialIndex::CELL_SIZE = 8.f` 같이 한 게임 도메인 값을 Engine 공용 클래스 `static constexpr` 로 박제 시 다른 게임/맵 재사용 불가. InitDesc 주입 (`SpatialGridDesc { worldOrigin, cellSize, halfExtentX, halfExtentZ }`) 또는 게임-specific 클래스 분리.
- 음수 좌표 정수 truncation (P-12, 위험). 맵 원점 중심 좌표 (LoL -140~+140) 의 negative quadrant 에서 `static_cast<int32_t>(world / cell)` 사용 시 0-방향 절삭으로 셀 경계 1 cell 어긋남. -0.5/8 → cast 0 (실제로는 -1 cell). `std::floor((world - origin) / cellSize)` 사용. 단위 테스트: cell(-0.5)==-1 / cell(-7.9)==-1 / cell(-8.0)==-1 / cell(-8.1)==-2.
- 미존재 API 호출 (P-13, 위험). `m_SystemScheduler.GetSystem<T>()` 같이 헤더에 없는 API 박제. grep 으로 모든 API 헤더 실재 검증 강제. 우회: (a) raw 포인터 캐시, (b) ECS event/request 컴포넌트 (1-frame `XxxNotifyComponent` + 다음 phase System ForEach 소비). 사례: B-13 v1 03 → v2 `TowerAggroNotifyComponent`.
- 행동 정책 무의식적 변경 (P-14, 위험). 성능 최적화 박제 시 자료 구조/검색 mask 슬며시 확장 → 게임 행동 변경. 사례: B-13 v1 04 의 MinionAI mask = `Minion+Champion+Turret+JungleMob+Inhibitor+Nexus` 6종. 현재 enemy minion only 라인전 동작이 변경. 회피: 최적화는 행동 보존. 도메인 확장은 별도 PR.
- 헤더 외부 의존 미include (P-15, 위험). 신규 컴포넌트 헤더가 다른 헤더의 enum/struct 사용 시 그 의존 헤더 직접 include. TU 가 두 헤더 같이 include 가정 = fragile. 사례: B-13 v1 02 `VisionComponents.h` 의 `WardComponent::ownerTeam = eTeam`. `GameplayComponents.h` 미include. 값 멤버는 forward declare 불가.
- 산술 검증 누락 (P-16, 신규). `u32_t mask = 32` 같은 박제 시 2^N 명시 안 함. 캐스트/시프트 경계 명시 필요. 회피: `static_assert(sizeof(mask) * 8 == 32)` + 2^N 단위 테스트.
- Typedef 일괄 변경 = ABI 폭발 (P-17, 신규). `using EntityID = u32_t` → `u64_t` 변경 시 30+ 헤더 + DLL ABI 깨짐. 회피: 타입 변경 별도 PR + grep 영향 범위 사전 확인 + dllexport 클래스 멤버 영향.
- RHI/Engine 인프라 미인지 (P-18, 신규). `RHIHandle` 같은 기존 인프라 모르고 신규 `XxxResourceID` 박제. 회피: `Engine/Public/RHI/RHIHandles.h` + `CRHIResourceTable.h` 먼저 read.
- Render/Simulation 결합 (P-19, 신규). Render Pass 가 ECS Query 직접 호출 → 시뮬 변경 시 렌더 깨짐 + 멀티스레드 안전성 깨짐. 회피: Render Pass = read-only snapshot 받기, Sim 갱신은 별도 phase.

더 많은 갓차 (Phase B-7 / D / 가렌-제드 누적): `.md/archive/SESSION_LOG_2026-04.md`. 5월 누적: `.md/archive/SESSION_LOG_2026-05.md`.

---

## 6. Minimal Conventions

풀 컨벤션: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`. 본 절은 자주 어기는 항목만.

### 6.1 네이밍

- 클래스: `C` 접두사 (`CTimer`)
- struct (POD): C 접두사 금지 (`TransformComponent`)
- 인터페이스: `I` 접두사 (`IBuffer`, `ISystem`, `IScene`)
- 파일명: C/I 접두사 없음 (`Transform.h` → `CTransform`, `IBuffer.h` → `IBuffer`). 기존 C 접두사 파일은 점진 리네임.
- enum class: `e<Domain>` 접두사 (`eRHIFormat`)

### 6.2 타입 별칭 (신규 코드 강제)

- 사용: `f32_t / f64_t / i32_t / u32_t / u8_t / u16_t / u64_t / i8_t / i64_t / bool_t / wstring_t / tchar_t`
- 금지: raw `float / int / unsigned int`, legacy `float32 / int32 / uint32`
- 예외: Win32 (HWND/DWORD/UINT), DirectXMath (XMVECTOR/XMFLOAT3), 서드파티 (Assimp/FMOD)
- 공개 API 헤더 (`WintersMath.h` / `WintersTypes.h` / `EngineConfig.h` / `IWintersApp.h` / `WintersPaths.h`): Phase 4 일괄 정리 예정, 신규 추가만 컨벤션 적용.

### 6.3 클래스 설계

- 생성자 private. 외부 `new` 금지. `Create()` 정적 팩토리만
- 소멸자 public virtual. 스마트 포인터 수명
- 멤버 변수 private. public 멤버 함수로만 접근
- 변수 조작 = 해당 변수 선언 클래스 함수 내부에서만
- Dirty Flag, RAII, cbuffer 16바이트 정렬

```cpp
class CExample {
public:
    ~CExample() = default;
    static unique_ptr<CExample> Create() {
        auto p = unique_ptr<CExample>(new CExample());
        return p;
    }
private:
    CExample() = default;
    int m_Value = 0;
};
```

`make_unique` 사용 범위: private ctor 가 있는 Create 팩토리 안에서만 `unique_ptr<T>(new T())` 직접. 그 외 일반 코드는 `make_unique` OK.

### 6.4 멤버 변수 접두사

- `m_f` float / `m_v` Vec3 / `m_b` bool / `m_p` pointer / `m_` 기타 (matrix 등)

### 6.5 GameInstance 경계 (Tier 1/2)

- Tier 1 (저빈도, `CGameInstance::Get()->Method()` 포워딩): Timer / Input / Scene / Resource 요청 / Sound 고수준 / Network 세션
- Tier 2 (고빈도 핫패스, `CGameInstance::Get_JobSystem() -> IJobSystem*` 인터페이스 Getter, Client 포인터 캐시 후 직접): JobSystem / ECS World / RHI draw / Physics step
- DLL export: 신규 export `CGameInstance` 만. 내부 매니저 `WINTERS_API` 마크 금지
- `DECLARE_SINGLETON` 호출: Winters 는 포인터 반환. `CGameInstance::Get()->Method()` (`->` 사용)

Tier-2 RHI 포워딩 게터 (현재 = W5/W6 시점):
- `CGameInstance` 가 `Get_RHIDevice() -> IRHIDevice*` 정식 (W5 통과). `Get_MeshShader / Pipeline / BlendStateCache` 등 legacy 게터 잔존 (`_Legacy` rename 보류).
- RH-2 종료 시점에 `_Legacy` 제거 예정. Track 2 W7-9 DX12 진입 시 동시 정리.
- Client 가 반환 포인터 역참조 시에만 해당 헤더 include.

DLL 경계 위반 (Architecture debt, Hygiene follow-up). `Engine/Include/GameInstance.h` 가 4 개 내부 헤더 직접 include. Tier-1/2 forwarding 의도 모순. codex 정정으로 제거 난이도 상이:

```txt
Manager/Profiler/ProfilerOverlay.h     포인터 멤버      forward declare 가능
Core/Profiler/CPUProfiler.h            포인터 멤버      forward declare 가능
Sound/SoundChannel.h                   enum class       enum forward (enum class eSoundChannel : u8;) 별도 검토
ECS/Systems/EntityBlueprint.h          값 전달          정의 필요, 시그니처 변경 동반
```

Stabilization-0 후 별도 Hygiene PR. 즉시 작업 X.

### 6.6 Include / ComPtr

- Client 헤더: `#include "Defines.h"` (STL + Engine 타입 해결)
- Engine 헤더: `WintersAPI.h` + `WintersTypes.h` + `WintersMath.h` (Engine_Defines.h 미포함)
- 서브디렉토리 헤더는 폴더 경로 포함: `"ECS/Entity.h"` (unqualified 금지)
- ComPtr (SDK 노출 헤더): `#include <wrl/client.h>` + `Microsoft::WRL::ComPtr<...>` FQN 필수

### 6.7 HLSL

- cbuffer 행렬 `row_major` 필수
- mul: `mul(vector, matrix)` (행 벡터 × 행렬)
- 레지스터 슬롯 명시: `register(b0)` `register(t0)` `register(s0)` (DXC SPIR-V 호환, RH-1 이후)
- 슬롯 표준: b0=PerFrame(VP), b1=PerObject(World), b2=BoneMatrices, b3=PerMaterial(PBR) (Track 1 신설)

### 6.8 ImGui

- 적극 사용. 엔진 디버그/튜닝/에디터/연습모드 핵심
- `WINTERS_EDITOR` 매크로 Debug/Editor 활성, Release `#ifdef` 완전 제거
- 신규 시스템 = ImGui 슬라이더 노출 의무 (하드코딩 금지)
- 우선순위: Inspector → Material → Profiler → Animation → Physics → Console → Shader Reload → Network

### 6.9 Sound (FMOD)

- Client 는 `CGameInstance::Get()->PlaySoundOn(...)` / `PlayEffect` / `PlayBGM` 등 Tier1 포워딩으로만 접근
- `CSound_Manager` 직접 참조 금지
- 키는 `Resource/Sound/` 기준 상대경로 `wstring_t`. 예: `L"BGM/Title.wav"`, `L"Irelia/attack1.wav"`
- 채널: `eSoundChannel` enum 9 슬롯 (BGM / PlayerAction / PlayerVoice / UI / Ambient / Effect0~3)

---

## 7. Work Style Guardrails (코딩 행동 강령)

LLM 코딩 실수를 줄이는 4 원칙. 사소한 작업은 판단껏, 중요 작업은 강제 적용. 속도보다 신중함 편향. 사소한 버그 수정은 우회. 지표는 불필요 변경 줄어듦, 과공학 재작성 줄어듦, 질문이 실수 후 → 실수 전으로 이동.

### 7.1 Think Before Coding (가정 금지, 의문 표면화)

- 가정은 명시한다. 불확실하면 묻는다.
- 해석이 둘 이상이면 조용히 하나 고르지 말고 차이를 적는다.
- 더 단순한 접근이 있으면 먼저 제안한다 (정당화 가능하면 push back).
- 헷갈리면 멈추고 무엇이 모호한지 적는다.

### 7.2 Simplicity First (최소 코드)

- 요청받지 않은 기능은 넣지 않는다.
- 1회용 코드에 과한 추상화 만들지 않는다.
- 설정/유연성/확장성을 미리 과투자하지 않는다.
- 불가능한 시나리오 에러 핸들링 금지.
- 200줄이 50줄로 끝날 수 있으면 다시 줄인다.
- 자가 점검: "시니어가 과공학이라 할까?" Yes 면 단순화.

### 7.3 Surgical Changes (꼭 필요한 것만 수정)

- 필요한 줄만 건드린다.
- 인접 코드의 스타일/포맷/리팩터는 요청 없으면 하지 않는다.
- 내 변경으로 생긴 unused 만 치운다.
- 기존 dead code 는 메모만 남기고 함부로 지우지 않는다.
- 검증: 변경된 모든 줄이 사용자 요청에 직결되는가?

### 7.4 Goal-Driven Execution (검증 가능 목표)

작업을 검증 가능한 목표로 바꾼다:
- "validation 추가" → "잘못된 입력 테스트 작성 → 통과시키기"
- "버그 수정" → "재현 테스트 작성 → 통과시키기"
- "리팩터" → "전후 테스트 통과 보장"

다단계 작업은 각 단계 합격 조건을 적는다:

```txt
1. [Step] → verify: [check]
2. [Step] → verify: [check]
```

"작동하게 만들기" 같은 모호한 종료 기준 피한다.

### 7.5 위반 사례 박제 (재발 방지)

- 위반 #1 (가정): 2026-04-26 이렐리아 FBX render PNG = mesh diffuse 가정 → 1.5h 소모. 셰이더 안 읽고 CPU 가설 누적. 이후 winters-skills/debug-pipeline 도입.
- 위반 #2 (과공학): Phase FX v4 가 `eFxBlendMode` 신설 시도. BlendStateCache 의 `eBlendPreset` 이미 존재. 4 폴더 grep 누락. 이후 winters-skills/code 의 "기존 인프라 식별 우선" 강제.
- 위반 #3 (Surgical 위반): 2026-04-19 Phase B-6.7 맵 에디터. 컨벤션 위반 파일명/include/pragma once 누락 → Manager 재작성.
- 위반 #4 (검증 누락): lol2gltf `--flipX` silent ignore. md5 검증 안 하고 다음 단계 진입 → 좌우 반전 코드 우회.

---

## 8. Active Plans

본 절의 표는 진입 문서 인덱스 역할. 비교가 필요해 표를 유지한다 (codex 스타일 룰의 "정말 비교가 필요한 1~2 곳" 예외).

| 트랙 | 상태 | 진입 문서 |
|---|---|---|
| Stabilization-0 (codex 일회성 게이트) | 0-1 imgui dirty 결정만 잔여, 빌드/DX12 smoke/docs 통과 | CLAUDE.md §1.B Option Stabilization-0 |
| Stabilization-1 (5/6 신규, Phase 6C/6D/6D-2 후속) | 3 클라 hidden smoke roster/start/local binding/snapshot/Q skill hook 통과, 1-client full-map telemetry 통과, 직접 시각 smoke 잔여 | CLAUDE.md §1.B Option Stabilization-1 |
| Phase 7 Champion ECS / Scene Split 마스터 (5/6 신규) | 7A~7E 완료, S1 hidden/full-map telemetry 통과, direct-local selected+Sylas fallback telemetry 추가, 7F F-1/F-2/F-3 + 7G filters 구현 완료. Direct visual smoke 검증은 user-owned | `.md/TODO/05-06/PHASE_7_CHAMPION_ECS_SCENE_SPLIT_MASTER_PLAN.md` |
| BanPick / InGame champion pipeline cleanup | 자동 smoke 경로 추가, local binding/skill/full-map telemetry 통과, direct-local selected+Sylas fallback telemetry 추가, 직접 시각 smoke는 user-owned | `.md/TODO/05-06/BANPICK_INGAME_CHAMPION_PIPELINE_AND_LEGACY_CLEANUP.md` |
| Phase 6C Legacy Champion Removal 보고서 | 완료 (5/6) | `.md/TODO/05-06/PHASE_6C_IMPLEMENTATION_REPORT.md` |
| Phase 6D Scene_InGame 분리 (RosterSpawner) 보고서 | 완료 (5/6) | `.md/TODO/05-06/PHASE_6D_IMPLEMENTATION_REPORT.md` |
| Phase 6D-2 Scene_InGame 분리 (NetworkBridge) 보고서 | 완료 (5/6) | `.md/TODO/05-06/PHASE_6D_2_IMPLEMENTATION_REPORT.md` |
| BanPick Room Sync (TCP lobby + roster) | 박제 완료 | `.md/plan/sim/12_BANPICK_ROOM_SYNC_PLAN.md` |
| Legacy Champion ECS / Network Refactor | 박제 완료 | `.md/plan/sim/13_LEGACY_CHAMPION_ECS_NETWORK_REFACTOR_PLAN.md` |
| Champion Lighting Polish (LoL diffuse-only) | 박제 완료 | `.md/plan/graphics/11_CHAMPION_LIGHTING_POLISH_PLAN.md` |
| Track 1: PBR / GGX / SSAO | W1-W4 통과 + W5 부분 + W6 partial (IRHI facade 통과 + DX11 native bridge 잔존) | `.md/plan/graphics/GGX+A/00_INDEX.md` (+ 01~05) |
| Track 1 확장 (Clustered Deferred / TAA / IBL / Motion Vec / FromSoft Lessons) | 박제 완료, 2차 트랙 | `.md/plan/graphics/Graphics/00_INDEX.md` (+ 01~06) |
| Graphics Stage 마스터 인덱스 (전체 1~10) | 박제 완료 | `.md/plan/graphics/00_GRAPHICS_PLAN_INDEX.md` |
| Track 2: RHI 멀티 백엔드 | W3 RH-1 seed + W5 rename + W6 RH-3 8 인터페이스 통과, RH-7 연동 필요 | `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md` |
| Track 2: DX11 Legacy -> DX12/Vulkan/Mobile/Console 통합 마스터 | RH-7 Surface/Capability public contract 1차 완료, DX12SmokeHost 독립 빌드/8초 생존 통과 | `.md/plan/rhi/09_RHI_DX11_LEGACY_TO_DX12_VULKAN_MOBILE_CONSOLE_MASTER.md` |
| RHI Backend 전략 | DX12/Vulkan/Console/Mobile 방향성 정리. RHI 본격 구현 전 7E 선행 원칙 반영 | `.md/TODO/05-06/RHI_BACKEND_STRATEGY_DX12_VULKAN_CONSOLE_MOBILE.md` |
| Track 2: W2-W6 박제 (Twin Track) | 5 파일, codex 적용 일부 | `.md/plan/engine/2026-05-0{2,3,4,5,6}_WEEK_*_DETAILED_BAKE.md` |
| Track 2: W7-9 DX12 Bootstrap | Scaffold 박제됨 (28 파일 + Debug-DX12 config + D3D12MA), DX12SmokeHost 독립 빌드/8초 생존 통과 | `.md/plan/rhi/DX12_BOOTSTRAP_PLAN.md` (5/6 신설) + `.md/plan/engine/2026-05-07_WEEK_7_9_DETAILED_BAKE.md` |
| Track 2: W10-13 (RH-5 DX12 visual parity) | 박제 완료, W9 합격 후 | `.md/plan/engine/2026-05-10_WEEK_10_13_DETAILED_BAKE.md` |
| Track 2: W14-17 (RH-6 Vulkan, 선택) | 박제 완료, cross-platform 결정 시 | `.md/plan/engine/2026-05-14_WEEK_14_17_DETAILED_BAKE.md` |
| Track 3: Fiber JobSystem 안정화 (Phase 5-B-pre) | 헤더 3 + JobSystem.cpp FiberShell 본체 박제, CFiberPool / WaitForCounter Yield / Get_WorkerSlot 잔여 | `.md/plan/engine/FIBER_JOBSYSTEM_STABILIZATION_PLAN.md` (5/6 신설) + `.md/plan/engine/FIBER_JOB_SYSTEM_v2.md` (codex 6 건) |
| Track 4: Gameplay Stabilization (B-13 회귀 검증) | B-13 시스템 (SpatialHash/Vision/TurretAI/BT/MCTS) 박제됨, 행동 보존 검증 대기 | `.md/plan/B13/00_INDEX_MASTER.md` (v2) |
| Twin Track 통합 마스터 | 박제 완료 (codex 14 패치 반영) | `.md/plan/engine/2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md` |
| TCP MVP 2-client demo (Sim-04a v2) | 박제 완료, 보류 | `.md/plan/sim/04a_MVP_2CLIENT_TCP_DEMO_v2.md` (+ D-0~D-3 sub-plan) |
| UDP NetStack (Sim-10 v2) | 박제 완료, 04a 합격 후 | `.md/plan/sim/10_UDP_LOL_NETSTACK_MASTER_v2.md` |
| Champion Pipeline (B-10 잔여 / B-12 메쉬 분리) | 12체 폴더 박제 완료, ChampionCatalog/SkillTable 하드코딩 축소 대기 | `.md/plan/Champion/00_CHAMPION_PIPELINE_ROADMAP.md` |
| Asset Pipeline (.wmesh/.wskel/.wanim Stage 1~12) | Stage 3 완료, .wmesh 31 개 변환 | `.md/plan/WintersFormat/00_WINTERS_FORMAT_INDEX.md` |
| Backend (Phase 0~7 통과, Phase 8 진행) | Phase 0~7 통과, Phase 8 C++ Network SDK winhttp.h 박제 시작 | `.md/plan/backend/00_BACKEND_PLAN_INDEX.md` |
| Security (Phase 0~3) | 박제 완료 | `.md/plan/security/00_SECURITY_INDEX.md` |
| Physics (Stage 1~10) | 박제 완료 | `.md/plan/physics/00_PHYSICS_PLAN_INDEX.md` |
| AI Bot (Stage 0~13) | 박제 완료, 챔프 파이프라인 후 | `.md/plan/ai/00_AI_PLAN_INDEX.md` |
| Effect Tool (Niagara급 v4) | `17_NIAGARA_FULL_REWRITE_MASTER.md` 가 권위. 13 V3는 deprecated/replaced-by 17. 다음 구현 진입은 17의 EFX-0 → EFX-1 순 | `.md/plan/EffectTool/00_EFFECT_TOOL_PLAN_INDEX.md` + `.md/plan/EffectTool/17_NIAGARA_FULL_REWRITE_MASTER.md` |

### 8.1 스킬 명령

- `/code` 코드 작성 + 리뷰 사이클 (`winters-skills/code/SKILL.md`)
- `/debug-pipeline` 셰이더 우선 Read + 데이터 직접 계측
- `/review` 수정 계획서 검증 + 상세 수정 계획서 생성
- `/testing` GoogleTest 단위 테스트
- `/todo` 전체 구현 계획
- `/phase-d-next` / `/phase-2-yasuo` 챔프 진입 매크로

### 8.2 계획서 규칙 (행동 강제)

1. 계획서 작성·참조 시 예외 없이 세션 대화에 전체 내용 (전문) 붙여넣기. 요약/축약 금지. 길이 문제는 유저가 판단.
2. 생성할 파일마다 h/cpp 코드 전문 포함.
3. .h/.cpp 파일 경로 명시 (예: `Engine/Code/RHI/DX11/CDX11Device.cpp`).
4. 줄 번호 명시 (예: L42-L55).
5. 수정 전/후 코드 cpp 블록 포함.
6. 추상적 지시 금지. 반드시 코드 명시.
7. 기존 .md 관련 내용 있으면 세션에 불러와서 보여줄 것 (요약 금지, 전문).

### 8.3 문서 작성 스타일 (codex 패턴, 모든 .md 박제 시 의무)

기준 문서: `.md/TODO/05-06/PHASE_6A_6E_CODE_REVIEW.md` (codex 작성, 1254 lines).
적용 범위: `.md/**/*.md` 전체. 계획서 / 검토 노트 / 가이드 / 세션 로그 모두.

스타일 룰 12 항목:

1. 헤더 계층은 단순하게. `#` 제목 / `##` 큰 묶음 / `###` 작은 묶음 3 단계까지. `####` 이상은 쓰지 않는다.
2. 메타 정보는 콜론 형식. 볼드 X.

```txt
작성일: 2026-05-06
출처: 파일 경로
목적:
- bullet 1
- bullet 2
```

3. 표 (table) 는 거의 쓰지 않는다. 정말 비교가 필요한 1~2 곳만 (예: §8 Active Plans 인덱스 표). 그 외는 짧은 prose + bullet 또는 코드 블록으로 대체.
4. 이모지 / 특수 기호 0. 별표 / 체크 / X / 경고 / 신호등 같은 마크 일절 금지. 강조는 짧은 한국어 단어 (예: "필수", "권장", "주의") 로.
5. 볼드 (`**`) 최소화. 한 문서에 5~10 곳 이내. 정말 핵심 한 단어만.
6. 변경 파일 / 파일 후보 / 트리 / 디렉토리 구조 = `txt` 코드 블록.

```txt
Shared/Schemas/LobbyState.fbs
Client/Public/Network/Client/GameSessionClient.h
```

7. 코드 박제는 언어 명시. `cpp` / `xml` / `fbs` / `hlsl` / `powershell` / `bat` / `txt`.
8. 인라인 코드는 백틱으로 감싸기. 평문에 영어 단어가 코드 식별자면 백틱 사용 (`Hello`, `OnSessionJoin`, `m_pNetwork`).
9. 영어 + 한국어 혼용 단어는 백틱 또는 한국어로 풀어쓰기. `PITFALL` / `GATE` / `MILESTONE` 같은 영어 단어 평문 사용 금지. "박제 함정" / "관문" / "단계" 등 한국어로.
10. 리스트는 dash `-` 위주. 짧은 한국어 1~2 줄. 긴 prose 는 별도 문단으로.
11. 짧은 설명 prose 1~2 줄 후 코드 블록. 코드 블록 후 짧은 prose. 긴 prose 연속 금지.
12. sub-section 번호는 `### 6E-1.` `### 6E-2.` 형식. 마지막은 `### 6E 검증 명령` `### 6E 완료 기준` 으로 일관 마무리.

피해야 할 패턴:

```txt
- 표 8+ 개 박제
- 별표/체크/X/경고 등 이모지 다수
- 볼드 다수 (한 줄에 3~4 곳)
- "PITFALL P-1~P-19", "GATE A~H" 같은 영어 약어 평문
- 메타 정보에 볼드 사용
- 긴 prose 연속 5~10 줄
```

좋은 예시: `.md/TODO/05-06/PHASE_6A_6E_CODE_REVIEW.md` 본문 전체 그대로 따른다.

### 8.4 보안 / 안티치트 (12 원칙 한 줄 요약)

풀 본문 + 즉시 적용 금지/허용 + /review 체크리스트: `.md/plan/security/00_SECURITY_INDEX.md`.

1. 서버 권위 (위험). Client prediction OK, 서버 reconciliation 필수. `player.gold += 100` 직접 변경 금지.
2. Client EXE 표면적 최소화. 민감 로직 Engine DLL.
3. 심볼 가시성. `WINTERS_ENGINE` 최소, 내부 매니저 export 금지.
4. 중요 값 이중 저장 + 무결성 (Phase 6 ProtectedF32 패턴).
5. 로그/문자열 누출 방지. Release `OutputDebugString` 금지 (`#ifdef _DEBUG` 필수).
6. 빌드 플래그. `/guard:cf /GS /sdl /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /OPT:ICF /OPT:REF /LTCG`.
7. 네트워크 패킷. size-bounded, 시퀀스/timestamp, AES-GCM/ChaCha20-Poly1305.
8. 인증/결제. `CryptProtectMemory`, JWT 짧은 수명, HTTPS pinning, 영수증 서버 검증.
9. 파일 무결성. `.wmesh` / `.wanim` / `.wmat` SHA256, 셰이더 cso + 해시.
10. 안티 디버그 (Phase 6). PEB 직접, `NtQueryInformationProcess`, RDTSC 타이밍.
11. DLL 주입 저항. `SetProcessMitigationPolicy(ProcessSignaturePolicy)`, IAT 후킹 탐지.
12. 커널 안티치트 (Phase 6). IOCTL 유저모드 통신, `WINTERS_CRITICAL` 매크로 후크 지점.

---

## 9. Archive Pointer

세션 로그:
- 4월 누적: `.md/archive/SESSION_LOG_2026-04.md`. Phase B-4 ~ B-10 직전 완료 / 2차 사이클 / 미결 / 산출물 상세
- 5월 누적: `.md/archive/SESSION_LOG_2026-05.md`. 5/1 Twin Track / 5/2 압축 / 5/3-4 PLAN_AUTHORING_PITFALLS + B-13 / 5/5 DX12 Scaffold + Fiber v2 / 5/6 동기화 검증 + 의존성 분석 + Phase 6C/6D/6D-2 + Phase 7 Master + filters PR-1

의존성 그래프 박제:
- `.md/archive/DEPENDENCY_GRAPH_2026-05-06.md`. Engine 내부 모듈 매트릭스 + mermaid 그래프 + 위반 4건 + Track 1/2/3 위치 + 핫스팟 노드

Phase 6/7 보고서 묶음 (5/6~5/7):
- `.md/TODO/05-06/PHASE_6A_6E_CODE_REVIEW.md` (codex 골격)
- `PHASE_6C_IMPLEMENTATION_REPORT.md`
- `PHASE_6D_IMPLEMENTATION_REPORT.md`
- `PHASE_6D_2_IMPLEMENTATION_REPORT.md`
- `PHASE_6E_HYBRID_FILTERS_FINAL_PLAN.md`
- `PHASE_7_CHAMPION_ECS_SCENE_SPLIT_MASTER_PLAN.md`
- `PHASE_7D_IMPLEMENTATION_REPORT.md`
- `PHASE_7D_4B_5A_IMPLEMENTATION_REPORT.md`
- `PHASE_7D_5B_5D_YASUO_SKILLHOOK_REPORT.md`
- `PHASE_7D_6A_IRELIA_Q_SKILLHOOK_REPORT.md`
- `BANPICK_INGAME_CHAMPION_PIPELINE_AND_LEGACY_CLEANUP.md`
- `RHI_BACKEND_STRATEGY_DX12_VULKAN_CONSOLE_MOBILE.md`

CLAUDE.md 백업:
- 압축 전 원본 (롤백용): `.md/archive/CLAUDE_MD_2026-05-01_PRE_COMPRESSION.md`
- 5/6 본 갱신 직전: `.md/archive/CLAUDE_MD_2026-05-06_PRE_UPDATE.md`
- 5/6 codex 스타일 적용 직전: `.md/archive/CLAUDE_MD_2026-05-06_PRE_CODEX_STYLE.md`
- 압축 검토 계획서 (codex): `.md/plan/engine/CLAUDE_COMPRESSION_REVIEW_PLAN_2026_05_01.md`

장기 로드맵 / 아키텍처:
- `.md/roadmap/LOL_30DAY_MASTER_PLAN.md` / `.md/roadmap/ROADMAP.md`
- Engine 아키텍처: `.md/architecture/WINTERS_ENGINE_ARCHITECTURE_FINAL.md`
- 게임플레이 아키텍처: `.md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md`
- 학원 DX11 매핑: `.md/architecture/CLASS_DAY8_VS_WINTERS.md` (병합 철학 결정 매트릭스)
