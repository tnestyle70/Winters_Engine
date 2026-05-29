# Winters Engine — 작업 운영 브리프

**범용 DX11 C++20 게임 엔진** — 단일 엔진 DLL 로 LoL 모작 (`WintersLOL.exe`) + 엘든링 모작 (`WintersElden.exe`).

> 이 문서는 **운영 브리프**다 (백과사전 X). 상세 설계는 `.md/architecture/`, `.md/plan/`, `.md/guide/` 에 있다.
> 마지막 갱신: **2026-05-02** — Week 2~W17 박제 + codex W3-W6 적용 검토 완료 + Fiber 트랙 신설.
> 4월 누적 세션 로그: `.md/archive/SESSION_LOG_2026-04.md`. 압축 전 원본: `.md/archive/CLAUDE_MD_2026-05-01_PRE_COMPRESSION.md`.

---

## 1. Current Focus (2026-05-02 갱신)

### A. 진행 트랙 3개

**Track 1 (Graphics) — DX11 PBR + GGX + Forward+ + SSAO**
- W1~W4 ✅ codex 적용 + 작성자 검토 완료
  - W1: `BRDF_GGX.hlsli` + RH-0 §1 TODO marker
  - W2: PBR + cbuffer 4-light array (Forward+ tile compute 대신 inline) + 7 챔프 PBR opt-in
  - W3 ⚠️ 축소: Forward+ → cbuffer fixed 4-light (UAV/compute plumbing 부재 + RH-1 seed only)
  - W4: `NormalPass` + `SSAOPass` + `GTAO_CS` + `GTAO_Blur_CS` + 9 leak consumer 7/9 마이그
- W5 ⚠️ 부분: `Get_RHIDevice()` 정식 rename (`IRHIDevice*` 반환) ✅ — 단 Public DX11 헤더 제거 / 6 챔프 차별화 보류 (codex dependency note 정합)
- W6 ✅ Track 2 핵심 + Track 1 보류:
  - RH-3 4 인터페이스 + `RHIDescriptors` + 64-bit handle Tag template + `CRHIResourceTable` + `ShaderCompiler` ✅
  - 셰이더 `register(... space0)` 명시 ✅
  - `CMaterialPBR` / `CTexture` handle 마이그 보류 (codex "발판" 의도)

**Track 2 (RHI 멀티 백엔드) — DX11 → DX12 → Vulkan**
- W3 ✅: RH-1 seed 3 파일 (`IRHIDevice + RHITypes + RHIHandles`) — 9 인터페이스 중 3개
- W5 ✅: `Get_RHIDevice()` 정식 (`IRHIDevice*` 반환), `Get_NewRHIDevice` 흡수
- W6 ✅: `IRHIDevice` 9 메서드 추가 (`CreatePipeline/RenderPass/BindGroup/Layout` + `Destroy*` + `UpdateBindGroup`) + CDX11Device emulation 4 hit
- W7-9 / W10-13 / W14-17 박제 완료 — codex 진입 대기

**Track 3 (★ 신규) — Fiber JobSystem (Phase 5-B)**
- 현 Phase 5-A MVP: Chase-Lev WorkStealingDeque + 글로벌 큐 hybrid + busy-wait `WaitForCounter`
- 기준 박제: `.md/plan/engine/FIBER_JOB_SYSTEM.md` (Naughty Dog GDC 2015 모델, 13 신규 + 8 수정 파일)
- codex 추가 검토 (2026-05-02):
  - **호출부 유지** — `Submit` / `WaitForCounter` / `CJobCounter` API 불변, Fiber 는 `CJobSystem` 내부 구현 디테일
  - **1차 목표 = Fiber shell only** — `WaitForCounter` yield 없이, 단순 `ConvertThreadToFiber` + 1 job 실행 + 복귀
  - **`Get_WorkerSlot()` 함정** — Fiber 가 다른 worker 에서 resume 가능 → thread-local worker index 만으로 위험. `MinionAISystem.cpp:297` 의 per-slot buffer 패턴 영향. Fiber 적용 시 "현재 실행 컨텍스트의 stable scratch slot" 반환으로 변경

### B. 즉시 진입 명령 (3 옵션)

**Option A (★ 권장) — Fiber JobSystem (codex 권장 흐름)**

```
1. Engine/Public/Core/Fiber/FiberTypes.h 박제 (의존 0)
2. Engine/Public/Core/Fiber/Fiber.h+.cpp (Win32 ConvertThreadToFiber / Create / SwitchTo / Reset)
3. Engine/Public/Core/Fiber/FiberPool.h+.cpp (128 미리 할당, Acquire/Release)
4. Engine/Public/Core/Fiber/FiberJobSystem.h+.cpp shell only
5. CJobSystem 의 내부를 FiberJobSystem 으로 위임 (public API 불변)

검증: Debug 빌드 + 1만 job counter stress
첫 PR 단위: shell only — WaitForCounter yield 는 다음 커밋
다음 커밋: WaitForCounter Fiber-aware → Scene_InGame:397 의 Set_JobSystem 주석 복구 검증

선결: Phase 5-A Chase-Lev Main-push race 정식 수정 (옵션 A: Main → Global Queue, Worker → 자기 Deque)
```

**Option B — Track 1 W6 잔여 caller 마이그**
- `CMaterialPBR / CTexture` handle 마이그 (`RHIBufferHandle/TextureHandle/BindGroupHandle`)
- `ModelRenderer / FxSystem` 의 `DX11Shader/DX11Pipeline` → `IRHIPipelineState`
- W7 RH-5 DX12 진입 전 caller 정식 마이그

**Option C — Track 2 W7-9 RH-5 DX12 진입**
- D3D12MA 편입 + Engine.sln Debug-DX12 컨피그
- `Engine/Private/RHI/DX12/` 14 파일 신설
- LoL exe 정상 종료까지 (시각 검증은 W10-13)

### C. 미결 (이번 세션 누적)

- **(★ Fiber 진입 전제) Phase 5-A Chase-Lev Main-push race** — `Submit` 이 Main thread 호출 시 Worker Deque Push 위반. `Scene_InGame.cpp:397` 의 `pNav->Set_JobSystem(pJS)` 주석 복구 필요. 옵션 A: Main → Global Queue, Worker → 자기 Deque.
- **AnimUpdate 병렬화** — Phase 5-B Fiber 진입 후 추가 +2~3ms 절감 기대.
- **W5 Public DX11 헤더 제거 보류** — `Engine/Public/RHI/DX11/` 9 파일 + `CDX11Device.h` 잔존. Public ID3D11* 노출 12 hit. W7 DX12 진입 전 정리 필요.
- **6 챔프 metallic/roughness 차별화** — Yasuo/Sylas/Kalista/Garen/Zed/Riven 디폴트 동일 (W5 §3.2 미진행).
- **Track 1 W6 caller 마이그** — `CMaterialPBR/CTexture/ModelRenderer` 의 PSO/BindGroup handle 경로 미진입 (codex "발판" 보고).
- **W4-W6 별도 빌드 산출물** (`WintersGame_Week4.exe ~ Week6.exe`) 미생성 — `Week3.exe` (May 2 07:44) 가 최신.
- **이전 누적**: UI HP 팀색 바 PNG (`single_bar_blue/red.png` WIC 진단), Phase B-10 잔여 4 챔프 (피오라/리븐/이즈리얼/요네), 04a v2 D-0~D-3 sub-plan (TCP MVP) — 우선순위 보류.

---

## 2. Read First

| 순서 | 문서 | 목적 |
|---|---|---|
| 1 | `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md` | 네이밍 / DLL 경계 / 팩토리 / 타입 alias 확정 규칙 |
| 2 | `.md/process/PLAN_AUTHORING_PITFALLS.md` ★ | **계획서 박제 시 7 함정 + 5 단계 게이트 — 박제 진입 전 의무 통과** (2026-05-04 신규, 13_YONE v1 사고) |
| 3 | `.md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md` | 11 섹션 / 150 챔프 타겟 / 7-레이어 구조 |
| 4 | `winters-skills/code/SKILL.md` + `winters-skills/code-scaffolding/SKILL.md` | 코드 작성 사이클 + 공통 함정 |
| 5 | `winters-skills/debug-pipeline/SKILL.md` | 렌더/이펙트/스킬 버그 디버깅 (셰이더 우선 Read + 데이터 직접 계측) |
| 6 | 본 CLAUDE.md §5 (Critical Rules & Gotchas) | 자주 발생하는 사고 |
| 7 | `.md/roadmap/LOL_30DAY_MASTER_PLAN.md` | 게임 콘텐츠 Phase 0~8 |

---

## 3. Repo Quick Map

```
Engine/                           WintersEngine.dll
  Public/{AssetFormat,Core,ECS,Editor,Framework,Manager,Platform,RHI,Renderer,Resource,Scene,Sound}
  Private/  대응 .cpp
  Include/  공개 API (DLL 경계)
  ThirdPartyLib/{Assimp,DirectXTK,FMOD,ImGui}  자립 (vcpkg 미사용)
Client/                           WintersGame.exe
  Private/{GameObject/Champion/{Ezreal,Garen,Irelia,Kalista,Riven,Yasuo,Zed},GamePlay,Manager,Map,Network,Scene,UI}
  Bin/Resource/{Shader,Font,Sound,Texture}  PostBuild xcopy
Server/                           IOCP, GameRoom, ServerWorld, SnapshotBuilder
  Private/Network/{IOCPCore,Session,FrameParser}, Security/
Shared/                           deterministic sim + FlatBuffers schema
  GameSim/Components,Systems,Registry  (Health/Mana/SkillState/Move/Buff/Stat/...)
Shaders/                          6 HLSL — Mesh3D / Skinned3D / Default3D / FxMesh / FxSprite / Triangle (전부 unlit)
Tools/WintersAssetConverter/      .wmesh / .wskel / .wanim 변환기
Services/                         Go 백엔드 — auth/leaderboard/matchmaking/payment/profile/shop
EngineSDK/inc/                    Engine 공개 헤더 폴더 구조 유지 복사 (UpdateLib.bat 자동 동기화)
.md/                              architecture/plan/guide/roadmap/archive
```

**모듈 의존 방향** (Engine 필터 번호순, 낮은 번호는 높은 번호에 의존 X):
- `00.Manager` (RHI Device/Pipeline/Shader/Buffer) → `01.Core` (Timer/Input/Transform/JobSystem/Profiler) → `02.Structure` (Framework/Entry) → `03.Renderer` → `04.Editor` (ImGui) → `05.ECS` → `06.Resource` → `07.Physics` → `08.Audio` → `09.Network` → `10.JobSystem` → `11.Scene` → `12.Collision` → `13.AI`

**향후 분리**: `WintersLOL/` (Client+Server+Data MOBA 일체) / `WintersElden/` (액션RPG, ER 모작).

---

## 4. Current Implemented State

### 4.1 렌더링 / RHI (DX11 단일 백엔드)

- **CDX11Device** (`Engine/Public/RHI/CDX11Device.h`): `ID3D11Device*` / `ID3D11DeviceContext*` 직접. CGameInstance 8개 leak getter (`Get_RHIDevice` / `Get_MeshShader` / `Get_MeshPipeline` / `Get_BlendStateCache` 등).
- **공유 셰이더/파이프라인** (`CEngineApp.h`): Mesh3D / Skinned3D / FxMesh / FxSprite — 엔진이 소유.
- **셰이더 6개 전부 unlit**: 광원 / BRDF / SSAO / IBL / tone mapping / G-Buffer / Deferred / TAA / DXR — **모두 0%** (코드).
- **Renderer 5종**: ModelRenderer / CubeRenderer / PlaneRenderer / FxStaticMeshRenderer / TriangleRenderer — Immediate-mode (RenderGraph 0).
- **Track 1 진입 시점에 신설**: `Shaders/BRDF/` 폴더, `Mesh3D_PBR.hlsl`, `CMaterialPBR`, cbuffer b3 PerMaterial.

### 4.2 ECS / 게임플레이

- **Engine/Public/ECS**: TransformComponent, GameplayComponents, NavAgentComponent, FxMeshComponent + ComponentStore + CWorld + ISystem + CSystemSchedular.
- **Phase B-7a (ModelRenderer 분해) 미진입**: ModelRenderer 여전히 monolithic.
- **Scene_InGame.cpp 비대 (~3000줄)**: ECS / 레거시 CTransform / 챔프 튜닝 / FX / 네트워크 snapshot / 디버그 UI / 직접 렌더 호출 한 클래스 집중. 분해 진행 중 (ChampionRegistry / SkillRegistry / VisualHookRegistry).
- **챔피언 7체** (`Client/Private/GameObject/Champion/`): Ezreal / Garen / Irelia / Kalista / Riven / Yasuo / Zed. 각 폴더에 FxPresets / Skills / ProjectileSystem.

### 4.3 SharedSim / 서버 (작동하는 뼈대 존재)

- **Shared/GameSim**: Components (Health/Mana/SkillState/Stat/MoveTarget/Buff) + Systems (Move/SkillCooldown/DamageQueue/Death/Buff/Stat) + GameplayHookRegistry.
- **Server/GameRoom.cpp**: 30Hz tick + Phase_DrainCommands / Phase_ExecuteCommands / Phase_SimulationSystems / Phase_BroadcastSnapshot.
- **04a v2 sub-plan 박제 완료** (D-0~D-3): 진입 대기.

### 4.4 자산 파이프라인

- **`.wmesh` / `.wskel` / `.wanim` 자체 포맷** (`Engine/Public/AssetFormat/`): Stage 3 박제 완료 (2026-04-28). 6 챔프 fast-path.
- **CModel::LoadModel**: `.wmesh` fast-path / `.wmesh+.wskel` skinned fast-path / 실패 시 Assimp fallback.
- **WintersAssetConverter.exe**: 27개 FBX/GLB → `.wmesh` 검증 (Irelia 60MB → 1.2MB = 50× 압축).

### 4.5 Go 백엔드 (Phase 0~7 완료)

| Phase | 서비스 | 포트 | 상태 |
|---|---|---|---|
| 0 | 인프라 (Docker+pkg) | — | ✅ |
| 1~6 | Auth / Leaderboard / Matchmaking / Profile / Payment / Shop | 8081~8086 | ✅ |
| 7 | Kafka E2E (MatchCompleted, SkinPurchased) | — | ✅ |
| 8 | C++ Client SDK (WinHTTP) | — | 🔄 |

상세: `.md/plan/backend/00_BACKEND_PLAN_INDEX.md`.

---

## 5. Critical Rules & Gotchas

> 평면 60+ 갓차를 **6 카테고리** 로 압축. 4월 이전 누적 사고는 `.md/archive/SESSION_LOG_2026-04.md` 참조.

### 5.1 빌드 / 툴체인

- **vcxproj `/utf-8` 필수**: 한글 주석 CP949 해석 시 C4819 + 가짜 파서 오류 (C1075 '{').
- **`.bat` 파일은 ASCII 전용**: cmd.exe 가 ANSI (CP949) 로 읽음. UTF-8 한글 주석 = for 루프/xcopy 부분 실패. `chcp 65001` 도 첫 줄 이전엔 미적용.
- **vc143.pdb lock**: VS (`devenv.exe`) 켜져 있으면 cl.exe 가 pdb 잡힘 → C1041. 빌드 전 VS 종료 또는 `/p:MultiProcessorCompilation=false /maxcpucount:1`. mspdbsrv leak 시 kill.
- **Engine 단독 빌드 → EngineSDK/inc 자동 동기화**: `Engine.vcxproj` PostBuild Event. Client 빌드 전 Engine 1회 필수.
- **EngineSDK/inc 직접 수정 금지** (CRITICAL): `xcopy /Y /D` 단방향 복사. Engine 원본 touch 시 SDK 수정 유실. **항상 Engine/Include + Engine/Public 원본 수정 후 UpdateLib.bat**. F12 점프 시 SDK 경로 가능성 → 탭 제목 + Solution Explorer 로 실제 경로 확인.
- **Server.vcxproj 전제 (Sim-10 v2 M1)**: `<FloatingPointModel>Precise</FloatingPointModel>` (Debug+Release) + `Mswsock.lib` + Engine project reference.
- **Transport boundary**: `Server/Game/`, `Server/Security/`, `Shared/` 에서 `WSARecv`/`WSASend`/`recvfrom`/`sendto` 0 hit 강제 (grep). UDP 마이그 안전성 핵심.

### 5.2 코드 안전 (DLL / 타입 / Include)

- **공개 헤더 `std::` 명시 필수**: Engine/Public + Engine/Include 의 `.h` 는 `using namespace std` 가정 X. Client TU 파싱 실패. 공개 헤더는 `std::vector` / `std::function`.
- **공개 헤더 `bool_t`/`i32_t` alias 사용 금지**: `bool_t` 는 `namespace Engine` 안에만 존재. namespace 밖 공개 API 는 `bool` / `int32_t` 또는 `WintersTypes.h` 의 `f32_t`/`f64_t` (global alias).
- **EngineSDK/inc 는 폴더 구조 유지** (`xcopy /S` 단방향 복사): 서브디렉토리 헤더는 항상 폴더 경로 포함 (`"ECS/Entity.h"` / `"Resource/Texture.h"` / `"RHI/CDX11Device.h"`). Engine 빌드만 통과하고 Client unqualified include 사용 시 C1083 실패. flat include 는 `Engine/Include` 루트 헤더 (`WintersAPI.h` / `WintersTypes.h` / `WintersMath.h` 등) 에만 허용.
- **ComPtr FQN 필수 (SDK 노출 헤더)**: `#include <wrl/client.h>` + `Microsoft::WRL::ComPtr<...>` 완전 수식. `WintersPCH.h` 의 `using` alias 의존 시 Client C7568. 참고: `RHI/CDX11Device.h`, `RHI/DX11/BlendStateCache.h`.
- **`WINTERS_ENGINE` dllexport 클래스 + `unique_ptr` 멤버**: copy ctor / assign 명시 `= delete` 필수. 누락 시 MSVC `construct_at` SFINAE 실패 → vector::vector 무관 컴파일 에러 체인. 선례: `CWorld.h`, `CSystemSchedular.h`.
- **`imgui.h` include + `_DEBUG` 빌드 + `DBG_NEW`**: `Engine_Defines.h:41` `#define new DBG_NEW` 가 imgui placement new 와 충돌. include 를 `#ifdef _DEBUG / #undef new / ... / #define new DBG_NEW / #endif` 로 감싸기.

### 5.3 런타임 안전 (수명 / 이스케이프)

- **`Scene_Manager::Change_Scene` 즉시 self-destruct** (CRITICAL): 호출 직후 이전 Scene unique_ptr 가 그 자리에서 소멸. Scene 메서드 (OnUpdate/OnImGui) 안에서 `Change_Scene` 호출 후 코드 계속 실행 = use-after-free. **반드시 즉시 `return`**. BanPick 류 ImGui 분기에서 `Change_Scene` → `ImGui::End()` → `return;` 3 줄 패턴 강제.
- **`m_pActiveSkillDef` 댕글링** (Buffer Too Small 크래시): stage2 분기 `SkillDef s2 = *def;` 로컬 후 `&def` 저장 = 스택 해제. 해결: `SkillDef m_ActiveSkillDefStorage{}` 멤버 + 값 복사 후 포인터 저장.
- **Wide string `\U` / `\u` / `\W` 이스케이프 충돌** (CRITICAL): `L"C:\Users\..."` 의 `\U` = universal-character-name 규칙 (8 hex). C4129/C4566 + 경로 깨짐. **forward slash** (`L"Client/Bin/Resource/..."`) 또는 raw literal (`LR"(C:\Users\...)"`). Windows API 모두 `/` 허용. CWD = `LocalDebuggerWorkingDirectory=$(SolutionDir)`.
- **F12 = VS "Break into Debugger" 단축키**: 게임 키 바인딩 사용 시 디버깅 중 ntdll break (힙 손상으로 오진 가능). M / Tab / F1~F11 사용.
- **PlayAnimationByName 매칭 실패 silent**: Model.cpp `FindAnimationIndex` -1 반환 시 조용히 early-return. 증상: "락 타이머 정상인데 애니만 idle/run 유지". L244 에 `if (idx<0) OutputDebugStringA(...)` 1줄 추가 권장. 이렐리아 W `spell2_1`→`spell2` 사고.
- **봇 챔프도 OnEnter idle 애니 명시 기동 필수**: PlayAnimation 호출 전 `m_bPlaying=false` → 본 매트릭스 identity → bind pose 렌더. 무기 submesh 가진 챔프 (Kalista/Viego 등) 는 무기가 손 본 로컬 좌표 떠 있어 "안 보이는 것처럼".

### 5.4 데이터 / 포맷

- **`.wmesh` skinned 정점 76B layout** (CRITICAL): `WMeshFormat::VertexSkinned` POD 의 byte offset 을 셰이더 IL `D3D11_INPUT_ELEMENT_DESC AlignedByteOffset` 와 byte 단위 일치. 확정: pos[3] 0~11 / nrm[3] 12~23 / uv[2] 24~31 / tan[3] 32~43 / indices[4] 44~59 (uint32×4) / weights[4] 60~75. 임의 추가 필드 = GPU 가 다음 필드 byte 밀어 잘못 읽음 → 정점 NaN/0 collapse, 메시 사라짐 (애니/Transform 정상 = 진단 어려움). 가렌 사고 (2026-04-28).
- **Assimp aiMatrix4x4 → XMFLOAT4X4 전치 필수**: Assimp = column-vector / DX = row-vector. `ConvertMatrix()` 전치 누락 시 본 오프셋/Rest Pose/GlobalInverseRoot 전부 틀려 스키닝 폭발.
- **Assimp 스키닝 표준**: `Final = Offset × GlobalTransform × GlobalInverseRoot` (DX row-major). GlobalInverseRoot 누락 시 루트 노드 트랜스폼만큼 메시 틀어짐. 채널 없는 본 = Identity 가 아닌 Rest Pose 초기화.
- **스켈레톤 모델 stride 혼재 금지**: 스켈레톤 있으면 본 없는 서브메시도 VTXANIM (76B). VTXMESH (44B) 와 섞이면 skinned IL 이 44B 정점 잘못 읽어 폭발. 본 없는 메시는 `weight[0]=1.0, index[0]=0`.
- **LoL FBX 머티리얼 텍스처 경로 없을 수 있음**: Diffuse 경로 없어 `LoadTextures()` 무동작. `SetOverrideTexture()` 대신 `LoadMeshTexture(meshIndex, path)` 수동.
- **Blender FBX Export = glTF PBR 텍스처 경로 기록 불가**: glTF/glb → Blender Import → FBX Export 시 텍스처 참조 0개. 해결: glb 를 Assimp 직접 로드 + `pScene->GetEmbeddedTexture()`.
- **lol2gltf 1.0.0 `--flipX` silent ignore**: `mapgeo2gltf` `-x false` 4가지 형식 모두 무시 (CLI parser 버그, md5 검증 확정). 코드에서 맵 transform `SetScale({-0.01f, 0.01f, 0.01f})` X 미러 우회 + Mesh3D 가 `D3D11_CULL_NONE` 라 winding 안전.
- **변환 도구 결과 검증 필수**: 옵션 silent ignore 가능. md5 / 정점 분포 / 시각 확인 후 다음 단계 진입.
- **바이너리 POD 포맷 변경 시 VERSION bump + 기존 .dat 삭제**: 필드 추가 시 `sizeof` 변경 → 이전 포맷 호환 X. `STAGE_VERSION` 증가 + `static_assert(sizeof()==기대값)` + 기존 `.dat` 삭제. 버전 불일치 = count 쓰레기 → 거대 할당 → 힙 손상.

### 5.5 셰이더 / 렌더

- **HLSL `row_major` 필수**: DirectXMath = row-major / HLSL 기본 column-major. cbuffer 행렬에 `row_major matrix` / `row_major float4x4` 필수. 누락 = 카메라 이동 따른 비대칭 클리핑/왜곡.
- **mul 순서**: `mul(vector, matrix)` (행 벡터 × 행렬).
- **셰이더 수정 후 OutDir 강제 동기화 필수** (Phase FX-8): `D3DCompileFromFile` 가 OutDir (`Client/Bin/Debug/Shaders/`) 읽음. MSBuild incremental 이 .hlsl 변경 미감지 → PostBuild xcopy skip → 옛 .hlsl 그대로. **신호**: Output 창 셰이더 컴파일 에러 메시지에 `Bin/Debug/Shaders/` 경로. **해결**: `cp Shaders/*.hlsl Client/Bin/Debug/Shaders/` 직접 또는 Rebuild Solution 또는 PostBuild xcopy 에 `/Y` 강제.
- **LoL FX FBX 의 `render/*.png` ≠ mesh diffuse**: LoL 클라이언트 이펙트 카메라 캡처 스프라이트. FBX UV 가 알파 0 영역 가리켜 `clip(texColor.a-0.05f)` 가 모든 픽셀 버림 → 화면 출력 0 픽셀 ("스폰 안 됨"처럼). CPU 디버거로 못 잡음. 해결: 진짜 머티리얼 텍스처 (`irelia_base_blades_passive_4_texture.png` 형태) 를 `SetMeshTexture` 바인딩. `render/*.png` 는 빌보드 (FxBillboardComponent) 전용.
- **Additive blend + `vColor` RGB ≥ 1.0 = 흰색 포화**: PNG 흰색 위주 (LoL FX glow_color: RGB=1, alpha 그라데이션) + tint G/B 1.0 초과 → clamp → 0.7/1.0/1.0. Additive `Src + Dst` alpha 무시 + 픽셀 중첩 → clamp 가속 → 흰색. 해결: vColor RGB ≤ 1.0 또는 AlphaBlend (글로우 약함).
- **CPlaneRenderer 기본 CULL_BACK → 지면 퀘드 특정 각도 컬링**: DX11 기본 RS 뒷면 컬링. `bBillboard=false` 지면 퀘드 + `fYaw` 회전 = 뒷면 노출 → 사라짐. 해결: `D3D11_CULL_NONE` TwoSided RS 백업/바인딩/복원.
- **모델 yaw `+XM_PI` 보정**: 모델 뒷면=정면 컨벤션. `atan2(forward.x, forward.z) + XM_PI` 의 결과로 월드 forward 계산 시 부호 뒤집기 `Vec3 fwd{ -sinf(yaw), 0, -cosf(yaw) }`. 안 뒤집으면 마우스 반대 방향 버그. FX 스폰 (mis_spear/W stage2/R 투사체/dash 트레일) 모두 동일 부호. 90° (XM_PI/2) 와 180° (XM_PI) 헷갈리지 말 것.

### 5.6 게임 로직 / 스킬

- **`SkillDef.lockDurationSec ↔ FBX 애니 길이 매칭** (CRITICAL): `m_fLastActionTimer = lockDurationSec` 0 도달 시 자동 idle/run 강제 전환. lockDurationSec < 애니 길이 = 중간 커트. AS=1.0 정속에서만 발현 (AS 올리면 우연히 우회 → 디버깅 어려움). 해결: SkillTable lockDurationSec = FBX 원본 길이. 애니별 배속은 `SkillDef.animPlaySpeed` 독립 필드. 이렐리아 실측: 평타=1.0 / Q=0.5 / W1=4.0 / E=1.0 / R=5.0. 부등식: `lockDuration × animPlaySpeed ≥ recoveryFrame / FBX_FPS` (위배 시 recoveryFrame hook 미발동).
- **castFrame 감지 블록 분리 금지**: 여러 블록 두면 첫 블록 끝 `m_fActivePrevFrame=curF` 로 두 번째 블록 `HasFramePassed(castFrame, prevFrame)` 항상 false. 단일 블록에 `bCastHit/bRecoveryHit` 플래그 캐싱 + 모든 반응 후 맨 마지막에 prevFrame 갱신.
- **애니 전환 시 castFrame 재발동**: `PlayAnimationByName` 이 `m_dCurrentTime=0` 리셋 → 같은 castFrame 여러 애니 반복 통과. `m_bCastFrameFired` / `m_bRecoveryFrameFired` bool 멤버 + ApplyLocalPrediction 새 스킬 시 리셋. 1회만 보장.
- **Pathfinder empty path silent fail 금지**: NavGrid unwalkable 셀 + 도달 불가 = 빈 vector. `bHasGoal=false; vel.fSpeed=0` 만 처리하면 Chase/Pursuit 제자리 stuck. 추적 의도 분기에서 `target - selfPos` 직선 fallback + `bPathDirty=true` 다음 프레임 재시도.
- **ECS Phase 순서 = data dependency**: 같은 phase 내 시스템은 JobSystem Submit 으로 병렬 (race). Producer phase < Consumer phase 필수. AI(1) → Nav(2) 순서 지키지 않으면 첫 Chase 프레임 stale velocity. swap/추가 시 Producer/Consumer 의존 그래프 + 같은 phase race 검증.
- **enum class 이름 충돌 across namespace**: `Winters::Map::eTeam` (u32_t) vs `Engine::eTeam` (uint8_t) 동일 이름 다른 namespace 공존 = `using namespace` 만으로 해결 X. `Winters::Map::eTeam::Blue` fully qualify.
- **`CInput` 마우스 LButton 없음**: `m_Keys[256]` WndProc 에서 키보드만 기록. `IsKeyPressed(VK_LBUTTON)` 영원히 false. `GetAsyncKeyState(VK_LBUTTON) & 0x8000` 직접 + 멤버 bool 이전 상태 보관.

### 5.7 계획서 박제 (Plan Authoring) — 2026-05-04 신규 (누적 사례)

> **`.md/plan/**/*.md` 박제 시 사고 10종**. 상세 + **8 단계 게이트** (A→H): [`.md/process/PLAN_AUTHORING_PITFALLS.md`](.md/process/PLAN_AUTHORING_PITFALLS.md). 박제 진입 전 의무 통과. 누적 사례: 13_YONE v1 P1×2+P2×2 / B-13 마스터+sub P1×5+P2×5.

- **PIMPL 추측 의사코드 박제 금지** (P-2, CRITICAL): PIMPL 클래스 신규 메서드 박제 시 헤더만 보고 내부 멤버/헬퍼 추측 금지. **`.cpp` 의 `struct Impl { ... };` 정의 라인 Read 후 본문 인용** + 호출 helper 를 grep 으로 실재 검증. 사례: 13_YONE v1 §3.5.
- **신규 mask/visibility 추가 시 모든 render path 동시 박제** (P-3, CRITICAL): `Render()` (main) + `RenderNormalPass(...)` (PBR G-Buffer) + shadow/depth 다중. 한 pass 만 박제 시 hidden submesh 가 SSAO/depth 에 ghost. `grep -rn "pRenderer->Render"` 호출자 식별 + 동시 박제. 사례: 13_YONE v1.
- **`extern` 함수 / `static_cast<Scene_X*>` Scene 직접 의존 금지** (P-4, CRITICAL): Champion skill 이 Scene 직접 호출 시 서버/멀티 Scene/엘든링 깨짐. **ECS request 패턴**: `XxxRequestComponent` 1-frame 부착 → 별도 system 다음 phase 소비. 사례: 13_YONE v1 vs v2.
- **bitmask 폭은 본 계획서 자체 거론 미래 사례 + 50%** (P-7): `u32_t mask = 32` 한도. 엘든링 보스 50~100 submesh 거론하면서 32-bit 박제 자기 모순. **`std::array<u64_t, 4>` = 256 bit** 또는 sparse. 사례: 13_YONE v1 §3.1 ↔ §6.
- **§1 Preflight TODO 0 강제** (P-1+P-6): "필요" / "추정" / "TBD" 가 §1 표에 남으면 §3 본문 진입 금지. 데이터 결정이 변환/실행 결과 의존이면 박제 범위를 그 직전까지 한정. 사례: 13_YONE v1 §1.
- **인용 의미 반전** (P-8, CRITICAL): 다른 .md / .h L## 인용 시 부사어 (`...의 룰`, `...준수`) 만 박제 + 직접 인용 블록 없으면 **의미 정반대로 박제 가능**. 사례: B-13 v1 00 마스터 §3 의 "CLAUDE.md L795 의 'flat 구조' 룰" 인용 — 실제 AGENTS.md L437 은 "**서브디렉토리 헤더는 반드시 폴더 경로 포함**" 정반대. v1 그대로 박제 시 Client TU C1083. **회피**: L## 인용 옆에 그 줄 직접 인용 블록 박제 + 의미 일치 검증.
- **ECS Scheduler 동시성 모델 가정** (P-9, CRITICAL): `ISystem::GetPhase()` `uint32_t` — 0.5/1.5 fractional 박제는 0/1 절삭 (의미 무효). 같은 phase 시스템 2개 이상 = **JobSystem 병렬** ([SystemScheduler.cpp:22-42](Engine/Private/ECS/SystemScheduler.cpp:22) 등록 순서 무관). Producer→Consumer 의존 시스템은 **다른 정수 phase**. 같은 phase 는 read+write 0 인 시스템만 (write set 다른 시스템 묶기는 OK). 사례: B-13 v1 SpatialHash=0.5/TurretAI=1.5.
- **Owner Scope 매트릭스 미결정** (P-10, CRITICAL): Spatial/Vision/NavGrid 같은 World-단위 인덱스를 `CGameInstance` Tier-2 + `CEngineApp` 전역으로 박제 시 서버 멀티 GameRoom (Sim-10 v2) / 멀티 Scene / 엘든링 멀티 World 깨짐. **권장 매트릭스**: Spatial/Vision/NavGrid → `CWorld` owned, RHI Device → `CEngineApp` 전역, Sound/Input/Resource → `CGameInstance` Tier-1, Scene-한정 → Scene 멤버. 사례: B-13 v1 04 §4.
- **도메인 상수 Engine 박제** (P-11): `CSpatialIndex::CELL_SIZE = 8.f` 같이 한 게임 도메인 값을 Engine 공용 클래스 `static constexpr` 로 박제 시 다른 게임/맵 재사용 불가. **InitDesc 주입** (`SpatialGridDesc { worldOrigin, cellSize, halfExtentX, halfExtentZ }`) 또는 게임-specific 클래스 분리.
- **음수 좌표 정수 truncation** (P-12, CRITICAL): 맵 원점 중심 좌표 (LoL -140~+140) 의 negative quadrant 에서 `static_cast<int32_t>(world / cell)` 사용 시 **0-방향 절삭** 으로 셀 경계 1 cell 어긋남. -0.5/8 → cast 0 (실제로는 -1 cell). **`std::floor((world - origin) / cellSize)`** 사용. 단위 테스트: cell(-0.5)==-1 / cell(-7.9)==-1 / cell(-8.0)==-1 / cell(-8.1)==-2.
- **미존재 API 호출** (P-13, CRITICAL): `m_SystemScheduler.GetSystem<T>()` 같이 헤더에 없는 API 박제. **grep 으로 모든 API 헤더 실재 검증** 강제. 우회: (a) raw 포인터 캐시, (b) **ECS event/request 컴포넌트** (1-frame `XxxNotifyComponent` + 다음 phase System ForEach 소비). 사례: B-13 v1 03 → v2 `TowerAggroNotifyComponent`.
- **행동 정책 무의식적 변경** (P-14, CRITICAL): 성능 최적화 박제 시 자료 구조/검색 mask 슬며시 확장 → 게임 행동 변경. 사례: B-13 v1 04 의 MinionAI mask = `Minion+Champion+Turret+JungleMob+Inhibitor+Nexus` 6종 — 현재 enemy minion only 라인전 동작이 변경. **회피**: 최적화는 **행동 보존**. 도메인 확장은 별도 PR.
- **헤더 외부 의존 미include** (P-15, CRITICAL): 신규 컴포넌트 헤더가 다른 헤더의 enum/struct 사용 시 **그 의존 헤더 직접 include**. TU 가 두 헤더 같이 include 가정 = fragile. 사례: B-13 v1 02 `VisionComponents.h` 의 `WardComponent::ownerTeam = eTeam` — `GameplayComponents.h` 미include. 값 멤버는 forward declare 불가.

> 더 많은 갓차 (Phase B-7 / D / 가렌-제드 누적): `.md/archive/SESSION_LOG_2026-04.md`.

---

## 6. Minimal Conventions

> 풀 컨벤션: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`. 본 절은 자주 어기는 항목만.

### 6.1 네이밍

- 클래스: `C` 접두사 (`CTimer`)
- struct (POD): C 접두사 금지 (`TransformComponent`)
- 인터페이스: `I` 접두사 (`IBuffer`, `ISystem`, `IScene`)
- 파일명: C/I 접두사 없음 (`Transform.h` → `CTransform`, `IBuffer.h` → `IBuffer`). 기존 C 접두사 파일은 점진 리네임.
- enum class: `e<Domain>` 접두사 (`eRHIFormat`)

### 6.2 타입 별칭 (신규 코드 강제)

- **사용**: `f32_t / f64_t / i32_t / u32_t / u8_t / u16_t / u64_t / i8_t / i64_t / bool_t / wstring_t / tchar_t`
- **금지**: raw `float / int / unsigned int`, legacy `float32 / int32 / uint32`
- **예외**: Win32 (HWND/DWORD/UINT), DirectXMath (XMVECTOR/XMFLOAT3), 서드파티 (Assimp/FMOD)
- **공개 API 헤더**: `WintersMath.h`/`WintersTypes.h`/`EngineConfig.h`/`IWintersApp.h`/`WintersPaths.h` — Phase 4 일괄 정리 예정, 신규 추가만 컨벤션 적용.

### 6.3 클래스 설계 (★)

- 생성자 **private** — 외부 `new` 금지. `Create()` 정적 팩토리만
- 소멸자 **public virtual** — 스마트 포인터 수명
- 멤버 변수 **private** — public 멤버 함수로만 접근
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

- **Tier 1**: Timer/Input/Scene/Resource 요청/Sound 고수준/Network 세션 — 저빈도 / `CGameInstance::Get()->Method()` 포워딩
- **Tier 2**: JobSystem/ECS World/RHI draw/Physics step — 고빈도 핫패스. `CGameInstance::Get_JobSystem() -> IJobSystem*` 인터페이스 Getter, Client 포인터 캐시 후 직접
- **DLL export**: 신규 export `CGameInstance` 만. 내부 매니저 `WINTERS_API` 마크 금지
- **`DECLARE_SINGLETON` 호출**: Winters 는 포인터 반환 — `CGameInstance::Get()->Method()` (`->` 사용)

#### Tier-2 RHI 포워딩 게터 (현재)

- `CGameInstance` 가 `Get_RHIDevice()` / `Get_MeshShader()` / `Get_MeshPipeline()` / `Get_BlendStateCache()` 등 4개 게터 제공.
- ⚠️ Track 2 RH-0 진입 시 이 4개 + 추가 4개 = 총 8개를 `_Legacy` 로 rename 후 `IRHIDevice*` 신규 게터로 교체. RH-2 종료 시점에 `_Legacy` 제거.
- Client 가 반환 포인터 역참조 시에만 해당 헤더 include.

### 6.6 Include / ComPtr (★)

- Client 헤더: `#include "Defines.h"` (STL + Engine 타입 해결)
- Engine 헤더: `WintersAPI.h` + `WintersTypes.h` + `WintersMath.h` (Engine_Defines.h 미포함)
- 서브디렉토리 헤더는 폴더 경로 포함: `"ECS/Entity.h"` (unqualified 금지)
- **ComPtr (SDK 노출 헤더)**: `#include <wrl/client.h>` + `Microsoft::WRL::ComPtr<...>` FQN 필수

### 6.7 HLSL

- cbuffer 행렬 `row_major` 필수
- mul: `mul(vector, matrix)` (행 벡터 × 행렬)
- 레지스터 슬롯 명시: `register(b0)` `register(t0)` `register(s0)` (DXC SPIR-V 호환, RH-1 이후)
- 슬롯 표준: b0=PerFrame(VP), b1=PerObject(World), b2=BoneMatrices, **b3=PerMaterial(PBR)** ★ Track 1 신설

### 6.8 ImGui

- 적극 사용 — 엔진 디버그/튜닝/에디터/연습모드 핵심
- `WINTERS_EDITOR` 매크로 Debug/Editor 활성, Release `#ifdef` 완전 제거
- 신규 시스템 = ImGui 슬라이더 노출 의무 (하드코딩 금지)
- 우선순위: Inspector → Material → Profiler → Animation → Physics → Console → Shader Reload → Network

### 6.9 Sound (FMOD)

- Client 는 `CGameInstance::Get()->PlaySoundOn(...)` / `PlayEffect` / `PlayBGM` 등 Tier1 포워딩으로만 접근
- `CSound_Manager` 직접 참조 금지
- 키는 `Resource/Sound/` 기준 상대경로 `wstring_t`. 예: `L"BGM/Title.wav"`, `L"Irelia/attack1.wav"`
- 채널: `eSoundChannel` enum 9 슬롯 (BGM / PlayerAction / PlayerVoice / UI / Ambient / Effect0~3)

---

## 7. Work Style Guardrails (★ 코딩 행동 강령)

LLM 코딩 실수를 줄이는 4 원칙. 사소한 작업은 판단껏, 중요 작업은 강제 적용.
**Tradeoff**: 속도보다 신중함 편향. 사소한 버그 수정은 우회.
**지표**: 불필요 변경 줄어듦, 과공학 재작성 줄어듦, 질문이 실수 후 → 실수 전으로 이동.

### 1. Think Before Coding (가정 금지, 의문 표면화)

- 가정은 명시한다. 불확실하면 묻는다.
- 해석이 둘 이상이면 조용히 하나 고르지 말고 차이를 적는다.
- 더 단순한 접근이 있으면 먼저 제안한다 (정당화 가능하면 push back).
- 헷갈리면 멈추고 무엇이 모호한지 적는다.

### 2. Simplicity First (최소 코드)

- 요청받지 않은 기능은 넣지 않는다.
- 1회용 코드에 과한 추상화 만들지 않는다.
- 설정/유연성/확장성을 미리 과투자하지 않는다.
- 불가능한 시나리오 에러 핸들링 금지.
- 200줄이 50줄로 끝날 수 있으면 다시 줄인다.
- 자가 점검: "시니어가 과공학이라 할까?" Yes 면 단순화.

### 3. Surgical Changes (꼭 필요한 것만 수정)

- 필요한 줄만 건드린다.
- 인접 코드의 스타일/포맷/리팩터는 요청 없으면 하지 않는다.
- 내 변경으로 생긴 unused 만 치운다.
- 기존 dead code 는 메모만 남기고 함부로 지우지 않는다.
- 검증: 변경된 모든 줄이 사용자 요청에 직결되는가?

### 4. Goal-Driven Execution (검증 가능 목표)

- 작업을 검증 가능한 목표로 바꾼다.
  - "validation 추가" → "잘못된 입력 테스트 작성 → 통과시키기"
  - "버그 수정" → "재현 테스트 작성 → 통과시키기"
  - "리팩터" → "전후 테스트 통과 보장"
- 다단계 작업은 각 단계 합격 조건을 적는다:
  ```
  1. [Step] → verify: [check]
  2. [Step] → verify: [check]
  ```
- "작동하게 만들기" 같은 모호한 종료 기준 피한다.

### 위반 사례 박제 (재발 방지)

- **#1 위반 (가정)**: 2026-04-26 이렐리아 FBX render PNG = mesh diffuse 가정 → 1.5h 소모. 셰이더 안 읽고 CPU 가설 누적. 이후 winters-skills/debug-pipeline 도입.
- **#2 위반 (과공학)**: Phase FX v4 가 `eFxBlendMode` 신설 시도 → BlendStateCache 의 `eBlendPreset` 이미 존재. 4 폴더 grep 누락. 이후 winters-skills/code 의 "기존 인프라 식별 우선" 강제.
- **#3 위반 (Surgical 위반)**: 2026-04-19 Phase B-6.7 맵 에디터 — 컨벤션 위반 파일명/include/pragma once 누락 → Manager 재작성.
- **#4 위반 (검증 누락)**: lol2gltf `--flipX` silent ignore → md5 검증 안 하고 다음 단계 진입 → 좌우 반전 코드 우회.

---

## 8. Active Plans

| 트랙 | 상태 | 진입 문서 |
|---|---|---|
| **Track 1: PBR / GGX / Forward+ / SSAO** | W1-W4 ✅ + W5 부분 + W6 보류 (caller 마이그) | `.md/plan/graphics/GGX+A/00_INDEX.md` (+ 01~05) |
| Track 1 확장 (Clustered Deferred / TAA / IBL / Motion Vec / FromSoft Lessons) | 박제 완료, 2차 트랙 | `.md/plan/graphics/Graphics/00_INDEX.md` (+ 01~06) |
| Graphics Stage 마스터 인덱스 (전체 1~10) | 박제 완료 | `.md/plan/graphics/00_GRAPHICS_PLAN_INDEX.md` |
| **Track 2: RHI 멀티 백엔드** | W3 RH-1 seed + W5 rename + W6 RH-3 4 인터페이스 ✅ | `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md` |
| Track 2: W2-W6 박제 (Twin Track) | 5 파일, codex 적용 일부 | `.md/plan/engine/2026-05-0{2,3,4,5,6}_WEEK_*_DETAILED_BAKE.md` |
| Track 2: W7-9 (RH-5 DX12 compile-only) | 박제 완료, codex 진입 대기 | `.md/plan/engine/2026-05-07_WEEK_7_9_DETAILED_BAKE.md` |
| Track 2: W10-13 (RH-5 DX12 visual parity) | 박제 완료, W9 합격 후 | `.md/plan/engine/2026-05-10_WEEK_10_13_DETAILED_BAKE.md` |
| Track 2: W14-17 (RH-6 Vulkan, 선택) | 박제 완료, cross-platform 결정 시 | `.md/plan/engine/2026-05-14_WEEK_14_17_DETAILED_BAKE.md` |
| **Track 3: Fiber JobSystem (Phase 5-B)** ★ 신규 | 박제 완료 + codex 추가 검토 (2026-05-02) | `.md/plan/engine/FIBER_JOB_SYSTEM.md` |
| Twin Track 통합 마스터 | 박제 완료 (codex 14 패치 반영) | `.md/plan/engine/2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md` |
| TCP MVP 2-client demo (Sim-04a v2) | 박제 완료, 보류 | `.md/plan/sim/04a_MVP_2CLIENT_TCP_DEMO_v2.md` (+ D-0~D-3 sub-plan) |
| UDP NetStack (Sim-10 v2) | 박제 완료, 04a 합격 후 | `.md/plan/sim/10_UDP_LOL_NETSTACK_MASTER_v2.md` |
| Champion Pipeline (B-10 잔여 / B-12 메쉬 분리) | 박제 완료 | `.md/plan/Champion/00_CHAMPION_PIPELINE_ROADMAP.md` |
| Asset Pipeline (.wmesh/.wskel/.wanim Stage 1~12) | Stage 3 완료 | `.md/plan/WintersFormat/00_WINTERS_FORMAT_INDEX.md` |
| Backend (Phase 0~7 ✅, Phase 8 진행) | Phase 0~7 ✅ | `.md/plan/backend/00_BACKEND_PLAN_INDEX.md` |
| Security (Phase 0~3) | 박제 완료 | `.md/plan/security/00_SECURITY_INDEX.md` |
| Physics (Stage 1~10) | 박제 완료 | `.md/plan/physics/00_PHYSICS_PLAN_INDEX.md` |
| AI Bot (Stage 0~13) | 박제 완료, 챔프 파이프라인 후 | `.md/plan/ai/00_AI_PLAN_INDEX.md` |
| Effect Tool (Stage 1~10) | 박제 완료 | `.md/plan/EffectTool/00_EFFECT_TOOL_PLAN_INDEX.md` |

### 스킬 (Skill 명령)

- `/code` — 코드 작성+리뷰 사이클 (`winters-skills/code/SKILL.md`)
- `/debug-pipeline` — 셰이더 우선 Read + 데이터 직접 계측
- `/review` — 수정 계획서 검증 + 상세 수정 계획서 생성
- `/testing` — GoogleTest 단위 테스트
- `/todo` — 전체 구현 계획
- `/phase-d-next` / `/phase-2-yasuo` — 챔프 진입 매크로

### 계획서 규칙 (★ 행동 강제)

1. **계획서 작성·참조 시 예외 없이 세션 대화에 전체 내용 (전문) 붙여넣기**. 요약/축약 금지. 길이 문제는 유저가 판단.
2. 생성할 파일마다 h/cpp 코드 전문 포함
3. .h/.cpp 파일 경로 명시 (예: `Engine/Code/RHI/DX11/CDX11Device.cpp`)
4. 줄 번호 명시 (예: L42-L55)
5. 수정 전/후 코드 cpp 블록 포함
6. 추상적 지시 금지 — 반드시 코드 명시
7. 기존 .md 관련 내용 있으면 세션에 불러와서 보여줄 것 (요약 금지, 전문)

### 보안 / 안티치트 (12 원칙 한 줄 요약)

> 풀 본문 + 즉시 적용 금지/허용 + /review 체크리스트: `.md/plan/security/00_SECURITY_INDEX.md`.

1. **서버 권위 (CRITICAL)** — Client prediction OK, 서버 reconciliation 필수. `player.gold += 100` 직접 변경 금지.
2. Client EXE 표면적 최소화 — 민감 로직 Engine DLL.
3. 심볼 가시성 — `WINTERS_ENGINE` 최소, 내부 매니저 export 금지.
4. 중요 값 이중 저장 + 무결성 (Phase 6 ProtectedF32 패턴).
5. 로그/문자열 누출 방지 — Release `OutputDebugString` 금지 (`#ifdef _DEBUG` 필수).
6. 빌드 플래그 — `/guard:cf /GS /sdl /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /OPT:ICF /OPT:REF /LTCG`.
7. 네트워크 패킷 — size-bounded, 시퀀스/timestamp, AES-GCM/ChaCha20-Poly1305.
8. 인증/결제 — `CryptProtectMemory`, JWT 짧은 수명, HTTPS pinning, 영수증 서버 검증.
9. 파일 무결성 — `.wmesh`/`.wanim`/`.wmat` SHA256, 셰이더 cso + 해시.
10. 안티 디버그 (Phase 6) — PEB 직접, `NtQueryInformationProcess`, RDTSC 타이밍.
11. DLL 주입 저항 — `SetProcessMitigationPolicy(ProcessSignaturePolicy)`, IAT 후킹 탐지.
12. 커널 안티치트 (Phase 6) — IOCTL 유저모드 통신, `WINTERS_CRITICAL` 매크로 후크 지점.

---

## 9. Archive Pointer

- **세션 로그 (4월 누적)**: `.md/archive/SESSION_LOG_2026-04.md` — Phase B-4 ~ B-10 직전 완료 / 2차 사이클 / 미결 / 산출물 상세
- **CLAUDE.md 압축 전 원본** (롤백용): `.md/archive/CLAUDE_MD_2026-05-01_PRE_COMPRESSION.md`
- **CLAUDE.md 압축 검토 계획서** (codex): `.md/plan/engine/CLAUDE_COMPRESSION_REVIEW_PLAN_2026_05_01.md`
- **장기 로드맵**: `.md/roadmap/LOL_30DAY_MASTER_PLAN.md` / `.md/roadmap/ROADMAP.md`
- **Engine 아키텍처 / 게임플레이 아키텍처**: `.md/architecture/WINTERS_ENGINE_ARCHITECTURE_FINAL.md` / `WINTERS_GAMEPLAY_ARCHITECTURE.md`
- **학원 DX11 × Winters 매핑**: `.md/architecture/CLASS_DAY8_VS_WINTERS.md` (병합 철학 결정 매트릭스)
