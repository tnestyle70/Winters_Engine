# Stage 3 — Executor + 표준 노드 구현

## 목표

`FxGraph` 를 **실행 스텝 리스트** 로 컴파일하고, 매 프레임 4 스테이지 (System / Emitter / Spawn / Update)
순서로 실행한다. 표준 노드 9 종을 구현해서 **첫 불꽃 데모** 를 가동.

## 왜 스텝 리스트로 컴파일하는가

매 프레임 그래프를 위상 정렬하면:
- 해시맵 조회 비용
- 엣지 탐색
- stage 필터링

전부 반복된다. **한 번 컴파일** 해서 함수 포인터 배열로 풀어두면 프레임마다 for-loop + 함수 호출만 남는다.
Niagara 의 VectorVM 도 같은 사상 — 그래프는 **설계 시간**, 런타임은 **평탄한 바이트코드**.

## ExecContext

```cpp
// Engine/Public/FX/Executor/FxExecContext.h
#pragma once
#include "FxTypes.h"
#include "ParticlePool.h"

namespace Engine::FX {

struct FxExecContext
{
    CParticlePool* pool = nullptr;

    f32_t deltaTime   = 0.f;
    f32_t emitterAge  = 0.f;        // 이미터 생성 후 경과 시간

    // Spawn 스테이지에서 설정되는 신규 파티클 범위
    std::uint32_t spawnRangeBegin = 0;
    std::uint32_t spawnRangeEnd   = 0;

    // per-instance xorshift32 상태 (결정성 경로)
    std::uint32_t rngState = 0x9E3779B9u;

    // 판정 FX 여부 (Asset 의 m_bDeterministic 이 흘러옴)
    bool_t bDeterministic = false;

    // 외부 전달 (Transform, 카메라 위치 등 — 확장 여지)
    Mat4 emitterWorldMatrix = Mat4::Identity();
};

// 노드 실행 함수 시그니처
using NodeExecFn = void(*)(const class Node& node, FxExecContext& ctx);

} // namespace Engine::FX
```

## Executor

```cpp
// Engine/Public/FX/Executor/FxExecutor.h
#pragma once
#include "FxGraph.h"
#include "FxExecContext.h"

namespace Engine::FX {

class CFxExecutor
{
public:
    static std::unique_ptr<CFxExecutor> Create() { return std::unique_ptr<CFxExecutor>(new CFxExecutor()); }
    ~CFxExecutor() = default;

    // 그래프를 컴파일 (위상 정렬 + 함수 포인터 바인딩)
    bool Compile(const CFxGraph& graph);

    // 스테이지별 실행
    void RunSystem (FxExecContext& ctx);
    void RunEmitter(FxExecContext& ctx);
    void RunSpawn  (FxExecContext& ctx);   // spawnRange 만
    void RunUpdate (FxExecContext& ctx);   // 전체 alive

    // 컴파일된 스텝 수 (디버그)
    std::uint32_t GetStepCount(eStage s) const;

private:
    CFxExecutor() = default;

    struct Step {
        Node       node;       // 파라미터 포함한 복사본
        NodeExecFn fn;
    };

    std::vector<Step> m_systemSteps;
    std::vector<Step> m_emitterSteps;
    std::vector<Step> m_spawnSteps;
    std::vector<Step> m_updateSteps;
};

} // namespace Engine::FX
```

### 구현

```cpp
// Engine/Private/FX/Executor/FxExecutor.cpp
#include "FxExecutor.h"
#include "FxTopoSort.h"
#include "NodeRegistry.h"

namespace Engine::FX {

bool CFxExecutor::Compile(const CFxGraph& graph)
{
    m_systemSteps.clear();
    m_emitterSteps.clear();
    m_spawnSteps.clear();
    m_updateSteps.clear();

    auto compileStage = [&](eStage s, std::vector<Step>& out) -> bool
    {
        std::vector<NodeId> order;
        if (!TopologicalSort(graph, s, order)) return false;
        for (NodeId id : order) {
            const Node& n = graph.Get(id);
            Step st;
            st.node = n;                       // 복사 (파라미터 포함)
            st.fn   = NodeRegistry::Resolve(n.kind);
            if (st.fn == nullptr) continue;    // 미구현 노드는 skip
            out.push_back(std::move(st));
        }
        return true;
    };

    if (!compileStage(eStage::System,  m_systemSteps))  return false;
    if (!compileStage(eStage::Emitter, m_emitterSteps)) return false;
    if (!compileStage(eStage::Spawn,   m_spawnSteps))   return false;
    if (!compileStage(eStage::Update,  m_updateSteps))  return false;
    return true;
}

void CFxExecutor::RunSystem (FxExecContext& c) { for (auto& s : m_systemSteps)  s.fn(s.node, c); }
void CFxExecutor::RunEmitter(FxExecContext& c) { for (auto& s : m_emitterSteps) s.fn(s.node, c); }
void CFxExecutor::RunSpawn  (FxExecContext& c) { for (auto& s : m_spawnSteps)   s.fn(s.node, c); }
void CFxExecutor::RunUpdate (FxExecContext& c) { for (auto& s : m_updateSteps)  s.fn(s.node, c); }

std::uint32_t CFxExecutor::GetStepCount(eStage s) const
{
    switch (s) {
        case eStage::System:  return static_cast<std::uint32_t>(m_systemSteps.size());
        case eStage::Emitter: return static_cast<std::uint32_t>(m_emitterSteps.size());
        case eStage::Spawn:   return static_cast<std::uint32_t>(m_spawnSteps.size());
        case eStage::Update:  return static_cast<std::uint32_t>(m_updateSteps.size());
    }
    return 0;
}

} // namespace Engine::FX
```

## NodeRegistry

`eNodeKind → NodeExecFn` 매핑. 컴파일 타임 상수 테이블보다 switch 로 시작:

```cpp
// Engine/Public/FX/Executor/NodeRegistry.h
#pragma once
#include "FxExecContext.h"

namespace Engine::FX::NodeRegistry {

NodeExecFn Resolve(eNodeKind kind);

// 노드 구현이 자기를 등록할 때 쓰는 헬퍼 (Stage 10 챔피언 self-register 패턴과 유사)
void RegisterNode(eNodeKind kind, NodeExecFn fn);

} // namespace Engine::FX::NodeRegistry
```

```cpp
// Engine/Private/FX/Executor/NodeRegistry.cpp
#include "NodeRegistry.h"
#include <unordered_map>

namespace Engine::FX::NodeRegistry {

static std::unordered_map<eNodeKind, NodeExecFn> s_map;

NodeExecFn Resolve(eNodeKind kind)
{
    auto it = s_map.find(kind);
    return it != s_map.end() ? it->second : nullptr;
}

void RegisterNode(eNodeKind kind, NodeExecFn fn)
{
    s_map[kind] = fn;
}

} // namespace Engine::FX::NodeRegistry
```

## 난수 — xorshift32 + 결정성 경로

```cpp
// Engine/Public/FX/Executor/FxRandom.h
#pragma once
#include <cstdint>

namespace Engine::FX {

inline std::uint32_t Xorshift32(std::uint32_t& state)
{
    std::uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x <<  5;
    state = x;
    return x;
}

inline f32_t RandFloat01(std::uint32_t& state)
{
    return (Xorshift32(state) & 0xFFFFFFu) / f32_t(0x1000000);
}

inline f32_t RandRange(std::uint32_t& state, f32_t a, f32_t b)
{
    return a + (b - a) * RandFloat01(state);
}

inline Vec3 RandUnitSphere(std::uint32_t& state)
{
    // cos theta uniform in [-1, 1], phi uniform in [0, 2π]
    const f32_t z   = RandRange(state, -1.f, 1.f);
    const f32_t r   = std::sqrt(std::max(0.f, 1.f - z * z));
    const f32_t phi = RandRange(state, 0.f, 6.283185307f);
    return { r * std::cos(phi), r * std::sin(phi), z };
}

} // namespace Engine::FX
```

**결정적 FX** 는 `ctx.rngState` 를 서버에서 받은 seed 로 초기화. Xorshift 는 같은 seed → 같은 수열.

## 표준 노드 구현

모두 `FxNodeImpl.cpp` 한 파일에 모음 (static 함수 9 개). 정적 등록은 `InitStandardNodes()` 로.

```cpp
// Engine/Private/FX/Executor/FxNodeImpl.cpp
#include "FxGraph.h"
#include "FxExecContext.h"
#include "FxAttributeRegistry.h"
#include "FxRandom.h"
#include "NodeRegistry.h"
#include <cmath>

using namespace Engine::FX;

// ═══════════════════════════════════════════════════
// Spawn
// ═══════════════════════════════════════════════════

// SpawnBurst: 이미터 생성 후 첫 프레임에만 N 개 할당
static void ExecSpawnBurst(const Node& n, FxExecContext& c)
{
    const std::int32_t count = std::get<std::int32_t>(n.params.at("count"));
    if (c.emitterAge > c.deltaTime) return;   // 첫 프레임만

    std::uint32_t actual = 0;
    const std::uint32_t begin = c.pool->Allocate(static_cast<std::uint32_t>(count), &actual);
    c.spawnRangeBegin = begin;
    c.spawnRangeEnd   = begin + actual;
}

// SpawnRate: 초당 rate 개 균등 생성. 소수점 누적은 emitter state 가 필요하나
// Phase 1 MVP 는 매 프레임 정수 할당 + 잔여 누적을 ctx.pool 외부의 per-emitter state 로 위임
// (임시로 ctx 에 accumulator 필드 추가 고려)
static void ExecSpawnRate(const Node& n, FxExecContext& c)
{
    const f32_t rate = std::get<f32_t>(n.params.at("rate"));
    // 간이 구현: round-to-int (잔여는 누락되나 MVP 는 허용)
    const std::int32_t count = static_cast<std::int32_t>(rate * c.deltaTime);
    if (count <= 0) return;

    std::uint32_t actual = 0;
    const std::uint32_t begin = c.pool->Allocate(static_cast<std::uint32_t>(count), &actual);
    c.spawnRangeBegin = begin;
    c.spawnRangeEnd   = begin + actual;
}

// ═══════════════════════════════════════════════════
// Initialize
// ═══════════════════════════════════════════════════

static void ExecInitPositionPoint(const Node&, FxExecContext& c)
{
    auto* pos = c.pool->Data<Vec3>(Attr::Position);
    for (std::uint32_t i = c.spawnRangeBegin; i < c.spawnRangeEnd; ++i)
        pos[i] = { 0.f, 0.f, 0.f };
}

static void ExecInitPositionSphere(const Node& n, FxExecContext& c)
{
    const f32_t radius = std::get<f32_t>(n.params.at("radius"));
    auto* pos = c.pool->Data<Vec3>(Attr::Position);
    for (std::uint32_t i = c.spawnRangeBegin; i < c.spawnRangeEnd; ++i) {
        const Vec3 dir = RandUnitSphere(c.rngState);
        const f32_t r  = radius * std::cbrt(RandFloat01(c.rngState));  // 구 내부 균일
        pos[i] = { dir.x * r, dir.y * r, dir.z * r };
    }
}

static void ExecInitPositionBox(const Node& n, FxExecContext& c)
{
    const Vec3 halfExtent = std::get<Vec3>(n.params.at("halfExtent"));
    auto* pos = c.pool->Data<Vec3>(Attr::Position);
    for (std::uint32_t i = c.spawnRangeBegin; i < c.spawnRangeEnd; ++i) {
        pos[i] = {
            RandRange(c.rngState, -halfExtent.x, halfExtent.x),
            RandRange(c.rngState, -halfExtent.y, halfExtent.y),
            RandRange(c.rngState, -halfExtent.z, halfExtent.z)
        };
    }
}

static void ExecInitVelocityCone(const Node& n, FxExecContext& c)
{
    const Vec3  axis    = std::get<Vec3>(n.params.at("direction"));
    const f32_t halfAng = std::get<f32_t>(n.params.at("coneAngleRad"));
    const f32_t speedMn = std::get<f32_t>(n.params.at("speedMin"));
    const f32_t speedMx = std::get<f32_t>(n.params.at("speedMax"));

    auto* vel = c.pool->Data<Vec3>(Attr::Velocity);

    // axis 정규화. 기본 +Y 기준에서 Rodrigues 회전 준비 (간이 방법: two-axis frame)
    const Vec3 A = axis.Normalized();
    Vec3 ortho = (std::abs(A.x) < 0.9f) ? Vec3{1,0,0} : Vec3{0,1,0};
    const Vec3 B = Vec3::Cross(A, ortho).Normalized();
    const Vec3 C = Vec3::Cross(A, B);

    const f32_t cosHalf = std::cos(halfAng);

    for (std::uint32_t i = c.spawnRangeBegin; i < c.spawnRangeEnd; ++i) {
        const f32_t z   = RandRange(c.rngState, cosHalf, 1.f);    // cone 내부
        const f32_t r   = std::sqrt(std::max(0.f, 1.f - z * z));
        const f32_t phi = RandRange(c.rngState, 0.f, 6.283185307f);
        const f32_t cx  = r * std::cos(phi);
        const f32_t cy  = r * std::sin(phi);
        // 로컬 (B, C, A) 프레임 → 월드
        const Vec3 dir = {
            B.x * cx + C.x * cy + A.x * z,
            B.y * cx + C.y * cy + A.y * z,
            B.z * cx + C.z * cy + A.z * z
        };
        const f32_t sp = RandRange(c.rngState, speedMn, speedMx);
        vel[i] = { dir.x * sp, dir.y * sp, dir.z * sp };
    }
}

static void ExecInitVelocityInDir(const Node& n, FxExecContext& c)
{
    const Vec3 dir = std::get<Vec3>(n.params.at("direction"));
    const f32_t sp = std::get<f32_t>(n.params.at("speed"));
    auto* vel = c.pool->Data<Vec3>(Attr::Velocity);
    for (std::uint32_t i = c.spawnRangeBegin; i < c.spawnRangeEnd; ++i)
        vel[i] = { dir.x * sp, dir.y * sp, dir.z * sp };
}

static void ExecInitLifetime(const Node& n, FxExecContext& c)
{
    const f32_t mn = std::get<f32_t>(n.params.at("min"));
    const f32_t mx = std::get<f32_t>(n.params.at("max"));
    auto* life = c.pool->Data<f32_t>(Attr::Lifetime);
    auto* age  = c.pool->Data<f32_t>(Attr::Age);
    for (std::uint32_t i = c.spawnRangeBegin; i < c.spawnRangeEnd; ++i) {
        life[i] = RandRange(c.rngState, mn, mx);
        age[i]  = 0.f;
    }
}

static void ExecInitColor(const Node& n, FxExecContext& c)
{
    const Vec4 color = std::get<Vec4>(n.params.at("color"));
    auto* col = c.pool->Data<Vec4>(Attr::Color);
    for (std::uint32_t i = c.spawnRangeBegin; i < c.spawnRangeEnd; ++i)
        col[i] = color;
}

static void ExecInitSize(const Node& n, FxExecContext& c)
{
    const f32_t mn = std::get<f32_t>(n.params.at("min"));
    const f32_t mx = std::get<f32_t>(n.params.at("max"));
    auto* size = c.pool->Data<f32_t>(Attr::Size);
    for (std::uint32_t i = c.spawnRangeBegin; i < c.spawnRangeEnd; ++i)
        size[i] = RandRange(c.rngState, mn, mx);
}

// ═══════════════════════════════════════════════════
// Update
// ═══════════════════════════════════════════════════

static void ExecUpdateGravity(const Node& n, FxExecContext& c)
{
    const Vec3 g = std::get<Vec3>(n.params.at("gravity"));
    const f32_t dt = c.deltaTime;
    auto* vel = c.pool->Data<Vec3>(Attr::Velocity);
    const std::uint32_t N = c.pool->AliveCount();
    for (std::uint32_t i = 0; i < N; ++i) {
        vel[i].x += g.x * dt;
        vel[i].y += g.y * dt;
        vel[i].z += g.z * dt;
    }
}

static void ExecUpdateDrag(const Node& n, FxExecContext& c)
{
    const f32_t k = std::get<f32_t>(n.params.at("drag"));
    // exp(-k dt) — 지수 감쇠 (frame-rate independent)
    const f32_t factor = std::exp(-k * c.deltaTime);
    auto* vel = c.pool->Data<Vec3>(Attr::Velocity);
    const std::uint32_t N = c.pool->AliveCount();
    for (std::uint32_t i = 0; i < N; ++i) {
        vel[i].x *= factor;
        vel[i].y *= factor;
        vel[i].z *= factor;
    }
}

// Curl Noise (Bridson 2007) — 3D 벡터필드에서 비압축성 속도 교란
// Phase 1 MVP: 간이 2D 펄린 유도. 풀 구현은 Phase 2.
static void ExecUpdateCurlNoise(const Node& n, FxExecContext& c)
{
    const f32_t strength = std::get<f32_t>(n.params.at("strength"));
    const f32_t scale    = std::get<f32_t>(n.params.at("scale"));
    const f32_t dt       = c.deltaTime;

    auto* pos = c.pool->Data<Vec3>(Attr::Position);
    auto* vel = c.pool->Data<Vec3>(Attr::Velocity);
    const std::uint32_t N = c.pool->AliveCount();

    auto hash = [](f32_t x, f32_t y, f32_t z) {
        const std::uint32_t h = std::uint32_t(x * 73856093) ^
                                std::uint32_t(y * 19349663) ^
                                std::uint32_t(z * 83492791);
        return (h & 0xFFFFFFu) / f32_t(0x1000000) * 2.f - 1.f;
    };

    for (std::uint32_t i = 0; i < N; ++i) {
        // 임시 간이 curl 근사 (진짜 curl 은 유도 편미분 2 회 필요)
        const f32_t cx = hash(pos[i].y * scale, pos[i].z * scale, 0.f);
        const f32_t cy = hash(pos[i].z * scale, pos[i].x * scale, 1.f);
        const f32_t cz = hash(pos[i].x * scale, pos[i].y * scale, 2.f);
        vel[i].x += cx * strength * dt;
        vel[i].y += cy * strength * dt;
        vel[i].z += cz * strength * dt;
    }
}

// Color-over-life: keyframe 선형 보간
struct ColorKey { f32_t t; Vec4 color; };
static void ExecUpdateColorOverLife(const Node& n, FxExecContext& c)
{
    // 파라미터는 JSON 에서 배열로 들어옴. 런타임은 정렬된 std::vector<ColorKey> 가정.
    // FxGraphSerializer 가 Node 로드 시 params["keyframes"] 를 vector<ColorKey> 로 파싱.
    const auto* keys = std::any_cast<std::vector<ColorKey>>(&n.params.at("keyframes_parsed"));
    if (!keys || keys->empty()) return;

    auto* col  = c.pool->Data<Vec4>(Attr::Color);
    auto* age  = c.pool->Data<f32_t>(Attr::Age);
    auto* life = c.pool->Data<f32_t>(Attr::Lifetime);
    const std::uint32_t N = c.pool->AliveCount();

    for (std::uint32_t i = 0; i < N; ++i) {
        const f32_t t = (life[i] > 1e-6f) ? (age[i] / life[i]) : 1.f;
        // 이진 탐색 생략 (보통 2~4 키). 선형 스캔.
        std::size_t k = 0;
        for (; k + 1 < keys->size(); ++k)
            if (t < (*keys)[k + 1].t) break;

        const ColorKey& a = (*keys)[k];
        const ColorKey& b = (*keys)[std::min(k + 1, keys->size() - 1)];
        const f32_t range = std::max(b.t - a.t, 1e-6f);
        const f32_t u = std::clamp((t - a.t) / range, 0.f, 1.f);
        col[i].x = a.color.x + (b.color.x - a.color.x) * u;
        col[i].y = a.color.y + (b.color.y - a.color.y) * u;
        col[i].z = a.color.z + (b.color.z - a.color.z) * u;
        col[i].w = a.color.w + (b.color.w - a.color.w) * u;
    }
}

static void ExecUpdateSizeOverLife(const Node& n, FxExecContext& c)
{
    const f32_t startScale = std::get<f32_t>(n.params.at("startScale"));
    const f32_t endScale   = std::get<f32_t>(n.params.at("endScale"));
    auto* size = c.pool->Data<f32_t>(Attr::Size);
    auto* age  = c.pool->Data<f32_t>(Attr::Age);
    auto* life = c.pool->Data<f32_t>(Attr::Lifetime);
    const std::uint32_t N = c.pool->AliveCount();

    // baseSize 는 InitSize 에서 들어온 값이므로, size *= lerp(startScale, endScale, t)
    // 하지만 base 를 잃어버리면 누적 바뀜 → InitSize 를 "base" 별도 attr 로 둘지 고민.
    // Phase 1 MVP: 절대값 기반 — size = lerp(startScale, endScale, t)
    for (std::uint32_t i = 0; i < N; ++i) {
        const f32_t t = (life[i] > 1e-6f) ? (age[i] / life[i]) : 1.f;
        size[i] = startScale + (endScale - startScale) * t;
    }
}

// IntegratePosition: pos += vel * dt. 별도 노드로 둬서 Gravity/Drag 이후 순서 강제.
static void ExecUpdateIntegratePosition(const Node&, FxExecContext& c)
{
    const f32_t dt = c.deltaTime;
    auto* pos = c.pool->Data<Vec3>(Attr::Position);
    auto* vel = c.pool->Data<Vec3>(Attr::Velocity);
    const std::uint32_t N = c.pool->AliveCount();
    for (std::uint32_t i = 0; i < N; ++i) {
        pos[i].x += vel[i].x * dt;
        pos[i].y += vel[i].y * dt;
        pos[i].z += vel[i].z * dt;
    }
}

// AgeAndKill: age += dt, age >= life 면 swap-back kill. ★ 뒤에서부터 순회 필수
static void ExecUpdateAgeAndKill(const Node&, FxExecContext& c)
{
    const f32_t dt = c.deltaTime;
    auto* age  = c.pool->Data<f32_t>(Attr::Age);
    auto* life = c.pool->Data<f32_t>(Attr::Lifetime);

    for (std::int32_t i = std::int32_t(c.pool->AliveCount()) - 1; i >= 0; --i) {
        age[i] += dt;
        if (age[i] >= life[i])
            c.pool->Kill(std::uint32_t(i));
    }
}

// ═══════════════════════════════════════════════════
// Registration
// ═══════════════════════════════════════════════════

namespace Engine::FX::StandardNodes {

void InitStandardNodes()
{
    using namespace NodeRegistry;
    RegisterNode(eNodeKind::SpawnBurst,              ExecSpawnBurst);
    RegisterNode(eNodeKind::SpawnRate,               ExecSpawnRate);

    RegisterNode(eNodeKind::InitPositionPoint,       ExecInitPositionPoint);
    RegisterNode(eNodeKind::InitPositionSphere,      ExecInitPositionSphere);
    RegisterNode(eNodeKind::InitPositionBox,         ExecInitPositionBox);
    RegisterNode(eNodeKind::InitVelocityCone,        ExecInitVelocityCone);
    RegisterNode(eNodeKind::InitVelocityInDir,       ExecInitVelocityInDir);
    RegisterNode(eNodeKind::InitLifetime,            ExecInitLifetime);
    RegisterNode(eNodeKind::InitColor,               ExecInitColor);
    RegisterNode(eNodeKind::InitSize,                ExecInitSize);

    RegisterNode(eNodeKind::UpdateGravity,           ExecUpdateGravity);
    RegisterNode(eNodeKind::UpdateDrag,              ExecUpdateDrag);
    RegisterNode(eNodeKind::UpdateCurlNoise,         ExecUpdateCurlNoise);
    RegisterNode(eNodeKind::UpdateColorOverLife,     ExecUpdateColorOverLife);
    RegisterNode(eNodeKind::UpdateSizeOverLife,      ExecUpdateSizeOverLife);
    RegisterNode(eNodeKind::UpdateIntegratePosition, ExecUpdateIntegratePosition);
    RegisterNode(eNodeKind::UpdateAgeAndKill,        ExecUpdateAgeAndKill);
}

} // namespace Engine::FX::StandardNodes
```

`InitStandardNodes()` 는 엔진 Initialize 타이밍에 **1 회** 호출 (`CGameInstance::Initialize_Engine()`).

## EmitterInstance — 한 프레임 루프

```cpp
// Engine/Public/FX/Instance/EmitterInstance.h
#pragma once
#include "FxExecutor.h"

namespace Engine::FX {

class CEmitterInstance
{
public:
    static std::unique_ptr<CEmitterInstance> Create(const CFxGraph& graph,
                                                    std::uint32_t capacity,
                                                    std::uint32_t seed,
                                                    bool bDeterministic);

    void Tick(f32_t dt);

    bool IsAlive() const { return m_pool->AliveCount() > 0 || m_emitterAge < m_maxEmitterLifetime; }
    CParticlePool& Pool() { return *m_pool; }

    void SetWorldMatrix(const Mat4& m) { m_worldMatrix = m; }
    void SetMaxEmitterLifetime(f32_t sec) { m_maxEmitterLifetime = sec; }

private:
    CEmitterInstance() = default;

    std::unique_ptr<CParticlePool> m_pool;
    std::unique_ptr<CFxExecutor>   m_executor;

    f32_t         m_emitterAge         = 0.f;
    f32_t         m_maxEmitterLifetime = 3.f;     // 이후는 spawn 중지만
    std::uint32_t m_rngState           = 0;
    bool_t        m_bDeterministic     = false;
    Mat4          m_worldMatrix        = Mat4::Identity();
};

} // namespace Engine::FX
```

```cpp
// Engine/Private/FX/Instance/EmitterInstance.cpp
#include "EmitterInstance.h"
#include "FxAttributeRegistry.h"

namespace Engine::FX {

std::unique_ptr<CEmitterInstance>
CEmitterInstance::Create(const CFxGraph& graph,
                         std::uint32_t capacity,
                         std::uint32_t seed,
                         bool bDeterministic)
{
    auto inst = std::unique_ptr<CEmitterInstance>(new CEmitterInstance());
    inst->m_pool = CParticlePool::Create();
    RegisterDefaultAttributes(*inst->m_pool);
    // 노드가 요구하는 추가 attribute 는 graph 를 스캔해서 등록 (생략)
    inst->m_pool->Reserve(capacity);

    inst->m_executor = CFxExecutor::Create();
    if (!inst->m_executor->Compile(graph))
        return nullptr;                     // 사이클 / 미구현 노드

    inst->m_rngState       = seed == 0 ? 0x9E3779B9u : seed;
    inst->m_bDeterministic = bDeterministic;
    return inst;
}

void CEmitterInstance::Tick(f32_t dt)
{
    m_emitterAge += dt;

    FxExecContext ctx;
    ctx.pool               = m_pool.get();
    ctx.deltaTime          = dt;
    ctx.emitterAge         = m_emitterAge;
    ctx.rngState           = m_rngState;
    ctx.bDeterministic     = m_bDeterministic;
    ctx.emitterWorldMatrix = m_worldMatrix;
    ctx.spawnRangeBegin    = ctx.spawnRangeEnd = m_pool->AliveCount();

    m_executor->RunSystem (ctx);
    m_executor->RunEmitter(ctx);

    // Emitter lifetime 넘기면 spawn 중지 (파티클은 수명 다할 때까지 계속 갱신)
    if (m_emitterAge < m_maxEmitterLifetime)
        m_executor->RunSpawn(ctx);

    m_executor->RunUpdate(ctx);

    m_rngState = ctx.rngState;
}

} // namespace Engine::FX
```

## FxInstance — System 레벨 (이미터 N 개 합성)

```cpp
// Engine/Public/FX/Instance/FxInstance.h
class CFxInstance
{
public:
    static std::unique_ptr<CFxInstance> CreateFromAsset(const CFxAsset& asset,
                                                        const Vec3& worldPos,
                                                        std::uint32_t seed);

    void Tick(f32_t dt);
    void SetWorldMatrix(const Mat4& m);
    bool IsAlive() const;

    const std::vector<std::unique_ptr<CEmitterInstance>>& Emitters() const { return m_emitters; }

private:
    CFxInstance() = default;
    std::vector<std::unique_ptr<CEmitterInstance>> m_emitters;
    Mat4 m_worldMatrix = Mat4::Identity();
    FxAssetHandle m_assetHandle = 0;
};
```

한 에셋에 이미터 N 개 포함 (아직 Phase 1 MVP 는 이미터 1 개 전용으로 시작해도 OK).

## 의사 FxInstancePool — 재활용

한타 씬 = 수백 FX 스폰. `new / delete` 대신 풀:

```cpp
class CFxInstancePool
{
public:
    static CFxInstancePool& Get() { static CFxInstancePool p; return p; }

    FxInstanceID Acquire();
    void         Release(FxInstanceID id);

    CFxInstance* Resolve(FxInstanceID id);

private:
    std::vector<std::unique_ptr<CFxInstance>> m_slots;
    std::vector<FxInstanceID> m_freeList;
};
```

## 첫 데모 — "불꽃 터지기"

```cpp
// Client/Private/Scene/Scene_InGame.cpp (데모 코드)

void Scene_InGame::OnEnter_FxDemo()
{
    using namespace Engine::FX;
    StandardNodes::InitStandardNodes();   // 1 회

    auto g = CFxGraph::Create();

    // Spawn stage
    NodeId n1 = g->AddNode(eNodeKind::SpawnBurst, eStage::Spawn);
    g->GetMutable(n1).params["count"] = std::int32_t(50);

    NodeId n2 = g->AddNode(eNodeKind::InitPositionSphere, eStage::Spawn);
    g->GetMutable(n2).params["radius"] = 0.2f;

    NodeId n3 = g->AddNode(eNodeKind::InitVelocityCone, eStage::Spawn);
    g->GetMutable(n3).params["direction"]    = Vec3{0, 1, 0};
    g->GetMutable(n3).params["coneAngleRad"] = 0.6f;
    g->GetMutable(n3).params["speedMin"]     = 2.0f;
    g->GetMutable(n3).params["speedMax"]     = 5.0f;

    NodeId n4 = g->AddNode(eNodeKind::InitLifetime, eStage::Spawn);
    g->GetMutable(n4).params["min"] = 0.8f;
    g->GetMutable(n4).params["max"] = 1.5f;

    NodeId n5 = g->AddNode(eNodeKind::InitColor, eStage::Spawn);
    g->GetMutable(n5).params["color"] = Vec4{1.f, 0.6f, 0.2f, 1.f};

    NodeId n6 = g->AddNode(eNodeKind::InitSize, eStage::Spawn);
    g->GetMutable(n6).params["min"] = 0.1f;
    g->GetMutable(n6).params["max"] = 0.25f;

    // Update stage
    NodeId u1 = g->AddNode(eNodeKind::UpdateGravity, eStage::Update);
    g->GetMutable(u1).params["gravity"] = Vec3{0, -2.f, 0};

    NodeId u2 = g->AddNode(eNodeKind::UpdateDrag, eStage::Update);
    g->GetMutable(u2).params["drag"] = 0.5f;

    NodeId u3 = g->AddNode(eNodeKind::UpdateIntegratePosition, eStage::Update);
    NodeId u4 = g->AddNode(eNodeKind::UpdateAgeAndKill, eStage::Update);

    // 의존성 엣지
    g->Connect(n1, 1, n2, 1);
    g->Connect(n2, 1, n3, 1);
    g->Connect(n3, 1, n4, 1);
    g->Connect(n4, 1, n5, 1);
    g->Connect(n5, 1, n6, 1);
    g->Connect(u1, 1, u2, 1);
    g->Connect(u2, 1, u3, 1);
    g->Connect(u3, 1, u4, 1);

    m_demoEmitter = CEmitterInstance::Create(*g, 4096, 0xDEADBEEFu, false);
}

void Scene_InGame::OnUpdate_FxDemo(f32_t dt)
{
    if (m_demoEmitter)
        m_demoEmitter->Tick(dt);
}
```

이 시점에서는 **렌더러 없음** → 파티클 배열만 갱신됨. Stage 5 에서 DX11 인스턴싱 추가하면 화면에 뿌려진다.
대신 ImGui 로 `pool.AliveCount()` 를 그려서 진행 확인:

```cpp
void Scene_InGame::OnImGui_FxDemo()
{
    ImGui::Begin("FX Demo");
    if (m_demoEmitter) {
        ImGui::Text("Alive particles: %u", m_demoEmitter->Pool().AliveCount());
        ImGui::Text("Capacity: %u",        m_demoEmitter->Pool().Capacity());
    }
    ImGui::End();
}
```

## 단위 테스트

```cpp
TEST(FxExecutor, SpawnBurst_FillsRange)
{
    StandardNodes::InitStandardNodes();

    auto g = CFxGraph::Create();
    NodeId a = g->AddNode(eNodeKind::SpawnBurst, eStage::Spawn);
    g->GetMutable(a).params["count"] = std::int32_t(10);
    NodeId b = g->AddNode(eNodeKind::InitLifetime, eStage::Spawn);
    g->GetMutable(b).params["min"] = 1.f;
    g->GetMutable(b).params["max"] = 1.f;
    g->Connect(a, 1, b, 1);

    auto e = CEmitterInstance::Create(*g, 100, 1u, false);
    ASSERT_NE(e, nullptr);
    e->Tick(0.016f);
    EXPECT_EQ(e->Pool().AliveCount(), 10u);
}

TEST(FxExecutor, AgeAndKill_RemovesExpired)
{
    StandardNodes::InitStandardNodes();
    auto g = CFxGraph::Create();
    g->AddNode(eNodeKind::SpawnBurst, eStage::Spawn);    // count=5
    // (params 주입 생략)
    auto e = CEmitterInstance::Create(*g, 10, 1u, false);

    // 가짜로 파티클 넣고 수명 0.01 설정 후 1 tick
    // (직접 Pool 조작)
    e->Pool().Allocate(5);
    auto* life = e->Pool().Data<f32_t>(Attr::Lifetime);
    auto* age  = e->Pool().Data<f32_t>(Attr::Age);
    for (int i = 0; i < 5; ++i) { life[i] = 0.01f; age[i] = 0.f; }

    e->Tick(0.02f);   // 전부 수명 초과
    EXPECT_EQ(e->Pool().AliveCount(), 0u);
}
```

## Gotchas

- **Spawn Rate 잔여 누적 누락 (MVP 제약)**: `SpawnRate(rate=25/s, dt=0.008s)` = 0.2 개 → 0 개 할당으로 버림. 실제 게임에서는 정확하나, Phase 1 MVP 에서는 허용. Stage 6 전에 `EmitterInstance` 에 `m_spawnAccumulator` 멤버 추가
- **Spawn → Init 순서 강제**: Init 노드는 `ctx.spawnRangeBegin/End` 에 의존 → `SpawnBurst/Rate` 가 반드시 **앞에** 나와야. 위상 정렬이 엣지로 이걸 보장. 엣지 누락 시 Init 이 빈 범위에 돌아 "아무것도 안 보임" 증상 발생 → `FxGraphValidator` 가 경고
- **UpdateAgeAndKill 은 맨 마지막**: Color/Size over life 가 age 를 참조. Kill 이 먼저 돌면 잘못된 swap 발생. 엣지로 맨 뒤 고정
- **params 의 variant 키 누락**: `std::get<f32_t>(params.at("min"))` 이 타입 틀리면 `bad_variant_access` 예외 → JSON 파서에서 타입 정규화
- **변수 `float` 대신 `f32_t`**: CLAUDE.md 컨벤션. 단 Node 정의 안에서 `Vec3`/`Vec4` 는 `XMFLOAT` 래핑이라 OK
- **Init 노드에서 할당이 0 인 경우**: `SpawnBurst(count=50)` 에 pool capacity 가 30 밖에 없으면 actual=30. Init 노드는 `[begin, end)` 범위만 돌므로 자동 처리됨
- **pool 이 nullptr**: Tick 이전에 Create 가 실패했는데 Tick 이 돌면 crash. `m_executor` 도 nullptr 체크

## 구현 순서

1. `FxExecContext.h` (struct)
2. `FxRandom.h` (xorshift32)
3. `NodeRegistry.h` + `.cpp`
4. `FxExecutor.h` + `.cpp` (Compile + Run*)
5. 9 개 노드 구현 (`FxNodeImpl.cpp`)
6. `StandardNodes::InitStandardNodes()`
7. `EmitterInstance.h` + `.cpp`
8. 데모 Scene 통합 (`Scene_InGame::OnEnter_FxDemo`)
9. ImGui 로 `AliveCount` 확인
10. 단위 테스트 3~5 케이스

## 다음 Stage

Stage 4 — Expression VM. 현재는 "색 / 크기 곡선" 이 하드코딩 keyframe. 아티스트가 **임의 수식** 으로 정의할 수 있게 바이트코드 VM 추가.

Stage 5 — 렌더링이 먼저 급하면 Stage 4 를 뒤로 미뤄도 OK. 의존성: 없음.
