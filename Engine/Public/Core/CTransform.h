#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

// ─────────────────────────────────────────────────────────────────
//  CTransform  |  위치/회전/스케일 → 월드 행렬
//
//  Dirty Flag 패턴: 변경 시에만 행렬 재계산.
//  매 프레임 SetPosition 없이도 GetWorldMatrix() 호출 시
//  캐싱된 값 반환 → 불필요한 XMMATRIX 곱셈 제거.
//
//  SRT 순서: Scale → RotateX → RotateY → RotateZ → Translate
// ─────────────────────────────────────────────────────────────────

//  [DEPRECATED] CTransform
//
//  이 클래스는 non-ECS 코드 호환용으로만 유지됩니다.
//  신규 코드는 TransformComponent + TransformSystem을 사용하세요:
//    #include "ECS/Components/TransformComponent.h"
//    #include "ECS/Systems/TransformSystem.h"
//
//  Phase 1a 마이그레이션 후 제거 예정 (Phase 2 시작 시).

class WINTERS_ENGINE CTransform
{
public:
    CTransform() = default;

    // ── 위치 ──────────────────────────────────────────────────
    void SetPosition(const Vec3& pos) { m_Position = pos; m_bDirty = true; }
    void SetPosition(float x, float y, float z) { SetPosition({ x, y, z }); }
    const Vec3& GetPosition() const { return m_Position; }

    // ── 회전 (오일러, 라디안) ─────────────────────────────────
    void SetRotation(const Vec3& rot) { m_Rotation = rot; m_bDirty = true; }
    void SetRotationX(float rad) { m_Rotation.x = rad; m_bDirty = true; }
    void SetRotationY(float rad) { m_Rotation.y = rad; m_bDirty = true; }
    void SetRotationZ(float rad) { m_Rotation.z = rad; m_bDirty = true; }
    const Vec3& GetRotation() const { return m_Rotation; }

    // ── 스케일 ────────────────────────────────────────────────
    void SetScale(const Vec3& s) { m_Scale = s; m_bDirty = true; }
    void SetScale(float uniform) { SetScale({ uniform, uniform, uniform }); }
    const Vec3& GetScale() const { return m_Scale; }

    // ── 월드 행렬 (Dirty면 재계산) ────────────────────────────
    const Mat4& GetWorldMatrix();

private:
    Vec3  m_Position = {};
    Vec3  m_Rotation = {};                 // Euler (radians)
    Vec3  m_Scale    = { 1.f, 1.f, 1.f };
    Mat4  m_WorldMatrix;                   // 캐싱
    bool  m_bDirty   = true;
};
