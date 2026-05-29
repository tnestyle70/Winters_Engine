# Physics 디버그 도구 — ImGui 튜너 + DebugDraw + Replay

## 목표

물리 시스템을 **블랙박스로 두지 말 것**. ImGui 로 모든 설정 실시간 조작 + DebugDraw 로 공간 정보
표현 + Replay 로 버그 재현.

## CLAUDE.md ImGui 정책 연동

"빌드 1번으로 모든 값 튜닝" = 물리 시뮬 파라미터는 전부 ImGui 슬라이더로 노출. 하드코딩 금지.

## Physics Debugger 메인 창

```
┌─ Physics Debugger ──────────────────────────────────────┐
│ [Pause: F9]  [Step: F10]  [Slow: ▶ 0.25x]               │
├─────────────────────────────────────────────────────────┤
│ Tabs: [World] [Broad] [Narrow] [Rigid] [Constraint]     │
│       [PBD] [CCD] [Socket] [Damage]                     │
├─────────────────────────────────────────────────────────┤
│ [선택한 탭 내용]                                        │
└─────────────────────────────────────────────────────────┘
```

### World 탭

월드 전역 설정 + 성능 지표.

```
[Gravity]            (0.0, -9.81, 0.0)    [drag 3-slider]
[Fixed Timestep]     16.67 ms             [slider 1~33 ms]
[Velocity Iterations]8                    [slider 1~20]
[Position Iterations]3                    [slider 1~10]
[Deterministic]      [ ]                  [checkbox]

── Performance (last frame) ─────────────────────────
  Step time total   : 2.34 ms
    Broad phase     : 0.21 ms
    Narrow phase    : 0.85 ms
    Constraints     : 0.95 ms (8 iter)
    PBD             : 0.12 ms
    CCD             : 0.21 ms
  Active bodies     : 42 (3 sleeping)
  Contact manifolds : 18
  Constraints       : 5
```

### Broad Phase 탭

- 현재 BroadPhase 구현 (SAP / BVH 전환 드롭다운)
- 트리 깊이 / 균형 지수 (BVH)
- SAP endpoint 수 (3축)
- 현재 후보 쌍 수 / 활성 쌍 수
- **DebugDraw 체크박스**:
  - [ ] Show AABB (모든 동적)
  - [ ] Show BVH internal nodes (색상 = 깊이)
  - [ ] Show candidate pair lines (노란 선)

### Narrow Phase 탭

- 현재 프레임 충돌 검사 수 (AABB-AABB / Sphere / Capsule / SAT / GJK)
- GJK 평균 반복 수
- EPA 평균 반복 수
- 접촉 매니폴드 리스트 (각 manifold 상세 — 법선, 관통량, 접촉점)
- **DebugDraw**:
  - [ ] Show contact points (빨강 점)
  - [ ] Show contact normals (노랑 화살표)
  - [ ] Show SAT axes (디버깅 모드)

### Rigid Body 탭

선택된 바디 상세 + 조작:
```
Selected Entity: #42 CrateBox
Type: Dynamic    Mass: 1.0 kg    InvMass: 1.0
Inertia Local: [ 0.17 0.00 0.00 ]
               [ 0.00 0.17 0.00 ]
               [ 0.00 0.00 0.17 ]
Position:      (2.34, 5.12, 0.00)
Rotation:      Quat(0.707, 0, 0.707, 0)
Linear Vel:    (0.0, -9.81, 0.0)    Speed: 9.81 m/s
Angular Vel:   (0.0, 0.5, 0.0)     Speed: 0.5 rad/s
Linear Damp:   [0.010] slider
Angular Damp:  [0.050] slider
Restitution:   [0.30] slider
Friction:      [0.50] slider

── Apply (debug) ──────────────────────
[Apply Force]  direction [...] magnitude [1000]
[Apply Impulse]
[Wake Up] / [Sleep]
[Teleport to Camera]
```

### Constraint 탭

- 활성 조인트 리스트
- 각 조인트 타입별 파라미터 실시간 조작
- 풀이 iteration 별 residual 그래프 (수렴도 시각화)

### PBD 탭

- 파티클 시스템 리스트 (천/로프/소프트바디)
- 선택된 시스템의 파티클 수 + 제약 수
- **파라미터 슬라이더**:
  - Stiffness (0 ~ 1)
  - Compliance (XPBD)
  - Iteration count
  - Damping
- **조작**:
  - [ ] Show particles
  - [ ] Show distance constraints (색상 = stretch 여부)
  - [Pin 선택 파티클]
  - [Unpin all]

### CCD 탭

```
[Enable CCD globally]       [x]
[Velocity threshold]        [50.0 m/s]
── CCD-enabled bodies ────────────────
  #104 Projectile_Arrow     speed 1234.5 m/s  [CCD ON]
  #107 Projectile_Q         speed 567.8 m/s   [CCD ON]
  #205 Yasuo                speed 6.2 m/s     [CCD OFF — under threshold]

── Recent TOI Results ─────────────────
  #104 vs #300 (wall)  TOI=0.34  normal=(1,0,0)
  #107 vs #250 (enemy) TOI=0.89  normal=(-0.7,0,0.7)
```

### Socket 탭 (Phase C-3 연동)

- 소켓을 가진 엔티티 리스트
- 각 소켓의 parent bone 이름 + 로컬 오프셋
- **DebugDraw**:
  - [ ] Show socket axes (3축)
  - [ ] Show bone-to-socket line (파랑)
  - [Set local offset...] 매트릭스 편집기

### Damage 탭

- 최근 N 프레임 내 Hitbox-Hurtbox 이벤트 로그
- 각 이벤트: 시각, 공격자, 피격자, 피해량, 피해 타입
- 필터 (특정 엔티티 / 특정 스킬)

## DebugDraw 인터페이스 (Phase C-2 기반)

```cpp
class IDebugDraw
{
public:
    virtual void DrawLine(const Vec3& p0, const Vec3& p1, const Color& color) = 0;
    virtual void DrawAABB(const AABB& box, const Color& color) = 0;
    virtual void DrawSphere(const Vec3& center, f32_t radius, const Color& color) = 0;
    virtual void DrawCapsule(const Vec3& p0, const Vec3& p1, f32_t radius, const Color& color) = 0;
    virtual void DrawArrow(const Vec3& from, const Vec3& to, const Color& color) = 0;
    virtual void DrawText3D(const Vec3& pos, const std::string& text, const Color& color) = 0;
    virtual void DrawTransform(const Mat4& m, f32_t axisLen) = 0;
};
```

모든 Physics 디버그 시스템이 `IDebugDraw` 를 받아 렌더링. Release 빌드에선 no-op.

## Physics Replay 시스템

### 녹화

매 fixed timestep 마다 모든 바디 상태 스냅샷 → 파일 저장.

```cpp
class CPhysicsRecorder
{
public:
    void Start(const std::string& path);
    void Stop();

    void RecordFrame(CWorld& world, f32_t simulationTime);

private:
    struct BodySnapshot {
        EntityID entity;
        Vec3     position;
        Quat     rotation;
        Vec3     linearVel, angularVel;
    };
    struct Frame {
        f32_t time;
        std::vector<BodySnapshot> bodies;
        std::vector<ContactSnapshot> contacts;
    };
    std::ofstream m_file;   // FlatBuffers
};
```

### 재생

```cpp
class CPhysicsReplayer
{
public:
    bool Load(const std::string& path);
    void Seek(f32_t time);
    void Pause() / Play() / Step();
    void ApplyFrameToWorld(CWorld& world);
};
```

ImGui 컨트롤:
```
Replay: [▷][⏸][⏭][⟲]  time: [─●────────] 3.45s / 12.00s
Speed:  [0.25x ▼]
Bodies: [All ▼]  Filter: [     ]
```

## Determinism Tester

같은 입력 → 같은 결과 검증. 버그 재현성.

```cpp
class CDeterminismChecker
{
public:
    void RecordReference(const std::string& path, CWorld& world, i32_t steps);
    bool CompareAgainstReference(const std::string& path, CWorld& world, i32_t steps);
};
```

CI 파이프라인에서 돌려 리그레션 검출.

## Profiler 연동

각 물리 시스템의 CPU 시간을 Profiler 마커로:

```cpp
void CPhysicsStepSystem::Step(CWorld& world, f32_t dt)
{
    PROFILE_SCOPE("Physics::Step");
    {
        PROFILE_SCOPE("Physics::BroadPhase");
        BroadPhaseSystem::UpdatePairs(world);
    }
    {
        PROFILE_SCOPE("Physics::NarrowPhase");
        NarrowPhaseSystem::GenerateContacts(world);
    }
    // ...
}
```

ImGui Profiler 창에서 각 Stage 별 시간 히스토그램.

## 단축키

| 키 | 기능 |
|---|---|
| F9 | 물리 Pause/Resume |
| F10 | Physics Single Step (1 fixed timestep) |
| F11 | Slow Motion 토글 (0.25x) |
| F12 | DebugDraw 마스터 토글 |
| Ctrl+Shift+R | Replay 녹화 시작/중지 |
| Ctrl+Click (월드) | 해당 위치 바디 선택 |

## 연습모드 통합

연습모드 씬에선 Physics Debugger 가 디폴트로 표시. 플레이어가:
- 물체 드래그 앤 드롭
- 힘/임펄스 적용 (마우스 드래그로 방향 + 크기 지정)
- 중력 on/off
- 시간 역재생 (Replay 역방향)

## 빌드 플래그

```cpp
#ifdef WINTERS_EDITOR
    #define PHYSICS_DEBUG_DRAW(x) (x)
#else
    #define PHYSICS_DEBUG_DRAW(x) do {} while(0)
#endif
```

릴리스 빌드는 디버그 오버헤드 제로. 마커/Profiler 는 최소 비용으로 유지.

## 구현 순서

1. `IDebugDraw` 추상 (Phase C-2 동반)
2. ImGui Physics Debugger 메인 창 뼈대
3. World / Rigid Body 탭 (간단 상태 표시)
4. DebugDraw 시각화 (AABB, Contact)
5. Constraint iteration 수렴 그래프
6. BVH 트리 시각화
7. Socket 탭 (Phase C-3 동반)
8. PBD 파티클 시각화
9. Physics Replay 녹화/재생
10. Determinism Checker (테스트 자동화)
