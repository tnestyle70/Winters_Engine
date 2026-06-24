#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "DynamicCamera.h"
#include "Core/CInput.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace
{
    constexpr f32_t kEdgeScrollBandPixels = 18.f;
    constexpr f32_t kEdgeScrollSpeed = 38.f;
    constexpr f32_t kEdgeScrollMaxDeltaSec = 0.05f;
}

unique_ptr<CDynamicCamera> CDynamicCamera::Create(
    const Vec3& vEye, const Vec3& vAt, const Vec3& vUp,
    f32_t fFov, f32_t fAspect, f32_t fNear, f32_t fFar)
{
    auto pInstance = unique_ptr<CDynamicCamera>(new CDynamicCamera());
    pInstance->Ready(vEye, vAt, vUp, fFov, fAspect, fNear, fFar);
    pInstance->m_fSpeed = 50.f;
    return pInstance;
}

void CDynamicCamera::Update(f32_t fTimeDelta, const CInput& input)
{
    if (input.IsKeyDown(VK_F2))
    {
        if (!m_bF2Check)
        {
            m_bF2Check = true;
            if (!m_bFreeCameraMode)
            {
                SetFollowMode(false);
                m_bFix = true;
                Enter_FPSMode(input);
            }
            else
            {
                SetFollowMode(true);
            }
        }
    }
    else { m_bF2Check = false; }

    const bool_t bSpaceFollow = input.IsKeyDown(VK_SPACE) &&
        m_pTargetTransform != nullptr &&
        !m_bFreeCameraMode;
    if (bSpaceFollow)
    {
        if (!m_bSpaceFollowActive)
        {
            m_bFollowInitialized = false;
            m_bSpaceFollowActive = true;
        }
        m_bFollowMode = true;
    }
    else if (m_bSpaceFollowActive)
    {
        m_bFollowMode = false;
        m_bFollowInitialized = false;
        m_bSpaceFollowActive = false;
    }

    if (m_pTargetTransform && m_bFollowMode)
        Update_FollowCam(fTimeDelta);
    else
        Update_FreeCam(fTimeDelta, input);

    if (bSpaceFollow)
    {
        m_bEdgeScrollActive = false;
    }
    else
    {
        ApplyEdgeScroll(fTimeDelta, input);
    }

    if (m_fShakeTimer < m_fShakeDuration)
    {
        m_fShakeTimer += fTimeDelta;
        f32_t fStrength = m_fShakeIntensity
                        * (1.f - m_fShakeTimer / m_fShakeDuration);
        m_vEye.x += ((rand() % 100) / 100.f - 0.5f) * fStrength;
        m_vEye.y += ((rand() % 100) / 100.f - 0.5f) * fStrength;
        m_vEye.z += ((rand() % 100) / 100.f - 0.5f) * fStrength;
    }

    RecalcView();
}

void CDynamicCamera::Update_FollowCam(f32_t fTimeDelta)
{
    const Vec3 vTargetPos = m_pTargetTransform->GetPosition();
    const Vec3 vTargetEye = vTargetPos + m_vFollowOffset;
    const Vec3 vTargetAt = vTargetPos + Vec3(0.f, 1.5f, 0.f);

    if (!m_bFollowInitialized || fTimeDelta <= 0.f)
    {
        m_vEye = vTargetEye;
        m_vAt = vTargetAt;
        m_bFollowInitialized = true;
        return;
    }

    const f32_t fClampedDt = fTimeDelta > 0.05f ? 0.05f : fTimeDelta;
    const f32_t fAlpha = 1.f - std::exp(-m_fFollowResponse * fClampedDt);
    auto lerp = [fAlpha](const Vec3& a, const Vec3& b)
    {
        return Vec3{
            a.x + (b.x - a.x) * fAlpha,
            a.y + (b.y - a.y) * fAlpha,
            a.z + (b.z - a.z) * fAlpha
        };
    };

    m_vEye = lerp(m_vEye, vTargetEye);
    m_vAt = lerp(m_vAt, vTargetAt);
}

void CDynamicCamera::Update_FreeCam(f32_t fTimeDelta, const CInput& input)
{
    if (m_bFreeCameraMode)
        Key_Input(fTimeDelta, input);

    if (m_bFreeCameraMode && input.IsKeyDown(VK_TAB))
    {
        if (!m_bTabCheck)
        {
            m_bTabCheck = true;
            m_bFix = !m_bFix;
            if (m_bFix)
                Enter_FPSMode(input);
            else
                Exit_FPSMode();
        }
    }
    else
    {
        m_bTabCheck = false;
    }

    if (m_bFreeCameraMode && m_bFix)
    {
        // Raw Input (WM_INPUT) 기반 — 커서 워프/Mouse_Fix 불필요
        Mouse_Move(input);
    }
}

void CDynamicCamera::SetFollowMode(bool bFollow)
{
    if (m_bFollowMode != bFollow)
        m_bFollowInitialized = false;

    m_bFollowMode = bFollow;
    m_bFreeCameraMode = !bFollow;

    if (bFollow)
    {
        m_bFix = false;
        Exit_FPSMode();
    }
}

void CDynamicCamera::Enter_FPSMode(const CInput& input)
{
    HWND hWnd = input.GetWindowHandle();
    if (!hWnd) return;

    // ── 1. 커서를 창 중앙으로 한 번 리셋 ──
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    POINT ptCenter = { (rcClient.right - rcClient.left) / 2,
                       (rcClient.bottom - rcClient.top) / 2 };
    ClientToScreen(hWnd, &ptCenter);
    SetCursorPos(ptCenter.x, ptCenter.y);

    // ── 2. 커서를 창 영역 안으로 잠금 (OS 강제) ──
    POINT ptTL = { 0, 0 };
    POINT ptBR = { rcClient.right, rcClient.bottom };
    ClientToScreen(hWnd, &ptTL);
    ClientToScreen(hWnd, &ptBR);
    RECT rcClip = { ptTL.x, ptTL.y, ptBR.x, ptBR.y };
    ClipCursor(&rcClip);

    // ── 3. 커서 숨김 (참조 카운트 균형 유지) ──
    if (!m_bCursorHidden)
    {
        ShowCursor(FALSE);
        m_bCursorHidden = true;
    }

    // ── 4. 스무딩 누적값 리셋 — 모드 전환 순간 튀는 것 방지 ──
    m_fSmoothDX = 0.f;
    m_fSmoothDY = 0.f;
}

void CDynamicCamera::Exit_FPSMode()
{
    //커서 잠금 해제
    ClipCursor(nullptr);

    //커서 복원
    if (m_bCursorHidden)
    {
        ShowCursor(TRUE);
        m_bCursorHidden = false;
    }
}

void CDynamicCamera::SnapToTarget()
{
    if (!m_pTargetTransform) return;
    Vec3 vTargetPos = m_pTargetTransform->GetPosition();
    m_vEye = vTargetPos + m_vFollowOffset;
    m_vAt  = vTargetPos + Vec3(0.f, 1.5f, 0.f);
    m_bFollowInitialized = true;
    RecalcView();
}

void CDynamicCamera::JumpToWorldXZ(const Vec3& vWorldPos)
{
    const Vec3 vEyeOffset{
        m_vEye.x - m_vAt.x,
        m_vEye.y - m_vAt.y,
        m_vEye.z - m_vAt.z
    };

    Exit_FPSMode();
    m_bFollowMode = false;
    m_bFreeCameraMode = false;
    m_bFix = false;
    m_bFollowInitialized = false;

    m_vAt.x = vWorldPos.x;
    m_vAt.z = vWorldPos.z;
    m_vEye = {
        m_vAt.x + vEyeOffset.x,
        m_vAt.y + vEyeOffset.y,
        m_vAt.z + vEyeOffset.z
    };

    RecalcView();
}

void CDynamicCamera::StartShake(f32_t fDuration, f32_t fIntensity)
{
    m_fShakeDuration  = fDuration;
    m_fShakeTimer     = 0.f;
    m_fShakeIntensity = fIntensity;
}

void CDynamicCamera::OnImGui()
{
    ImGui::SetNextWindowPos(ImVec2(300.f, 50.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320.f, 240.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(false, ImGuiCond_FirstUseEver);


    if (ImGui::Begin("Camera Debug"))
    {
        ImGui::Text("-- Follow Offset --");
        ImGui::DragFloat3("Offset", reinterpret_cast<float*>(&m_vFollowOffset),
            0.5f, -50.f, 50.f);

        ImGui::Separator();
        ImGui::Text("-- Current State --");
        ImGui::Text("Eye : (%.1f, %.1f, %.1f)", m_vEye.x, m_vEye.y, m_vEye.z);
        ImGui::Text("At  : (%.1f, %.1f, %.1f)", m_vAt.x, m_vAt.y, m_vAt.z);
        ImGui::Text("Mode: %s (F2 toggle)", m_bFollowMode ? "Follow" : "Free");

        if (ImGui::Button("Reset Offset"))
            m_vFollowOffset = { -12.f, 20.f, -12.f };

        ImGui::SameLine();
        if (ImGui::Button("Snap To Target"))
            SnapToTarget();
    }
    ImGui::End();
}

void CDynamicCamera::Key_Input(f32_t fTimeDelta, const CInput& input)
{
    using namespace DirectX;
    XMMATRIX matInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_ViewMatrix.m));
    XMFLOAT4X4 cam;
    XMStoreFloat4x4(&cam, matInv);

    Vec3 vRight = Vec3{ cam._11, cam._12, cam._13 }.Normalized();
    Vec3 vLook  = Vec3{ cam._31, cam._32, cam._33 }.Normalized();
    f32_t spd = m_fSpeed * fTimeDelta;

    if (input.IsKeyDown('W')) { m_vEye += vLook * spd;      m_vAt += vLook * spd; }
    if (input.IsKeyDown('S')) { m_vEye += vLook * (-spd);   m_vAt += vLook * (-spd); }
    if (input.IsKeyDown('A')) { m_vEye += vRight * (-spd);  m_vAt += vRight * (-spd); }
    if (input.IsKeyDown('D')) { m_vEye += vRight * spd;     m_vAt += vRight * spd; }
}

void CDynamicCamera::Mouse_Move(const CInput& input)
{
    using namespace DirectX;

    f32_t dx = input.GetMouseDeltaX();
    f32_t dy = input.GetMouseDeltaY();

    // ── 입력 스무딩 (EMA) ──
    constexpr f32_t kSmooth = 0.4f;
    m_fSmoothDX += (dx - m_fSmoothDX) * (1.f - kSmooth);
    m_fSmoothDY += (dy - m_fSmoothDY) * (1.f - kSmooth);
    dx = m_fSmoothDX;
    dy = m_fSmoothDY;

    // ── 상하 회전 (Pitch) — Gimbal Lock 방지: ±85도 클램핑 ──
    constexpr f32_t kMaxPitch = XMConvertToRadians(85.f);

    if (std::abs(dy) > 0.001f)
    {
        f32_t fPitchDelta = dy * m_fMouseSensitivity;

        f32_t fNewPitch = m_fPitchAccum + fPitchDelta;
        if (fNewPitch > kMaxPitch)  fPitchDelta = kMaxPitch - m_fPitchAccum;
        if (fNewPitch < -kMaxPitch) fPitchDelta = -kMaxPitch - m_fPitchAccum;
        m_fPitchAccum += fPitchDelta;

        XMMATRIX matInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_ViewMatrix.m));
        XMFLOAT4X4 cam;
        XMStoreFloat4x4(&cam, matInv);
        XMFLOAT3 fRight = { cam._11, cam._12, cam._13 };

        XMFLOAT3 fLook = { m_vAt.x - m_vEye.x, m_vAt.y - m_vEye.y, m_vAt.z - m_vEye.z };
        XMVECTOR vLook = XMLoadFloat3(&fLook);

        vLook = XMVector3TransformNormal(vLook,
            XMMatrixRotationAxis(XMLoadFloat3(&fRight), fPitchDelta));

        XMFLOAT3 r;
        XMStoreFloat3(&r, vLook);
        m_vAt = { m_vEye.x + r.x, m_vEye.y + r.y, m_vEye.z + r.z };
    }

    // ── 좌우 회전 (Yaw) ──
    if (std::abs(dx) > 0.001f)
    {
        XMFLOAT3 fLook = { m_vAt.x - m_vEye.x, m_vAt.y - m_vEye.y, m_vAt.z - m_vEye.z };
        XMVECTOR vLook = XMLoadFloat3(&fLook);

        vLook = XMVector3TransformNormal(vLook,
            XMMatrixRotationAxis(XMVectorSet(0, 1, 0, 0), dx * m_fMouseSensitivity));

        XMFLOAT3 r;
        XMStoreFloat3(&r, vLook);
        m_vAt = { m_vEye.x + r.x, m_vEye.y + r.y, m_vEye.z + r.z };
    }
}

void CDynamicCamera::ApplyEdgeScroll(f32_t fTimeDelta, const CInput& input)
{
    auto deactivate = [this]()
    {
        m_bEdgeScrollActive = false;
    };

    if (fTimeDelta <= 0.f || m_bFreeCameraMode || m_bFix)
    {
        deactivate();
        return;
    }

    HWND hWnd = input.GetWindowHandle();
    if (!hWnd || GetForegroundWindow() != hWnd)
    {
        deactivate();
        return;
    }

    RECT rcClient{};
    if (!GetClientRect(hWnd, &rcClient))
    {
        deactivate();
        return;
    }

    const f32_t fClientW = static_cast<f32_t>(rcClient.right - rcClient.left);
    const f32_t fClientH = static_cast<f32_t>(rcClient.bottom - rcClient.top);
    if (fClientW <= 1.f || fClientH <= 1.f)
    {
        deactivate();
        return;
    }

    POINT ptCursor{};
    if (!GetCursorPos(&ptCursor) || !ScreenToClient(hWnd, &ptCursor))
    {
        deactivate();
        return;
    }

    const f32_t fMouseX = static_cast<f32_t>(ptCursor.x);
    const f32_t fMouseY = static_cast<f32_t>(ptCursor.y);
    if (fMouseX < 0.f || fMouseY < 0.f ||
        fMouseX >= fClientW || fMouseY >= fClientH)
    {
        deactivate();
        return;
    }

    const f32_t fBandX =
        (fClientW < kEdgeScrollBandPixels * 2.f)
        ? fClientW * 0.5f
        : kEdgeScrollBandPixels;
    const f32_t fBandY =
        (fClientH < kEdgeScrollBandPixels * 2.f)
        ? fClientH * 0.5f
        : kEdgeScrollBandPixels;

    f32_t fMoveX = 0.f;
    f32_t fMoveY = 0.f;
    if (fMouseX <= fBandX)
        fMoveX = -1.f;
    else if (fMouseX >= fClientW - 1.f - fBandX)
        fMoveX = 1.f;

    if (fMouseY <= fBandY)
        fMoveY = 1.f;
    else if (fMouseY >= fClientH - 1.f - fBandY)
        fMoveY = -1.f;

    if (fMoveX == 0.f && fMoveY == 0.f)
    {
        deactivate();
        return;
    }

    Vec3 vForward = GetForward();
    vForward.y = 0.f;
    vForward = vForward.Normalized();
    if (vForward.Length() <= 0.0001f)
        vForward = { 0.f, 0.f, 1.f };

    Vec3 vRight = GetRight();
    vRight.y = 0.f;
    vRight = vRight.Normalized();
    if (vRight.Length() <= 0.0001f)
        vRight = { 1.f, 0.f, 0.f };

    Vec3 vMoveDir = (vRight * fMoveX) + (vForward * fMoveY);
    vMoveDir.y = 0.f;
    vMoveDir = vMoveDir.Normalized();
    if (vMoveDir.Length() <= 0.0001f)
    {
        deactivate();
        return;
    }

    if (m_bFollowMode)
    {
        m_bFollowMode = false;
        m_bFollowInitialized = false;
    }

    const f32_t fDelta = (fTimeDelta > kEdgeScrollMaxDeltaSec)
        ? kEdgeScrollMaxDeltaSec
        : fTimeDelta;
    const Vec3 vDelta = vMoveDir * (kEdgeScrollSpeed * fDelta);
    m_vEye += vDelta;
    m_vAt += vDelta;

#ifdef _DEBUG
    if (!m_bEdgeScrollActive)
    {
        char szMsg[160]{};
        std::snprintf(
            szMsg,
            sizeof(szMsg),
            "[CameraEdgeScroll] begin x=%.0f y=%.0f dir=(%.2f,%.2f)\n",
            fMouseX,
            fMouseY,
            vMoveDir.x,
            vMoveDir.z);
        OutputDebugStringA(szMsg);
    }
#endif
    m_bEdgeScrollActive = true;
}
