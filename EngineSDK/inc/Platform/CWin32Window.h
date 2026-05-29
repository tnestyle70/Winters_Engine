#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "WintersTypes.h"
#include "Platform/IPlatformSurface.h"
#include "Platform/IPlatformWindow.h"

// ─────────────────────────────────────────────────────────────────
//  CWin32Window  |  Win32 윈도우 생성 / 메시지 루프
//
//  내부(Engine/Header)에만 존재. Client에 노출되지 않음.
//  Client 코드는 HWND를 직접 다루지 않는다.
//
//  플랫폼 추상화 방향:
//    현재: Win32 직접 구현
//    향후: IWindow 인터페이스 → Win32Window / LinuxWindow / ConsoleWindow
//
//  WndProc 설계:
//    CreateWindowExW의 lpParam에 this 포인터를 전달,
//    WM_NCCREATE에서 SetWindowLongPtrW로 연결.
//    정적 함수에서 인스턴스 메서드처럼 처리 가능.
// ─────────────────────────────────────────────────────────────────
//다양한 플랫폼을 다룰 때 사용하기 위한 Class!
struct WindowDesc
{
    WStr    title      = L"Winters Engine";
    int32   width      = 1280;
    int32   height     = 720;
    bool    fullscreen = false;
};

class CWin32Window final : public IPlatformWindow, public IPlatformSurface
{
public:
    CWin32Window()  = default;
    ~CWin32Window() { Destroy(); }

    bool    Create(const WindowDesc& desc);
    void    Destroy();

    // 메시지 루프 한 사이클 처리
    // false 반환 = WM_QUIT 수신 → 게임 루프 종료
    bool    PumpMessages();

    HWND    GetHandle() const { return m_hWnd;   }
    int32   GetWidth()  const { return m_Width;  }
    int32   GetHeight() const { return m_Height; }

    eWintersPlatform Get_Platform() const override { return eWintersPlatform::Windows; }
    ePlatformLifecycleState Get_LifecycleState() const override
    {
        return m_hWnd ? ePlatformLifecycleState::Active : ePlatformLifecycleState::Destroyed;
    }
    PlatformNativeHandle Get_NativeHandle() const override
    {
        PlatformNativeHandle handle{};
        handle.platform = eWintersPlatform::Windows;
        handle.window = m_hWnd;
        handle.module = GetModuleHandleW(nullptr);
        return handle;
    }
    IPlatformSurface* Get_Surface() override { return this; }
    const IPlatformSurface* Get_Surface() const override { return this; }
    u32_t Get_Width() const override { return static_cast<u32_t>(m_Width); }
    u32_t Get_Height() const override { return static_cast<u32_t>(m_Height); }
    RHISurfaceDesc Get_RHISurfaceDesc() const override
    {
        RHISurfaceDesc desc{};
        desc.type = eRHIPlatformSurfaceType::Win32HWND;
        desc.nativeHandle = m_hWnd;
        desc.width = static_cast<u32_t>(m_Width);
        desc.height = static_cast<u32_t>(m_Height);
        desc.vsync = true;
        desc.fullscreen = false;
        desc.lifecycleState = m_hWnd
            ? eRHISurfaceLifecycleState::Active
            : eRHISurfaceLifecycleState::Destroyed;
        return desc;
    }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND    m_hWnd   = nullptr;
    int32   m_Width  = 0;
    int32   m_Height = 0;
};
