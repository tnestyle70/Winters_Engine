#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

class CInput;
//이거도 공개하는 게 맞음? GameInstance에 안 넣어도 됨?
class WINTERS_ENGINE CCamera
{
protected:
    // 기반 클래스 — 생성자 protected (자식만 호출 가능, 외부 new 차단)
    CCamera() = default;

public:
    virtual ~CCamera() = default;

    // 초기화
    void Ready(const Vec3& vEye, const Vec3& vAt, const Vec3& vUp,
               f32_t fFov, f32_t fAspect, f32_t fNear, f32_t fFar);

    void SetPerspective(f32_t fovY, f32_t aspect, f32_t nearZ, f32_t farZ);
    void SetPosition(const Vec3& pos) { m_vEye = pos; }

    // Getter
    const Vec3& GetEye()    const { return m_vEye; }
    const Vec3& GetAt()     const { return m_vAt; }
    const Vec3& GetUp()     const { return m_vUp; }

    const Mat4& GetViewMatrix()       const { return m_ViewMatrix; }
    const Mat4& GetProjectionMatrix() const { return m_ProjMatrix; }
    Mat4 GetViewProjection() const;

    Vec3 GetForward() const;
    Vec3 GetRight()   const;

    void SetMoveSpeed(f32_t speed) { m_fSpeed = speed; }

    // 가상 업데이트 - Client에서 오버라이드
    virtual void Update(f32_t deltaTime, const CInput& input);

protected:
    void RecalcView();

    Vec3    m_vEye = { 0.f, 10.f, -10.f };
    Vec3    m_vAt  = { 0.f, 0.f, 1.f };
    Vec3    m_vUp  = { 0.f, 1.f, 0.f };

    f32_t m_fFov    = 0.f;
    f32_t m_fAspect = 0.f;
    f32_t m_fNear   = 0.f;
    f32_t m_fFar    = 0.f;
    f32_t m_fSpeed  = 50.f;

    Mat4    m_ViewMatrix;
    Mat4    m_ProjMatrix;

private:
    // FPS 카메라 기본 동작 (디버그/에디터용)
    void Key_Input(f32_t deltaTime, const CInput& input);
};
