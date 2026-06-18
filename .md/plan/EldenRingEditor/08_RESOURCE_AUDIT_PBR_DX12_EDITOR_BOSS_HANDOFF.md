Session - Elden 리소스 검토 기준으로 WP_A_7051 PBR 화면 표시 후 DX12 editor, Niagara WFX, boss blackboard tuning으로 이어간다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/EldenRingClient/Public/EldenRingProbeScene.h

기존 코드:

```cpp
#pragma once

#include "IScene.h"

#include <memory>

class CRHITestCubeRenderer;

class CEldenRingAssetProbeScene final : public IScene
{
public:
	static std::unique_ptr<CEldenRingAssetProbeScene> Create();
	~CEldenRingAssetProbeScene() override;
	
	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t deltaTime) override;
	void OnRender() override;
	void OnImGui() override;

private:
	CEldenRingAssetProbeScene() = default;

	std::unique_ptr<CRHITestCubeRenderer> m_pCubeRenderer;
};
```

아래로 교체:

```cpp
#pragma once

#include "IScene.h"

#include <memory>

class ModelRenderer;

class CEldenRingAssetProbeScene final : public IScene
{
public:
	static std::unique_ptr<CEldenRingAssetProbeScene> Create();
	~CEldenRingAssetProbeScene() override;

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t deltaTime) override;
	void OnRender() override;
	void OnImGui() override;

private:
	CEldenRingAssetProbeScene() = default;

	std::unique_ptr<ModelRenderer> m_pProbeRenderer;
	f32_t m_fProbeSeconds = 0.f;
};
```

1-2. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenAssetProbeScene.cpp

기존 코드:

```cpp
#include "EldenRingProbeScene.h"

#include "EldenRingRHITestCubeRenderer.h"
#include "GameInstance.h"
#include "RHI/IRHIDevice.h"

#include <Windows.h>

std::unique_ptr<CEldenRingAssetProbeScene> CEldenRingAssetProbeScene::Create()
{
    return std::unique_ptr<CEldenRingAssetProbeScene>(new CEldenRingAssetProbeScene());
}

CEldenRingAssetProbeScene::~CEldenRingAssetProbeScene() = default;

bool CEldenRingAssetProbeScene::OnEnter()
{
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    if (!pDevice)
        return false;

    m_pCubeRenderer = CRHITestCubeRenderer::Create(pDevice);

#if defined(_DEBUG)
    if (m_pCubeRenderer && m_pCubeRenderer->IsReady())
        OutputDebugStringA("[EldenRingAssetProbeScene] RHI cube scene ready\n");
    else
        OutputDebugStringA("[EldenRingAssetProbeScene] RHI cube scene failed\n");
#endif

    return m_pCubeRenderer != nullptr;
}

void CEldenRingAssetProbeScene::OnExit()
{
    m_pCubeRenderer.reset();
}

void CEldenRingAssetProbeScene::OnUpdate(f32_t deltaTime)
{
    if (m_pCubeRenderer)
        m_pCubeRenderer->Update(deltaTime);
}

void CEldenRingAssetProbeScene::OnRender()
{
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();

    if (m_pCubeRenderer)
        m_pCubeRenderer->Render(pDevice);
}

void CEldenRingAssetProbeScene::OnImGui()
{
}
```

아래로 교체:

```cpp
#include "EldenRingProbeScene.h"

#include "GameInstance.h"
#include "RHI/IRHIDevice.h"
#include "Renderer/ModelRenderer.h"
#include "WintersMath.h"

#include <Windows.h>

#include <DirectXMath.h>
#include <memory>

namespace
{
    constexpr char kProbeMeshPath[] =
        "Client/Bin/Resource/EldenRing/Assets/WP_A_7051_EffectMesh/Model/WP_A_7051.wmesh";
    constexpr wchar_t kProbePBRShaderPath[] = L"Shaders/Mesh3D_PBR.hlsl";
    constexpr f32_t kProbeScale = 0.018f;
}

std::unique_ptr<CEldenRingAssetProbeScene> CEldenRingAssetProbeScene::Create()
{
    return std::unique_ptr<CEldenRingAssetProbeScene>(new CEldenRingAssetProbeScene());
}

CEldenRingAssetProbeScene::~CEldenRingAssetProbeScene() = default;

bool CEldenRingAssetProbeScene::OnEnter()
{
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    if (!pDevice)
        return false;

    if (pDevice->GetBackend() != eRHIBackend::DX11)
    {
#if defined(_DEBUG)
        OutputDebugStringA("[EldenRingAssetProbeScene] WP_A_7051 PBR smoke uses legacy DX11 ModelRenderer for now; run --rhi=dx11\n");
#endif
        return false;
    }

    m_pProbeRenderer = std::make_unique<ModelRenderer>();
    if (!m_pProbeRenderer->Initialize(kProbeMeshPath, kProbePBRShaderPath))
    {
        m_pProbeRenderer.reset();
#if defined(_DEBUG)
        OutputDebugStringA("[EldenRingAssetProbeScene] WP_A_7051 PBR model load failed\n");
#endif
        return false;
    }

    m_pProbeRenderer->SetForceStaticMeshPath(true);
    m_pProbeRenderer->UpdateTransform(Mat4::Scale(kProbeScale, kProbeScale, kProbeScale));

#if defined(_DEBUG)
    OutputDebugStringA("[EldenRingAssetProbeScene] WP_A_7051 PBR model ready\n");
#endif
    return true;
}

void CEldenRingAssetProbeScene::OnExit()
{
    m_pProbeRenderer.reset();
    m_fProbeSeconds = 0.f;
}

void CEldenRingAssetProbeScene::OnUpdate(f32_t deltaTime)
{
    m_fProbeSeconds += deltaTime;

    if (m_pProbeRenderer)
        m_pProbeRenderer->Update(deltaTime);
}

void CEldenRingAssetProbeScene::OnRender()
{
    if (!m_pProbeRenderer)
        return;

    const Vec3 vEye{ 0.f, 1.35f, -4.0f };
    const Vec3 vTarget{ 0.f, 0.65f, 0.f };
    const Mat4 matWorld =
        Mat4::Scale(kProbeScale, kProbeScale, kProbeScale) *
        Mat4::RotationY(m_fProbeSeconds * 0.35f);
    const Mat4 matView = Mat4::LookAt(vEye, vTarget, Vec3{ 0.f, 1.f, 0.f });
    const Mat4 matProjection = Mat4::Perspective(
        DirectX::XMConvertToRadians(50.f),
        16.f / 9.f,
        0.05f,
        100.f);
    const Mat4 matViewProjection = matView * matProjection;

    m_pProbeRenderer->UpdateTransform(matWorld);
    m_pProbeRenderer->UpdateCamera(matViewProjection, vEye);
    m_pProbeRenderer->Render();
}

void CEldenRingAssetProbeScene::OnImGui()
{
}
```

1-3. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/main.cpp

기존 코드:

```cpp
        if (!pCommandLine)
            return eEngineRHIBackend::DX12;
```

아래로 교체:

```cpp
        if (!pCommandLine)
            return eEngineRHIBackend::DX11;
```

기존 코드:

```cpp
        return eEngineRHIBackend::DX12;
```

아래로 교체:

```cpp
        return eEngineRHIBackend::DX11;
```

1-4. C:/Users/tnest/Desktop/Winters/EldenRingEditor/Private/main.cpp

기존 코드:

```cpp
#include <Windows.h>

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "EldenRingEditorApp.h"
#include "WintersEngine.h"

#include <cwchar>

extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace
{
    eEngineRHIBackend ParseRequestedRHIBackend()
    {
        const wchar_t* const pCommandLine = ::GetCommandLineW();
        if (!pCommandLine)
            return eEngineRHIBackend::DX11;

        if (wcsstr(pCommandLine, L"--rhi=dx12") || wcsstr(pCommandLine, L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
            return eEngineRHIBackend::Null;

        return eEngineRHIBackend::DX11;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTNACE, LPWSTR, int)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    EngineConfig config{};
    config.windowTitle = L"EldenRingEditor";
    config.windowWidth = 1600;
    config.windowHeight = 900;
    config.fullscreen = false;
    config.vsync = true;
    config.targetFPS = 60;
    config.iNumScenes = 1;
    config.rhiBackend = ParseRequestedRHIBackend();
    config.allowRHIFallback = true;

    return WintersRun(&app, config);
}
```

아래로 교체:

```cpp
#include <Windows.h>

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "EldenRingEditorApp.h"
#include "WintersEngine.h"

#include <cwchar>

extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace
{
    eEngineRHIBackend ParseRequestedRHIBackend()
    {
        const wchar_t* const pCommandLine = ::GetCommandLineW();
        if (!pCommandLine)
            return eEngineRHIBackend::DX11;

        if (wcsstr(pCommandLine, L"--rhi=auto") || wcsstr(pCommandLine, L"/rhi:auto"))
            return eEngineRHIBackend::Auto;
        if (wcsstr(pCommandLine, L"--rhi=dx12") || wcsstr(pCommandLine, L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
            return eEngineRHIBackend::Null;

        return eEngineRHIBackend::DX11;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    CEldenRingEditorApp app;

    EngineConfig config{};
    config.windowTitle = L"EldenRingEditor";
    config.windowWidth = 1600;
    config.windowHeight = 900;
    config.fullscreen = false;
    config.vsync = true;
    config.targetFPS = 60;
    config.iNumScenes = 1;
    config.rhiBackend = ParseRequestedRHIBackend();
    config.allowRHIFallback = true;

    return WintersRun(&app, config);
}
```

1-5. C:/Users/tnest/Desktop/Winters/EldenRingEditor/CMakeLists.txt

기존 코드:

```cmake
set(WINTERS_ELDENRING_EDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(WINTERS_ELDENRING_OUTPUT_DIR "${WINTERS_ELDENRING_EDITOR_DIR}/Bin")

set(WINTERS_ELDENRING_EDITOR_SOURCES
	"${WINTERS_ELDENRING_EDITOR_DIR}/Private/main.cpp"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Private/EldenRingEditorApp.cpp"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Private/EldenRingEditorScene.cpp"
)

set(WINTERS_ELDENRING_EDITOR_HEADERS
	"${WINTERS_ELDENRING_EDITOR_DIR}/Public/EldenRingEditorApp.h"
	"${WINTERS_ELDENRING_EDITOR_DIR}/Public/EldenRingEditorScene.h"
)

add_executable(WintersEldenEditor WIN32
	${WINTERS_ELDENRING_EDITOR_SOURCES}
	${WINTERS_ELDENRING_EDITOR_HEADERS}
)

set_target_properties(WintersEldenRingEditor PROPERTIES
	OUTPUT_NAME "WintersEldenRingEditor"
	FOLDER "Editor/EldenRing"
	VS_DEBUGGER_WORKING_DIRECTORY "${WINTERS_ROOT_DIR}"
)

WintersApplyMsvcCommonOptions(WintersEldenRingEditor)
WintersSetOutputDirectories(WintersEldenRingEditor "${WINTERS_ELDEN_EDITOR_OUTPUT_DIR}")

target_compile_definitions(WintersEldenRingEditor PRIVATE
	WIN32 
	UNICODE
	_UNICODE
	_WINDOWS
	NOMINMAX
	WINTERS_PROFILING
	WINTERS_ELDENRING_EDITOR
	$<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Debug>:WINTERS_ENABLE_NON_AI_DEBUG_STRING=1>
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(WintersEldenEditor PRIVATE
    "${WINTERS_ROOT_DIR}"
    "${WINTERS_ELDEN_EDITOR_DIR}/Public"
    "${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FlatBuffers/Inc"
)

target_link_libraries(WintersEldenEditor PRIVATE
    Winters::Engine
)

source_group(TREE "${WINTERS_ELDEN_EDITOR_DIR}" FILES
    ${WINTERS_ELDEN_EDITOR_SOURCES}
    ${WINTERS_ELDEN_EDITOR_HEADERS}
)

add_custom_command(TARGET WintersEldenEditor POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:WintersEngine>"
        "$<TARGET_FILE_DIR:WintersEldenEditor>"
    COMMENT "Copy WintersEngine runtime DLL to EldenRingEditor output"
    VERBATIM
)

add_custom_command(TARGET WintersEldenEditor POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "$<$<CONFIG:Debug>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/Assimp/Bin/Debug>$<$<CONFIG:Release>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/Assimp/Bin/Release>"
        "$<TARGET_FILE_DIR:WintersEldenEditor>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "$<$<CONFIG:Debug>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/DirectXTK/Bin/Debug>$<$<CONFIG:Release>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/DirectXTK/Bin/Release>"
        "$<TARGET_FILE_DIR:WintersEldenEditor>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FMOD/Bin/fmod.dll"
        "$<TARGET_FILE_DIR:WintersEldenEditor>"
    COMMENT "Copy third-party runtime DLLs to EldenRingEditor output"
    VERBATIM
)

add_custom_command(TARGET WintersEldenEditor POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${WINTERS_ROOT_DIR}/Shaders"
        "$<TARGET_FILE_DIR:WintersEldenEditor>/Shaders"
    COMMENT "Copy shaders to EldenRingEditor output"
    VERBATIM
)
```

아래로 교체:

```cmake
set(WINTERS_ELDEN_EDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(WINTERS_ELDEN_EDITOR_OUTPUT_DIR "${WINTERS_ELDEN_EDITOR_DIR}/Bin")

set(WINTERS_ELDEN_EDITOR_SOURCES
    "${WINTERS_ELDEN_EDITOR_DIR}/Private/main.cpp"
    "${WINTERS_ELDEN_EDITOR_DIR}/Private/EldenRingEditorApp.cpp"
    "${WINTERS_ELDEN_EDITOR_DIR}/Private/EldenRingEditorScene.cpp"
)

set(WINTERS_ELDEN_EDITOR_HEADERS
    "${WINTERS_ELDEN_EDITOR_DIR}/Public/EldenRingEditorApp.h"
    "${WINTERS_ELDEN_EDITOR_DIR}/Public/EldenRingEditorScene.h"
)

add_executable(WintersEldenEditor WIN32
    ${WINTERS_ELDEN_EDITOR_SOURCES}
    ${WINTERS_ELDEN_EDITOR_HEADERS}
)

set_target_properties(WintersEldenEditor PROPERTIES
    OUTPUT_NAME "WintersEldenEditor"
    FOLDER "Editor/EldenRing"
    VS_DEBUGGER_WORKING_DIRECTORY "${WINTERS_ROOT_DIR}"
)

WintersApplyMsvcCommonOptions(WintersEldenEditor)
WintersSetOutputDirectories(WintersEldenEditor "${WINTERS_ELDEN_EDITOR_OUTPUT_DIR}")

target_compile_definitions(WintersEldenEditor PRIVATE
    WIN32
    UNICODE
    _UNICODE
    _WINDOWS
    NOMINMAX
    WINTERS_PROFILING
    WINTERS_ELDENRING_EDITOR
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Debug>:WINTERS_ENABLE_NON_AI_DEBUG_STRING=1>
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(WintersEldenEditor PRIVATE
    "${WINTERS_ROOT_DIR}"
    "${WINTERS_ELDEN_EDITOR_DIR}/Public"
    "${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FlatBuffers/Inc"
)

target_link_libraries(WintersEldenEditor PRIVATE
    Winters::Engine
)

source_group(TREE "${WINTERS_ELDEN_EDITOR_DIR}" FILES
    ${WINTERS_ELDEN_EDITOR_SOURCES}
    ${WINTERS_ELDEN_EDITOR_HEADERS}
)

add_custom_command(TARGET WintersEldenEditor POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:WintersEngine>"
        "$<TARGET_FILE_DIR:WintersEldenEditor>"
    COMMENT "Copy WintersEngine runtime DLL to EldenRingEditor output"
    VERBATIM
)

add_custom_command(TARGET WintersEldenEditor POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "$<$<CONFIG:Debug>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/Assimp/Bin/Debug>$<$<CONFIG:Release>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/Assimp/Bin/Release>"
        "$<TARGET_FILE_DIR:WintersEldenEditor>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "$<$<CONFIG:Debug>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/DirectXTK/Bin/Debug>$<$<CONFIG:Release>:${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/DirectXTK/Bin/Release>"
        "$<TARGET_FILE_DIR:WintersEldenEditor>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${WINTERS_ROOT_DIR}/Engine/ThirdPartyLib/FMOD/Bin/fmod.dll"
        "$<TARGET_FILE_DIR:WintersEldenEditor>"
    COMMENT "Copy third-party runtime DLLs to EldenRingEditor output"
    VERBATIM
)

add_custom_command(TARGET WintersEldenEditor POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${WINTERS_ROOT_DIR}/Shaders"
        "$<TARGET_FILE_DIR:WintersEldenEditor>/Shaders"
    COMMENT "Copy shaders to EldenRingEditor output"
    VERBATIM
)
```

1-6. C:/Users/tnest/Desktop/Winters/Client/Bin/Resource/EldenRing/Boss/Margit/boss_tuning.json

새 파일:

```json
{
  "schema": "winters.eldenring.boss_tuning.v1",
  "bossId": "c2130_margit",
  "blackboard": {
    "scanRange": 32.0,
    "leashRange": 42.0,
    "meleeRange": 4.5,
    "midRange": 12.0,
    "phase2HpRatio": 0.55
  },
  "hfsm": {
    "engageCooldown": 0.35,
    "comboRecover": 0.75,
    "staggerRecover": 1.40
  },
  "behaviorTree": {
    "preferGapCloseDistance": 10.0,
    "retreatHpRatio": 0.10,
    "comboScoreMargin": 0.15
  }
}
```

1-7. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/BossAIComponent.h

새 파일:

```cpp
CONFIRM_NEEDED - Shared/GameSim component registration, snapshot debug schema, ChampionAIComponent.h의 tuning/trace 복제 범위를 확정한 뒤 전체 파일 본문을 작성한다.

필수 책임:
- boss kind: Margit, TreeGuard, GenericMob.
- blackboard keys: target entity, distance, hp ratio, phase, stagger, arena anchor, last attack id.
- HFSM state: Idle, Patrol, Engage, Combo, Recover, Retreat, Staggered, Dead.
- behavior tree decision result는 GameCommand 후보로만 나간다.
- Client/editor panel은 tuning command와 debug view만 만들고 gameplay truth를 직접 만들지 않는다.
```

1-8. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/BossAI/BossAISystem.cpp

새 파일:

```cpp
CONFIRM_NEEDED - CChampionAISystem의 GameCommand producer 패턴과 GameRoom tick drain 순서를 다시 확인한 뒤 전체 구현 본문을 작성한다.

구현 기준:
- BossAI는 Transform/HP/cooldown을 직접 mutate하지 않는다.
- BossAI는 outCommands 또는 기존 pending command queue에 GameCommand만 생산한다.
- 서버 Snapshot/Event가 client visual, animation, FX, UI로 내려간다.
- Editor runtime tuning은 서버 command로 들어가고 debug trace로 되돌아온 값만 표시한다.
```

2. 검증

현재 리소스 검토 기준:
- `Client/Bin/Resource/EldenRing` 파일 수량: `.dcx` 3201, `.png` 564, `.gfx` 121, `.wmesh` 90, `.wmat` 90, `.dds` 65, `.wanim` 20, `.json` 13, `.fbx` 8, `.wskel` 4.
- cooked 카테고리: `SourceBundles` 3192, `Assets` 609, `UI` 213, `Characters` 167, `Manifests` 9, `Maps` 2.
- `eldenring_pipeline_status.json`: `wmesh=90`, `wmat=90`, `wskel=4`, `wanim=20`, `totalWmeshBytes=166349970`.
- texture suffix: `_a` 295, `_n` 165, `_m` 66, `_r` 8, `_em` 25, `_1m` 6, `_3m` 1.
- 첫 PBR smoke target: `Client/Bin/Resource/EldenRing/Assets/WP_A_7051_EffectMesh/Model/WP_A_7051.wmesh`, converter info `submeshes=6 bones=39 vertices=1485 indices=3420 stride=76`.
- 후속 static fallback 후보: `c3010_static_fallback.wmesh`는 `bones=354`, `c3251_flver_static.wmesh`는 `bones=679`라 256 bone shader/cbuffer 제한 해제 전 첫 smoke에 쓰지 않는다.

검증 명령:
- `git diff --check`
- `& .\Tools\Bin\Debug\WintersAssetConverter.exe info "Client\Bin\Resource\EldenRing\Assets\WP_A_7051_EffectMesh\Model\WP_A_7051.wmesh"`
- `msbuild EldenRingClient/Include/EldenRingClient.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `EldenRingClient/Bin/Debug/WintersElden.exe --rhi=dx11`
- `cmake --build --preset elden-debug`

수동 확인:
- Debug Output에 `[EldenRingAssetProbeScene] WP_A_7051 PBR model ready`가 출력된다.
- `WintersElden.exe --rhi=dx11`에서 회전하는 `WP_A_7051` mesh가 보인다.
- `ModelRenderer::UsesPBR()`가 true인 경로로 들어간다.
- `--rhi=dx12`는 아직 PBR smoke 합격 기준이 아니다. DX12는 editor bootstrap과 RHI material/texture binding 세션에서 별도 합격시킨다.
- `WintersEldenEditor`는 먼저 DX11 ImGui 빈 scene으로 빌드/부팅된다.
- DX12 editor 진입 전 `Engine/Private/Framework/CEngineApp.cpp`의 non-DX11 ImGui skip과 `Engine/Private/Editor/ImGuiLayer.cpp`의 DX12 backend 부재를 확인한다.
- DX12 ImGui bootstrap 통과 뒤 `EldenRingEditor/Public/EldenRingEditorScene.h`, `EldenRingEditor/Private/EldenRingEditorScene.cpp`, `Client/Private/UI/WfxEffectToolPanel.cpp`를 다시 읽고 Asset Catalog, WFX/Niagara, Boss Blackboard panel state를 구체 코드로 작성한다.
- WFX/Niagara 작업은 기존 `Client/Private/UI/WfxEffectToolPanel.cpp`의 WFX document load/save를 깨지 않고, editor panel 이식 전 `Client/Bin/Resource/EldenRing` texture scan부터 통과시킨다.
- Boss blackboard/HFSM/BT tuning은 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual` 흐름을 보존한다.

후속 동기화:
- Engine public header 변경 시 `UpdateLib.bat` 실행 필요.
- 새 `.h/.cpp`가 추가되면 MSBuild `.vcxproj` 포함 여부와 CMake target 포함 여부를 함께 확인한다.
