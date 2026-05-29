#include "Core/CInput.h"
#include "Renderer/CCamera.h"
#include "WintersMath.h"
#include <DirectXMath.h>
using namespace DirectX;
// ─────────────────────────────────────────────────────────────────
//  CInput 구현
//
//  OnMouseMove     : WM_MOUSEMOVE → 절대 커서 좌표 기록 (UI/Debug)
//  OnRawMouseDelta : WM_INPUT    → HID 레벨 상대 델타 누적
//
//  매 프레임 끝에 EndFrame() 에서 델타 리셋.
// ─────────────────────────────────────────────────────────────────

bool CInput::IsKeyPressed(uint8 vKey) const
{
    return m_Keys[vKey] && !m_pPrevKeys[vKey];
}

bool CInput::IsKeyReleased(uint8 vKey) const
{
    return !m_Keys[vKey] && m_pPrevKeys[vKey];
}

bool CInput::IsRButtonPressed() const
{
    return m_bRButton && !m_pPrevRButton;
}

bool CInput::IsRButtonReleased() const
{
    return !m_bRButton && m_pPrevRButton;
}

bool CInput::IsLButtonPressed() const
{
    return m_bLButton && !m_pPrevLButton;
}

bool CInput::IsLButtonReleased() const
{
    return !m_bLButton && m_pPrevLButton;
}

void CInput::OnMouseMove(int32 x, int32 y)
{
    //절대 좌표만 추적 (UI/Debug)
    m_MouseX = x;
    m_MouseY = y;
}

void CInput::OnRawMouseDelta(int32 dx, int32 dy)
{
    m_MouseDeltaX += static_cast<f32_t>(dx);
    m_MouseDeltaY += static_cast<f32_t>(dy);
}

CInput::MouseRay CInput::GetMouseWorldRay(const CCamera& cam, int32_t screenW, int32_t screenH) const
{
    //Screen -> NDC
    f32_t mx = static_cast<f32_t>(m_MouseX);
    f32_t my = static_cast<f32_t>(m_MouseY);
    f32_t ndcX = (2.f * mx / static_cast<f32_t>(screenW)) - 1.f;
    f32_t ndcY = 1.f - (2.f * my / static_cast<f32_t>(screenH));

    // Mat4 → XMMATRIX (프로젝트 헬퍼)
    XMMATRIX view = cam.GetViewMatrix().ToXMMATRIX();
    XMMATRIX proj = cam.GetProjectionMatrix().ToXMMATRIX();
    XMMATRIX invVP = XMMatrixInverse(nullptr, XMMatrixMultiply(view, proj));

    // near/far 언프로젝트 → 방향 정규화
    XMVECTOR nearV = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.f, 1.f), invVP);
    XMVECTOR farV = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.f, 1.f), invVP);
    XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(farV, nearV));

    MouseRay r;
    XMFLOAT3 o, d;
    XMStoreFloat3(&o, nearV);
    XMStoreFloat3(&d, dir);
    r.Origin = { o.x, o.y, o.z };
    r.Dir = { d.x, d.y, d.z };
    return r;
}

Vec3 CInput::GetMouseGroundPos(const CCamera& cam, int32_t screenW, int32_t screenH) const
{
    MouseRay r = GetMouseWorldRay(cam, screenW, screenH);
    if (fabsf(r.Dir.y) < 1e-4f) return { 0.f, 0.f, 0.f };   // 거의 수평 → 지면과 교차 불가
    f32_t t = -r.Origin.y / r.Dir.y;
    if (t < 0.f) return { 0.f, 0.f, 0.f };                   // 카메라 뒤쪽
    return { r.Origin.x + r.Dir.x * t, 0.f, r.Origin.z + r.Dir.z * t };
}
