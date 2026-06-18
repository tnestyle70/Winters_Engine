Session - 입력 반응과 카메라 감각 개선

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Public/DynamicCamera.h

기존 코드:
```cpp
    void SetFollowTarget(CTransform* pTarget) { m_pTargetTransform = pTarget; }
    void SetFollowOffset(const Vec3& vOffset) { m_vFollowOffset = vOffset; }

    // Follow <-> Free ?꾪솚 (F2)
    void SetFollowMode(bool bFollow) { m_bFollowMode = bFollow; }
```

아래로 교체:
```cpp
    void SetFollowTarget(CTransform* pTarget)
    {
        if (m_pTargetTransform != pTarget)
            m_bFollowInitialized = false;
        m_pTargetTransform = pTarget;
    }
    void SetFollowOffset(const Vec3& vOffset) { m_vFollowOffset = vOffset; }

    // Follow <-> Free ?꾪솚 (F2)
    void SetFollowMode(bool bFollow)
    {
        if (m_bFollowMode != bFollow)
            m_bFollowInitialized = false;
        m_bFollowMode = bFollow;
    }
```

기존 코드:
```cpp
    Vec3 m_vFollowOffset = { -3.f, 11.5f, -5.f };

    bool m_bFollowMode = true;
```

아래에 추가:
```cpp
    f32_t m_fFollowResponse = 18.f;
    bool_t m_bFollowInitialized = false;
```

1-2. C:/Users/tnest/Desktop/Winters/Client/Private/DynamicCamera.cpp

기존 코드:
```cpp
void CDynamicCamera::Update_FollowCam(f32_t fTimeDelta)
{
    Vec3 vTargetPos = m_pTargetTransform->GetPosition();
    m_vEye = vTargetPos + m_vFollowOffset;
    m_vAt  = vTargetPos + Vec3(0.f, 1.5f, 0.f);
}
```

아래로 교체:
```cpp
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
```

기존 코드:
```cpp
void CDynamicCamera::SnapToTarget()
{
    if (!m_pTargetTransform) return;
    Vec3 vTargetPos = m_pTargetTransform->GetPosition();
    m_vEye = vTargetPos + m_vFollowOffset;
    m_vAt  = vTargetPos + Vec3(0.f, 1.5f, 0.f);
    RecalcView();
}
```

아래로 교체:
```cpp
void CDynamicCamera::SnapToTarget()
{
    if (!m_pTargetTransform) return;
    Vec3 vTargetPos = m_pTargetTransform->GetPosition();
    m_vEye = vTargetPos + m_vFollowOffset;
    m_vAt  = vTargetPos + Vec3(0.f, 1.5f, 0.f);
    m_bFollowInitialized = true;
    RecalcView();
}
```

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp

기존 코드:
```cpp
                    const u32_t moveSeq =
                        scene.m_pCommandSerializer->SendMove(
                            *scene.m_pNetworkView,
                            moveIntent,
                            moveFacingDirection);
```

아래에 추가:
```text
CONFIRM_NEEDED
이 지점 이후의 로컬 반응은 이미 일부 존재하지만, 이동 명령 send와 yaw 보호, NavAgent 목표 갱신, run 애니메이션 시작이 여러 블록에 나뉘어 있다.
구현 세션에서는 아래를 하나의 "즉시 피드백" 단계로 묶는다.
- accepted move target이면 send 성공 여부와 무관하게 로컬 목적지/이동 애니메이션/클릭 표시를 같은 프레임에 갱신
- network active에서는 서버 권위 이동 결과를 기다리지 않고 약한 로컬 이동 피드백만 제공
- 서버 스냅샷에서 큰 오차가 들어오면 S05의 reconciliation으로 수렴
```

2. 검증

```text
빌드:
- git diff --check
- Client/Private/DynamicCamera.cpp 단위 컴파일 또는 Client Debug x64 빌드

플레이 QA:
- 우클릭 한 번: 클릭 표시, 플레이어 yaw, run 애니메이션이 1프레임 이내 반응
- 우클릭 연타: 반 바퀴 yaw 뒤집힘이나 카메라 떨림 없음
- 서버 스냅샷 수신 중: 카메라가 순간 스냅을 그대로 확대해서 보여주지 않음

프로파일러:
- 카메라 smoothing이 CPU 비용으로 보이지 않아야 한다.
- Update/Scene_InGame::OnUpdate 수치가 증가하지 않아야 한다.
```
