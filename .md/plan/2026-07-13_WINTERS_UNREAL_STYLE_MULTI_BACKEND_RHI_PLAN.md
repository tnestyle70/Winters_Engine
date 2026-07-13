Session - Unreal Engine 5.7.4의 Dynamic RHI와 Restricted platform extension 감사를 기준으로 LoL DX11 기본 경로를 보존하면서 DX12, Vulkan, Mobile, Console로 확장 가능한 Winters RHI를 단계적으로 반영한다. 상세 구조와 현재 감사 근거는 `.md/architecture/WINTERS_UNREAL_STYLE_MULTI_BACKEND_RHI_ARCHITECTURE.md`를 따른다. 현재 worktree에서 RHI 관련 core 파일이 이미 수정 중이므로 이 문서는 합치기 순서와 첫 수정 packet을 고정하며, 소유권 확인 전 runtime 코드를 덮어쓰지 않는다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Tools/Harness/Run-S17RhiValidation.ps1

기존 코드:

```powershell
        @{ Name = "WintersElden_probe_dx12"; Exe = "EldenRingClient\Bin\$Configuration\WintersElden.exe"; Args = @("--scene=probe"); Cwd = "EldenRingClient\Bin\$Configuration" },
```

아래로 교체:

```powershell
        @{ Name = "WintersElden_probe_dx12"; Exe = "EldenRingClient\Bin\$Configuration\WintersElden.exe"; Args = @("--scene=probe", "--rhi=dx12"); Cwd = "EldenRingClient\Bin\$Configuration" },
```

이 교체는 이름만 DX12인 DX11 실행을 제거한다. 다음 packet에서 smoke process의 backend identity를 파일/pipe로 수집하기 전까지는 `--rhi=dx12` 명시가 최소 진실성 gate다.

### 1-2. C:/Users/user/Desktop/Winters/Engine/Public/RHI/IRHIBackendModule.h

새 파일:

```text
CONFIRM_NEEDED - `Engine/Private/Framework/CEngineApp.cpp`, `Engine/Include/EngineConfig.h`,
`Engine/Include/Engine.vcxproj`의 현재 dirty 변경 소유자를 확인하고 merge base를 다시 읽은 뒤 전체 파일 본문을 작성한다.

확정해야 할 계약:
- module identity와 device instance를 분리한다.
- `GetBackend`, `GetName`, `IsCompiledIn`, `ProbeSupport`, `CreateDevice`를 제공한다.
- probe 결과에 compiled/platform/surface/adapter/feature/shader-package 실패 코드를 둔다.
- vendor/native graphics type을 public header에 노출하지 않는다.
- `std::string`을 DLL ABI로 내보내지 않고 고정 reason code와 bounded message를 사용한다.
```

### 1-3. C:/Users/user/Desktop/Winters/Engine/Public/RHI/RHIBackendRegistry.h

새 파일:

```text
CONFIRM_NEEDED - `IRHIBackendModule.h` 전체 계약 확정 후 전체 파일 본문을 작성한다.

확정해야 할 계약:
- compiled module 등록과 lookup만 소유한다.
- requested backend, forced/auto selection mode, fallback policy, `RHISurfaceDesc`, feature profile을 입력받는다.
- selected backend, fallback 여부, probe report, `unique_ptr<IRHIDevice>`를 결과로 반환한다.
- LoL의 Auto 기본값은 DX11이며 forced request는 fail-closed다.
- 미래 Vulkan/Console 이름을 등록해 성공처럼 보이게 하는 placeholder module은 만들지 않는다.
```

### 1-4. C:/Users/user/Desktop/Winters/Engine/Private/RHI/RHIBackendRegistry.cpp

새 파일:

```text
CONFIRM_NEEDED - 1-2와 1-3 확정 후 전체 파일 본문을 작성한다.

확정해야 할 구현:
- concrete DX11/DX12 header include는 이 cpp와 각 backend module cpp 안으로 격리한다.
- DX11/DX12 module의 실제 `ProbeSupport`와 `CreateDevice`를 등록한다.
- `RHISurfaceDesc::Win32HWND`가 아닌 surface를 Windows module이 거절한다.
- DX12 adapter/device creation 실패를 structured reason으로 반환한다.
- selection result를 `OutputDebugStringA`와 profiler metadata에 기록한다.
```

### 1-5. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드 범위:

```cpp
#include "RHI/DX11/CDX11Device.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "RHI/DX11/BlendStateCache.h"
#include "RHI/DX12/DX12Device.h"
```

부터 anonymous namespace의 `CreateDX11DeviceForWindow`, `CreateDX12DeviceForWindow`, `tryDX11`, `tryDX12`, `switch (config.rhiBackend)`, legacy fallback block까지가 backend bootstrap 교체 범위다.

아래로 교체:

```text
CONFIRM_NEEDED - 1-2~1-4의 전체 파일 본문과 현재 `CEngineApp.cpp` dirty 변경을 합친 뒤 정확한 C++ 교체 블록을 작성한다.

교체 결과:
- CEngineApp는 `RHIBackendRegistry`에 selection request를 전달한다.
- concrete DX11/DX12 device header와 local factory lambda를 알지 않는다.
- `--rhi=dx12` 같은 forced request가 실패하면 DX11 fallback 없이 초기화 실패한다.
- Auto만 명시된 candidate/fallback 정책을 사용한다.
- requested/selected backend와 fallback reason을 반드시 출력한다.
```

### 1-6. C:/Users/user/Desktop/Winters/Engine/Include/EngineConfig.h

기존 코드:

```cpp
enum class eEngineRHIBackend : u32_t
{
    Auto = 0,
    DX12,
    DX11,
    Null,
    Vulkan,
    Metal,
    Xbox,
    PS5,
};
```

아래로 교체:

```text
CONFIRM_NEEDED - selection mode와 concrete backend identity를 분리하는 1-2/1-3 계약 확정 후 전체 enum/config 교체 블록을 작성한다.

교체 결과:
- Auto/Forced는 selection policy다.
- DX11/DX12/Vulkan 등은 backend identity다.
- product default, fallback list, feature profile, render profile을 서로 다른 field로 둔다.
- enum에 존재하지만 미구현인 Console backend를 지원 완료로 해석하지 않도록 compiled-module list를 별도로 둔다.
```

### 1-7. C:/Users/user/Desktop/Winters/Engine/Public/RHI/RHICapabilities.h

기존 코드 범위:

```cpp
inline RHICapabilities RHI_MakeDefaultCapabilities(eRHIBackend backend)
```

함수 전체를 아래 정책으로 교체:

```text
CONFIRM_NEEDED - DX11/DX12 native feature-query 결과와 renderer requirement 목록을 먼저 확정한 뒤 정확한 C++ 교체 블록을 작성한다.

교체 결과:
- backend enum만 보고 bindless, async compute, VRS 등을 true로 두지 않는다.
- backend가 device 생성 후 adapter/driver query로 채운 `RHICapabilities`를 반환한다.
- default는 보수적인 false/최소값이다.
- `LoLDesktop`과 `MobileForward` requirement validation은 capability storage와 분리한다.
```

### 1-8. C:/Users/user/Desktop/Winters/Engine/Public/RHI/IRHIDevice.h

기존 코드 범위:

```cpp
    virtual IRHISwapChain* CreateSwapChain(const RHIWindowHandle& window) { (void)window; return nullptr; }
```

부터 기본 `nullptr`/빈 handle을 반환하는 optional-looking resource method들을 아래 정책으로 교체:

```text
CONFIRM_NEEDED - DX11/DX12 양 backend가 현재 실제 구현하는 method matrix와 RHI conformance test 목록을 작성한 뒤 정확한 virtual signature 교체 블록을 작성한다.

교체 결과:
- product 필수 method는 pure virtual 또는 명시적 unsupported result로 구현을 강제한다.
- backend에서 의미가 없는 barrier 같은 operation만 documented no-op를 허용한다.
- 미구현 method가 compile 성공 뒤 화면 누락으로 나타나지 않게 한다.
```

### 1-9. C:/Users/user/Desktop/Winters/Engine/Public/Framework/CEngineApp.h

기존 코드 범위:

```cpp
class CBlendStateCache;
class DX11Pipeline;
class DX11Shader;
```

및 모든 `DX11Shader*`, `DX11Pipeline*`, `CBlendStateCache*` public getter와 owner field를 삭제한다.

삭제 후 추가:

```text
CONFIRM_NEEDED - `ModelRenderer`, `FxStaticMeshRenderer`, `FogOfWarRenderer`, `NormalPass`, `PlaneRenderer`가 사용하는 shared shader/pipeline caller를 전부 열거하고 RHI pipeline/material owner로 교체하는 packet에서 정확한 header 본문을 작성한다.

교체 결과:
- CEngineApp public API는 concrete graphics shader/pipeline/cache type을 노출하지 않는다.
- shared shader/pipeline 수명은 RenderCore/resource owner가 맡는다.
```

### 1-10. C:/Users/user/Desktop/Winters/Engine/Public/Renderer/RenderWorldSnapshot.h

기존 코드:

```cpp
struct RenderMeshItem
{
    Mat4 matWorld = Mat4::Identity();
    RHIMeshSlice mesh{};
    RHITextureHandle hAlbedoTexture{};
    RHISamplerHandle hSampler{};
    Vec4 vTint{ 1.f, 1.f, 1.f, 1.f };
    bool_t bDepthWrite = true;
};
```

아래로 교체:

```text
CONFIRM_NEEDED - 현재 `CMesh`, `CModel`, `CAnimator`의 bone palette ownership과 최대 bone/instance 수를 재검사한 뒤 static/skinned draw item의 전체 struct 본문을 작성한다.

교체 결과:
- static과 skinned draw를 타입 또는 layout flag로 명시한다.
- bone palette는 raw `ID3D11ShaderResourceView*`가 아니라 RHI buffer/storage handle과 offset/count로 전달한다.
- material은 albedo 1장만이 아니라 normalized material binding을 참조한다.
- UI/FX/FOW submission은 필요한 pass/domain을 명시한다.
```

### 1-11. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

삭제할 기존 코드:

```cpp
    // RenderWorldSnapshot/RHISceneRenderer currently has no skinned vertex path or bone palette.
    // Skinned models must stay on the legacy animated renderer until RHI skinning is explicit.
    if (m_pImpl->pSharedModel->HasSkeleton())
        return 0;
```

아래 코드로 바로 교체하지 않고, 1-10의 skinned snapshot 및 `CRHISceneRenderer` bone binding 구현과 같은 packet에서 삭제한다.

```text
CONFIRM_NEEDED - skip만 먼저 지우면 duplicate bind-pose/T-pose regression이 재발한다. skinned RHI pipeline과 bone palette binding 전체 코드가 준비된 뒤 정확한 교체 블록을 작성한다.
```

### 1-12. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/RHISceneRenderer.cpp

기존 코드 범위:

```cpp
#include <Windows.h>
```

와 `DirectX::XMFLOAT*`, inline `kSceneMeshShader`, runtime `RHI_CompileHlslShader` 사용을 portable renderer/shader artifact path로 교체한다.

```text
CONFIRM_NEEDED - shader manifest/binding reflection format과 `Mat4`의 row/column-major upload 규칙을 확정한 뒤 정확한 전체 교체 블록을 작성한다.

교체 결과:
- generic renderer cpp가 Windows header나 DirectX math type을 요구하지 않는다.
- DXBC/DXIL/SPIR-V를 backend/feature profile에 따라 cooked shader package에서 읽는다.
- editor/debug hot reload만 runtime compile을 허용한다.
- static/skinned/material/UI/FX pass가 같은 snapshot contract를 사용한다.
```

### 1-13. C:/Users/user/Desktop/Winters/Engine/Private/RHI/RHIShaderCompiler.cpp

현재 파일은 DX11 debug/runtime compiler로 이름을 한정하고, 공통 shader cook 도구를 별도 packet으로 추가한다.

```text
CONFIRM_NEEDED - 현재 shader build/copy scripts와 DXC binary 위치를 검사한 뒤 새 cook tool의 전체 파일 본문을 작성한다.

목표 산출물:
- DX11 SM5: DXBC
- DX12 SM6: DXIL
- Vulkan Desktop/Mobile: SPIR-V
- normalized reflection + source/compiler hash + permutation manifest
```

### 1-14. C:/Users/user/Desktop/Winters/Engine/Private/RHI/RHITextureLoader.cpp

현재 WIC/COM decode를 Windows development loader로 한정하고, shipping-like runtime은 cooked texture artifact를 사용하도록 분리한다.

```text
CONFIRM_NEEDED - `.wtex` 현재 schema와 texture converter/cooker를 검사한 뒤 정확한 교체 및 새 파일 전체 본문을 작성한다.

교체 결과:
- common runtime RHI가 WIC/COM을 요구하지 않는다.
- Desktop은 BC 계열, Android는 ASTC/ETC2 variant를 manifest로 선택한다.
- decode/IO와 GPU upload ownership을 분리한다.
```

### 1-15. C:/Users/user/Desktop/Winters/Engine/Private/RHI/Vulkan/

새 파일 집합:

```text
CONFIRM_NEEDED - P0~P2의 DX11/DX12 product parity와 shader package가 통과한 뒤 Vulkan SDK/VMA/DXC 버전, Windows surface, allocator, queue/sync 정책을 확정하고 각 새 h/cpp의 전체 본문을 작성한다.

첫 구현 범위:
- module/support probe
- instance/physical/logical device
- Windows surface/swapchain
- queue/command pool/command buffer
- buffer/texture/sampler/pipeline/bind group
- barrier/semaphore/fence
- debug messenger and validation
- pipeline cache
```

### 1-16. C:/Users/user/Desktop/Winters/Engine/Private/Platform/Android/

새 파일 집합:

```text
CONFIRM_NEEDED - Windows Vulkan parity 이후 Android NDK/SDK/Gradle target과 application shell ownership을 확정하고 각 새 h/cpp/build file의 전체 본문을 작성한다.

첫 구현 범위:
- app/window/native surface
- suspend/resume/surface lost/recreate
- touch/input, filesystem, asset package, audio platform adapters
- ARM64 package/deploy
- MobileForward profile and ASTC/ETC2 selection
```

### 1-17. C:/Users/user/Desktop/Winters/Restricted/Platforms/<Console>/

새 파일 집합:

```text
CONFIRM_NEEDED - 플랫폼사 developer 승인, NDA/권한, SDK/toolchain/devkit, private repository와 restricted CI가 확보되기 전에는 파일을 만들지 않는다.

승인 후에만 작성할 범위:
- platform build factory/SDK probe
- private window/surface/lifecycle/input/filesystem
- private RHI backend module
- private shader compiler/cook/package/deploy
- devkit diagnostics and certification tests

Public tree에는 vendor API type, SDK include/lib path, confidential define, fake backend implementation을 추가하지 않는다.
```

### 1-18. C:/Users/user/Desktop/Winters/Engine/Include/Engine.vcxproj

새 RHI module/registry 파일이 확정되면 기존 RHI compile/include 항목 근처에 등록한다.

```text
CONFIRM_NEEDED - 1-2~1-4의 실제 파일 생성 packet에서 현재 dirty project 파일을 다시 읽고 정확한 XML 추가 블록을 작성한다.
```

## 2. 검증

미검증:

- 이 문서 작성 session에서는 Winters runtime code와 project file을 수정하지 않았다.
- Unreal checkout에는 build된 editor/RHI DLL/APK가 없어 UE runtime은 검증하지 않았다.
- 이 로컬 Unreal checkout에는 console extension/SDK/toolchain/RHI가 없어 console 구현 내용은 검증할 수 없다.
- 사용자의 Sony/Microsoft/Nintendo developer 승인 상태와 Epic console entitlement는 로컬 source만으로 확인할 수 없다.
- 현재 S17 harness의 `WintersElden_probe_dx12`는 `--rhi=dx12`를 전달하지 않으므로 수정 전 최신 harness를 DX12 실행 증거로 사용하지 않는다.

문서 검증 명령:

```powershell
git diff --check -- .md/architecture/WINTERS_UNREAL_STYLE_MULTI_BACKEND_RHI_ARCHITECTURE.md .md/plan/2026-07-13_WINTERS_UNREAL_STYLE_MULTI_BACKEND_RHI_PLAN.md
rg -n "Session -|Unreal|IDynamicRHIModule|Vulkan|Android|Restricted|Console|DX11|DX12" .md/architecture/WINTERS_UNREAL_STYLE_MULTI_BACKEND_RHI_ARCHITECTURE.md .md/plan/2026-07-13_WINTERS_UNREAL_STYLE_MULTI_BACKEND_RHI_PLAN.md
```

P0 구현 후 검증 명령:

```powershell
msbuild Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64 /m
msbuild Winters.sln /t:EldenRingClient /p:Configuration=Debug-DX12 /p:Platform=x64 /m
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Run-S17RhiValidation.ps1 -ReportPath .md/build/2026-07-13_RHI_BACKEND_SELECTION_REPORT.md
git diff --check
```

P0 수동/자동 확인:

- `WintersElden.exe --scene=probe --rhi=dx11` 로그의 requested/selected backend가 모두 DX11인지 확인.
- `WintersElden.exe --scene=probe --rhi=dx12` 로그의 requested/selected backend가 모두 DX12인지 확인.
- 존재하지 않는 `--rhi=vulkan` 요청이 DX11로 조용히 fallback하지 않고 structured reason과 함께 실패하는지 확인.
- harness report가 실행 이름뿐 아니라 실제 selected backend와 adapter/capability dump를 기록하는지 확인.

P1 DX11 제품 parity 확인:

- LoL 정상 F5에서 roster, map, minion, skinned champion, UI, minimap, FOW, FX, post-process를 숨기지 않고 확인.
- `rg -n "ID3D11|d3d11.h|DX11Shader|DX11Pipeline" Engine/Public Client/Public` 결과를 허용 목록 0으로 만든다.
- `ModelRenderer::AppendRenderSnapshotMeshes`의 skeleton skip 제거 뒤 T-pose/duplicate regression이 없는지 capture 비교.

P2 DX12 parity 확인:

- 동일 scripted camera/replay에서 DX11과 DX12 capture를 생성하고 요소 누락, alpha/depth, animation pose, UI 좌표를 비교.
- D3D12 debug layer/PIX에서 descriptor lifetime, resource barrier, live object 오류가 0인지 확인.
- resize/fullscreen 반복과 30분 soak를 통과하는지 확인.

P3 Vulkan 확인:

- RHI conformance suite를 DX11/DX12/Vulkan에 동일 실행.
- Vulkan validation error 0, shader reflection/binding mismatch 0.
- Windows Vulkan LoLDesktop reference scene을 DX11 capture와 비교.

P4 Android 확인:

- 실제 ARM64 Android device에서 install/launch.
- background/foreground, surface loss/recreate, 해상도 변경 정책 확인.
- GPU/CPU frame time, memory, texture format, thermal throttling을 MobileForward budget과 비교.

P5 Console 확인:

- restricted CI에서만 compile/package/deploy.
- 실제 devkit에서 lifecycle, input, storage, network/platform service, crash/GPU capture 확인.
- vendor validation과 certification requirement는 공개 문서가 아니라 승인된 private 자료 기준으로 수행.

후속 동기화:

- Engine public header 변경 packet에서는 `UpdateLib.bat`로 `EngineSDK/inc`를 동기화한다.
- 각 phase가 합격할 때만 `.md/build`에 backend identity, build command, runtime duration, capture 경로, 남은 제한을 기록한다.
- Vulkan/Mobile/Console은 실제 합격 전 이력서에 지원 완료로 기재하지 않는다.
