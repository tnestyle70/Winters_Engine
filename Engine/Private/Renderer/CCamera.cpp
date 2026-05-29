#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Renderer/CCamera.h"
#include "Core/CInput.h"
#include <cmath>

void CCamera::Ready(const Vec3& vEye, const Vec3& vAt, const Vec3& vUp,
                    f32_t fFov, f32_t fAspect, f32_t fNear, f32_t fFar)
{
    m_vEye = vEye;
    m_vAt  = vAt;
    m_vUp  = vUp;
    m_fFov = fFov;
    m_fAspect = fAspect;
    m_fNear = fNear;
    m_fFar = fFar;

    m_ProjMatrix = Mat4::Perspective(m_fFov, m_fAspect, m_fNear, m_fFar);
    RecalcView();
}

void CCamera::SetPerspective(f32_t fovY, f32_t aspect, f32_t nearZ, f32_t farZ)
{
    m_fFov = fovY;
    m_fAspect = aspect;
    m_fNear = nearZ;
    m_fFar = farZ;
    m_ProjMatrix = Mat4::Perspective(m_fFov, m_fAspect, m_fNear, m_fFar);
}

Vec3 CCamera::GetForward() const
{
    return Vec3{ m_vAt.x - m_vEye.x, m_vAt.y - m_vEye.y, m_vAt.z - m_vEye.z }.Normalized();
}

Vec3 CCamera::GetRight() const
{
    return Vec3::Cross(m_vUp, GetForward()).Normalized();
}

Mat4 CCamera::GetViewProjection() const
{
    return m_ViewMatrix * m_ProjMatrix;
}

void CCamera::Update(f32_t deltaTime, const CInput& input)
{
    // Tab 토글
    if (input.IsKeyDown(VK_TAB))
    {
        if (!m_bTabCheck) { m_bTabCheck = true; m_bFix = !m_bFix; }
    }
    else { m_bTabCheck = false; }

    // WASD 이동
    Key_Input(deltaTime, input);

    // Tab ON이면 FPS 마우스 회전 (Raw Input 기반, 커서 워프 불필요)
    if (m_bFix)
    {
        Mouse_Move(input);
    }

    RecalcView();
}

void CCamera::Key_Input(f32_t deltaTime, const CInput& input)
{
    // View 역행렬에서 Right/Look 추출
    using namespace DirectX;

    XMMATRIX matInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_ViewMatrix.m));
    XMFLOAT4X4 cam;
    XMStoreFloat4x4(&cam, matInv);

    Vec3 vRight = Vec3{ cam._11, cam._12, cam._13 }.Normalized();
    Vec3 vLook  = Vec3{ cam._31, cam._32, cam._33 }.Normalized();
    f32_t speed = m_fSpeed * deltaTime;

    // W: 앞으로
    if (input.IsKeyDown('W'))
    {
        m_vEye += vLook * speed;
        m_vAt += vLook * speed;
    }
    // S: 뒤로
    if (input.IsKeyDown('S'))
    { 
        m_vEye += vLook * (-speed); 
        m_vAt += vLook * (-speed); 
    }

    // A: 왼쪽
    if (input.IsKeyDown('A'))
    {   m_vEye += vRight * (-speed);
        m_vAt += vRight * (-speed); 
    }

    // D: 오른쪽
    if (input.IsKeyDown('D'))
    {   m_vEye += vRight * speed; 
        m_vAt += vRight * speed; 
    }
}

void CCamera::Mouse_Move(const CInput& input)
{
    f32_t dx = input.GetMouseDeltaX();
    f32_t dy = input.GetMouseDeltaY();
    if (std::abs(dx) < 0.001f && std::abs(dy) < 0.001f) return;

    using namespace DirectX;
    XMFLOAT3 fLook = { m_vAt.x - m_vEye.x, m_vAt.y - m_vEye.y, m_vAt.z - m_vEye.z };
    XMVECTOR vLook = XMLoadFloat3(&fLook);

    // 상하 회전 (Right축 기준)
    if (std::abs(dy) > 0.001f)
    {
        XMMATRIX matInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_ViewMatrix.m));
        XMFLOAT4X4 cam;
        XMStoreFloat4x4(&cam, matInv);
        XMFLOAT3 fRight = { cam._11, cam._12, cam._13 };
        vLook = XMVector3TransformNormal(vLook, XMMatrixRotationAxis(XMLoadFloat3(&fRight), dy * 0.01f));
    }

    // 좌우 회전 (Y축 기준)
    if (std::abs(dx) > 0.001f)
    {
        vLook = XMVector3TransformNormal(vLook, XMMatrixRotationAxis(XMVectorSet(0,1,0,0), dx * 0.01f));
    }

    XMFLOAT3 r;
    XMStoreFloat3(&r, vLook);
    m_vAt = { m_vEye.x + r.x, m_vEye.y + r.y, m_vEye.z + r.z };
}

void CCamera::RecalcView()
{
    m_ViewMatrix = Mat4::LookAt(m_vEye, m_vAt, m_vUp);
}
