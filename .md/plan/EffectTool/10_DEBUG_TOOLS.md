# FX 디버그 도구 — ImGui Debugger + DebugDraw + Replay + Profiler

## 목표

FX 시스템을 **블랙박스로 두지 말 것**. ImGui 로 모든 설정 실시간 조작 + DebugDraw 로 공간 정보
표현 + Replay 로 버그 재현. CLAUDE.md "빌드 1번으로 모든 값 튜닝" 철학 준수.

## CLAUDE.md ImGui 정책 연동

"빌드 1번으로 모든 값 튜닝" — 튜닝 파라미터는 전부 ImGui 슬라이더 노출. 하드코딩 금지.
FX 는 특히 **게임 느낌의 85%** 를 결정하므로 에디터 중요도 최상.

## FX Debugger 메인 창

```
┌─ FX Debugger ──────────────────────────────────────────┐
│ [Pause: F6]  [Step: F7]  [Slow: ▶ 0.25x]               │
├────────────────────────────────────────────────────────┤
│ Tabs: [World] [Assets] [Instance] [Emitter] [Sim]      │
│       [Render] [Network] [Replay] [Profiler]           │
├────────────────────────────────────────────────────────┤
│ [선택한 탭 내용]                                        │
└────────────────────────────────────────────────────────┘
```

### World 탭

FX 월드 전역 상태.

```
[Time Scale]           1.00x             [slider 0~3]
[Global FX Enable]     [x]               [checkbox]
[Paused]               [ ]               [checkbox]

── FxSystem Resource Budget ─────────────────────
  Active FxInstance:  42 / 1024
  Active Emitter:    127 / 4096
  Total Particles:  8432 (CPU) + 0 (GPU)
  Dead Instance Q:    12

── Performance (last frame) ─────────────────────
  Total FX time  : 1.82 ms
    Sim          : 0.91 ms
    Sort         : 0.12 ms
    Pack (SoA→AoS): 0.29 ms
    Draw         : 0.50 ms
  Draw Calls     : 8
  Tri Count      : 67,456
```

### Assets 탭

로드된 FX 에셋 리스트 + 디스크 재로드 (Hot Reload).

```
┌─ FxAsset Library ──────────────────────────────┐
│ Filter: [Irelia___________]                    │
│ ┌─────────────────────────────────────────────┐│
│ │ Tag                 | Path            |Size ││
│ │ Irelia/Q_Hit        | FX/.../Q_Hit.fxg| 2KB ││
│ │ Irelia/Q_Trail      | FX/.../Trail.fxg| 3KB ││
│ │ Irelia/R_Blade      | FX/.../Blade.fxg|12KB ││
│ └─────────────────────────────────────────────┘│
│ [Reload Selected]  [Reload All]                │
│ [Open in Editor]   [Spawn at Camera]           │
└────────────────────────────────────────────────┘
```

`[Spawn at Camera]` 버튼 = 카메라 앞 2 미터에 해당 에셋 테스트 스폰. 이펙트 확인 용.

### Instance 탭

현재 살아있는 FxInstance 전체 리스트 + 선택.

```
ID  | Asset            | World Pos        | Age  | Alive
  1 | Irelia/Q_Hit     | (2.3, 0.0, 4.1) | 0.23 | 48
  2 | Irelia/Q_Trail   | follow #42       | 0.45 | 128
  3 | Map/BaronPit     | (-10, 5, 30)    | 12.3 | 200
```

선택 시 상세 (RightBottom 패널):

```
Instance #2  [Irelia/Q_Trail]
  Attached to Entity  : #42 (Irelia)
  World Matrix        : [table 4x4]
  Time Scale          : 1.0    [slider]
  [Force Kill]  [Teleport to Camera]  [Isolate]  [Pause]

  Emitters:
    Emitter #0  (parts = 85/1024)
    Emitter #1  (parts = 43/256)
```

**Isolate** 버튼 = 선택 인스턴스 외 전부 일시 Pause → 디버깅 시 한 FX 만 관찰.

### Emitter 탭

선택된 FxInstance 의 Emitter 별 상세.

```
Emitter #0 (Irelia/Q_Trail::main)
  Pool Alive: 85 / 1024
  ─────────────────────────────
  Attributes (avg values):
    Position:  (3.22, 0.13, 1.88)  spread(2.4, 0.4, 1.1)
    Velocity:  (0.00, 3.45, 0.00)  spread(0.1, 0.5, 0.1)
    Color:     avg(1.00, 0.60, 0.20, 0.80)
    Size:      0.18  spread 0.06
    Age:       0.28
    Lifetime:  0.95
  ─────────────────────────────
  [Show Particles as Points (DebugDraw)]
  [Show Velocity Vectors (DebugDraw)]
  [Dump Snapshot (CSV)]
```

`[Dump Snapshot]` = 현재 프레임 파티클 데이터 CSV 저장. Python / Excel 에서 분석.

### Sim 탭

Executor 컴파일 상태 + 스텝별 시간.

```
Compiled Graph Steps:
  System Stage  : 0 nodes
  Emitter Stage : 1 node  (EmitterDuration)
  Spawn Stage   : 5 nodes (Burst, Pos, Vel, Life, Color)
  Update Stage  : 4 nodes (Gravity, Drag, IntegratePos, AgeKill)

Per-Step Time (CPU, avg over 120 frames):
  Spawn Stage   : 0.021 ms
    SpawnBurst         : 0.003 ms
    InitPositionSphere : 0.006 ms
    InitVelocityCone   : 0.008 ms
    InitLifetime       : 0.002 ms
    InitColor          : 0.002 ms
  Update Stage  : 0.082 ms
    Gravity            : 0.008 ms
    Drag               : 0.015 ms
    IntegratePosition  : 0.012 ms
    AgeAndKill         : 0.047 ms  ← 핫스팟
```

핫스팟 탐지 용. AgeAndKill 이 느리면 kill 이 너무 자주 발생 (너무 짧은 lifetime) 의 신호.

### Render 탭

```
[Draw Calls]        8
[Total Instances]   8432
[Draw Mode]         [CPU Instancing ▼]    (or GPU Indirect)
[Blend Mode View]   [Additive overlay]

── Debug ──────────────────────────────────
[ ] Wireframe
[ ] Overdraw heatmap
[ ] Show AABB
[ ] Show Camera right/up axes
[ ] Disable blending (solid)
[ ] Disable texture (vertex color only)

Atlas in use: T_FxAtlas_01.dds  1024×1024
```

### Network 탭 (Phase 4 이후)

```
Last FxSpawn Packet  : 234 bytes  @ tick 12450
Pending Determinism Check: 0 discrepancies

Deterministic FX only   : [x]
Force visual FX remote  : [ ]   (서버가 클라이언트의 시각 FX 도 동기)
```

### Replay 탭

```
[▶ Record Start]  [⏸ Record Stop]  [⟲ Clear]
Duration: 12.3s     Size: 4.2 MB (gzip: 1.1 MB)
Save to: [./fx_replay_001.bin]   [Save]

── Load & Replay ───────────────────────────────
Load: [fx_replay_001.bin]   [Load]
Timeline: [──●────────] 3.45s / 12.00s
[▶] [⏸] [⏭ +1f] [⏮ -1f] [1.0x ▼]
```

### Profiler 탭

Profiler 마커 연동:

```
Marker Tree (last frame, ms):
  FX::Sim         ───────  0.91
    └ EmitterTick ─────    0.85
        └ ExecRun         0.82
  FX::Render      ─────    0.50
    ├ Sort         ───     0.12
    ├ Pack         ───     0.29
    └ Draw         ──      0.09
  FX::GC          ─        0.04   (dead instance 회수)
```

## DebugDraw 인터페이스

Phase C-2 DebugDraw 기반 활용:

```cpp
// FX 전용 DebugDraw 헬퍼
class CFxDebugDraw
{
public:
    static void DrawInstanceBox(FxInstanceID id, const Color& c);
    static void DrawEmitterParticlesAsPoints(const CEmitterInstance& em, const Color& c);
    static void DrawEmitterVelocityVectors(const CEmitterInstance& em, f32_t scaleMeter);
    static void DrawSpawnShape(const Node& n, const Mat4& worldMat);
    // InitPositionSphere → sphere, InitPositionBox → AABB 렌더
};
```

```cpp
void CFxDebugDraw::DrawEmitterParticlesAsPoints(
    const CEmitterInstance& em, const Color& c)
{
    const auto& pool = em.Pool();
    const auto* pos = pool.Data<Vec3>(Attr::Position);
    const std::uint32_t N = pool.AliveCount();
    for (std::uint32_t i = 0; i < N; ++i)
        IDebugDraw::Get()->DrawLine(pos[i], pos[i] + Vec3{0, 0.01f, 0}, c);
}
```

## Replay 시스템

### 녹화

매 프레임 FxInstance 상태 스냅샷 → 파일 저장.

```cpp
// Engine/Public/FX/Debug/FxRecorder.h
#pragma once
#include <fstream>
#include <vector>
#include "FxTypes.h"

namespace Engine::FX::Debug {

class CFxRecorder
{
public:
    static std::unique_ptr<CFxRecorder> Create(const std::wstring& path);
    ~CFxRecorder();

    void RecordFrame(const class CFxSystem& sys, f32_t simulationTime);
    void Flush();

private:
    struct InstanceSnapshot {
        FxInstanceID   id;
        FxAssetHandle  asset;
        Mat4           worldMatrix;
        std::uint32_t  totalParticles;
    };
    struct Frame {
        f32_t time;
        std::vector<InstanceSnapshot> instances;
    };

    std::ofstream m_file;        // 바이너리 + 프레임 헤더
    std::uint32_t m_frameCount = 0;
};

} // namespace Engine::FX::Debug
```

포맷: FlatBuffers 또는 커스텀 LEB128. 목표: 100 fps × 10 초 = 1000 프레임 < 10 MB.

### 재생

```cpp
class CFxReplayer
{
public:
    bool Load(const std::wstring& path);
    void Seek(f32_t time);
    void Play() / Pause() / Step();

    // 현재 프레임의 스냅샷으로 실제 FxSystem 을 덮어씌움
    void ApplyFrameToSystem(class CFxSystem& sys);

    f32_t GetDuration() const;
    f32_t GetCurrentTime() const;
};
```

## Hot Reload — `.fxg` 수정 → 즉시 반영

개발 생산성에 가장 중요한 기능. 에셋 파일을 저장하면 알아서 재로드.

```cpp
class CFxHotReloadWatcher
{
public:
    void Start(const std::wstring& rootDir);
    void Stop();

    // 매 프레임 폴링
    void Poll();

private:
    struct Watched {
        std::wstring   path;
        FxAssetHandle  handle;
        std::uint64_t  lastWriteTime;
    };
    std::vector<Watched> m_files;
};

void CFxHotReloadWatcher::Poll()
{
    for (auto& w : m_files) {
        // FindFirstFileW 로 lastWriteTime 조회
        WIN32_FILE_ATTRIBUTE_DATA attr;
        GetFileAttributesExW(w.path.c_str(), GetFileExInfoStandard, &attr);
        std::uint64_t now = (std::uint64_t(attr.ftLastWriteTime.dwHighDateTime) << 32)
                          | attr.ftLastWriteTime.dwLowDateTime;
        if (now != w.lastWriteTime) {
            // 재로드
            auto* asset = CFxAssetLibrary::Get().Resolve(w.handle);
            if (asset) {
                CFxAsset::LoadFromFileInto(w.path, *asset);
                // 활성 인스턴스 재컴파일 (그래프 바뀌었으므로)
                CGameInstance::Get()->Get_FxSystem()->RecompileUsers(w.handle);
            }
            w.lastWriteTime = now;
        }
    }
}
```

에디터 Ctrl+S → 1 초 내 게임 화면에 변경 반영. "빌드 1번으로 모든 값 튜닝" 의 극단.

## Determinism Checker

서버 권위 멀티에서 결정적 FX 가 실제로 일치하는지 검증:

```cpp
class CFxDeterminismChecker
{
public:
    // 기준 스냅샷을 기록 (서버 측 또는 마스터 클라)
    void RecordReference(const std::wstring& path,
                         const CFxSystem& sys,
                         std::uint32_t tickCount);

    // 재생 시 매 tick 비교
    bool CompareAgainstReference(const std::wstring& path,
                                 const CFxSystem& sys);

    // 차이 리포트
    struct Discrepancy {
        std::uint32_t tick;
        FxInstanceID  instance;
        std::uint32_t particleIdx;
        Vec3          expected;
        Vec3          actual;
    };
    const std::vector<Discrepancy>& GetDiscrepancies() const { return m_diffs; }

private:
    std::vector<Discrepancy> m_diffs;
};
```

CI 파이프라인에서 regression 검출 용.

## 단축키

| 키 | 기능 |
|---|---|
| F6 | FX Pause/Resume |
| F7 | FX Single Step (1 fixed timestep) |
| F8 | FX Slow Motion 토글 (0.25x) |
| Ctrl+F | FX Debugger 창 토글 |
| Ctrl+Shift+F | Scene_FxNodeEditor 진입 (Stage 6) |
| Ctrl+Shift+R | Replay 녹화 시작/중지 |
| Ctrl+Click (월드) | 해당 위치 FX 인스턴스 선택 |

**주의**: F12 는 Visual Studio Break 단축키 (CLAUDE.md Gotcha), 사용 금지.

## 빌드 플래그

```cpp
// Engine_Macro.h 에 추가
#ifdef _DEBUG
    #define FX_DEBUG_DRAW(expr) do { expr; } while(0)
    #define FX_PROFILE_SCOPE(name) Engine::Profiler::ScopedMarker _m(name)
#else
    #define FX_DEBUG_DRAW(expr) do { } while(0)
    #define FX_PROFILE_SCOPE(name) ((void)0)
#endif

#ifdef WINTERS_EDITOR
    #define FX_EDITOR_UI(expr) do { expr; } while(0)
#else
    #define FX_EDITOR_UI(expr) do { } while(0)
#endif
```

## 연습모드 통합

Scene_InGame 의 **연습모드** (Phase B 목표) 에선 FX Debugger 가 기본 표시.
플레이어가 스킬을 쓸 때마다:
1. 어떤 FxAsset 이 스폰됐는지 인스턴스 탭에서 자동 선택
2. Inspector 에 파라미터 즉시 편집 가능
3. 수정한 값을 `[Save to asset]` 버튼으로 디스크 반영

**에디터 UI = 게임 모드** 의 철학이 FX 에서 가장 강력하게 구현되는 지점.

## 로그 레벨

```cpp
enum class eFxLogLevel { Trace, Info, Warn, Error };

#define FX_LOG_INFO(fmt, ...)  Engine::Log::Write(eFxLogLevel::Info, fmt, __VA_ARGS__)
#define FX_LOG_WARN(fmt, ...)  Engine::Log::Write(eFxLogLevel::Warn, fmt, __VA_ARGS__)
#define FX_LOG_ERROR(fmt, ...) Engine::Log::Write(eFxLogLevel::Error, fmt, __VA_ARGS__)
```

사건별 로그:
- `FX_LOG_INFO("Spawned FxInstance #%u from asset '%s'", id, tag);`
- `FX_LOG_WARN("FxAsset '%s' has validation errors: %s", tag, msg);`
- `FX_LOG_ERROR("Failed to load FxAsset from %ws: %s", path, err);`

ImGui 에 Log 창으로 실시간 표시 + 파일 쓰기.

## 성능 경고

- 인스턴스 > 500: 경고
- 인스턴스 > 1024: 새 스폰 거부 (에디터 Replay 포함)
- 파티클 총합 > 50K (CPU) 또는 > 1M (GPU): 경고
- 한 프레임 FX 시간 > 3 ms: 경고
- 한 프레임 FxSpawn 호출 > 50: 경고 (스팸)

```cpp
void CFxDebugger::CheckBudget(const CFxSystem& sys)
{
    const auto stats = sys.GetStats();
    if (stats.activeInstances > 500)
        FX_LOG_WARN("FX instance count high: %u", stats.activeInstances);
    // ...
}
```

## 시각 테스트 케이스

디버거에 "Test Scenes" 드롭다운:

```
[Test: Isolate one instance]
[Test: 1000 particles stress]
[Test: Determinism — same seed]
[Test: Hot reload file change]
[Test: Network simulated packet loss]
```

각 버튼이 미리 정의된 시나리오를 실행. 버그 재현성 확보.

## 구현 순서

1. `CFxDebugger` 메인 창 뼈대 (탭 네비게이션)
2. World 탭 — 시간 제어 + 성능 지표
3. Assets 탭 — 에셋 리스트 + Spawn at Camera 버튼
4. Instance 탭 — 살아있는 인스턴스 리스트
5. Emitter 탭 — 선택 인스턴스의 이미터 상세
6. DebugDraw 시각화 (파티클 포인트, velocity arrow)
7. Sim 탭 — 노드별 프로파일 (PROFILE_SCOPE 매크로 연동)
8. Render 탭 — 와이어프레임 / 오버드로 디버그
9. Hot Reload Watcher
10. Replay 녹화 / 재생
11. Determinism Checker
12. 연습모드 통합 (Scene_InGame 에 Debugger 상시 노출)

## 다음 단계

디버그 도구까지 구현이 끝나면 **Phase G 전체 1 사이클** 완성.
추후 확장은 각 Stage 문서의 "다음" 섹션 참고.

### 장기 확장 (Phase G.2 예정)

- Ribbon / Trail 렌더러 (궤적)
- Static Mesh Particle (파편)
- GPU Collision (Phase D BVH 공유)
- 서브 이미터 (파티클 → 새 파티클 스폰)
- Force Field 노드 (중력정, 소용돌이)
- Spatial Query 노드 (가까운 적 찾기)
- Decal / Projector 통합

### Phase H 후보 (연속 시스템)

- Shader Graph (머티리얼 노드 그래프) — FX 노드 에디터 재사용
- Animation Graph (HFSM + BT + 노드)
- Behavior Tree 에디터 — Phase F AI 봇 공통 인프라
