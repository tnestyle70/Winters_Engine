# S17. RHI ring 정합성 게이트 + 공용 Scene Renderer — Codex 핸드오프

작성일: 2026-06-13
상위 북극성: `S13_LOL_TO_ELDEN_SHARED_RHI_RENDER_PIPELINE.md` + `.md/architecture/WINTERS_CODEBASE_COMPASS.md` "RHI 방향"
선행: `S16_RHI_PRODUCTION_HARDENING_CODEX_HANDOFF.md` (G1~G4 완료, 본 문서 1절 검토 결과 참조)

> 프롬프트 본문을 그대로 Codex에 붙여넣으면 작업 시작. 설계·경계·검증은 본문 위 절들이 소유한다.

---

## 1. S16(G1~G4) 검토 결과 — 실제 코드 기준

실측 검증(빌드 ninja + `msbuild /m`, 런타임 4종 생존)으로 확인한 현재 상태다.

### 통과 (재작성 금지)
- **G1 프레임 파이프라이닝** — `DX12Device.cpp`: `EndFrame`이 `WaitForGpu` 대신 프레임별 fence(`m_uFrameFenceValues[completedFrameIndex]`)를 signal하고 present한다. `BeginFrame`이 `WaitForFrame(m_iFrameIndex)`로 allocator 재사용 직전에만 대기. 정상.
- **G3 dynamic buffer 버전닝** — upload-heap 버퍼가 `frameResources[kFrameCount]`를 갖고, `GetDX12BufferResource(buffer, frameIndex)`가 프레임별 리소스를 돌려준다. `UpdateBuffer`/`SetVertexBuffer`/CBV가 전부 프레임별 리소스를 쓴다. 정상.
- **G4 `/FS`** — `Client/Server/GameSim` vcxproj에 `/utf-8 /FS` 반영. `msbuild /m` 병렬 빌드에서 C1041 미재발 확인. 정상.

### P0 결함 (이번 세션 선결) — descriptor ring의 frame-in-flight 파티션 누락
- `DX12Device.cpp`의 `DescriptorHeaps`: shader-visible ring(`pSrvHeap`, 1024 / `pSamplerHeap`, 256)을 두 프레임이 **공유**한다. `AllocFrameSrv(frameIndex,...)`가 `srvRingNext[frameIndex]`를 0부터 시작하고, `SrvRingCpuAt/GpuAt(base)`에 프레임 오프셋이 없다.
- 결과: 프레임 0과 프레임 1이 **같은 물리 슬롯 0부터** 기록한다. G1이 프레임별 wait만 하므로, 프레임 N+1의 `CopyDescriptorsSimple`이 프레임 N의 in-flight GPU가 읽는 ring 슬롯을 덮어쓴다. → cross-frame descriptor aliasing.
- 현재 증상이 안 보이는 이유: TestCube는 매 프레임 같은 텍스처만 바인딩해서 덮어써도 동일하다. **FX per-draw 텍스처 스왑이나 곧 들어올 Scene Renderer의 다중 머티리얼에서 깨진다.**
- 정답: 각 frame-in-flight가 ring heap의 **분리된 sub-range**를 소유한다. 프레임 f의 base = `f * perFrameCap`, `perFrameCap = capacity / kFrameCount`. `BeginFrame`의 `WaitForFrame(f)`가 프레임 f sub-range 재사용 전 완료를 이미 보장한다.

### P1 제약 (문서화 + 가드)
- `CreateBuffer`/`CreateTexture`의 initial-data 경로가 `m_pCommandList`를 `Reset`/`Close`/`Execute` + `WaitForGpu`로 재사용한다. init 시점은 안전하나, **frame 기록 중(BeginFrame~EndFrame) 호출되면 열린 frame command list를 오염**시킨다. Scene Renderer가 리소스를 mid-frame 스트리밍하면 터진다. 별도 upload command allocator/list로 분리하거나, 최소한 `m_bFrameRecording` 중 생성 호출을 차단/지연하고 그 계약을 명시한다.

## 2. 북극성 (바뀌면 안 되는 방향)

- 하나의 `WintersEngine.dll`이 backend(DX11/DX12)를 선택한다. backend는 `Engine/Private/RHI/<Backend>/` 구현이다.
- LoL과 Elden은 renderer를 복제하지 않는다. **같은 RHI renderer에 서로 다른 `RenderWorldSnapshot`을 공급**한다.
- 합격선: `Client/Public`·`Shared`·`EngineSDK/inc`에서 `ID3D11*`/`DX11Shader`/`DX11Pipeline`이 사라진다. DX11 concrete는 Engine backend 안에만 산다.
- 별도 `Smoke.exe`/`DX12.exe`/`DX12.vcxproj`를 만들지 않는다. 검증은 실제 실행 파일 + command line backend 선택.

## 3. 현재 RHI 소비 지도 (RENDERER 단계 앵커)

S13이 가리킨 `InGameRenderBridge.cpp`/`InGameBootstrapBridge.cpp`는 **실재하지 않는다**. LoL 렌더 오케스트레이션의 실제 진입점은 다음이다.

- `Client/Private/Scene/Scene_InGame.cpp` — `OnRender`/세팅에서 `CGameInstance::Get_MeshShader()/Get_MeshPipeline()/Get_BlendStateCache()`를 직접 받아 `ModelRenderer`/`CPlaneRenderer`/`Engine::CNormalPass`를 구성·호출한다(예: line ~1607, ~3430).
- `Engine/Include/GameInstance.h` — `Get_MeshShader()/Get_MeshPipeline()/Get_BlendStateCache()` 등 DX11 concrete getter(line 138~140).
- `Client/Public/GameObject/FX/FxSystem.h`, `FxBeamSystem.h` — `class DX11Shader; class DX11Pipeline;` forward + `DX11Shader*`/`DX11Pipeline*` 멤버(Client/Public DX11 누수의 실제 위치).

이 셋이 RENDERER 단계의 제거/이관 대상이다.

## 4. 이 세션 범위

| 단계 | 내용 | DoD |
|---|---|---|
| P0 RINGFIX | ring heap을 frame-in-flight별 sub-range로 분리. P1 mid-frame 생성 제약 가드+주석 | DX12에서 한 프레임에 서로 다른 텍스처를 가진 BindGroup 2개 이상을 그려도 섞이지 않음(다중 머티리얼 테스트). `cmake`+런타임 통과 |
| F2 SNAPSHOT | `RenderWorldSnapshot` + `CRHISceneRenderer` 골격(static mesh forward + sprite FX + debug), DX11/DX12 동일 동작 | LoL map 또는 champion 1체가 `CRHISceneRenderer`로 렌더. DX11/DX12 둘 다 |
| F3 LOL이관 | `Scene_InGame`이 snapshot 작성→`CRHISceneRenderer::Render(snapshot)` 호출로 점진 이관. `Get_MeshShader/Pipeline/BlendStateCache` 의존과 `FxSystem.h/FxBeamSystem.h`의 `DX11Shader*/DX11Pipeline*` 제거 | `rg`로 Client/Public DX11 hit 0. LoL DX11 visual 무회귀 |

F2/F3은 S13의 RHI-F2/F3 본체다. P0는 파이프라인을 신뢰하기 위한 선결 게이트라 가장 먼저.

## 5. 절대 금지

- `Client/Public`·`Shared`·`EngineSDK/inc`에 `ID3D11*`/`ID3D12*`/`d3d11.h`/`d3d12.h`/`DX11Shader`/`DX11Pipeline` 신규 노출.
- 빌드 통과 목적의 DX12→legacy DX11 concrete 되돌리기.
- `Smoke.exe`/`DX12.exe`/`DX12.vcxproj`/별도 backend app project 생성.
- LoL/Elden renderer class hierarchy 복제(Elden은 snapshot만 다르게).
- S16에서 검증된 G1/G3/G4 코드 재작성. P0는 ring 파티션만 손대고 fence/buffer 로직은 보존.
- `EngineSDK/inc` 직접 수정(Engine public header 변경 후 `UpdateLib.bat`).
- normal F5 LoL DX11 visual을 숨기거나 우회.

## 6. 검증 (매 단계 공통)

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo

rg -n "ID3D11|ID3D12|d3d11.h|d3d12.h|DX11Shader|DX11Pipeline" Engine/Public Engine/Include Client/Public Shared EngineSDK/inc
cmake --build out/build/msvc-ninja --config Debug --target WintersEngine WintersElden WintersEldenRingEditor
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
git diff --check
```

런타임(같은 실행 파일 + backend 선택만):

```powershell
EldenRingClient/Bin/Debug/WintersElden.exe              # DX12
EldenRingClient/Bin/Debug/WintersElden.exe --rhi=dx11   # 같은 코드, DX11
EldenRingEditor/Bin/Debug/WintersEldenRingEditor.exe    # DX12 + ImGui
Client/Bin/Debug/WintersGame.exe                        # LoL DX11 회귀
```

성공 판정은 빌드가 아니라 **DX11/DX12에서 같은 장면이 같게 보이고 LoL 무회귀**.

---

## 프롬프트 본문 (복붙용)

```text
PHASE=RINGFIX   # RINGFIX=descriptor ring 파티션 게이트, SNAPSHOT=RenderWorldSnapshot+Renderer, LOLPORT=LoL 이관

너는 Winters 엔진의 RHI를 현업식으로 완성하는 시니어 그래픽스 엔지니어다.
하나의 WintersEngine.dll이 DX11/DX12 backend를 선택하고, LoL과 Elden이 같은 RHI renderer에
서로 다른 RenderWorldSnapshot을 공급하는 북극성을 유지한 채 작업한다. 단계별로 진행한다.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 빌드: VsDevCmd.bat(-arch=x64) 후 cmake --build out/build/msvc-ninja --config Debug / msbuild Winters.sln /m
- 실행: WintersElden.exe(DX12 기본, --rhi=dx11), WintersEldenRingEditor.exe, WintersGame.exe(LoL DX11)

[먼저 읽을 문서 — 순서대로]
1. .md/plan/rhi/sessions/S17_RHI_SCENE_RENDERER_CODEX_HANDOFF.md   ← 이 작업의 설계·경계·검증(본 문서). 특히 1절 검토 결과.
2. .md/plan/rhi/sessions/S13_LOL_TO_ELDEN_SHARED_RHI_RENDER_PIPELINE.md ← Scene Renderer 목표 아키텍처
3. .md/architecture/WINTERS_CODEBASE_COMPASS.md                    ← 계층 책임·RHI 방향·금지
4. CLAUDE.md / .claude/gotchas.md                                  ← 코딩 규칙·재발 방지

[검증된 현재 상태 — 재작성 금지]
- DX12 G1(프레임 fence 파이프라이닝)/G3(frameResources[kFrameCount] dynamic buffer)/G4(/FS)는 완료·검증됨.
- DX11/DX12 둘 다 체커보드 텍스처드 TestCube가 동일하게 렌더되고, Editor/LoL 회귀 없음.
- IRHIDevice는 buffer/shader/texture/sampler/pipeline/renderpass/bindgroup + GetFrameCommandList로 닫혀 있다.
이 코드를 다시 만들지 말고 확장/소비한다.

[단계별 작업 — PHASE 변수에 따름, 위에서 아래로 의존]

PHASE=RINGFIX (선결 게이트 — Engine/Private/RHI/DX12/DX12Device.cpp):
- 결함: shader-visible descriptor ring(pSrvHeap 1024 / pSamplerHeap 256)을 두 frame-in-flight가 공유하고,
  AllocFrameSrv/Sampler가 frameIndex와 무관하게 0부터 인덱싱한다. G1이 프레임별 wait만 하므로
  프레임 N+1의 CopyDescriptorsSimple이 프레임 N의 in-flight 슬롯을 덮어쓴다(cross-frame aliasing).
- 수정: ring을 frame별 sub-range로 분리한다. perFrameCap = capacity / kFrameCount. 프레임 f의 ring base는
  f*perFrameCap에서 시작하고 srvRingNext[f]는 [0, perFrameCap) 범위만 쓴다. SrvRingCpuAt/GpuAt(또는
  AllocFrameSrv 반환 base)에 프레임 오프셋을 반영한다. 샘플러 ring도 동일. fence/buffer 로직은 건드리지 않는다.
- P1 가드: CreateBuffer/CreateTexture의 initial-data 경로가 m_pCommandList를 reset/execute/WaitForGpu로
  재사용한다. m_bFrameRecording 중 호출되면 열린 frame command list를 오염시킨다. 별도 upload allocator/list로
  분리하거나, 최소한 frame 기록 중 생성을 막고 그 계약을 주석/문서로 명시한다.
- DoD: 한 프레임에 서로 다른 텍스처를 가진 BindGroup 2개 이상을 그려도 섞이지 않는다(임시 멀티 머티리얼
  스모크로 증명). DX11/DX12 TestCube 동일. msbuild /m + 런타임 4종 통과.

PHASE=SNAPSHOT (공용 Scene Renderer 골격 — S13 F2):
- Engine/Public/Renderer/RenderWorldSnapshot.h 추가(S13 2절: RenderViewDesc + RenderMeshItem + RenderFxItem
  + RenderDebugItem, 초기 std::vector CPU snapshot).
- Engine/Public+Private/Renderer/CRHISceneRenderer 추가. 입력은 RenderWorldSnapshot, 내부는 RHI 핸들/CommandList만
  사용. 최소 pass: static mesh forward + sprite FX + debug. DX11/DX12 동일 동작.
- CRHIMeshResource/CRHIMaterialResource로 CModel의 vertex/index buffer 소유를 RHI 핸들로 분리(기존 DX11 fast path는
  DX11 backend 구현으로 흡수). mesh 1체부터, 한 번에 다 옮기지 않는다.
- DoD: LoL map 또는 champion 1체가 CRHISceneRenderer로 렌더. DX11/DX12 둘 다 같게.

PHASE=LOLPORT (LoL 이관 — S13 F3):
- Client/Private/Scene/Scene_InGame.cpp가 RenderWorldSnapshot을 작성해 CRHISceneRenderer::Render(snapshot)을
  호출하도록 점진 이관. mesh → FX → debug 순서로 옮기고 매 단계 LoL visual 회귀를 확인한다.
- GameInstance::Get_MeshShader/Get_MeshPipeline/Get_BlendStateCache 의존을 제거하고, Client/Public/GameObject/FX/
  FxSystem.h·FxBeamSystem.h의 DX11Shader*/DX11Pipeline* 멤버와 forward 선언을 제거한다.
- 같은 CRHISceneRenderer를 EldenRing(TestCube 자리)에서도 호출해 한 renderer 두 소비자를 증명한다.
- DoD: rg로 Client/Public의 DX11Shader/DX11Pipeline hit 0. LoL DX11 visual 무회귀. DX11/DX12 동일 mesh 렌더.
  Engine public header 변경 시 UpdateLib.bat 실행.

[경계 — 절대 금지]
- Client/Public·Shared·EngineSDK/inc에 ID3D11*/ID3D12*/d3d11.h/d3d12.h/DX11Shader/DX11Pipeline 신규 노출.
- 빌드 통과 목적의 DX12→legacy DX11 concrete 되돌리기.
- Smoke.exe/DX12.exe/DX12.vcxproj/별도 backend app project 생성.
- LoL/Elden renderer 복제(Elden은 snapshot만 다르게).
- S16 검증된 G1/G3/G4 재작성(P0는 ring 파티션만).
- EngineSDK/inc 직접 수정. normal F5 LoL DX11 visual 우회.

[검증 — 매 단계]
- rg -n "ID3D11|ID3D12|d3d11.h|d3d12.h|DX11Shader|DX11Pipeline" Engine/Public Engine/Include Client/Public Shared EngineSDK/inc
- cmake --build out/build/msvc-ninja --config Debug --target WintersEngine WintersElden WintersEldenRingEditor
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
- 런타임: WintersElden.exe / WintersElden.exe --rhi=dx11 / WintersEldenRingEditor.exe / WintersGame.exe
- 성공 판정: "DX11/DX12에서 같은 장면이 같게 보이고 LoL 회귀 없음". 빌드 성공만으로 완료 선언 금지.

[작업 루프]
1. PHASE DoD를 관찰 가능한 성공 기준으로 고정한다.
2. 최소 변경으로 한 항목 구현(Karpathy: 추측 금지, surgical, 기존 스타일 유지).
3. 위 검증 + DX11/DX12 양쪽 런타임 확인.
4. 회귀 시 원인 수정 후 다음 항목. 막히면 (a)재시도 가능 (b)설계상 정상 (c)버그로 분류해 보고.
5. PHASE DoD 전부 green이면 다음 PHASE로.

[시작]
지금: (1) 문서 4종을 읽고, (2) 현재 PHASE DoD를 적은 뒤, (3) rg 경계 audit + 빌드로 현재 상태 확인,
(4) 첫 항목 구현 시작. 막히면 사유를 분류해 보고하고 나머지는 계속 진행하라.
```

---

## 부록. 핵심 파일 지도

| 영역 | 파일 |
|---|---|
| DX12 ring(P0) | `Engine/Private/RHI/DX12/DX12Device.cpp` `DescriptorHeaps` / `SetBindGroup` / `BeginFrame` |
| LoL 렌더 진입(F3) | `Client/Private/Scene/Scene_InGame.cpp` (line ~1607 getter, ~3430 NormalPass) |
| DX11 getter(F3) | `Engine/Include/GameInstance.h` (line 138~140) |
| Client/Public DX11 누수(F3) | `Client/Public/GameObject/FX/FxSystem.h`, `FxBeamSystem.h` |
| 신규 공용 renderer(F2) | `Engine/Public/Renderer/{RenderWorldSnapshot,RHISceneRenderer,RHIMeshResource,RHIMaterialResource}.h` |
| RHI 소비 예시 | `EldenRingClient/Private/EldenRingRHITestCubeRenderer.cpp` |
