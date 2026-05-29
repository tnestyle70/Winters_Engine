# Winters Engine — 프로급 카메라 시스템 계획서

## 현업 스튜디오 카메라 분석

---

### Rockstar (GTA V / RDR2)

**핵심 기술: Spring Arm + Context-Aware Camera**

| 요소 | 구현 |
|------|------|
| 추적 방식 | Spring Arm — 플레이어와 카메라 사이에 가상의 스프링 연결 |
| 스무딩 | Critically Damped Spring — 오버슈트 없이 최단 시간에 목표 도달 |
| 충돌 회피 | SphereCast (카메라 → 플레이어 방향) + 벽 관통 방지 |
| 컨텍스트 전환 | 도보/차량/조준/커버 등 상황별 카메라 프리셋 자동 전환 |
| FOV 연동 | 차량 속도에 비례하여 FOV 동적 확대 (속도감 연출) |
| 관성(Inertia) | 차량 급회전 시 카메라가 지연되어 따라옴 — 물리적 느낌 |
| Look-Ahead | 이동 방향으로 카메라가 약간 선행 (프레이밍 오프셋) |

**GTA V 카메라 파라미터 추정:**
- 기본 distance: ~4m (도보), ~8m (차량)
- Spring halflife: ~0.15s (빠른 반응), 차량: ~0.3s (느긋한 관성)
- 충돌 SphereCast radius: ~0.3m
- FOV: 65° (도보) → 85° (고속 차량)

---

### FromSoftware (엘든링 / 다크소울)

**핵심 기술: Lock-On Orbit + Pitch Clamp**

| 요소 | 구현 |
|------|------|
| 기본 모드 | 3인칭 Orbit Camera — 우스틱으로 Yaw/Pitch 직접 제어 |
| Lock-On | 타겟 락온 시 Eye→Target 방향으로 카메라 자동 회전 |
| 타겟 선택 | Frontal Cone + 거리 가중치 — 정면 가까운 적 우선 |
| Pitch 제한 | 위: +60°, 아래: -40° (지면 관통 방지) |
| 충돌 | Raycast (Eye→Target) + 즉시 줌인 (벽 뒤로 안 가게) |
| 보스전 | 거대 보스는 Lock-On 포인트를 몸통 중심이 아닌 약점 부위에 배치 |
| 카메라 쉐이크 | 타격 시 Duration + Intensity + Decay 기반 쉐이크 |

**엘든링 카메라 파라미터 추정:**
- Orbit distance: ~3.5m
- Lock-On 최대 거리: ~25m
- Frontal cone: ±60°
- Pitch clamp: -40° ~ +60°
- 충돌 줌인 속도: 즉시 (줌아웃은 Spring으로 서서히)

---

### Pearl Abyss (검은사막 / 붉은사막)

**핵심 기술: High-Speed Action Camera + Free Orbit**

| 요소 | 구현 |
|------|------|
| 기본 모드 | Free Orbit — 마우스로 360° 회전, 줌 인/아웃 |
| 전투 카메라 | 액션 스킬 시 카메라 자동 회전 + FOV 펀치 (타격감) |
| 줌 단계 | 마우스 휠로 distance 조절 (최소 ~1m ~ 최대 ~15m) |
| 대규모 전투 | 공성전 시 카메라 distance 자동 확대 + FOV 넓힘 |
| 스킬 연출 | 특정 스킬 사용 시 시네마틱 카메라 (웨이포인트 보간) |
| 캐릭터 프레이밍 | 캐릭터를 화면 중앙이 아닌 1/3 지점에 배치 (Rule of Thirds) |

---

### CD Projekt RED (위쳐 3 / 사이버펑크)

**핵심 기술: Contextual Camera + Cinematic Blend**

| 요소 | 구현 |
|------|------|
| 위쳐 3 | 3인칭 Orbit + 전투 시 distance 자동 축소 |
| 사이버펑크 | 1인칭 전용 (차량 3인칭) — Head Bob + Weapon Sway |
| 대화 카메라 | 자동 시네마틱 앵글 — 캐릭터 위치/감정에 따라 앵글 선택 |
| 전환 블렌딩 | 모드 전환 시 파라미터 공간 보간 (위치 보간 X, 파라미터 보간 O) |
| 말 탑승 | 위쳐 로치 탑승 시 Spring Arm distance 확대 + 관성 증가 |

---

## 공통 핵심 기술 정리

### 1. Critically Damped Spring (모든 AAA 공통)

프레임 독립적, 오버슈트 없는 스무딩. Unity의 `SmoothDamp`과 동일 원리.

```cpp
// Daniel Holden (theorangeduck) 구현 기반
void critical_spring_damper_exact(
    f32_t& x, f32_t& v,          // 현재값, 현재속도 (in/out)
    f32_t x_goal, f32_t v_goal,   // 목표값, 목표속도
    f32_t halflife,               // 목표의 63%에 도달하는 시간
    f32_t dt)                     // 델타타임
{
    f32_t d = halflife_to_damping(halflife);
    f32_t c = x_goal + (d * v_goal) / ((d * d) / 4.f);
    f32_t y = d / 2.f;
    f32_t j0 = x - c;
    f32_t j1 = v + j0 * y;
    f32_t eydt = fast_negexp(y * dt);

    x = eydt * (j0 + j1 * dt) + c;
    v = eydt * (v - j1 * y * dt);
}

f32_t halflife_to_damping(f32_t halflife, f32_t eps = 1e-5f)
{
    return (4.f * 0.69314718056f) / (halflife + eps);
}

f32_t fast_negexp(f32_t x)
{
    return 1.f / (1.f + x + 0.48f * x * x + 0.235f * x * x * x);
}
```

**핵심 파라미터: halflife**
- 0.05s: 즉각 반응 (FPS 에임)
- 0.15s: 빠른 추적 (3인칭 도보)
- 0.30s: 느긋한 관성 (차량, 탑승)
- 0.50s: 시네마틱 (대화, 컷신)

### 2. SphereCast 충돌 회피

```
[플레이어] ----SphereCast(r=0.3)---→ [카메라 목표 위치]
                                      ↓ 충돌 감지 시
                              [충돌점 - offset] = 실제 카메라 위치
```

- 줌인(충돌): **즉시** (벽 관통 방지)
- 줌아웃(해제): **Spring으로 서서히** (자연스러움)

### 3. 카메라 모드 시스템

```
enum class ECameraMode
{
    Free,           // WASD + 마우스 (에디터/디버그)
    Follow,         // 탑뷰 추적 (LoL/던전스)
    Orbit,          // 3인칭 Orbit (엘든링/GTA)
    LockOn,         // 타겟 락온 (엘든링 전투)
    Cinematic,      // 웨이포인트 시네마틱
    End
};
```

### 4. 파라미터 공간 블렌딩 (CDPR 방식)

모드 전환 시 **위치를 직접 Lerp하지 않고**, 카메라 파라미터(distance, pitch, yaw, offset)를 보간:

```cpp
struct CameraParams {
    Vec3  vTrackingPos;     // 추적 대상 위치
    Vec2  vFraming;         // 화면 내 프레이밍 (-1~1)
    f32_t fDistance;        // 카메라-대상 거리
    f32_t fPitch;           // 수직 각도
    f32_t fYaw;             // 수평 각도
    f32_t fFov;             // 시야각
};
// 전환: Params_A → Params_B를 Spring으로 각각 보간
```

이 방식의 장점: 카메라가 추적 대상을 항상 화면에 유지하면서 자연스럽게 전환.

---

## Winters Engine 카메라 구현 로드맵

### Phase 0: 현재 (완료)
- [x] CCamera 기반 클래스 (protected 멤버, virtual Update)
- [x] CDynamicCamera (Follow + Free + 쉐이크)
- [x] DX9→DX11 Mouse_Move 변환 (XMConvertToRadians)

### Phase 1: 스프링 댐퍼 시스템 (Engine 레벨)
**새 파일: `Engine/Public/Core/SpringDamper.h`**

```cpp
#pragma once
#include "WintersTypes.h"

namespace Winters
{
    // Critically Damped Spring — 1D
    void SpringDamper(f32_t& x, f32_t& v,
                      f32_t x_goal, f32_t v_goal,
                      f32_t halflife, f32_t dt);

    // Vec3 버전
    void SpringDamperVec3(Vec3& x, Vec3& v,
                          const Vec3& x_goal, const Vec3& v_goal,
                          f32_t halflife, f32_t dt);

    // 각도 버전 (래핑 처리)
    void SpringDamperAngle(f32_t& x, f32_t& v,
                           f32_t x_goal, f32_t v_goal,
                           f32_t halflife, f32_t dt);
}
```

### Phase 2: CCamera 파라미터화 리팩터링

현재 Eye/At/Up 직접 조작 → **파라미터 기반**으로 전환:

```cpp
// Engine/Public/Renderer/CCamera.h 에 추가
struct CameraParams
{
    Vec3  vTrackPos;        // 추적 위치
    Vec2  vFraming;         // 프레이밍 오프셋
    f32_t fDistance = 5.f;  // 카메라-대상 거리
    f32_t fPitch   = 0.f;  // 수직 각도 (rad)
    f32_t fYaw     = 0.f;  // 수평 각도 (rad)
    f32_t fFov     = XMConvertToRadians(60.f);
};
```

### Phase 3: CDynamicCamera 모드 시스템

```cpp
// Client/Public/DynamicCamera.h
enum class ECameraMode { Free, Follow, Orbit, LockOn, Cinematic, End };

class CDynamicCamera : public CCamera
{
    // 기존 멤버 + 추가:
    ECameraMode m_eMode = ECameraMode::Follow;

    // Spring 상태 (위치/각도별 velocity 보존)
    Vec3  m_vPosVelocity = {};
    f32_t m_fYawVelocity = 0.f;
    f32_t m_fPitchVelocity = 0.f;
    f32_t m_fDistVelocity = 0.f;

    // Spring 파라미터
    f32_t m_fSpringHalflife = 0.15f;   // 기본 반응성

    // Orbit 제한
    f32_t m_fPitchMin = XMConvertToRadians(-40.f);
    f32_t m_fPitchMax = XMConvertToRadians(60.f);

    // Lock-On
    CTransform* m_pLockOnTarget = nullptr;
    f32_t m_fLockOnMaxDist = 25.f;
    f32_t m_fLockOnConeAngle = XMConvertToRadians(60.f);
};
```

### Phase 4: 충돌 회피 (Physics 연동 후)

Jolt Physics 통합 후 SphereCast 기반 충돌 감지:
- 줌인: 즉시 (halflife = 0.01s)
- 줌아웃: 서서히 (halflife = 0.3s)

### Phase 5: 시네마틱 카메라

웨이포인트 + Catmull-Rom 스플라인 보간:
- 킬캠, 스킬 연출, 대화 카메라
- 타임라인 에디터 (ImGui)

---

## 즉시 적용 가능한 개선 (현재 Phase 0 → 1)

### 1. Mouse_Move에 Spring 적용

현재: `XMConvertToRadians(dx / 10.f)` → 즉시 적용 (딱딱함)
개선: Yaw/Pitch를 Spring 목표로 설정, Spring이 실제 적용

### 2. Follow 카메라에 Spring 적용

현재: `m_vEye = vTargetPos + m_vFollowOffset` → 즉시 텔레포트
개선: `SpringDamperVec3(m_vEye, m_vPosVelocity, goal, {}, halflife, dt)`

### 3. FOV 동적 조절

이동 속도에 따라 FOV 약간 확대 (속도감):
```cpp
f32_t fSpeedRatio = Length(playerVelocity) / maxSpeed;
f32_t fTargetFov = Lerp(baseFov, baseFov * 1.15f, fSpeedRatio);
SpringDamper(m_fFov, m_fFovVelocity, fTargetFov, 0.f, 0.3f, dt);
```

---

## 참고 자료

| 자료 | 내용 |
|------|------|
| [Spring-It-On (Daniel Holden)](https://theorangeduck.com/page/spring-roll-call) | Critically Damped Spring C++ 구현 전문 |
| [The Art of Damping](https://www.alexisbacot.com/blog/the-art-of-damping) | SmoothDamp 6가지 방식 비교 |
| [Third Person Cameras (Little Polygon)](https://blog.littlepolygon.com/posts/cameras/) | 파라미터 공간 카메라 설계 |
| [Accurate Collision Zoom](https://www.gamedeveloper.com/programming/accurate-collision-zoom-for-cameras) | SphereCast 충돌 줌 |
| [GDC: 50 Game Camera Mistakes](https://gdcvault.com/play/192/Designing-and-Implementing-a-Dynamic) | AAA 카메라 실수 50가지 |
| [GDC: Fundamentals of Camera Design](https://media.gdcvault.com/gdc05/slides/GD_Haigh-Hutchinson_FundamentalsReal-TimeCameraDesign2.pdf) | 실시간 카메라 설계 기초 |
| [엘든링 Lock-On 분석](https://www.jeleniauskas.com/writing/improving-elden-ring's-lock-on-experience) | Lock-On UX 개선 제안 |
| [Damped Springs (Ryan Juckett)](https://www.ryanjuckett.com/damped-springs/) | 스프링 수학 증명 |
