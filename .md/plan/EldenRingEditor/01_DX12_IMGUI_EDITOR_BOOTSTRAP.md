Session - DX12 ImGui Editor bootstrap

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Public/Editor/ImGuiLayer.h

기존 코드:

```cpp
class CImGuiLayer
{
public:
	CImGuiLayer() = default;
	~CImGuiLayer() { ShutDown(); }

	CImGuiLayer(const CImGuiLayer&) = delete;
	CImGuiLayer& operator=(const CImGuiLayer&) = delete;

	bool Initialize(void* hWnd, IRHIDevice* pDevice);

	void BeginFrame();
	void EndFrame();
	void ShutDown();
	bool WantsCaptureMouse() const;
	bool WantsCaptureKeyboard() const;

private:
	bool m_bInitialized = false;
};
```

아래로 교체:

```cpp
class CImGuiLayer
{
public:
	CImGuiLayer() = default;
	~CImGuiLayer();

	CImGuiLayer(const CImGuiLayer&) = delete;
	CImGuiLayer& operator=(const CImGuiLayer&) = delete;

	bool Initialize(void* hWnd, IRHIDevice* pDevice);

	void BeginFrame();
	void EndFrame();
	void ShutDown();
	bool IsInitialized() const { return m_bInitialized; }
	bool WantsCaptureMouse() const;
	bool WantsCaptureKeyboard() const;

private:
	struct DX12BackendState;

	bool m_bInitialized = false;
	eRHIBackend m_eBackend = eRHIBackend::DX11;
	IRHIDevice* m_pDevice = nullptr;
	DX12BackendState* m_pDX12State = nullptr;
};
```

의도:
- Public header에 `d3d12.h`, `wrl/client.h`, DX12 backend type을 노출하지 않는다.
- DX12 전용 descriptor heap과 backend init 정보는 `DX12BackendState`로 cpp에 숨긴다.
- `IsInitialized()`를 추가해서 `CEngineApp`가 DX11/DX12 공통 ImGui 렌더 경로를 안전하게 판단한다.

1-2. C:/Users/tnest/Desktop/Winters/Engine/Private/Editor/ImGuiLayer.cpp

기존 코드:

```cpp
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <Windows.h>
#include <d3d11.h>
```

아래에 추가:

```cpp
#include "imgui_impl_dx12.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
```

기존 코드:

기존 코드:

```cpp
	ID3D11DeviceContext* GetNativeDX11Context(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D11DeviceContext*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext));
	}
```

아래에 추가:

```cpp
	ID3D12Device* GetNativeDX12Device(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D12Device*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX12Device));
	}

	ID3D12CommandQueue* GetNativeDX12CommandQueue(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D12CommandQueue*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX12CommandQueue));
	}

	ID3D12GraphicsCommandList* GetNativeDX12CommandList(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D12GraphicsCommandList*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX12CommandList));
	}
```

기존 코드:

```cpp
bool CImGuiLayer::Initialize(void* hWnd, IRHIDevice* pDevice)
{
	if (m_bInitialized)
		return true;

	ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
	ID3D11DeviceContext* pNativeContext = GetNativeDX11Context(pDevice);
	if (!pNativeDevice || !pNativeContext)
		return false;
```

아래로 교체:

```cpp
CImGuiLayer::~CImGuiLayer()
{
	ShutDown();
}

bool CImGuiLayer::Initialize(void* hWnd, IRHIDevice* pDevice)
{
	if (m_bInitialized)
		return true;
	if (!hWnd || !pDevice)
		return false;

	const eRHIBackend backend = pDevice->GetBackend();
	if (backend != eRHIBackend::DX11 && backend != eRHIBackend::DX12)
		return false;
```

기존 코드:

```cpp
	//3. Platform + Renderer Backend Initialize
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(pNativeDevice, pNativeContext);

	m_bInitialized = true;

	return true;
}
```

아래로 교체:

```cpp
	//3. Platform + Renderer Backend Initialize
	if (!ImGui_ImplWin32_Init(hWnd))
		return false;

	bool bRendererReady = false;
	if (backend == eRHIBackend::DX11)
	{
		ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
		ID3D11DeviceContext* pNativeContext = GetNativeDX11Context(pDevice);
		bRendererReady = pNativeDevice && pNativeContext &&
			ImGui_ImplDX11_Init(pNativeDevice, pNativeContext);
	}
	else
	{
		CONFIRM_NEEDED - ImGui DX12 backend init block will allocate one shader-visible CBV/SRV/UAV descriptor heap,
		fill ImGui_ImplDX12_InitInfo with Device, CommandQueue, NumFramesInFlight=2, RTVFormat=DXGI_FORMAT_R8G8B8A8_UNORM,
		and use the new callback allocator API exposed by Engine/External/imgui/backends/imgui_impl_dx12.h.
		The exact code body must be finalized while editing because this backend version supports both legacy handles and InitInfo callbacks.
	}

	if (!bRendererReady)
	{
		ImGui_ImplWin32_Shutdown();
		return false;
	}

	m_eBackend = backend;
	m_pDevice = pDevice;
	m_bInitialized = true;
	return true;
}
```

기존 코드:

```cpp
void CImGuiLayer::BeginFrame()
{
	if (!m_bInitialized)
		return;
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}
```

아래로 교체:

```cpp
void CImGuiLayer::BeginFrame()
{
	if (!m_bInitialized)
		return;

	if (m_eBackend == eRHIBackend::DX11)
		ImGui_ImplDX11_NewFrame();
	else if (m_eBackend == eRHIBackend::DX12)
		ImGui_ImplDX12_NewFrame();

	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}
```

기존 코드:

```cpp
void CImGuiLayer::EndFrame()
{
	if (!m_bInitialized)
		return;

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
```

아래로 교체:

```cpp
void CImGuiLayer::EndFrame()
{
	if (!m_bInitialized)
		return;

	ImGui::Render();

	if (m_eBackend == eRHIBackend::DX11)
	{
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		return;
	}

	ID3D12GraphicsCommandList* pCommandList = GetNativeDX12CommandList(m_pDevice);
	if (pCommandList)
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList);
}
```

기존 코드:

```cpp
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_bInitialized = false;
```

아래로 교체:

```cpp
	if (m_eBackend == eRHIBackend::DX11)
		ImGui_ImplDX11_Shutdown();
	else if (m_eBackend == eRHIBackend::DX12)
		ImGui_ImplDX12_Shutdown();

	delete m_pDX12State;
	m_pDX12State = nullptr;

	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_pDevice = nullptr;
	m_eBackend = eRHIBackend::DX11;
	m_bInitialized = false;
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Public/Framework/CEngineApp.h

기존 코드:

```cpp
    bool m_bSceneRuntimeEnabled = false;
    bool m_bDX11RuntimeEnabled = false;
    bool m_bGameInitialized = false;
```

아래로 교체:

```cpp
    bool m_bSceneRuntimeEnabled = false;
    bool m_bDX11RuntimeEnabled = false;
    bool m_bImGuiRuntimeEnabled = false;
    bool m_bGameInitialized = false;
```

1-4. C:/Users/tnest/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:

```cpp
    if (m_bDX11RuntimeEnabled)
    {

    if(!m_ImGui.Initialize(m_Window.GetHandle(), m_pDevice.get()))
    {
        OutputDebugStringA("[CEngineApp] ImGui initialization failed\n");
        return false;
    }
```

아래로 교체:

```cpp
    m_bImGuiRuntimeEnabled = m_ImGui.Initialize(m_Window.GetHandle(), m_pDevice.get());
    if (!m_bImGuiRuntimeEnabled)
    {
        OutputDebugStringA("[CEngineApp] ImGui initialization failed\n");
        return false;
    }

    if (m_bDX11RuntimeEnabled)
    {
```

기존 코드:

```cpp
    if (m_bDX11RuntimeEnabled)
    {
        {
            WINTERS_PROFILE_SCOPE("ImGui::BeginFrame");
            m_ImGui.BeginFrame();
        }
```

아래로 교체:

```cpp
    if (m_bImGuiRuntimeEnabled)
    {
        {
            WINTERS_PROFILE_SCOPE("ImGui::BeginFrame");
            m_ImGui.BeginFrame();
        }
```

기존 코드:

```cpp
        {
            WINTERS_PROFILE_SCOPE("UI::Cursor");
            CGameInstance::Get()->UI_Render_Cursor();
        }
    }
    else if (m_bSceneRuntimeEnabled)
```

아래로 교체:

```cpp
        if (m_bDX11RuntimeEnabled)
        {
            WINTERS_PROFILE_SCOPE("UI::Cursor");
            CGameInstance::Get()->UI_Render_Cursor();
        }
    }
    else if (m_bSceneRuntimeEnabled)
```

기존 코드:

```cpp
    if (m_bDX11RuntimeEnabled)
    {
        CGameInstance::Get()->UI_Shutdown();

        m_ResourceCache.Clear();

        ReleaseSharedShaders();

        m_ImGui.ShutDown();
    }

    m_bSceneRuntimeEnabled = false;
    m_bDX11RuntimeEnabled = false;
```

아래로 교체:

```cpp
    if (m_bDX11RuntimeEnabled)
    {
        CGameInstance::Get()->UI_Shutdown();

        m_ResourceCache.Clear();

        ReleaseSharedShaders();
    }

    if (m_bImGuiRuntimeEnabled)
        m_ImGui.ShutDown();

    m_bSceneRuntimeEnabled = false;
    m_bDX11RuntimeEnabled = false;
    m_bImGuiRuntimeEnabled = false;
```

1-5. C:/Users/tnest/Desktop/Winters/Engine/Include/Engine.vcxproj

기존 코드:

```xml
<ClCompile Include="..\External\imgui\backends\imgui_impl_dx11.cpp" />
<ClCompile Include="..\External\imgui\backends\imgui_impl_win32.cpp" />
```

아래에 추가:

```xml
확인 필요: Engine.vcxproj / Engine.vcxproj.filters에 ..\External\imgui\backends\imgui_impl_dx12.cpp가 포함되어야 한다.
계획서 규칙상 프로젝트 XML 변경은 코드 세션에서 확인 후 별도 반영한다.
```

2. 검증

2-1. 검증 명령

```powershell
msbuild Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:EldenRingClient /p:Configuration=Debug-DX12 /p:Platform=x64
```

2-2. 런타임 확인

```text
1. EldenRingClient를 DX12 backend로 실행한다.
2. ImGui debug overlay가 DX12 clear/present 위에 표시되는지 확인한다.
3. Scene::ImGui, App::OnImGui, ProfilerOverlay가 DX11 전용 UI cursor 없이도 호출되는지 확인한다.
4. 종료 시 DX12 descriptor heap/ImGui backend shutdown live object 경고가 남지 않는지 확인한다.
```

2-3. 후속 동기화

```text
Engine/Public header가 바뀌므로 성공 빌드 후 UpdateLib.bat 경로로 EngineSDK/inc 동기화 여부를 확인한다.
DX12 ImGui가 안정화되기 전에는 기존 LoL DX11 Client의 UI_Initialize, shared shader, BlendStateCache 경로를 건드리지 않는다.
```
