# S16. RHI 현업식 완성 + 공용 Scene Renderer — Codex 핸드오프

작성일: 2026-06-13
상위 북극성: `S13_LOL_TO_ELDEN_SHARED_RHI_RENDER_PIPELINE.md` + `.md/architecture/WINTERS_CODEBASE_COMPASS.md`의 "RHI 방향"
세션 인덱스: `00_RHI_SESSION_INDEX.md` (이 문서는 S13의 F2~F3을 실제 main 위에서 잇는 핸드오프다)

> 이 문서의 "프롬프트 본문"을 그대로 Codex에 붙여넣으면 작업 시작.
> 설계 의도와 경계는 본문 위 절들이 소유하고, Codex는 프롬프트 본문 + 참조 문서로 움직인다.

---

## 1. 북극성 (바뀌면 안 되는 방향)

```text
WintersEngine.dll
├── WintersGame.exe / WintersLOL.exe   // MOBA, DX11 기본 경로
└── WintersElden.exe + EldenRingEditor // Action RPG, DX12 기준
```

- 하나의 엔진이 backend(DX11/DX12/…)를 **선택**한다. backend는 프로젝트가 아니라 `Engine/Private/RHI/<Backend>/` 구현이다.
- LoL과 Elden은 renderer class를 복제하지 않는다. **같은 RHI renderer에 서로 다른 `RenderWorldSnapshot`을 공급**한다.
- DX11은 폐기 대상이 아니라 첫 backend다. DX12는 같은 RHI contract를 구현하는 두 번째 backend다.
- 합격선: `Client/Public`·`Shared`·`EngineSDK/inc`에서 `ID3D11*`/`d3d11.h`/`DX11Shader`/`DX11Pipeline`이 사라진다. DX11 concrete는 Engine backend 안에만 산다.
- 별도 `Smoke.exe`/`DX12.exe`/`DX12.vcxproj`를 만들지 않는다. 검증은 실제 실행 파일 + command line backend 선택으로 한다.

## 2. 현재 검증된 상태 (Codex가 다시 만들지 말 것)

이 상태는 빌드(ninja + `msbuild Winters.sln`)와 런타임 스모크(DX11/DX12 양쪽)로 확인됐다.

### 인터페이스 — `Engine/Public/RHI/`
- `IRHIDevice`가 핸들 기반 API로 닫혀 있다: `CreateBuffer/Shader/Texture/Sampler/Pipeline/RenderPass/BindGroupLayout/BindGroup` + `GetFrameCommandList`.
- `RHIDescriptors.h`에 `RHISamplerDesc` + `eRHIFilter`/`eRHIAddressMode`가 있고, `IRHIDevice::CreateSampler/DestroySampler`가 인터페이스에 있다.
- `IRHICommandList`: `SetPipeline/SetBindGroup/SetVertexBuffer/SetIndexBuffer/Draw/DrawIndexed/UpdateBuffer/TransitionResource`.

### DX11 backend — `Engine/Private/RHI/DX11/CDX11Device.cpp` (RHI 경로 실동작)
- `CreateBuffer/CreateShader/CreateTexture/CreateSampler`가 리소스 테이블과 함께 실제 구현됨.
- `CreatePipeline`은 셰이더 핸들이 유효하면 input layout/rasterizer/depth/blend까지 네이티브 PSO를 만든다. 핸들 없이 desc만 보관하던 legacy 호출자는 그대로 동작한다(데이터 전용).
- `CDX11FrameCommandList`가 immediate context를 `IRHICommandList`로 감싼다. `SetBindGroup`은 CB/SRV/Sampler를 slot visibility 비트(Vertex/Pixel)대로 바인딩한다.

### DX12 backend — `Engine/Private/RHI/DX12/DX12Device.cpp`
- `CreateTexture`: committed resource + `GetCopyableFootprints` 정렬 staging 업로드 + SRV state 전이.
- shader-visible SRV heap(1024) / Sampler heap(256) + range free-list 할당자(`DescriptorHeaps`).
- BindGroup이 descriptor-set처럼 **연속 descriptor range를 영속 소유**한다. root signature는 CBV=root descriptor, SRV/Sampler=`register space = bindGroup index`의 descriptor table로 자동 생성된다.
- `BeginFrame`이 SRV/Sampler heap을 커맨드리스트에 바인딩한다.

### 공용/도구
- `ImGuiLayer`가 `IRHIDevice::GetBackend()`로 DX11/DX12 backend를 분기한다(DX12는 자체 SRV heap 할당자 + InitInfo 콜백).
- `CEngineApp`는 `m_bImGuiRuntimeEnabled`로 ImGui를 backend-중립으로 띄우고, legacy 부트스트랩(SharedShaders/UI/BlendStateCache)만 `m_bDX11RuntimeEnabled` gate에 남긴다.
- `EldenRingEditor`가 빌드 복구됨(타깃명 `WintersEldenRingEditor` 통일, 루트 `add_subdirectory`, `main.cpp` 수정). DX12 + ImGui로 뜬다.
- `EldenRingRHITestCubeRenderer`가 **체커보드 텍스처드 큐브**로, backend에 따라 `vs_5_0`(DX11)/`vs_5_1`(DX12)을 골라 **동일 코드가 두 backend에서** 돈다.
- 빌드 인프라: `cmake/WintersEngine.cmake`에 `External/tracy` include 추가, `ProfilerAPI.h`가 tracy include를 `push_macro/undef/pop_macro("new")`로 격리(디버그 `new` 매크로 충돌 차단).

## 3. 남은 갭 (이 세션이 메우는 대상)

| 갭 | 현재 | 목표 |
|---|---|---|
| G1 DX12 프레임 파이프라이닝 | `EndFrame`이 매 프레임 `WaitForGpu`로 완전 동기화 (CPU/GPU 오버랩 0) | 프레임별 fence 값으로 allocator 재사용 시점에만 대기. `kFrameCount` 인플라이트 활용 |
| G2 DX12 mid-frame descriptor 안전성 | BindGroup이 영속 descriptor range 소유 → 한 프레임에 `UpdateBindGroup`으로 텍스처 교체 시 마지막 descriptor만 보임 | per-frame shader-visible descriptor ring. `SetBindGroup` 시점에 CPU-staging heap → ring으로 `CopyDescriptors` 후 그 GPU 핸들 바인딩 |
| G3 DX12 dynamic buffer 버전닝 | upload-heap dynamic buffer가 `WaitForGpu` 덕에만 안전 | 파이프라이닝 도입 시 frame-in-flight 수만큼 ring 버전닝 |
| G4 빌드 안정성 | `msbuild`가 병렬 시 간헐 C1041(PDB 동시 쓰기). 임시로 `CL=/FS`로 우회 중 | 모든 `.vcxproj` ClCompile에 `/FS` 영구 반영 |
| G5 공용 Scene Renderer 부재 | LoL `InGameRenderBridge`가 `GameInstance::Get_*Shader/Get_*Pipeline/Get_BlendStateCache` 같은 DX11 concrete getter로 그린다 | `RenderWorldSnapshot` + `CRHISceneRenderer` 도입(S13 F2). LoL이 첫 소비자, Elden이 같은 renderer 소비 |

G1~G3은 DX12 device를 "현업식"으로 만드는 하드닝, G4는 위생, G5가 북극성(공용 renderer) 본체다. 순서는 G4 → G1~G3 → G5.

## 4. 작업 경계 (절대 금지)

- `Client/Public`·`Shared`·`EngineSDK/inc`에 `ID3D11*`/`ID3D12*`/`d3d11.h`/`d3d12.h`/`DX11Shader`/`DX11Pipeline`을 새로 노출하지 않는다.
- 빌드 통과를 위해 DX12 path를 legacy DX11 concrete type으로 되돌리지 않는다.
- `Smoke.exe`/`DX12.exe`/`DX12.vcxproj`/별도 backend app project를 만들지 않는다.
- LoL renderer와 Elden renderer를 별도 class hierarchy로 복제하지 않는다. Elden은 `RenderWorldSnapshot`만 Elden식으로 만든다.
- Section 2의 검증된 코드를 재작성하지 않는다. 확장/소비만 한다.
- `EngineSDK/inc/...`를 직접 수정하지 않는다. Engine public header 변경 후 `UpdateLib.bat`로 동기화한다.
- normal F5 LoL DX11 visual을 숨기거나 우회해서 갭을 메우지 않는다.

## 5. 검증 (모든 단계 공통)

```powershell
# 환경
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo

# 경계 audit (Public/SDK에 backend native가 새지 않는지)
rg -n "ID3D11|ID3D12|d3d11.h|d3d12.h|DX11Shader|DX11Pipeline" Engine/Public Engine/Include Client/Public Shared EngineSDK/inc

# 빌드 (둘 다 통과해야 함)
cmake --build out/build/msvc-ninja --config Debug --target WintersEngine WintersElden WintersEldenRingEditor
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m   # C1041 나면 G4 미완

git diff --check
```

런타임은 같은 실행 파일 + backend 선택으로만 검증한다.

```powershell
EldenRingClient/Bin/Debug/WintersElden.exe                 # DX12 기본 — 체커보드 큐브 + ImGui 오버레이
EldenRingClient/Bin/Debug/WintersElden.exe --rhi=dx11      # 같은 코드, DX11
EldenRingEditor/Bin/Debug/WintersEldenRingEditor.exe       # DX12 + ImGui
Client/Bin/Debug/WintersGame.exe                           # LoL DX11 회귀 (visual 유지)
```

성공 판정은 "코드가 빌드됐다"가 아니라 **DX11/DX12에서 같은 장면이 같게 보이고 LoL이 회귀하지 않는 것**이다.

---

## 프롬프트 본문 (복붙용)

```text
PHASE=G4   # G4=빌드위생, HARDEN=DX12파이프라이닝+디스크립터링, RENDERER=공용 Scene Renderer

너는 Winters 엔진의 RHI(Render Hardware Interface)를 현업식 production 수준으로
완성하는 시니어 그래픽스 엔지니어다. 하나의 WintersEngine.dll이 DX11/DX12 backend를
선택하고, LoL과 Elden이 같은 RHI renderer에 서로 다른 world snapshot을 공급하는
북극성을 유지한 채로 작업한다. 완료될 때까지 단계별로 진행한다.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 빌드: VsDevCmd.bat(-arch=x64) 후 cmake --preset msvc-ninja / msbuild Winters.sln
- 실행: WintersElden.exe(DX12 기본, --rhi=dx11 가능), WintersEldenRingEditor.exe, WintersGame.exe(LoL DX11)

[반드시 먼저 읽을 문서 — 순서대로]
1. .md/plan/rhi/sessions/S16_RHI_PRODUCTION_HARDENING_CODEX_HANDOFF.md  ← 이 작업의 설계·경계·검증(본 문서)
2. .md/plan/rhi/sessions/S13_LOL_TO_ELDEN_SHARED_RHI_RENDER_PIPELINE.md  ← 공용 renderer 목표 아키텍처(F2~F6)
3. .md/architecture/WINTERS_CODEBASE_COMPASS.md                          ← 계층 책임·RHI 방향·금지
4. CLAUDE.md / .claude/gotchas.md                                       ← 코딩 규칙·재발 방지

[현재 검증된 상태 — 재작성 금지, 확장만]
- IRHIDevice가 buffer/shader/texture/sampler/pipeline/renderpass/bindgroup + GetFrameCommandList로 닫혀 있다.
- CDX11Device: CreateBuffer/Shader/Texture/Sampler 실동작 + CreatePipeline 네이티브 PSO + CDX11FrameCommandList(immediate).
- CDX12Device: CreateTexture(footprint staging)/CreateSampler, SRV(1024)/Sampler(256) shader-visible heap +
  range free-list, BindGroup이 영속 descriptor range 소유, root sig(CBV=root, SRV/Sampler=table, space=group index),
  BeginFrame이 heap 바인딩.
- ImGuiLayer가 DX11/DX12 분기. EldenRingEditor 빌드 복구됨. TestCube는 체커보드 텍스처드 큐브(vs_5_0/5_1 자동).
세부는 S16 문서 2절을 본다. 이 코드를 다시 만들지 말고 소비/확장한다.

[단계별 작업 — PHASE 변수에 따름, 위에서 아래로 의존]

PHASE=G4 (빌드 위생, 선결):
- 솔루션의 모든 C++ .vcxproj(Engine/Include, Client/Include, Server/Include, Shared/GameSim/Include,
  EldenRingClient/Include, Tools 하위) Debug/Release ClCompile에 /FS를 영구 추가(이미 있으면 skip).
- DoD: 중간 산출물 삭제 후 msbuild Winters.sln /m 을 2회 연속 실행해도 C1041 미재발.

PHASE=HARDEN (DX12 production 파이프라이닝 — Engine/Private/RHI/DX12/DX12Device.cpp):
- G1: EndFrame의 매 프레임 WaitForGpu를 제거하고 프레임별 fence 값을 둔다. allocator[frameIndex] 재사용 직전에만
  해당 프레임 fence를 기다린다. kFrameCount 인플라이트를 실제로 활용한다.
- G2: per-frame shader-visible descriptor ring을 도입한다. BindGroup은 CPU-only staging heap에 descriptor를 굽고,
  SetBindGroup 시점에 staging→ring으로 CopyDescriptorsSimple 후 그 GPU 핸들을 root table에 바인딩한다.
  목적: 한 프레임에 UpdateBindGroup으로 텍스처를 바꿔도(FxStaticMeshRenderer per-draw 텍스처 스왑)
  실행 시점에 올바른 descriptor가 보이게 한다. ring은 프레임 경계에서 reset.
- G3: upload-heap dynamic buffer(UpdateBuffer 경로)를 frame-in-flight 수만큼 버전닝한다(파이프라이닝 race 차단).
- ImGuiLayer DX12 경로의 자체 SRV heap과 ring/heap이 충돌 없이 공존하는지 확인한다.
- DoD: WintersElden.exe(DX12)와 --rhi=dx11에서 체커보드 큐브가 동일. FxStaticMeshRenderer가 여러 텍스처를
  한 프레임에 그릴 때 텍스처가 섞이지 않음. ImGui 오버레이 정상.

PHASE=RENDERER (공용 Scene Renderer — S13 F2~F3 본체):
- RenderWorldSnapshot 타입을 추가한다(S13 2절: RenderViewDesc + RenderMeshItem + RenderFxItem + RenderDebugItem,
  초기엔 std::vector 기반 CPU snapshot). 위치는 Engine/Public/Renderer/RenderWorldSnapshot.h.
- CRHISceneRenderer(Engine/Public+Private/Renderer)를 추가한다. 입력은 RenderWorldSnapshot, 내부는 RHI 핸들/
  CommandList만 사용한다. 최소 pass: static mesh forward + sprite FX + debug. DX11/DX12 동일 동작.
- CRHIMeshResource/CRHIMaterialResource로 CModel의 vertex/index buffer 소유를 RHI 핸들로 분리한다(기존 DX11
  fast path는 DX11 backend 구현으로 흡수). 한 번에 다 옮기지 말고 mesh 1체부터.
- LoL InGameRenderBridge가 RenderWorldSnapshot을 작성해 CRHISceneRenderer::Render(snapshot)을 호출하도록
  점진 이관한다. GameInstance::Get_*Shader/Get_*Pipeline/Get_BlendStateCache 의존을 줄인다(한 번에 제거 금지,
  mesh→FX→debug 순서로 줄이고 매 단계 LoL visual 회귀 확인).
- 같은 CRHISceneRenderer를 EldenRing(TestCube 자리)에서도 호출해 한 renderer 두 소비자를 증명한다.
- DoD: rg로 Client/Public의 DX11Shader/DX11Pipeline/CBlendStateCache hit이 감소. LoL DX11 visual 유지.
  DX11/DX12 둘 다에서 같은 mesh가 같게 렌더. Engine public header 변경 시 UpdateLib.bat 실행.

[경계 — 절대 금지]
- Client/Public·Shared·EngineSDK/inc에 ID3D11*/ID3D12*/d3d11.h/d3d12.h/DX11Shader/DX11Pipeline 신규 노출 금지.
- 빌드 통과 목적으로 DX12 path를 legacy DX11 concrete로 되돌리기 금지.
- Smoke.exe/DX12.exe/DX12.vcxproj/별도 backend app project 생성 금지.
- LoL/Elden renderer class hierarchy 복제 금지(Elden은 snapshot만 다르게).
- EngineSDK/inc 직접 수정 금지(UpdateLib.bat로 동기화).
- normal F5 LoL DX11 visual을 숨기거나 우회해서 갭 메우기 금지.

[검증 — 매 단계]
- rg -n "ID3D11|ID3D12|d3d11.h|d3d12.h|DX11Shader|DX11Pipeline" Engine/Public Engine/Include Client/Public Shared EngineSDK/inc
- cmake --build out/build/msvc-ninja --config Debug --target WintersEngine WintersElden WintersEldenRingEditor
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
- git diff --check
- 런타임: WintersElden.exe / WintersElden.exe --rhi=dx11 / WintersEldenRingEditor.exe / WintersGame.exe
- 성공 판정: 빌드가 아니라 "DX11/DX12에서 같은 장면이 같게 보이고 LoL 회귀 없음".

[작업 루프]
1. PHASE의 DoD를 성공 기준으로 고정한다(테스트/관찰 가능한 형태로).
2. 가장 작은 변경으로 한 항목을 구현한다(Karpathy 가드레일: 추측 금지, 최소 구현, surgical).
3. 위 검증을 돌린다. DX11/DX12 양쪽 런타임 확인.
4. 회귀가 있으면 원인을 고친 뒤 다음 항목. 막히면(설계 판단 필요/영구 실패) 사유를 분류해 보고.
5. PHASE DoD가 전부 green이면 다음 PHASE로.

[시작]
지금: (1) 위 문서 4종을 읽고, (2) 현재 PHASE의 DoD를 성공 기준으로 적은 뒤,
(3) rg 경계 audit + 빌드로 현재 상태를 확인하고, (4) 첫 항목 구현을 시작하라.
막히면 사유를 (a)재시도 가능 (b)설계상 정상 (c)버그로 분류해 보고하고 나머지는 계속 진행하라.
```

---

## 부록 A. PHASE별 첫 명령 예시

```powershell
# G4 확인 (현재 우회책)
$env:CL='/FS'; msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m

# HARDEN 빌드+런타임
cmake --build out/build/msvc-ninja --config Debug --target WintersElden
EldenRingClient/Bin/Debug/WintersElden.exe          # DX12
EldenRingClient/Bin/Debug/WintersElden.exe --rhi=dx11

# RENDERER 경계 audit (hit 감소를 추적)
rg -c "DX11Shader|DX11Pipeline|CBlendStateCache" Client/Public Client/Private/Scene/InGameBootstrapBridge.cpp
```

## 부록 B. 핵심 파일 지도

| 영역 | 파일 |
|---|---|
| 인터페이스 | `Engine/Public/RHI/IRHIDevice.h`, `IRHICommandList.h`, `RHIDescriptors.h`, `RHITypes.h`, `RHIHandles.h` |
| DX11 backend | `Engine/Private/RHI/DX11/CDX11Device.{h,cpp}` |
| DX12 backend | `Engine/Private/RHI/DX12/DX12Device.{h,cpp}` |
| ImGui backend 분기 | `Engine/Private/Editor/ImGuiLayer.cpp`, `Engine/Private/Framework/CEngineApp.cpp` |
| LoL 렌더 진입 | `Client/Private/Scene/InGameRenderBridge.cpp`, `InGameBootstrapBridge.cpp` |
| 공용 renderer 목표 | `Engine/Public/Renderer/{RenderWorldSnapshot,RHISceneRenderer,RHIMeshResource,RHIMaterialResource}.h` (신규) |
| RHI 소비 예시 | `EldenRingClient/Private/EldenRingRHITestCubeRenderer.cpp` |
