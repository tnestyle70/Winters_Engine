#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

// HWND 멤버 때문에 Windows.h 필요 (헤더 자기 완결성)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cstring> //memcpy

class CCamera;

class WINTERS_ENGINE CInput
{
public:
    static CInput& Get()
    {
        static CInput instance;
        return instance;
    }

    // ── 키보드 ────────────────────────────────────────────────
    bool IsKeyDown(uint8 vKey) const { return m_Keys[vKey]; }
    bool IsKeyUp(uint8 vKey) const { return !m_Keys[vKey]; }

    // ── 마우스 ────────────────────────────────────────────────
    f32_t GetMouseDeltaX() const { return m_MouseDeltaX; }
    f32_t GetMouseDeltaY() const { return m_MouseDeltaY; }
    int32 GetMouseX() const { return m_MouseX; }
    int32 GetMouseY() const { return m_MouseY; }
    bool  IsRButtonDown() const { return m_bRButton; }
    bool IsLButtonDown() const { return m_bLButton; }

    // ── 에지 트리거 ───────────────────────────────────────────
    bool IsKeyPressed(uint8 vKey) const;
    bool IsKeyReleased(uint8 vKey) const;
    bool IsLButtonPressed() const;
    bool IsLButtonReleased() const;
    bool IsRButtonPressed() const;
    bool IsRButtonReleased() const;

    // ── WndProc에서 호출 (엔진 내부) ──────────────────────────
    void OnKeyDown(uint8 vKey) { m_Keys[vKey] = true; }
    void OnKeyUp(uint8 vKey) { m_Keys[vKey] = false; }
    void OnMouseMove(int32 x, int32 y);
    void OnRawMouseDelta(int32 dx, int32 dy);
    void OnLButtonDown() { m_bLButton = true; }
    void OnLButtonUp() { m_bLButton = false; }
    void OnRButtonDown() { m_bRButton = true; }
    void OnRButtonUp() { m_bRButton = false; }

    //마우스 피킹
    struct MouseRay { Vec3 Origin; Vec3 Dir; };
    MouseRay GetMouseWorldRay(const CCamera& cam, int32_t screenW, int32_t screenH) const;
    Vec3 GetMouseGroundPos(const CCamera& cam, int32_t screenW, int32_t screenH) const;

    // ── 프레임 끝에 호출 — 델타 초기화 ───────────────────────
    void EndFrame() { 
        m_MouseDeltaX = 0.f; m_MouseDeltaY = 0.f; 
        // 에지 트리거용 이전 프레임 상태 스냅
        std::memcpy(m_pPrevKeys, m_Keys, sizeof(m_Keys));
        m_pPrevRButton = m_bRButton;
        m_pPrevLButton = m_bLButton;
    }

    //Engine Window Handle
    void SetWindowHandle(HWND hWnd) { m_hWnd = hWnd; }
    HWND GetWindowHandle() const { return m_hWnd; }

private:
    CInput() = default;

    HWND m_hWnd = nullptr;

    bool  m_Keys[256] = {};

    bool m_pPrevKeys[256] = {};
    bool m_pPrevRButton = false;
    bool m_pPrevLButton = false;

    int32 m_MouseX = 0;
    int32 m_MouseY = 0;
    f32_t m_MouseDeltaX = 0.f;
    f32_t m_MouseDeltaY = 0.f;
    bool  m_bRButton = false;
    bool m_bLButton = false;
};
