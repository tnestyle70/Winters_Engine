#include "Core/CTransform.h"

// ─────────────────────────────────────────────────────────────────
//  CTransform 구현
//
//  SRT 순서: Scale → RotateX → RotateY → RotateZ → Translate
//  DirectXMath는 행-우선(row-major) 행렬, 왼쪽에서 곱해나감.
// ─────────────────────────────────────────────────────────────────

const Mat4& CTransform::GetWorldMatrix()
{
    if (!m_bDirty)
        return m_WorldMatrix;

    Mat4 s = Mat4::Scale(m_Scale);
    Mat4 r = Mat4::RotationX(m_Rotation.x)
           * Mat4::RotationY(m_Rotation.y)
           * Mat4::RotationZ(m_Rotation.z);
    Mat4 t = Mat4::Translation(m_Position);

    m_WorldMatrix = s * r * t;
    m_bDirty = false;

    return m_WorldMatrix;
}
