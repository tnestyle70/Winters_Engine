# DX12 RHI Remaining Plan And Progress

작성일: 2026-05-07

## 1. 현재 반영 상태

DX12 bootstrap은 메인 코드베이스에 반영되었다.

완료:
- `RHISurfaceDesc` / `RHICapabilities` public RHI 계약 추가.
- `IPlatformSurface` / `IPlatformWindow` / `PlatformNativeHandle` 추가.
- `CWin32Window`가 Win32 window와 RHI surface bridge 역할을 제공.
- `IRHIDevice`가 `RHISurfaceDesc` 기반 swapchain 생성 경로를 제공.
- `CDX12Device`가 `RHISurfaceDesc`로 device/swapchain bootstrap 가능.
- `DX12SmokeHost` 프로젝트를 `Winters.sln`에 등록.
- `Debug-DX12`에서 `Engine` + `DX12SmokeHost` 빌드 통과.
- `DX12SmokeHost.exe` 8초 생존 smoke 통과.
- 기존 `Debug` DX11 `Engine` 빌드 통과.
- Public/SDK에 노출되던 `Engine/Public/RHI/DX11/*`와 `CDX11Device.h`를 `Engine/Private/RHI/DX11/*`로 이동.
- `Engine_Defines.h` public `<d3d11.h>` include 제거.
- `UI_Manager` public header에서 `ID3D11ShaderResourceView*` 노출 제거.

현재 public DX11 leak 확인:
- `Engine/Public`
- `Engine/Include`
- `Client/Public`
- `EngineSDK/inc`

위 범위에서 `ID3D11`, `D3D11`, `d3d11.h`, `RHI/DX11`, `CDX11Device` 검색 결과는 코드 기준 0건이다.
단, `EngineSDK/inc/imgui.h` 내부 주석의 DX11 예시 1건은 third-party 주석이라 제외한다.

## 2. 이번 세션 추가 진행

### RH-10-A. CommandList 생성 루트 개방

`IRHICommandList`와 `CDX12CommandList`는 이미 존재했지만, `IRHIDevice`에서 command list를 생성하고 소유할 방법이 없었다.

이번 추가 반영:
- `IRHIDevice::CreateCommandList()`
- `IRHIDevice::DestroyCommandList(IRHICommandList*)`
- `CDX12Device::CreateCommandList()`
- `CDX12Device::DestroyCommandList(IRHICommandList*)`
- `CDX12Device` 내부 `std::vector<std::unique_ptr<CDX12CommandList>> m_CommandLists`

이제 renderer 또는 smoke test가 backend-neutral하게 command list를 요청할 수 있다.

## 3. 남은 작업 패키지

### RH-10-B. CommandList 실제 바인딩

목표:
- `SetPipeline`
- `SetBindGroup`
- `SetVertexBuffer`
- `SetIndexBuffer`
- `TransitionResource`

현재 `CDX12CommandList`의 draw/dispatch는 native call로 연결되어 있지만, pipeline/buffer/bind group은 stub이다.

다음 구현 순서:
1. DX12 buffer handle table을 `CDX12Device`에 추가.
2. `RHIBufferHandle -> CDX12BufferImpl*` lookup 추가.
3. vertex/index buffer view 생성.
4. `SetVertexBuffer` / `SetIndexBuffer` native binding.
5. `RHIResourceState -> D3D12_RESOURCE_STATES` 변환 helper 추가.
6. buffer/texture transition barrier 기록.

### RH-11. Descriptor / BindGroup

목표:
- CBV/SRV/UAV descriptor heap allocator.
- sampler heap allocator.
- bind group layout을 root signature 입력으로 변환.
- bind group resource update 시 descriptor write.

주의:
- DX12는 descriptor lifetime이 draw 제출보다 길어야 한다.
- frame allocator와 persistent descriptor allocator를 분리한다.

### RH-12. Shader / Pipeline 실제화

목표:
- DXC 기반 shader compile path 고정.
- root signature 생성.
- graphics/compute PSO 생성.
- `RHIPipelineDesc`를 DX12 PSO로 변환.

주의:
- 지금 `CDX12PipelineState`는 desc 보관용 scaffold다.
- 실제 PSO 생성은 shader bytecode handle table과 root signature가 먼저 필요하다.

### RH-13. First Real Draw

목표:
- DX12 전용 triangle smoke renderer.
- upload buffer로 vertex data 업로드.
- command list로 root signature / PSO / VB binding / draw.

성공 기준:
- `DX12SmokeHost`에서 clear color가 아닌 삼각형이 보인다.
- PIX/RenderDoc에서 command list가 유효하게 보인다.

### RH-14. Renderer Migration

권장 순서:
1. Triangle/Cube smoke renderer.
2. PlaneRenderer.
3. CTexture upload/read path.
4. static ModelRenderer.
5. skinned ModelRenderer.
6. Fx sprite / Fx mesh.
7. ImGui DX12 backend.
8. Client `Debug-DX12` build target 복구.

### RH-15. Cleanup

목표:
- `Debug-DX12`에서 DX11 renderer `.cpp`까지 컴파일되는 구조를 점진 축소.
- DX11 backend는 legacy optional backend로 유지.
- DX12 backend는 explicit RHI 기준 구현으로 승격.

주의:
- 한 번에 DX11 compile unit을 빼면 기존 public renderer symbols가 깨질 수 있다.
- 먼저 RHI-neutral renderer interface를 만들고 backend implementation을 나누는 순서가 안전하다.

## 4. 검증 명령

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' Winters.sln /t:DX12SmokeHost /p:Configuration=Debug-DX12 /p:Platform=x64 /m
& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' Winters.sln /p:Configuration=Debug-DX12 /p:Platform=x64 /m
& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64 /m
```

Smoke:

```powershell
$exe = Resolve-Path 'Engine\Bin\Debug-DX12\DX12SmokeHost.exe'
$work = Split-Path $exe
$p = Start-Process -FilePath $exe -WorkingDirectory $work -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 8
$alive = -not $p.HasExited
if ($alive) {
    Stop-Process -Id $p.Id -Force
    Wait-Process -Id $p.Id -Timeout 5 -ErrorAction SilentlyContinue
}
"AliveAfter8s=$alive ExitCode=$($p.ExitCode)"
```

## 5. 다음 즉시 작업

바로 이어서 할 작업은 RH-10-B다.

우선 구현 파일:
- `Engine/Private/RHI/DX12/DX12Buffer.h`
- `Engine/Private/RHI/DX12/DX12Buffer.cpp`
- `Engine/Private/RHI/DX12/DX12Device.h`
- `Engine/Private/RHI/DX12/DX12Device.cpp`
- `Engine/Private/RHI/DX12/DX12CommandList.cpp`
- `Engine/Public/RHI/RHIDescriptors.h`
- `Engine/Public/RHI/RHITypes.h`

우선 성공 기준:
- `IRHIDevice::CreateCommandList()`가 non-null을 반환한다.
- `IRHIQueue::Execute()`에 해당 command list를 넘길 수 있다.
- `Draw()` 호출까지 native DX12 command list에 기록된다.
- 아직 삼각형이 보이지 않아도 command recording/submit path가 살아 있어야 한다.

## 6. 2026-05-07 추가 검증 결과

이번 문서 작성 직후 RH-10-A command list 생성 루트까지 반영하고 아래 검증을 완료했다.

결과:
- `DX12SmokeHost / Debug-DX12`: 성공, 오류 0.
- `Winters.sln / Debug-DX12`: 성공, 오류 0.
- `DX12SmokeHost.exe` 8초 생존 smoke: `AliveAfter8s=True`.
- `Engine / Debug`: 성공, 오류 0.

잔여 경고:
- `Debug-DX12` 직접 빌드 중 기존 DLL export/STL 노출 계열 경고가 남는다.
- `Debug` Engine 빌드도 기존 C4251/C4275 경고가 남는다.
- 이번 RH-10-A 변경으로 새 오류는 발생하지 않았다.

## 7. 2026-05-07 Sandbox RH-10-B / RH-13 Progress

Applied in sandbox:
- RH-10-B command list binding path:
  - `IRHIDevice::CreateBuffer`
  - `IRHIDevice::DestroyBuffer`
  - `IRHIDevice::GetBufferNativeHandle`
  - `IRHIDevice::GetFrameCommandList`
  - `CDX12Device` buffer handle table and resolve helpers
  - `CDX12CommandList::SetVertexBuffer`
  - `CDX12CommandList::SetIndexBuffer`
  - `CDX12CommandList::TransitionResource(RHIBufferHandle, ...)`
  - `CDX12CommandList::UpdateBuffer`
- RH-13 first real draw smoke:
  - `DX12SmokeHost` now creates a native DX12 root signature, PSO, and upload vertex buffer.
  - `CEngineApp` keeps the DX12 frame command list open between `BeginFrame` and `EndFrame`.
  - `DX12SmokeHost::OnRender` records root signature, PSO, VB bind, and draw into the active frame command list.
- Public DLL access:
  - Added `WintersGetRHIDevice()` so smoke tools can access RHI through the exported engine boundary instead of linking `CEngineApp` internals.

Verification:
- `DX12SmokeHost / Debug-DX12 / Rebuild`: success, errors 0.
- `DX12SmokeHost.exe` 8 second smoke: `AliveAfter8s=True`.

Still staged for later:
- RH-11 full descriptor lifetime model.
- RH-12 full RHI shader/pipeline handle table and root signature generation.
- RH-14 renderer migration beyond the smoke triangle.
- RH-15 removal of DX11 renderer compile units from `Debug-DX12`.

## 8. 2026-05-07 Sandbox RH-11 / RH-12 / FX Sprite Progress

Branch point A: shader constant ABI moved out of DX11.
- New neutral header: `Engine/Public/Renderer/FxShaderConstants.h`.
- `DX11ConstantBuffer.h` now only owns the DX11 buffer helper and includes the neutral ABI header.
- Reason: `CBPerFrame`, `CBPerObject`, and `CBFxParams` are HLSL contracts, not DX11 contracts.

Core code:
```cpp
struct CBFxParams
{
    DirectX::XMFLOAT4 vTint;
    DirectX::XMFLOAT4 vUVRect;
    DirectX::XMFLOAT2 vUVScroll;
    f32_t fAlphaClip;
    f32_t fErodeThreshold;
    DirectX::XMFLOAT4 vStyleColorA;
    DirectX::XMFLOAT4 vStyleColorB;
    DirectX::XMFLOAT4 vRimColor;
    DirectX::XMFLOAT4 vStyleParams;
    DirectX::XMFLOAT4 vTimeParams;
    DirectX::XMFLOAT4 vMagicScrollA;
    DirectX::XMFLOAT4 vMagicShape;
    DirectX::XMFLOAT4 vMagicCore;
};
static_assert(sizeof(CBFxParams) % 16 == 0);
```

Branch point B: native smoke draw graduated to RHI shader/pipeline/texture.
- `IRHIDevice` now exposes `CreateShader`, `DestroyShader`, `CreateTexture`, `DestroyTexture`, and native texture lookup.
- `RHIPipelineDesc` now carries up to 4 bind group layout handles.
- `RHIBindGroupResource` now carries `eRHIBindingType`, so `b0` and `t0` can coexist in one group.
- `CDX12Device` owns shader, texture, buffer, pipeline, and bind group tables.
- `CDX12PipelineState` now creates a real root signature and graphics PSO.
- `CDX12BindGroup` writes CBV/SRV descriptors into a shader-visible CBV/SRV/UAV heap.

Core code:
```cpp
RHIBindingSlot slots[] = {
    { 0, eRHIBindingType::ConstantBuffer, eRHIShaderVisibility::Vertex },
    { 1, eRHIBindingType::ConstantBuffer, eRHIShaderVisibility::Vertex },
    { 2, eRHIBindingType::ConstantBuffer, eRHIShaderVisibility::All },
    { 0, eRHIBindingType::ShaderResource, eRHIShaderVisibility::Pixel },
};
```

Branch point C: reusable RHI FX sprite path added.
- New files:
  - `Engine/Public/Renderer/RHIFxSpriteRenderer.h`
  - `Engine/Private/Renderer/RHIFxSpriteRenderer.cpp`
  - `Engine/Public/RHI/RHITextureLoader.h`
  - `Engine/Private/RHI/RHITextureLoader.cpp`
- `CRHIFxSpriteRenderer` owns RHI shaders, bind group layout, one persistent bind group, per-draw constant buffers, and blend-mode pipelines.
- `RHI_CreateTextureFromFile` decodes WIC images to RGBA8 and uploads them through `IRHIDevice::CreateTexture`.

Core code:
```cpp
pCommandList->UpdateBuffer(m_pImpl->hCBPerFrame, &perFrame, sizeof(perFrame));
pCommandList->UpdateBuffer(m_pImpl->hCBPerObject, &perObject, sizeof(perObject));
pCommandList->UpdateBuffer(m_pImpl->hCBFxParams, &fxParams, sizeof(fxParams));
pDevice->UpdateBindGroup(m_pImpl->hBindGroup, resources, 4);
pCommandList->SetPipeline(hPipeline);
pCommandList->SetBindGroup(0, m_pImpl->hBindGroup);
pCommandList->SetVertexBuffer(0, m_pImpl->hVertexBuffer, sizeof(FxSpriteVertex), 0);
pCommandList->Draw(6, 1, 0, 0);
```

Branch point D: `CFxSystem` now has a DX12 branch.
- DX11 still uses `CPlaneRenderer` and `CTexture`.
- DX12 uses `CRHIFxSpriteRenderer` and RHI texture handles.
- This keeps current gameplay spawn/update logic intact while swapping only the rendering backend.

Current verification:
- `Tools/DX12SmokeHost/DX12SmokeHost.vcxproj / Debug-DX12 / Rebuild`: success.
- `Winters.sln / Debug-DX12`: success.
- `DX12SmokeHost.exe` 8 second smoke: `AliveAfter8s=True`.
- `Engine/Include/Engine.vcxproj / Debug`: success.
- `Client/Include/Client.vcxproj / Debug`: success after restoring the sandbox-local `Tools/Bin/flatc.exe` and guarding post-build copies for missing sandbox assets.
- Public/SDK DX11 leak scan: only the existing `imgui.h` comment remains.

Sandbox note:
- `Shared/Schemas/run_codegen.bat` requires `Tools/Bin/flatc.exe`; the sandbox worktree was missing that binary, so it was restored from the root workspace for verification.
- Client Debug still prints a post-build warning when `pwsh.exe` is absent, but `WintersGame.exe` is produced and MSBuild exits 0.
- The main workspace has the real `Client/Bin/Resource` payload, but the sandbox worktree does not carry that 20GB ignored asset tree. `Client.vcxproj` now copies shaders/resources only when source files exist, so compile verification stays usable in lightweight worktrees.

Remaining after this slice:
- Mesh FX / `CFxStaticMeshRenderer` still needs an RHI mesh path.
- Depth attachment and depth read/no-write FX pass are not yet expressed in the DX12 render pass path.
- Descriptor allocator is persistent and functional, but not yet a full frame-aware lifetime system.
- DX12 Client executable configuration is still not a first-class project config.

## 9. 2026-05-07 Mainline RH-13 Backend Selection / Client DX12 Config

Branch point E: backend selection moved to EngineApp instead of splitting scenes.
- `EngineConfig` now supports `Auto`, `DX12`, `DX11`, and `Null`.
- Default is `Auto`, with DX12 attempted first when the Engine build contains `WINTERS_RHI_BACKEND_DX12`.
- If DX12 device creation fails and fallback is allowed, EngineApp falls back to DX11 legacy.
- Client can override at launch with `--rhi=dx12`, `--rhi=dx11`, `--rhi=auto`, or `--rhi=null`.

Core code:
```cpp
switch (config.rhiBackend)
{
case eEngineRHIBackend::DX11:
    tryDX11();
    break;
case eEngineRHIBackend::DX12:
    tryDX12();
    break;
case eEngineRHIBackend::Auto:
    if (!tryDX12())
        tryDX11();
    break;
default:
    break;
}

if (!m_pDevice && config.allowRHIFallback && config.rhiBackend != eEngineRHIBackend::DX11)
    tryDX11();
```

Branch point F: Client now has first-class DX12 build configurations.
- `Client.vcxproj` now has `Debug-DX12|x64` and `Release-DX12|x64`.
- `Winters.sln` maps Client to those configurations in solution-level `Debug-DX12` / `Release-DX12`.
- Client post-build copies are guarded with `if exist`, so lightweight sandbox worktrees without the full resource payload can still compile.
- Client DX12 configs link against `Engine/Bin/{Debug-DX12,Release-DX12}` directly and copy the matching Engine DLL/PDB into the Client output folder.
- `RHIHandle<T>` is header-only again; the template itself is no longer marked `WINTERS_ENGINE`, avoiding accidental `dllimport` unresolved symbols from Client code.

Mainline verification:
- `Winters.sln / Debug-DX12`: success.
- `Client/Include/Client.vcxproj / Debug-DX12`: success, produces `Client/Bin/Debug-DX12/WintersGame.exe`.
- `Tools/DX12SmokeHost / Debug-DX12`: success, produces `Engine/Bin/Debug-DX12/DX12SmokeHost.exe`.
- `DX12SmokeHost.exe` 8 second smoke: `AliveAfter8s=True`.
- Public/SDK DX11 leak scan: only the existing `EngineSDK/inc/imgui.h` comment remains.

Updated remaining after RH-13:
- `Debug-DX12` can build Engine + Client + DX12SmokeHost as a linked configuration.
- DX12 remains bootstrap/render-path migration mode for the full Client scene until mesh/model/UI renderers are RHI-backed.
- Next concrete renderer work: beam/ribbon RHI path, `CPlaneRenderer` RHI replacement, then `CFxStaticMeshRenderer` mesh path.

## 10. 2026-05-07 Mainline RH-14 FX/Plane Render Migration

Branch point G: beam/ribbon FX moved onto the RHI sprite draw path for DX12.
- `CFxBeamSystem` now creates `CRHIFxSpriteRenderer` when the active backend is DX12.
- DX11 still uses `CPlaneRenderer` + `CTexture`.
- DX12 loads beam/ribbon textures through `RHI_CreateTextureFromFile`, caches `RHITextureHandle`s, and destroys them in `Shutdown`.
- `DrawSegment` is now backend-split at the final draw point, so spawn/update/lifetime logic stays shared.

Core code:
```cpp
if (pDevice->GetBackend() == eRHIBackend::DX12)
{
    p->m_pRHISprite = CRHIFxSpriteRenderer::Create(pDevice);
    return p->m_pRHISprite ? std::move(p) : nullptr;
}
```

Branch point H: utility plane users are DX12-backed without touching the legacy plane renderer.
- Attack range ring and turret projectile quad now use `CRHIFxSpriteRenderer` + `RHITextureHandle` under DX12.
- Legacy DX11 keeps the existing `CPlaneRenderer` setup.
- Scene shutdown releases the RHI utility textures explicitly through `IRHIDevice::DestroyTexture`.
- The remaining `CPlaneRenderer::Create` calls in `CFxSystem` and `CFxBeamSystem` are DX11-only branches.

Branch point I: `CFxStaticMeshRenderer` is DX12-safe.
- Full mesh rendering is not yet RHI-backed because `CModel/CMesh` still own DX11 vertex/index buffers and `CTexture` material bindings.
- Under DX12, `CFxStaticMeshRenderer::Create` now succeeds in a safe no-op mode.
- `PreloadMesh` registers placeholder entries so gameplay FX spawn/update logic can continue.
- `BeginFrame`, `DrawMesh`, and `EndFrame` return early under DX12 until an RHI mesh resource path is added.

Branch point J: DX12 RenderPass moved beyond a descriptor-only placeholder.
- `CDX12TextureImpl` now tracks RTV/DSV descriptor slots.
- `CDX12Device` owns texture RTV/DSV descriptor heaps, creates view descriptors for `RenderTarget` / `DepthStencil` textures, and frees slots on `DestroyTexture`.
- `CDX12CommandList::BeginRenderPass` resolves the pass, transitions color/depth attachments, clears when requested, sets viewport/scissor from the attachment size, and binds RTV/DSV with `OMSetRenderTargets`.

Branch point K: Client DX12 output folder now carries the FMOD runtime.
- `WintersEngine.dll` depends on `fmod.dll`.
- `Client/Bin/Debug-DX12` previously missed that DLL, so `WintersGame.exe --rhi=dx12` exited at loader startup with `0xC0000135`.
- `Client.vcxproj` now copies `Engine/ThirdPartyLib/FMOD/Bin/fmod.dll` for both `Debug-DX12` and `Release-DX12`.

Current verification:
- `Client/Include/Client.vcxproj / Debug-DX12`: success, produces `Client/Bin/Debug-DX12/WintersGame.exe`.
- `Winters.sln / Debug-DX12`: success, produces `WintersEngine.dll`, `WintersGame.exe`, and `DX12SmokeHost.exe`.
- `DX12SmokeHost.exe` 8 second smoke: `AliveAfter8s=True`.
- `WintersGame.exe --rhi=dx12 --banpick-smoke --smoke-start --smoke-log --smoke-champion=irelia` 8 second smoke: `ClientDX12SmokeAliveAfter8s=True`.
- Public/SDK header DX11 leak scan: only the existing `EngineSDK/inc/imgui.h` comment remains.

Remaining after RH-14:
- Real DX12 mesh FX requires a new RHI mesh resource layer or a `CModel/CMesh` split that exposes backend-neutral vertex/index buffers and material texture handles.
- Full Client scene still contains larger DX11 renderer islands: model renderers, managers, ImGui/UI, fog/SSAO/normal pass.
- DX12 descriptor heap is persistent and functional, but not yet a frame-retired/free-list model.

## 11. 2026-05-07 Mainline RH-14-B FBX FX Static Mesh Path

Branch point L: `CFxStaticMeshRenderer` now has a real DX12 RHI static mesh path.
- DX12 no longer returns a safe no-op for mesh FX.
- The renderer compiles an embedded FX mesh shader, creates an RHI bind group layout, bind group, constant buffers, blend-specific pipelines, and a default white texture at startup.
- `PreloadMesh` now loads FBX files directly through Assimp on the DX12 path, converts every Assimp mesh into `VTXMESH` vertices and `u32_t` indices, then uploads them into RHI vertex/index buffers.
- `DrawMesh` binds the RHI pipeline, bind group, vertex buffer, index buffer, texture, and constant buffers before issuing `DrawIndexed`.
- DX11 keeps the existing `CModel`, `DX11Shader`, `DX11Pipeline`, and `CTexture` path unchanged.

Why this branch exists:
- `CModel` and `CMesh` still own DX11 vertex/index buffers, so using them directly would keep FBX FX blocked on DX11 resource ownership.
- This path is deliberately local to FX mesh rendering, so gameplay spawn/update/lifetime code can move forward without waiting for the full model renderer migration.

Current behavior:
- Static or bind-pose FBX FX can now be rendered on DX12.
- Meshes with bones are accepted, but bones are logged and ignored on this first path.
- The primary material texture is loaded through `RHI_CreateTextureFromFile`; missing textures fall back to a 1x1 white RHI texture.
- The erode texture slot is currently bound to the fallback white texture because the current FX mesh component API exposes one texture path.

Current verification:
- `Engine/Include/Engine.vcxproj / Debug-DX12`: success.
- `Client/Include/Client.vcxproj / Debug-DX12`: success.
- `Winters.sln / Debug-DX12`: success, produces `WintersEngine.dll`, `WintersGame.exe`, and `DX12SmokeHost.exe`.
- `DX12SmokeHost.exe` 8 second smoke: `AliveAfter8s=True`.
- `WintersGame.exe --rhi=dx12 --banpick-smoke --smoke-start --smoke-log --smoke-champion=irelia` 8 second smoke: `ClientDX12SmokeAliveAfter8s=True`.

Remaining after RH-14-B:
- Add a second texture path or material slot mapping for erode/mask textures.
- Add depth attachment selection for FX mesh passes: depth test/read, depth no-write, and overlay modes.
- Promote the temporary dynamic/upload RHI mesh buffers to the final resource upload/default-heap path.
- Decide whether the final shared mesh resource layer comes from `CModel/CMesh` split or a separate `RHIMeshResource` asset cache.
- Add skinned/animated FX mesh support only after the static FBX FX path is stable in-game.

## 12. 2026-05-08 Mainline RH-14-C FBX FX Material Variant Completion

Current target closed:
- The DX12 FBX FX static mesh path now carries both diffuse and erode/mask texture paths.
- `FxEmitterDesc` now stores `strErodeTexturePath`, so JSON/asset-driven FX can bind `t1` instead of relying on code-only presets.
- `FxMeshComponent` erode paths now flow through `SpawnFromAsset`, `LegacyFxAdapter`, and `FxLegacyAssetDumper`.
- `CFxStaticMeshRenderer::PreloadMesh` now has a 3-argument overload in both `Engine/Public` and `EngineSDK/inc`, fixing Client SDK compile mismatches.
- `CRHIFxMeshResourceCache` no longer keys only by FBX path. It keys by `FBX + diffuse + erode`, so the same mesh can be reused with different FX materials safely.
- `UpdateLib.bat` no longer purges the whole SDK include tree during normal pre-builds unless `WINTERS_SDK_PURGE=1` is set. This avoids half-empty SDK include folders when a header is locked.

Core code:
```cpp
std::wstring MakeMeshKey(
    const std::string& strFbxPath,
    const std::wstring& strDiffuseTexturePath,
    const std::wstring& strErodeTexturePath)
{
    std::wstring key(strFbxPath.begin(), strFbxPath.end());
    key += L"\nD:";
    key += strDiffuseTexturePath;
    key += L"\nE:";
    key += strErodeTexturePath;
    return key;
}
```

Verification performed in this slice:
- Project registration checked for `RHIFxMeshResource` in `Engine.vcxproj` and `Engine.vcxproj.filters`.
- `Engine/Public` and `EngineSDK/inc` headers checked for matching `PreloadMesh` and `strErodeTexturePath` declarations.
- `git diff --check` passed for the edited files. Only CRLF normalization warnings remain.

Updated remaining after RH-14-C:
- Add depth attachment selection for FX mesh passes: depth test/read, depth no-write, and overlay modes.
- Promote temporary dynamic RHI mesh buffers to a final upload/default-heap resource path.
- Decide whether the final shared mesh resource layer comes from a `CModel/CMesh` split or the current separate `RHIFxMeshResource` cache.
- Add skinned/animated FX mesh support only after static FBX FX is stable in-game.
- Start visual parity pass: compare key Irelia/Ezreal FBX FX under DX11 legacy and DX12 RHI.

Primary goal reset:
- DX12 migration is now scoped around enabling `AAA_VFX_GRAYSCALE_AND_SHADER.md`.
- The next plan document is `.md/TODO/05-07/FX개념!/LOL_STYLE_FX_DX12_MIGRATION_PLAN_2026_05_08.md`.
- Priority is not whole-engine DX12 parity. Priority is LoL-style FX: grayscale/mask textures, material parameters, stylized shader, sprite/beam/ribbon/static-mesh FX, depth/blend correctness, and Irelia/Ezreal representative in-game validation.
