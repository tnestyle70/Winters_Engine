#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "DynamicCamera.h"
#include "Core/CInput.h"
#include <cmath>
#include <cstdlib>
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

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
            m_bFollowMode = !m_bFollowMode;
            m_bFix = !m_bFollowMode;
        }
    }
    else { m_bF2Check = false; }

    if (m_pTargetTransform && m_bFollowMode)
        Update_FollowCam(fTimeDelta);
    else
        Update_FreeCam(fTimeDelta, input);

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
    Vec3 vTargetPos = m_pTargetTransform->GetPosition();
    m_vEye = vTargetPos + m_vFollowOffset;
    m_vAt  = vTargetPos + Vec3(0.f, 1.5f, 0.f);
}

void CDynamicCamera::Update_FreeCam(f32_t fTimeDelta, const CInput& input)
{
    Key_Input(fTimeDelta, input);

    if (input.IsKeyDown(VK_TAB))
    {
        if (!m_bTabCheck) 
        {    
            m_bTabCheck = true; 
            m_bFix = !m_bFix;
            //상태 전환시 딱 한 번만 처리
            if (m_bFix)
                Enter_FPSMode(input);
            else
                Exit_FPSMode();
        }
    }
    else { m_bTabCheck = false; }

    if (m_bFix)
    {
        // Raw Input (WM_INPUT) 기반 — 커서 워프/Mouse_Fix 불필요
        Mouse_Move(input);
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