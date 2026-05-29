# ImGui DX11 이식 계획서

> **★ 정책 반전 (2026-04-16)**: 기존 gotchas #14 "ImGui Tools 전용 — Engine DLL 포함 금지" → **"ImGui 적극 사용, Engine DLL 통합 허용"**으로 변경.
> 이유: 런타임 튜닝 = 빌드 1번으로 모든 값 조정 = 기획/디자인 이터레이션 속도 = 게임 품질.
> Release 빌드는 `WINTERS_EDITOR` 매크로 `#ifdef`로 완전 제거.
> **시작일**: 2026-04-17 (Phase 1a 안정화 직후)

## Context
이전 프로젝트(Minecraft Dungeons)의 ImGui DX9 연동 패턴을 Winters Engine의 DX11 환경으로 이식한다.
- **이전**: `ImGuiLayer` + `imgui_impl_dx9` + `imgui_impl_win32` (DX9)
- **이식**: `CImGuiLayer` + `imgui_impl_dx11` + `imgui_impl_win32` (DX11)

핵심 변경: DX9 → DX11 백엔드 교체, Engine DLL 내부에 통합, PCH 충돌 방지.

## 우선순위 (정책 반전에 따른 추가)

내일 시작 시 다음 순서로 UI 추가:

1. **Entity Inspector** — TransformComponent (5명 캐릭터 위치/회전/스케일 슬라이더)
2. **Profiler Overlay** — 프레임 타임 플롯, FPS, GPU 시간
3. **Material Tuner** — albedo/roughness/metallic 슬라이더, 텍스처 미리보기
4. **Animation Blender** — 가중치, 본 회전 디버그
5. **Console** — `set_time_scale`, `reload_shaders`, `kill_player`
6. **Render Debug** — G-Buffer 채널 시각화 (Phase 2 RenderGraph 이후)
7. **Physics Debug** — 콜라이더 와이어프레임 (Jolt 도입 시)
8. **Network Debug** — 패킷 로그, RTT 플롯 (Phase 4 IOCP 이후)

**원칙**: 새 시스템 작성 시 **튜닝 가능한 모든 파라미터에 ImGui 슬라이더 의무화**. 하드코딩 금지.

---

## 1. 파일 구조

```
Engine/
  External/imgui/                          ← ImGui 소스 (GitHub v1.91.x docking 브랜치)
    imgui.h
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_internal.h
    imgui_demo.cpp                         ← 개발 중 데모 윈도우용 (선택)
    imconfig.h
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
    backends/
      imgui_impl_dx11.h
      imgui_impl_dx11.cpp
      imgui_impl_win32.h
      imgui_impl_win32.cpp
  Header/Editor/
    CImGuiLayer.h                          ← 새 파일 (래퍼 클래스)
  Code/Editor/
    CImGuiLayer.cpp                        ← 새 파일 (래퍼 구현)
```

---

## 2. 새 파일: `Engine/Header/Editor/CImGuiLayer.h`

```cpp
// Engine/Header/Editor/CImGuiLayer.h
#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;

// ─────────────────────────────────────────────────────────────────
//  CImGuiLayer  |  ImGui 초기화/프레임/종료 래퍼
//
//  이전 프로젝트(Minecraft Dungeons)의 ImGuiLayer 패턴을
//  DX11 + Winters Engine 아키텍처에 맞게 이식.
//
//  생명주기:
//    Initialize() → [BeginFrame() → ... → EndFrame()] × N → Shutdown()
//
//  엔진 내부(Engine/Header)에만 존재. Client에 노출되지 않음.
// ─────────────────────────────────────────────────────────────────

class CImGuiLayer
{
public:
    CImGuiLayer()  = default;
    ~CImGuiLayer() { Shutdown(); }

    CImGuiLayer(const CImGuiLayer&)            = delete;
    CImGuiLayer& operator=(const CImGuiLayer&) = delete;

    // HWND + DX11 디바이스/컨텍스트로 초기화
    bool    Initialize(void* hWnd,
                       ID3D11Device* pDevice,
                       ID3D11DeviceContext* pContext);

    // 프레임 시작: ImGui_ImplDX11_NewFrame + ImGui_ImplWin32_NewFrame + ImGui::NewFrame
    void    BeginFrame();

    // 프레임 끝: ImGui::Render + ImGui_ImplDX11_RenderDrawData
    void    EndFrame();

    // 정리: 백엔드 Shutdown + DestroyContext
    void    Shutdown();

    // ImGui가 입력을 캡처 중인지 (게임 입력 무시용)
    bool    WantsCaptureMouse()    const;
    bool    WantsCaptureKeyboard() const;

private:
    bool    m_bInitialized = false;
};
```

**L1**: `#pragma once`
**L3-4**: 전방 선언 — imgui 헤더를 포함하지 않고 DX11 타입만 선언
**L30**: `Initialize()` — void* hWnd로 받아 ImGui_ImplWin32_Init에 전달
**L34**: `BeginFrame()` — 매 프레임 OnRender() 전에 호출
**L37**: `EndFrame()` — OnRender() 후, Present() 전에 호출
**L40**: `Shutdown()` — 소멸자에서도 호출 (RAII)
**L43-44**: 입력 캡처 쿼리 — ImGui가 마우스/키보드 사용 중이면 게임 입력 무시

---

## 3. 새 파일: `Engine/Code/Editor/CImGuiLayer.cpp`

```cpp
// Engine/Code/Editor/CImGuiLayer.cpp
#include "Editor/CImGuiLayer.h"

#include "../../External/imgui/imgui.h"
#include "../../External/imgui/backends/imgui_impl_win32.h"
#include "../../External/imgui/backends/imgui_impl_dx11.h"

#include <d3d11.h>

// ─────────────────────────────────────────────────────────────────
//  CImGuiLayer 구현
//
//  Minecraft Dungeons 프로젝트의 ImGuiLayer 패턴을
//  DX9 → DX11로 변환하여 이식.
//
//  초기화 순서: CreateContext → 설정 → StyleColors → Win32 Init → DX11 Init
//  프레임 순서: DX11 NewFrame → Win32 NewFrame → ImGui::NewFrame
//  종료 순서: DX11 Shutdown → Win32 Shutdown → DestroyContext
// ─────────────────────────────────────────────────────────────────

bool CImGuiLayer::Initialize(void* hWnd,
                              ID3D11Device* pDevice,
                              ID3D11DeviceContext* pContext)
{
    if (m_bInitialized)
        return true;

    // ── 1. ImGui 컨텍스트 생성 ────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // 키보드 네비게이션
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // 도킹 레이아웃

    // ── 2. 테마 설정 (Modern Dark — 이전 프로젝트 동일) ───────
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 4.0f;
    style.ChildRounding    = 3.0f;
    style.FrameRounding    = 3.0f;
    style.GrabRounding     = 3.0f;
    style.PopupRounding    = 3.0f;
    style.ScrollbarRounding = 3.0f;

    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_Header]         = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered]  = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive]   = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
    style.Colors[ImGuiCol_Button]         = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered]  = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive]   = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
    style.Colors[ImGuiCol_FrameBg]        = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive]  = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);

    // ── 3. 플랫폼 + 렌더러 백엔드 초기화 ─────────────────────
    //  DX9: ImGui_ImplDX9_Init(device)
    //  DX11: ImGui_ImplDX11_Init(device, context)  ← 컨텍스트 추가 필요
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(pDevice, pContext);

    m_bInitialized = true;
    return true;
}

void CImGuiLayer::BeginFrame()
{
    if (!m_bInitialized)
        return;

    // DX9:  ImGui_ImplDX9_NewFrame()
    // DX11: ImGui_ImplDX11_NewFrame()  ← 동일 패턴
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void CImGuiLayer::EndFrame()
{
    if (!m_bInitialized)
        return;

    ImGui::Render();

    // DX9:  ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData())
    // DX11: ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData())  ← 동일 패턴
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void CImGuiLayer::Shutdown()
{
    if (!m_bInitialized)
        return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    m_bInitialized = false;
}

bool CImGuiLayer::WantsCaptureMouse() const
{
    if (!m_bInitialized)
        return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool CImGuiLayer::WantsCaptureKeyboard() const
{
    if (!m_bInitialized)
        return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}
```

**L4-6**: ImGui 헤더 include — External 상대경로 사용
**L28-30**: 중복 초기화 방지 (m_bInitialized 가드)
**L62-63**: DX9→DX11 핵심 차이 — `ImGui_ImplDX11_Init(pDevice, pContext)` (DX9는 device만)
**L70-76**: BeginFrame — 이전 프로젝트와 동일 패턴 (DX→Win32→ImGui 순서)
**L82-87**: EndFrame — Render → RenderDrawData 순서
**L91-98**: Shutdown — DX11→Win32→DestroyContext 순서 (초기화 역순)

---

## 4. 수정: `Engine/Code/Platform/CWin32Window.cpp` — WndProc ImGui 후킹

**파일**: `Engine/Code/Platform/CWin32Window.cpp`

### 수정 전 (L1-3):
```cpp
#include "Platform/CWin32Window.h"
#include "CInput.h"
```

### 수정 후 (L1-6):
```cpp
#include "Platform/CWin32Window.h"
#include "CInput.h"

// ImGui Win32 메시지 핸들러 전방 선언
// (imgui_impl_win32.h를 직접 포함하지 않고 extern으로 연결)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
```

> **주의**: `IMGUI_IMPL_API`가 해석 안 되면 아래처럼 순수 extern 사용:
> ```cpp
> extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
> ```

### 수정 전 (L102-164) WndProc 함수:
```cpp
LRESULT CALLBACK CWin32Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CWin32Window* pThis = nullptr;

    if (msg == WM_NCCREATE)
    {
        auto* pCS = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<CWin32Window*>(pCS->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CWin32Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    // ── 입력 메시지를 CInput에 전달 ──────────────────────────
    CInput& input = CInput::Get();

    switch (msg)
    {
    // ... (기존 case 문들)
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
```

### 수정 후 WndProc 함수:
```cpp
LRESULT CALLBACK CWin32Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // ── ImGui 메시지 먼저 처리 ────────────────────────────────
    // ImGui가 해당 메시지를 소비하면 true 반환 → 게임 로직에 전달 안 함
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    CWin32Window* pThis = nullptr;

    if (msg == WM_NCCREATE)
    {
        auto* pCS = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<CWin32Window*>(pCS->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CWin32Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    // ── 입력 메시지를 CInput에 전달 ──────────────────────────
    CInput& input = CInput::Get();

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        ValidateRect(hWnd, nullptr);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        input.OnKeyDown(static_cast<uint8>(wParam));
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_KEYUP:
        input.OnKeyUp(static_cast<uint8>(wParam));
        break;

    case WM_MOUSEMOVE:
        input.OnMouseMove(
            static_cast<int32>(LOWORD(lParam)),
            static_cast<int32>(HIWORD(lParam)));
        break;

    case WM_RBUTTONDOWN:
        input.OnRButtonDown();
        SetCapture(hWnd);
        break;

    case WM_RBUTTONUP:
        input.OnRButtonUp();
        ReleaseCapture();
        break;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
```

**핵심 변경**: L105-107에 `ImGui_ImplWin32_WndProcHandler` 호출 삽입.
이전 프로젝트의 `Window.cpp` L25-28과 동일한 패턴.
ImGui가 텍스트 입력/마우스 클릭 등을 처리하면 `true` 반환하여 게임 입력과 충돌 방지.

---

## 5. 수정: `Engine/Header/Framework/CEngineApp.h` — CImGuiLayer 멤버 추가

### 수정 전 (L1-6):
```cpp
#pragma once
#include "WintersTypes.h"
#include "EngineConfig.h"
#include "Platform/CWin32Window.h"
#include "RHI/CDX11Device.h"
#include "Core/CTimer.h"
```

### 수정 후 (L1-7):
```cpp
#pragma once
#include "WintersTypes.h"
#include "EngineConfig.h"
#include "Platform/CWin32Window.h"
#include "RHI/CDX11Device.h"
#include "Core/CTimer.h"
#include "Editor/CImGuiLayer.h"
```

### 수정 전 멤버 (L47-53):
```cpp
    IWintersApp*    m_pGameApp = nullptr;

    CWin32Window    m_Window;
    CDX11Device     m_Device;
    CTimer          m_Timer;

    bool            m_bRunning = false;
```

### 수정 후 멤버:
```cpp
    IWintersApp*    m_pGameApp = nullptr;

    CWin32Window    m_Window;
    CDX11Device     m_Device;
    CTimer          m_Timer;
    CImGuiLayer     m_ImGui;        // ImGui 레이어 (04. Editor)

    bool            m_bRunning = false;
```

---

## 6. 수정: `Engine/Code/Framework/CEngineApp.cpp` — ImGui 생명주기 통합

### 수정 전 Initialize (L34-49):
```cpp
    // ── Step 2. DX11 디바이스 초기화 ────────────────────────
    DeviceDesc devDesc;
    devDesc.hwnd       = m_Window.GetHandle();
    devDesc.width      = config.windowWidth;
    devDesc.height     = config.windowHeight;
    devDesc.vsync      = config.vsync;
    devDesc.fullscreen = config.fullscreen;

    if (!m_Device.Initialize(devDesc))
    {
        MessageBoxW(m_Window.GetHandle(),
            L"DX11 디바이스 초기화에 실패했습니다.\n"
            L"그래픽 카드가 D3D11을 지원하는지 확인하세요.",
            L"[CEngineApp] DX11 초기화 실패", MB_OK | MB_ICONERROR);
        return false;
    }
```

### 수정 후 Initialize (Step 2 뒤에 Step 2.5 추가):
```cpp
    // ── Step 2. DX11 디바이스 초기화 ────────────────────────
    DeviceDesc devDesc;
    devDesc.hwnd       = m_Window.GetHandle();
    devDesc.width      = config.windowWidth;
    devDesc.height     = config.windowHeight;
    devDesc.vsync      = config.vsync;
    devDesc.fullscreen = config.fullscreen;

    if (!m_Device.Initialize(devDesc))
    {
        MessageBoxW(m_Window.GetHandle(),
            L"DX11 디바이스 초기화에 실패했습니다.\n"
            L"그래픽 카드가 D3D11을 지원하는지 확인하세요.",
            L"[CEngineApp] DX11 초기화 실패", MB_OK | MB_ICONERROR);
        return false;
    }

    // ── Step 2.5. ImGui 초기화 ────────────────────────────────
    if (!m_ImGui.Initialize(m_Window.GetHandle(),
                            m_Device.GetDevice(),
                            m_Device.GetContext()))
    {
        OutputDebugStringA("[CEngineApp] ImGui initialization failed\n");
        return false;
    }
```

### 수정 전 Render (L96-105):
```cpp
void CEngineApp::Render()
{
    // ── BeginFrame: 백버퍼 + 뎁스 클리어 + 뷰포트 재설정 ─────
    m_Device.BeginFrame(0.08f, 0.08f, 0.12f, 1.f);

    if (m_pGameApp)
        m_pGameApp->OnRender();

    m_Device.EndFrame();
}
```

### 수정 후 Render:
```cpp
void CEngineApp::Render()
{
    // ── BeginFrame: 백버퍼 + 뎁스 클리어 + 뷰포트 재설정 ─────
    m_Device.BeginFrame(0.08f, 0.08f, 0.12f, 1.f);

    // ── ImGui 프레임 시작 ─────────────────────────────────────
    m_ImGui.BeginFrame();

    if (m_pGameApp)
        m_pGameApp->OnRender();

    // ── ImGui 프레임 끝 (드로우 데이터 제출) ──────────────────
    m_ImGui.EndFrame();

    m_Device.EndFrame();
}
```

### 수정 전 Shutdown (L112-124):
```cpp
void CEngineApp::Shutdown()
{
    if (m_pGameApp)
    {
        m_pGameApp->OnShutdown();
        m_pGameApp = nullptr;
    }

    m_Device.Destroy();
    m_Window.Destroy();

    s_pInstance = nullptr;
}
```

### 수정 후 Shutdown:
```cpp
void CEngineApp::Shutdown()
{
    if (m_pGameApp)
    {
        m_pGameApp->OnShutdown();
        m_pGameApp = nullptr;
    }

    // ImGui는 Device보다 먼저 해제 (DX11 리소스 참조 중)
    m_ImGui.Shutdown();

    m_Device.Destroy();
    m_Window.Destroy();

    s_pInstance = nullptr;
}
```

---

## 7. 수정: `Engine/Include/Engine.vcxproj` — 빌드 등록

### ImGui 소스 파일 추가 (L131 `</ItemGroup>` 직전에 삽입):

```xml
  <!-- 소스 파일 -->
  <ItemGroup>
    <!-- ... 기존 ClCompile 항목들 ... -->

    <!-- ImGui 코어 (PCH 비활성화 — 외부 라이브러리) -->
    <ClCompile Include="..\External\imgui\imgui.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_draw.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_tables.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_widgets.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_demo.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
    <!-- ImGui 백엔드 (DX11 + Win32) -->
    <ClCompile Include="..\External\imgui\backends\imgui_impl_dx11.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\External\imgui\backends\imgui_impl_win32.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
    <!-- CImGuiLayer (엔진 내부 래퍼) -->
    <ClCompile Include="..\Code\Editor\CImGuiLayer.cpp" />
  </ItemGroup>
```

### ImGui 헤더 파일 추가 (`</ItemGroup>` — 내부 헤더 그룹에 삽입):

```xml
    <!-- ImGui 헤더 -->
    <ClInclude Include="..\External\imgui\imgui.h" />
    <ClInclude Include="..\External\imgui\imgui_internal.h" />
    <ClInclude Include="..\External\imgui\backends\imgui_impl_dx11.h" />
    <ClInclude Include="..\External\imgui\backends\imgui_impl_win32.h" />
    <!-- CImGuiLayer 헤더 -->
    <ClInclude Include="..\Header\Editor\CImGuiLayer.h" />
```

### Include 경로 추가 (Debug/Release 둘 다):

**수정 전**:
```xml
<AdditionalIncludeDirectories>$(ProjectDir)..\Header;$(ProjectDir);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
```

**수정 후**:
```xml
<AdditionalIncludeDirectories>$(ProjectDir)..\External\imgui;$(ProjectDir)..\External\imgui\backends;$(ProjectDir)..\Header;$(ProjectDir);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
```

### PCH 충돌 방지 — ForcedIncludeFiles 예외 처리:

ImGui `.cpp` 파일들은 `<PrecompiledHeader>NotUsing</PrecompiledHeader>`로 설정하지만,
vcxproj의 `<ForcedIncludeFiles>WintersPCH.h</ForcedIncludeFiles>`가 전역 적용되므로
ImGui 파일에 대해 `ForcedIncludeFiles`도 비활성화해야 한다:

```xml
    <ClCompile Include="..\External\imgui\imgui.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
      <ForcedIncludeFiles></ForcedIncludeFiles>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_draw.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
      <ForcedIncludeFiles></ForcedIncludeFiles>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_tables.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
      <ForcedIncludeFiles></ForcedIncludeFiles>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_widgets.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
      <ForcedIncludeFiles></ForcedIncludeFiles>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_demo.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
      <ForcedIncludeFiles></ForcedIncludeFiles>
    </ClCompile>
    <ClCompile Include="..\External\imgui\backends\imgui_impl_dx11.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
      <ForcedIncludeFiles></ForcedIncludeFiles>
    </ClCompile>
    <ClCompile Include="..\External\imgui\backends\imgui_impl_win32.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
      <ForcedIncludeFiles></ForcedIncludeFiles>
    </ClCompile>
```

---

## 8. 수정: `Engine/Include/Engine.vcxproj.filters` — 필터 배치

### 필터 정의 추가 (기존 `04. Editor` 필터 하위):
```xml
    <Filter Include="04. Editor\00. ImGui">
      <UniqueIdentifier>{F1450001-0000-0000-0000-000000000001}</UniqueIdentifier>
    </Filter>
    <Filter Include="04. Editor\01. ImGuiBackend">
      <UniqueIdentifier>{F1450002-0000-0000-0000-000000000002}</UniqueIdentifier>
    </Filter>
    <Filter Include="04. Editor\02. Layer">
      <UniqueIdentifier>{F1450003-0000-0000-0000-000000000003}</UniqueIdentifier>
    </Filter>
```

### 소스 파일 필터 매핑:
```xml
    <!-- ImGui 코어 -->
    <ClCompile Include="..\External\imgui\imgui.cpp">
      <Filter>04. Editor\00. ImGui</Filter>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_draw.cpp">
      <Filter>04. Editor\00. ImGui</Filter>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_tables.cpp">
      <Filter>04. Editor\00. ImGui</Filter>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_widgets.cpp">
      <Filter>04. Editor\00. ImGui</Filter>
    </ClCompile>
    <ClCompile Include="..\External\imgui\imgui_demo.cpp">
      <Filter>04. Editor\00. ImGui</Filter>
    </ClCompile>
    <!-- ImGui 백엔드 -->
    <ClCompile Include="..\External\imgui\backends\imgui_impl_dx11.cpp">
      <Filter>04. Editor\01. ImGuiBackend</Filter>
    </ClCompile>
    <ClCompile Include="..\External\imgui\backends\imgui_impl_win32.cpp">
      <Filter>04. Editor\01. ImGuiBackend</Filter>
    </ClCompile>
    <!-- CImGuiLayer -->
    <ClCompile Include="..\Code\Editor\CImGuiLayer.cpp">
      <Filter>04. Editor\02. Layer</Filter>
    </ClCompile>
```

### 헤더 파일 필터 매핑:
```xml
    <!-- ImGui 헤더 -->
    <ClInclude Include="..\External\imgui\imgui.h">
      <Filter>04. Editor\00. ImGui</Filter>
    </ClInclude>
    <ClInclude Include="..\External\imgui\imgui_internal.h">
      <Filter>04. Editor\00. ImGui</Filter>
    </ClInclude>
    <ClInclude Include="..\External\imgui\backends\imgui_impl_dx11.h">
      <Filter>04. Editor\01. ImGuiBackend</Filter>
    </ClInclude>
    <ClInclude Include="..\External\imgui\backends\imgui_impl_win32.h">
      <Filter>04. Editor\01. ImGuiBackend</Filter>
    </ClInclude>
    <!-- CImGuiLayer 헤더 -->
    <ClInclude Include="..\Header\Editor\CImGuiLayer.h">
      <Filter>04. Editor\02. Layer</Filter>
    </ClInclude>
```

---

## 9. ImGui 소스 다운로드 방법

```bash
# Engine/External/imgui 디렉토리에 ImGui docking 브랜치 클론
cd Engine
mkdir External
cd External
git clone --branch docking --depth 1 https://github.com/ocornut/imgui.git
```

필요한 파일만 유지하고 나머지 삭제 가능:
```
imgui/
  imgui.h, imgui.cpp
  imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp
  imgui_internal.h, imgui_demo.cpp
  imconfig.h, imstb_rectpack.h, imstb_textedit.h, imstb_truetype.h
  backends/
    imgui_impl_dx11.h, imgui_impl_dx11.cpp
    imgui_impl_win32.h, imgui_impl_win32.cpp
```

---

## 10. 검증 방법

### 빌드 검증
1. VS에서 Engine 프로젝트 빌드 → 에러 없이 WintersEngine.dll 생성 확인
2. Client 프로젝트 빌드 → 링크 에러 없음 확인

### 런타임 검증
`CGameApp::OnRender()`에 ImGui 데모 윈도우 추가:

```cpp
// Client/Code/CGameApp.cpp — OnRender() 내부
#include "../../Engine/External/imgui/imgui.h"  // 임시 테스트용

void CGameApp::OnRender()
{
    // 기존 큐브 렌더링 코드...

    // ImGui 데모 윈도우 (테스트 후 제거)
    ImGui::ShowDemoWindow();
}
```

> **주의**: Client에서 imgui.h를 직접 include하려면 vcxproj의 AdditionalIncludeDirectories에
> `$(SolutionDir)Engine\External\imgui` 추가 필요. 또는 Engine/Include에 공개 API 헤더를
> 만들어서 래핑하는 방법도 있음 (향후 결정).

### 확인 체크리스트
- [ ] ImGui 데모 윈도우가 화면에 표시됨
- [ ] 마우스로 ImGui 윈도우 드래그/리사이즈 가능
- [ ] 키보드 입력이 ImGui 텍스트 필드에 전달됨
- [ ] ImGui 영역 밖 클릭 시 게임 입력 정상 동작
- [ ] ESC 키로 정상 종료
- [ ] 메모리 누수 없음 (종료 시 ImGui 정리 완료)

---

## 요약: 수정 파일 목록

| # | 파일 | 변경 유형 | 설명 |
|---|------|-----------|------|
| 1 | `Engine/External/imgui/*` | 새 파일 (외부) | ImGui 소스 다운로드 |
| 2 | `Engine/Header/Editor/CImGuiLayer.h` | 새 파일 | ImGui 래퍼 헤더 |
| 3 | `Engine/Code/Editor/CImGuiLayer.cpp` | 새 파일 | ImGui 래퍼 구현 |
| 4 | `Engine/Code/Platform/CWin32Window.cpp` | 수정 | WndProc에 ImGui 후킹 추가 (L102 앞에 3줄) |
| 5 | `Engine/Header/Framework/CEngineApp.h` | 수정 | `#include` + `m_ImGui` 멤버 추가 |
| 6 | `Engine/Code/Framework/CEngineApp.cpp` | 수정 | Initialize/Render/Shutdown에 ImGui 호출 삽입 |
| 7 | `Engine/Include/Engine.vcxproj` | 수정 | ImGui 소스/헤더 빌드 등록 + Include 경로 |
| 8 | `Engine/Include/Engine.vcxproj.filters` | 수정 | 04. Editor 하위 필터 배치 |
