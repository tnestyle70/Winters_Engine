# Stage 2 — ParticlePool (SoA) + swap-back Kill

## 목표

파티클 수만 개를 **캐시 친화적으로** 저장하고, **중간 삭제 비용 O(1)** 로 유지.
같은 attribute 배열이 연속 메모리에 놓여 SIMD / Compute 어느 쪽이든 자연스럽게 소화.

## 왜 SoA 인가

**AoS (Array of Structs)**: 파티클 하나 = 구조체 하나.
```cpp
struct Particle { Vec3 pos; Vec3 vel; Vec4 col; float size; float age; float life; };
std::vector<Particle> particles;   // AoS
```

| 약점 | 이유 |
|---|---|
| 캐시 낭비 | Gravity 업데이트는 vel 만 씀. pos/col/size/age 전부 캐시라인 점유 |
| SIMD 비효율 | SIMD 는 동일 필드 4 개 모아서 처리해야 이상적 |
| GPU 업로드 비효율 | Color 만 필요해도 구조체 전체 전송 |
| Compute Shader 친화성 0 | GPU 는 SoA 전용 사고 |

**SoA (Structure of Arrays)**: attribute 마다 배열 하나.
```cpp
std::vector<Vec3> positions;
std::vector<Vec3> velocities;
std::vector<Vec4> colors;
// ...
```

| 장점 | 이유 |
|---|---|
| 캐시 효율 | Gravity 루프는 velocities 만 읽음. 100% 적중 |
| SIMD 친화 | 4 파티클 velocity 를 한 XMVECTOR 에 적재 |
| GPU 업로드 | attribute 별 개별 업로드 가능 (StructuredBuffer per attr) |
| Niagara / VectorVM 과 동일 사고 | 나중에 GPU 이관 시 재작업 없음 |

Niagara, Unity VFX Graph, Ubisoft Fork (Rainbow Six) 전부 SoA. **선택의 여지가 없다.**

## 핵심 연산 — swap-back Kill

수명이 끝난 파티클을 중간에서 빼는 게 아니라 **끝과 swap** 후 `aliveCount` 만 줄임.
항상 `[0, aliveCount)` 가 살아있는 파티클이 되도록 유지 → 순회가 단순.

```
Before:  [0][1][2][3][4][5]   aliveCount = 6
         kill #2
Step 1:  swap index 2 with index 5 (last)
         [0][1][5][3][4][2]
Step 2:  aliveCount = 5
         alive slice = [0][1][5][3][4]
```

**주의**: 업데이트 루프가 앞에서부터 돈다면 kill 호출 후 방금 swap 된 `[2]` 슬롯의 새 데이터를
**이미 처리한 걸로 간주해서 건너뜀 → 버그**. 반드시 **뒤에서 앞으로** 순회해야 안전.
(CLAUDE.md 에도 "실전에서 엄청 많이 틀리는 버그" 로 기록된 함정)

## 인터페이스

```cpp
// Engine/Public/FX/Pool/ParticlePool.h
#pragma once
#include "FxTypes.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine::FX {

class CParticlePool
{
public:
    static std::unique_ptr<CParticlePool> Create() { return std::unique_ptr<CParticlePool>(new CParticlePool()); }
    ~CParticlePool() = default;

    // ── 셋업 (Emitter 생성 시 1 회) ───────────────
    void RegisterAttribute(const std::string& name, AttrType type);
    void Reserve(std::uint32_t maxParticles);

    // 등록된 attribute 목록
    const std::vector<AttributeDesc>& GetAttributes() const { return m_attrOrder; }
    bool HasAttribute(const std::string& name) const { return m_descs.count(name) > 0; }

    // ── 런타임 ───────────────────────────────────
    std::uint32_t Capacity()    const { return m_capacity; }
    std::uint32_t AliveCount()  const { return m_aliveCount; }

    // count 개 슬롯 확보. 반환값 = 새로 할당된 시작 인덱스.
    // 용량 초과 시 실제 할당된 개수만큼만 반환 (clamp).
    std::uint32_t Allocate(std::uint32_t count, std::uint32_t* outActuallyAllocated = nullptr);

    // swap-back 삭제. index 가 유효하지 않으면 무시.
    void Kill(std::uint32_t index);

    // 전체 리셋 (데이터는 유지되지만 aliveCount = 0)
    void Clear() { m_aliveCount = 0; }

    // ── Attribute 접근 ──────────────────────────
    template<typename T>
    T* Data(const std::string& name)
    {
        auto it = m_buffers.find(name);
        if (it == m_buffers.end()) return nullptr;
        return reinterpret_cast<T*>(it->second.data());
    }

    template<typename T>
    const T* Data(const std::string& name) const
    {
        auto it = m_buffers.find(name);
        if (it == m_buffers.end()) return nullptr;
        return reinterpret_cast<const T*>(it->second.data());
    }

private:
    CParticlePool() = default;

    std::uint32_t m_capacity   = 0;
    std::uint32_t m_aliveCount = 0;

    std::unordered_map<std::string, AttributeDesc> m_descs;
    std::unordered_map<std::string, std::vector<std::uint8_t>> m_buffers;
    std::vector<AttributeDesc> m_attrOrder;   // 등록 순서 (직렬화 / 렌더러 일관성)
};

} // namespace Engine::FX
```

## 구현

```cpp
// Engine/Private/FX/Pool/ParticlePool.cpp
#include "ParticlePool.h"
#include <cstring>

namespace Engine::FX {

void CParticlePool::RegisterAttribute(const std::string& name, AttrType type)
{
    if (m_descs.count(name) > 0) return;   // 이미 등록됨 (idempotent)

    AttributeDesc d;
    d.name   = name;
    d.type   = type;
    d.stride = SizeOfAttr(type);
    m_descs[name] = d;
    m_attrOrder.push_back(d);

    if (m_capacity > 0)
        m_buffers[name].resize(std::size_t(m_capacity) * d.stride);
}

void CParticlePool::Reserve(std::uint32_t maxParticles)
{
    m_capacity = maxParticles;
    for (auto& [name, desc] : m_descs)
        m_buffers[name].resize(std::size_t(maxParticles) * desc.stride);
    m_aliveCount = 0;
}

std::uint32_t CParticlePool::Allocate(std::uint32_t count,
                                       std::uint32_t* outActuallyAllocated)
{
    const std::uint32_t available = m_capacity - m_aliveCount;
    const std::uint32_t granted   = std::min(count, available);
    const std::uint32_t start     = m_aliveCount;
    m_aliveCount += granted;
    if (outActuallyAllocated) *outActuallyAllocated = granted;
    return start;
}

void CParticlePool::Kill(std::uint32_t index)
{
    if (index >= m_aliveCount) return;
    const std::uint32_t last = m_aliveCount - 1;

    if (index != last) {
        // 각 attribute 에 대해 swap-back
        for (const auto& desc : m_attrOrder) {
            auto& buf = m_buffers[desc.name];
            std::uint8_t* dst  = buf.data() + std::size_t(index) * desc.stride;
            std::uint8_t* srcL = buf.data() + std::size_t(last)  * desc.stride;
            std::memcpy(dst, srcL, desc.stride);
        }
    }

    --m_aliveCount;
}

} // namespace Engine::FX
```

## 타입 안전 뷰

raw `T*` 반환은 빠르지만 실수 유발. 디버그 빌드에서 길이 검증하는 Span 래퍼:

```cpp
// Engine/Public/FX/Pool/AttributeView.h
#pragma once
#include "ParticlePool.h"
#include <cassert>

namespace Engine::FX {

template<typename T>
class AttributeView
{
public:
    AttributeView(T* data, std::uint32_t aliveCount, std::uint32_t capacity)
        : m_data(data), m_alive(aliveCount), m_capacity(capacity) {}

    T& operator[](std::uint32_t i) {
        assert(i < m_capacity && "AttributeView out-of-range");
        return m_data[i];
    }
    const T& operator[](std::uint32_t i) const {
        assert(i < m_capacity && "AttributeView out-of-range");
        return m_data[i];
    }

    T* begin() { return m_data; }
    T* end()   { return m_data + m_alive; }

    std::uint32_t size()    const { return m_alive; }
    std::uint32_t capacity() const { return m_capacity; }

private:
    T*            m_data;
    std::uint32_t m_alive;
    std::uint32_t m_capacity;
};

template<typename T>
inline AttributeView<T> ViewAlive(CParticlePool& pool, const std::string& name)
{
    return AttributeView<T>(pool.Data<T>(name), pool.AliveCount(), pool.Capacity());
}

template<typename T>
inline AttributeView<T> ViewRange(CParticlePool& pool, const std::string& name,
                                   std::uint32_t begin, std::uint32_t end)
{
    // Spawn 노드가 신규 범위만 조작할 때 사용
    T* base = pool.Data<T>(name);
    return AttributeView<T>(base + begin, end - begin, pool.Capacity() - begin);
}

} // namespace Engine::FX
```

## 표준 Attribute 자동 등록

```cpp
// Engine/Public/FX/Pool/ParticlePool.h 에 추가될 헬퍼
inline void RegisterDefaultAttributes(CParticlePool& p)
{
    p.RegisterAttribute(Attr::Position, AttrType::Float3);
    p.RegisterAttribute(Attr::Velocity, AttrType::Float3);
    p.RegisterAttribute(Attr::Color,    AttrType::Float4);
    p.RegisterAttribute(Attr::Size,     AttrType::Float);
    p.RegisterAttribute(Attr::Age,      AttrType::Float);
    p.RegisterAttribute(Attr::Lifetime, AttrType::Float);
}
```

## 커스텀 Attribute

에셋이 추가 attribute 를 원하면 노드 실행 전에 `RegisterAttribute` 호출.
예) `Rotation` (Float), `Seed` (Int), `CustomFloat0` (Float2) 등.

노드 정의가 "내가 필요한 attribute 목록" 을 선언하면 Executor 가 컴파일 시점에 중복 없이 등록:

```cpp
// NodeRegistry.h 발췌 (예정)
struct NodeMetadata
{
    eNodeKind kind;
    eStage    stage;
    std::vector<AttributeDesc> requiredAttrs;   // 이 노드가 소비/생산하는 attr
    NodeExecFn fn;
};
```

## 성능 특성

| 연산 | 복잡도 | 비고 |
|---|---|---|
| Allocate(N) | O(1) | aliveCount 증가만 |
| Kill(i) | O(A) | A = attribute 수 (보통 6~10). 각 attr 16 byte 미만 memcpy |
| 전체 순회 | O(N) | N = aliveCount, 캐시 친화 |
| attribute 추가 | O(capacity) | 셋업 단계만, 런타임엔 하지 말 것 |

## 메모리 레이아웃

capacity = 2048, attrs = {Position:Float3, Velocity:Float3, Color:Float4, Size:Float, Age:Float, Lifetime:Float}
```
Position : [──── 2048 × 12 bytes = 24 KB ────]
Velocity : [──── 2048 × 12 bytes = 24 KB ────]
Color    : [──── 2048 × 16 bytes = 32 KB ────]
Size     : [──── 2048 × 4  bytes =  8 KB ────]
Age      : [──── 2048 × 4  bytes =  8 KB ────]
Lifetime : [──── 2048 × 4  bytes =  8 KB ────]
                                     총 104 KB
```

L2 캐시 (보통 256 KB~1 MB) 안에 들어감. L1 은 attribute 하나씩 순회해야 맞음.

## 멀티 이미터 풀 전략

이미터마다 `CParticlePool` 하나. 이미터 인스턴스는 풀 재활용:

```cpp
class CEmitterInstance
{
    std::unique_ptr<CParticlePool> m_pool;   // 이미터마다 하나
    // ...
};

class CFxInstance   // System 레벨
{
    std::vector<std::unique_ptr<CEmitterInstance>> m_emitters;
};
```

총 메모리 = **(이미터당 104 KB) × (평균 이미터 수 3) × (동시 FxInstance 수 128) = 약 40 MB**.
한타 씬 기준 상한. 제약 충분.

## 단위 테스트

```cpp
TEST(ParticlePool, RegisterAndAllocate)
{
    auto p = CParticlePool::Create();
    RegisterDefaultAttributes(*p);
    p->Reserve(1024);

    std::uint32_t actual;
    auto start = p->Allocate(100, &actual);
    EXPECT_EQ(start, 0u);
    EXPECT_EQ(actual, 100u);
    EXPECT_EQ(p->AliveCount(), 100u);
}

TEST(ParticlePool, KillSwapsWithLast)
{
    auto p = CParticlePool::Create();
    p->RegisterAttribute("v", AttrType::Float);
    p->Reserve(8);
    p->Allocate(4);

    auto* v = p->Data<float>("v");
    v[0] = 10.f; v[1] = 20.f; v[2] = 30.f; v[3] = 40.f;

    p->Kill(1);
    EXPECT_EQ(p->AliveCount(), 3u);
    EXPECT_FLOAT_EQ(v[0], 10.f);
    EXPECT_FLOAT_EQ(v[1], 40.f);   // 마지막이 index 1 로 이동
    EXPECT_FLOAT_EQ(v[2], 30.f);   // 원본 유지
}

TEST(ParticlePool, KillLastParticle_NoSwap)
{
    auto p = CParticlePool::Create();
    p->RegisterAttribute("v", AttrType::Float);
    p->Reserve(4);
    p->Allocate(3);
    auto* v = p->Data<float>("v");
    v[0] = 1.f; v[1] = 2.f; v[2] = 3.f;

    p->Kill(2);   // 마지막 index
    EXPECT_EQ(p->AliveCount(), 2u);
    EXPECT_FLOAT_EQ(v[0], 1.f);
    EXPECT_FLOAT_EQ(v[1], 2.f);
}

TEST(ParticlePool, AllocateOverCapacity_Clamps)
{
    auto p = CParticlePool::Create();
    p->RegisterAttribute("v", AttrType::Float);
    p->Reserve(10);
    std::uint32_t actual;
    p->Allocate(100, &actual);
    EXPECT_EQ(actual, 10u);
    EXPECT_EQ(p->AliveCount(), 10u);
}

TEST(ParticlePool, ReverseIterationIsSafeWithKill)
{
    // 순회 중 kill 해도 뒤→앞이면 안전
    auto p = CParticlePool::Create();
    p->RegisterAttribute("v", AttrType::Float);
    p->Reserve(8);
    p->Allocate(6);
    auto* v = p->Data<float>("v");
    for (int i = 0; i < 6; ++i) v[i] = float(i);

    // i = 3 과 i = 5 죽이기
    for (std::int32_t i = 5; i >= 0; --i) {
        if (i == 3 || i == 5) p->Kill(std::uint32_t(i));
    }
    EXPECT_EQ(p->AliveCount(), 4u);
    // 나머지 값 유효성 (순서는 신경 안 씀)
    std::vector<float> remaining(v, v + p->AliveCount());
    std::sort(remaining.begin(), remaining.end());
    EXPECT_FLOAT_EQ(remaining[0], 0.f);
    EXPECT_FLOAT_EQ(remaining[1], 1.f);
    EXPECT_FLOAT_EQ(remaining[2], 2.f);
    EXPECT_FLOAT_EQ(remaining[3], 4.f);
}
```

## Gotchas

- **`std::vector<bool>` 아니어도 bool attribute**: `std::vector<std::uint8_t>` 로 저장 (1 byte), `bool` 캐스팅 주의
- **capacity 가 0 일 때 Data() 호출**: `nullptr` 반환 → 루프가 돌지 않도록 Executor 에서 체크
- **Reserve 를 런타임에 다시 호출**: 기존 데이터 전부 날아감 → Emitter 생성 시 1 회만 호출, 런타임 변경 금지
- **swap-back 후 이전 슬롯 값**: 이전 값이 남아있음 (데이터 초기화 안 함). Update 가 전체 alive 를 매 프레임 갱신하므로 문제 없음. 하지만 **Spawn 노드가 신규 범위만 초기화하는 건 의미 있음** — 사망한 슬롯의 garbage 가 salvage 되기 전에 Init 노드가 덮어씌워줘야 함
- **멀티스레드 동시 Allocate 안 됨**: Kill 도 동일. 이미터 tick 은 단일 스레드 전제. JobSystem 병렬화는 **이미터 단위 병렬**, 이미터 내부는 순차

## 구현 순서

1. `ParticlePool.h` (interface)
2. `ParticlePool.cpp` (Register / Reserve / Allocate / Kill)
3. `AttributeView.h` (타입 안전 래퍼)
4. 단위 테스트 5 케이스 (위 `TEST` 블록)
5. `RegisterDefaultAttributes` 헬퍼
6. 벤치마크 — 10,000 파티클 Allocate + Update + 랜덤 Kill 반복 (1 ms 이내 목표)

## 성능 벤치마크 계획

```cpp
// 벤치마크 의사코드
void BM_ParticlePool_Update10K()
{
    auto p = CParticlePool::Create();
    RegisterDefaultAttributes(*p);
    p->Reserve(10000);
    p->Allocate(10000);

    auto* pos = p->Data<Vec3>("Position");
    auto* vel = p->Data<Vec3>("Velocity");

    auto start = std::chrono::steady_clock::now();
    for (int iter = 0; iter < 100; ++iter) {
        float dt = 0.016f;
        // Integrate
        for (std::uint32_t i = 0; i < p->AliveCount(); ++i) {
            pos[i].x += vel[i].x * dt;
            pos[i].y += vel[i].y * dt;
            pos[i].z += vel[i].z * dt;
        }
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    // 기대: 100 iter × 10K particle = 1M 연산 < 2 ms
}
```

## 다음 Stage

Stage 3 — `Executor` . 등록된 attribute 가 있는 ParticlePool 위에서, 위상 정렬된 노드 리스트를 실제로 실행한다.
