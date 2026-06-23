#pragma once
#include "Renderer/CCamera.h"
#include "Core/CTransform.h"
#include "Defines.h"

class CDynamicCamera : public CCamera
{
public:
    ~CDynamicCamera() override = default;

    static unique_ptr<CDynamicCamera> Create(
        const Vec3& vEye, const Vec3& vAt, const Vec3& vUp,
        f32_t fFov = XMConvertToRadians(60.f),
        f32_t fAspect = 1280.f / 720.f,
        f32_t fNear = 1.f,
        f32_t fFar = 1000.f);

    void Update(f32_t fTimeDelta, const CInput& input) override;

    // 플레이어 추적
    void SetFollowTarget(CTransform* pTarget)
    {
        if (m_pTargetTransform != pTarget)
            m_bFollowInitialized = false;
        m_pTargetTransform = pTarget;
    }
    void SetFollowOffset(const Vec3& vOffset) { m_vFollowOffset = vOffset; }

    // Follow <-> Free 전환 (F2)
    void SetFollowMode(bool bFollow)
    {
        if (m_bFollowMode != bFollow)
            m_bFollowInitialized = false;
        m_bFollowMode = bFollow;
    }
    bool IsFollowMode() const { return m_bFollowMode; }

    // 바로 플레이어 시점으로 스냅
    void SnapToTarget();
    void JumpToWorldXZ(const Vec3& vWorldPos);

    // 카메라 쉐이크
    void StartShake(f32_t fDuration, f32_t fIntensity);

    void OnImGui();

private:
    CDynamicCamera() = default;

    void Update_FollowCam(f32_t fTimeDelta);
    void Update_FreeCam(f32_t fTimeDelta, const CInput& input);

    void Key_Input(f32_t fTimeDelta, const CInput& input);
    void Mouse_Move(const CInput& input);
    void ApplyEdgeScroll(f32_t fTimeDelta, const CInput& input);

    //FPS cursor lock stats change
    void Enter_FPSMode(const CInput& input);
    void Exit_FPSMode();

private:
    CTransform* m_pTargetTransform = nullptr;

    bool m_bCursorHidden = false;

    Vec3 m_vFollowOffset = { -3.f, 11.5f, -5.f };
    f32_t m_fFollowResponse = 18.f;
    bool_t m_bFollowInitialized = false;

    bool m_bFollowMode = true;
    bool m_bFix = false;
    bool m_bF2Check = false;
    bool m_bTabCheck = false;
    bool_t m_bEdgeScrollActive = false;
    bool_t m_bSpaceFollowActive = false;

    f32_t m_fMouseSensitivity = 0.004f;
    f32_t m_fSmoothDX = 0.f;
    f32_t m_fSmoothDY = 0.f;
    f32_t m_fPitchAccum = 0.f;          // Pitch 누적 각도 (Gimbal Lock 방지용)

    f32_t m_fShakeDuration = 0.f;
    f32_t m_fShakeTimer = 0.f;
    f32_t m_fShakeIntensity = 0.f;
};
