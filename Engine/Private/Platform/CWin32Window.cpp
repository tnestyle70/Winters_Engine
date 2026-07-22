#include "Platform/CWin32Window.h"
#include "Core/CInput.h"
#include "WintersPaths.h"

#include <windowsx.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdio>

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

namespace
{
    // CUI_Manager::m_fCursorSize(32px)와 일치해야 소프트웨어 커서와 크기가 같다.
    constexpr int32 kCursorImageSize = 32;

    class CScopedCOMInit final
    {
    public:
        CScopedCOMInit()
        {
            const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            m_bNeedsUninit = SUCCEEDED(hr);
        }

        ~CScopedCOMInit()
        {
            if (m_bNeedsUninit)
                CoUninitialize();
        }

    private:
        bool m_bNeedsUninit = false;
    };

    void LogCursorImageFailure(const char* pStage, const wchar_t* pPath, HRESULT hr)
    {
        char msg[512]{};
        sprintf_s(msg, "[CWin32Window] cursor image FAIL: %s hr=0x%08X path=%ls\n",
            pStage,
            static_cast<unsigned>(hr),
            pPath ? pPath : L"(null)");
        OutputDebugStringA(msg);
    }

    // PNG → 32x32 straight-alpha BGRA → HCURSOR.
    // 핫스팟 (0,0): 소프트웨어 커서(CUI_Manager::DrawMouseCursor)가 텍스처의
    // 좌상단을 마우스 좌표에 앵커하므로 하드웨어 커서도 동일해야 한다.
    HCURSOR CreateCursorImageFromPng(const wchar_t* pPath)
    {
        CScopedCOMInit comInit;

        Microsoft::WRL::ComPtr<IWICImagingFactory2> pFactory;
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory2,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&pFactory));
        if (FAILED(hr))
        {
            LogCursorImageFailure("CoCreateInstance(WICImagingFactory2)", pPath, hr);
            return nullptr;
        }

        Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;
        hr = pFactory->CreateDecoderFromFilename(
            pPath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder);
        if (FAILED(hr))
        {
            LogCursorImageFailure("CreateDecoderFromFilename", pPath, hr);
            return nullptr;
        }

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pFrame;
        hr = pDecoder->GetFrame(0, &pFrame);
        if (FAILED(hr))
        {
            LogCursorImageFailure("GetFrame", pPath, hr);
            return nullptr;
        }

        Microsoft::WRL::ComPtr<IWICBitmapScaler> pScaler;
        hr = pFactory->CreateBitmapScaler(&pScaler);
        if (FAILED(hr))
        {
            LogCursorImageFailure("CreateBitmapScaler", pPath, hr);
            return nullptr;
        }

        hr = pScaler->Initialize(
            pFrame.Get(), kCursorImageSize, kCursorImageSize,
            WICBitmapInterpolationModeFant);
        if (FAILED(hr))
        {
            LogCursorImageFailure("Scaler.Initialize", pPath, hr);
            return nullptr;
        }

        Microsoft::WRL::ComPtr<IWICFormatConverter> pConverter;
        hr = pFactory->CreateFormatConverter(&pConverter);
        if (FAILED(hr))
        {
            LogCursorImageFailure("CreateFormatConverter", pPath, hr);
            return nullptr;
        }

        // CreateIconIndirect는 straight-alpha BGRA DIB를 받는다 (SDL/GLFW와 동일).
        hr = pConverter->Initialize(
            pScaler.Get(),
            GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr))
        {
            LogCursorImageFailure("FormatConverter.Initialize", pPath, hr);
            return nullptr;
        }

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = kCursorImageSize;
        bmi.bmiHeader.biHeight = -kCursorImageSize;  // top-down: WIC 행 순서와 일치
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        HBITMAP hColor = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        if (!hColor || !pBits)
        {
            LogCursorImageFailure("CreateDIBSection", pPath, HRESULT_FROM_WIN32(GetLastError()));
            if (hColor)
                DeleteObject(hColor);
            return nullptr;
        }

        const UINT rowPitchBytes = kCursorImageSize * 4u;
        hr = pConverter->CopyPixels(
            nullptr, rowPitchBytes, rowPitchBytes * kCursorImageSize,
            static_cast<BYTE*>(pBits));
        if (FAILED(hr))
        {
            LogCursorImageFailure("CopyPixels", pPath, hr);
            DeleteObject(hColor);
            return nullptr;
        }

        HBITMAP hMask = CreateBitmap(kCursorImageSize, kCursorImageSize, 1, 1, nullptr);
        if (!hMask)
        {
            LogCursorImageFailure("CreateBitmap(mask)", pPath, HRESULT_FROM_WIN32(GetLastError()));
            DeleteObject(hColor);
            return nullptr;
        }

        ICONINFO iconInfo = {};
        iconInfo.fIcon = FALSE;  // FALSE = 커서 (핫스팟 사용)
        iconInfo.xHotspot = 0;
        iconInfo.yHotspot = 0;
        iconInfo.hbmMask = hMask;
        iconInfo.hbmColor = hColor;
        HCURSOR hCursor = CreateIconIndirect(&iconInfo);
        if (!hCursor)
            LogCursorImageFailure("CreateIconIndirect", pPath, HRESULT_FROM_WIN32(GetLastError()));

        DeleteObject(hMask);
        DeleteObject(hColor);
        return hCursor;
    }
}

bool CWin32Window::Create(const WindowDesc& desc)
{
    m_bQuitRequested = false;
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
    SetSystemCursorVisible(false);

    return true;
}

void CWin32Window::Destroy()
{
    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
    SetSystemCursorVisible(true);
    UnregisterClassW(CLASS_NAME, GetModuleHandleW(nullptr));
    if (m_hCursorImage)
    {
        DestroyCursor(m_hCursorImage);
        m_hCursorImage = nullptr;
    }
}

void CWin32Window::SetSystemCursorVisible(bool bVisible)
{
    if (m_bSystemCursorVisible == bVisible)
        return;

    if (bVisible)
    {
        while (::ShowCursor(TRUE) < 0) {}
        // 다음 WM_SETCURSOR(마우스 이동)를 기다리지 않고 즉시 커스텀 이미지 표시.
        if (m_hCursorImage)
            ::SetCursor(m_hCursorImage);
    }
    else
    {
        while (::ShowCursor(FALSE) >= 0) {}
    }
    m_bSystemCursorVisible = bVisible;
}

bool CWin32Window::SetCursorImageFromFile(const wchar_t* pPath)
{
    if (!pPath || pPath[0] == 0)
        return false;

    wchar_t resolvedPath[MAX_PATH] = {};
    const wchar_t* pLoadPath = pPath;
    if (WintersResolveContentPath(pPath, resolvedPath, MAX_PATH))
        pLoadPath = resolvedPath;

    HCURSOR hCursor = CreateCursorImageFromPng(pLoadPath);
    if (!hCursor)
        return false;

    if (m_hCursorImage)
        DestroyCursor(m_hCursorImage);
    m_hCursorImage = hCursor;

    // DefWindowProc가 모든 WM_SETCURSOR에서 클래스 커서를 적용하므로
    // (ImGui는 NoMouseCursorChange로 개입하지 않음) 이 한 번의 등록으로 유지된다.
    if (m_hWnd)
        SetClassLongPtrW(m_hWnd, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(m_hCursorImage));
    if (m_bSystemCursorVisible)
        ::SetCursor(m_hCursorImage);

    return true;
}

bool CWin32Window::PumpMessages(uint32 maxMessages)
{
    if (m_bQuitRequested)
        return false;
    if (m_bPumpingMessages)
        return true;

    struct PumpScope final
    {
        explicit PumpScope(bool& pumping) : flag(pumping) { flag = true; }
        ~PumpScope() { flag = false; }
        bool& flag;
    } pumpScope(m_bPumpingMessages);

    MSG msg = {};
    uint32 processedMessages = 0u;
    while ((maxMessages == 0u || processedMessages < maxMessages) &&
        PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        ++processedMessages;
        if (msg.message == WM_QUIT)
        {
            m_bQuitRequested = true;
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (m_bQuitRequested)
            return false;
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
        if (pThis)
            pThis->m_bQuitRequested = true;
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
