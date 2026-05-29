#include "Platform/CWin32Window.h"
#include "Core/CInput.h"

#include <windowsx.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ─────────────────────────────────────────────────────────────────
//  CWin32Window 구현
//
//  WndProc 설계 포인트:
//    CreateWindowExW(... lpParam = this)로 this 포인터를 전달.
//    WM_NCCREATE에서 SetWindowLongPtrW(GWLP_USERDATA)에 저장.
//    이후 모든 메시지에서 GetWindowLongPtrW로 복원.
//    → 정적(static) 함수에서 인스턴스 멤버에 접근 가능.
//
//  플랫폼 추상화 방향:
//    IWindow::Create() / Destroy() / PumpMessages() 인터페이스 도입
//    Win32Window : IWindow
//    SDLWindow   : IWindow  (크로스플랫폼 필요 시)
// ─────────────────────────────────────────────────────────────────

// 클래스 이름을 바꾸면 예전에 등록된(흰색 hbrBackground 등) 윈도우 클래스와 충돌하지 않음.
static constexpr WStr CLASS_NAME = L"WintersWindowClass_D3D_v3";
static constexpr DWORD WINDOW_STYLE = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
static bool s_bCursorHIDden = false;

bool CWin32Window::Create(const WindowDesc& desc)
{
    m_Width  = desc.width;
    m_Height = desc.height;

    // ── 윈도우 클래스 등록 ───────────────────────────────────
    WNDCLASSEXW wc    = {};
    wc.cbSize         = sizeof(WNDCLASSEXW);
    wc.style          = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc    = WndProc;
    wc.hInstance      = GetModuleHandleW(nullptr);
    wc.hCursor        = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground  = nullptr;  // DX11이 화면을 관리 — Windows 배경 브러시 비활성화
    wc.lpszClassName  = CLASS_NAME;

    if (!RegisterClassExW(&wc))
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }

    // ── 클라이언트 영역 기준 윈도우 크기 계산 ───────────────
    RECT rect = { 0, 0, (LONG)desc.width, (LONG)desc.height };
    AdjustWindowRect(&rect, WINDOW_STYLE, FALSE);
    int32 winW = rect.right  - rect.left;
    int32 winH = rect.bottom - rect.top;

    // 화면 중앙 배치
    int32 screenW = GetSystemMetrics(SM_CXSCREEN);
    int32 screenH = GetSystemMetrics(SM_CYSCREEN);
    int32 posX    = (screenW - winW) / 2;
    int32 posY    = (screenH - winH) / 2;

    // 윈도우 생성
    m_hWnd = CreateWindowExW(0,
                             CLASS_NAME,
                             desc.title,
                             WINDOW_STYLE,
                             posX,
                             posY,
                             winW,
                             winH,
                             nullptr,
                             nullptr,
                             GetModuleHandleW(nullptr),
                             this);

    if (!m_hWnd)
        return false;

    //CInput이 커서 잠금 / 중앙 리셋할 때 참조할 HWND 캐싱
    CInput::Get().SetWindowHandle(m_hWnd);

    RAWINPUTDEVICE rid = {};

    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = 0;
    rid.hwndTarget = m_hWnd;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
        OutputDebugStringA("[CWin32Window] RegisterRawInputDevices failed\n");

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
    if (!s_bCursorHIDden)
    {
        ::ShowCursor(FALSE);
        s_bCursorHIDden = true;
    }

    return true;
}

void CWin32Window::Destroy()
{
    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
    if (s_bCursorHIDden)
    {
        ::ShowCursor(TRUE);
        s_bCursorHIDden = false;
    }
    UnregisterClassW(CLASS_NAME, GetModuleHandleW(nullptr));
}

bool CWin32Window::PumpMessages()
{
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return false;

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

LRESULT CALLBACK CWin32Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Close/destroy messages must bypass ImGui so the main loop can exit.
    CWin32Window* pThis = nullptr;

    if (msg == WM_NCCREATE)
    {
        // CreateWindowExW의 lpCreateParams에서 this 복원
        auto* pCS = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<CWin32Window*>(pCS->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CWin32Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    // ── 윈도우 생명주기 메시지 처리 ──────────────────────────
    switch (msg)
    {
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
        if (pThis && pThis->m_hWnd == hWnd)
            pThis->m_hWnd = nullptr;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    // ── 입력 메시지를 CInput에 전달 ──────────────────────────
    CInput& input = CInput::Get();

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        ValidateRect(hWnd, nullptr);
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
            static_cast<int32>(GET_X_LPARAM(lParam)),
            static_cast<int32>(GET_Y_LPARAM(lParam)));
        break;

    case WM_INPUT:
    {
        UINT dwSize = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),
            RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
        if (dwSize == 0 || dwSize > sizeof(RAWINPUT) * 2) break;

        BYTE buffer[sizeof(RAWINPUT) * 2];
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),
            RID_INPUT, buffer, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
            break;

        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);
        if (raw->header.dwType == RIM_TYPEMOUSE &&
            (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
        {
            input.OnRawMouseDelta(
                static_cast<int32>(raw->data.mouse.lLastX),
                static_cast<int32>(raw->data.mouse.lLastY));
        }
        break;
    }

    case WM_LBUTTONDOWN:
        input.OnMouseMove(
            static_cast<int32>(GET_X_LPARAM(lParam)),
            static_cast<int32>(GET_Y_LPARAM(lParam)));
        input.OnLButtonDown();
        SetCapture(hWnd);
        break;

    case WM_LBUTTONUP:
        input.OnMouseMove(
            static_cast<int32>(GET_X_LPARAM(lParam)),
            static_cast<int32>(GET_Y_LPARAM(lParam)));
        input.OnLButtonUp();
        ReleaseCapture();
        break;

    case WM_RBUTTONDOWN:
        input.OnMouseMove(
            static_cast<int32>(GET_X_LPARAM(lParam)),
            static_cast<int32>(GET_Y_LPARAM(lParam)));
        input.OnRButtonDown();
        SetCapture(hWnd);
        break;

    case WM_RBUTTONUP:
        input.OnMouseMove(
            static_cast<int32>(GET_X_LPARAM(lParam)),
            static_cast<int32>(GET_Y_LPARAM(lParam)));
        input.OnRButtonUp();
        ReleaseCapture();
        break;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
