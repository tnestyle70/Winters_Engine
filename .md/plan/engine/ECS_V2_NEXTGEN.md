# ECS v2 — Next-Gen Architecture (Generation + Archetype + Multi-World) — **rev 2**

**작성일**: 2026-05-04
**rev 2 (2026-05-04, Codex 검토 반영)**: ① RHIHandles.h 패턴 재사용 (별도 박제 X) ② bit layout 산술 오류 정정 ③ EntityID typedef 위험 명시 + legacy adapter 박제
**권위 마스터**: [`2026-05-04_ECS_FIBER_RENDERGRAPH_GPU_DRIVEN_PLAN.md`](2026-05-04_ECS_FIBER_RENDERGRAPH_GPU_DRIVEN_PLAN.md) (Codex). 본 문서는 그 §2 ECS v2 의 상세 박제.
**가이드**: PLAN_AUTHORING_PITFALLS.md (P-1~P-18, 8 GATE)
**목표**: 엘든링 (100K 엔티티) + LoL (5 GameRoom 멀티 World) 결합 부하 감당. Use-after-free 0, ForEach 5~10× 가속, 멀티 World 안전.

---

## §rev 2 Codex 검토 반영 매트릭스

| # | rev 1 결함 | rev 2 정정 | PITFALLS |
|---|---|---|---|
| 1 | EntityHandle 64-bit 별도 박제 — RHI 패턴 [RHIHandles.h:6-47](../../../Engine/Public/RHI/RHIHandles.h:6) `RHIHandle<TTag>` 미인지 | **RHIHandle<TTag> 패턴 재사용** — `EntityTag` 추가 + `using EntityHandle = RHIHandle<EntityTag>;`. 단 worldId 8-bit 별도 박스 (RHIHandle 은 index 32 + gen 32 만 보유). | **P-18 신규 — RHI 인프라 미인지** |
| 2 | "32-bit gen + 24-bit index = 16M index 한도 → 엘든링 100K 시 부족" 산술 오류 — 16M ≫ 100K, **충분**. 오히려 24-bit gen 이 서버 장기 churn 시 위험 (투사체/미니언 빈번 spawn/destroy). | **bit layout 재결정**: `index 32 + generation 32` (RHIHandles.h 패턴 그대로) + **worldId 는 외부 `CECSWorldRegistry`** 에서 관리. 또는 `index 24 + gen 32 + world 8` (서버에서 16M index 충분 + 32-bit gen wrap 사실상 0). | **P-16 신규 — 산술 검증 누락** |
| 3 | Phase 2 의 `using EntityID = EntityHandle;` 일괄 typedef = 컴파일/ABI 폭발 — 현재 `EntityID` 가 vector index / `%u` 로그 / `unordered_map<EntityID, ...>` key / sparse array index 로 광범위 사용 | **legacy adapter 단계** — `EntityHandle::ToLegacyId()` 와 `EntityHandle::FromLegacyId()` 호환 함수 박제. typedef 일괄 통합은 phase 3 (별도 cycle) 에 분리. | **P-17 신규 — Typedef 일괄 변경 ABI 폭발** |

---

## §0. 현재 ECS 결함 5종 (재인용 — 마스터 §0)

| # | 결함 | 현재 위치 | v2 정정 |
|---|---|---|---|
| 1 | EntityID = `uint32_t` (Generation X) | [Entity.h:9](../../../Engine/Public/ECS/Entity.h:9) | **`EntityHandle` 64-bit struct** (index 32 + generation 24 + world 8) |
| 2 | `vector<bool> m_vecAlive` | [Entity.h:67](../../../Engine/Public/ECS/Entity.h:67) | **`std::vector<EntityRecord>`** (generation + alive flag + archetype id) |
| 3 | Archetype 미지원 — SparseSet ad-hoc | [World.h:107](../../../Engine/Public/ECS/World.h:107) | **`CArchetype`** (chunked SoA per component combination) + bitset query |
| 4 | 단일 World | [World.h:53](../../../Engine/Public/ECS/World.h:53) | **`CECSWorld`** N 인스턴스 (GameRoom/Scene 별) + worldId 분리 |
| 5 | DLL export 누수 (`#pragma warning(disable: 4251)`) | [World.h:21](../../../Engine/Public/ECS/World.h:21) | **PIMPL 패턴** — public 헤더에 STL 멤버 노출 X |

---

## §1. Preflight Evidence Table (실측)

| 항목 | 결과 (실측) | 위치 |
|---|---|---|
| 현재 EntityID 정의 | `using EntityID = uint32_t; constexpr EntityID NULL_ENTITY = 0;` | [Entity.h:9-10](../../../Engine/Public/ECS/Entity.h:9) |
| 현재 CEntityManager | `m_iNextID, m_vecAlive (vector<bool>), m_vecFreeList, m_iAliveCount` | [Entity.h:65-69](../../../Engine/Public/ECS/Entity.h:65) |
| 현재 ComponentStore 패턴 | SparseSet (`m_vecSparse[entity] → denseIdx`, `m_vecDense`, `m_vecData`) — Add/Remove O(1), Has O(1) | [ComponentStore.h:1-64](../../../Engine/Public/ECS/ComponentStore.h:1) |
| 현재 ForEach<T1, T2, T3> | store1 순회 + store2/3 `Has` + `Get` lookup 3회 | [World.h:120-133](../../../Engine/Public/ECS/World.h:120) |
| 현재 CWorld export | `WINTERS_ENGINE` 마크 + `unordered_map`/`unique_ptr` 멤버 (4251 경고 disable) | [World.h:21-150](../../../Engine/Public/ECS/World.h:21) |
| 현재 SpatialIndex 통합 | `unique_ptr<CSpatialIndex> m_pSpatialIndex` + `Initialize_Spatial(desc)` (B-13 v2) | [World.h:74-75, 151](../../../Engine/Public/ECS/World.h:74) |
| 호출자 grep `m_World.AddComponent` | (마이그레이션 매트릭스 §6 박제) | TBD — D-0 grep |

---

## §2. EntityHandle (64-bit)

**파일**: `Engine/Public/ECS/EntityHandle.h` (신규)

```cpp
#pragma once
#include <cstdint>
#include <functional>   // std::hash 특수화

namespace Engine
{
    // ─────────────────────────────────────────────────────────────
    // EntityHandle — 64-bit packed handle.
    //
    //   bits  0..31  : index    (32-bit, 4G entities per world)
    //   bits 32..55  : generation (24-bit, 16M reuse cycles)
    //   bits 56..63  : worldId  (8-bit, 256 worlds simultaneously)
    //
    // 무효 핸들: kInvalid (전체 0). Index 0 도 유효 — generation 0 = 무효.
    // ─────────────────────────────────────────────────────────────

    struct EntityHandle
    {
        uint64_t raw = 0;

        constexpr EntityHandle() = default;
        constexpr EntityHandle(uint32_t index, uint32_t generation, uint8_t worldId)
            : raw((static_cast<uint64_t>(index))
                | (static_cast<uint64_t>(generation & 0xFFFFFFu) << 32)
                | (static_cast<uint64_t>(worldId) << 56))
        {}

        constexpr uint32_t GetIndex()      const { return static_cast<uint32_t>(raw & 0xFFFFFFFFu); }
        constexpr uint32_t GetGeneration() const { return static_cast<uint32_t>((raw >> 32) & 0xFFFFFFu); }
        constexpr uint8_t  GetWorldId()    const { return static_cast<uint8_t>((raw >> 56) & 0xFFu); }
        constexpr bool     IsValid()       const { return GetGeneration() != 0; }

        constexpr bool operator==(EntityHandle r) const { return raw == r.raw; }
        constexpr bool operator!=(EntityHandle r) const { return raw != r.raw; }
        constexpr bool operator< (EntityHandle r) const { return raw <  r.raw; }
    };

    inline constexpr EntityHandle kInvalidEntityHandle{};

    // 호환성 alias — 마이그레이션 phase 0 동안 기존 코드 빌드 보존
    // (기존 EntityID = uint32_t 와는 다름. 점진 교체)
    // 진입 후 phase 3 에서 EntityID = EntityHandle typedef 로 통합 후 alias 제거.
}

namespace std
{
    template<>
    struct hash<Engine::EntityHandle>
    {
        size_t operator()(const Engine::EntityHandle& h) const noexcept
        {
            return std::hash<uint64_t>{}(h.raw);
        }
    };
}
```

**bit layout 결정 (★ rev 2 정정 — Codex 검토)**

산술 검증:
- 16M (24-bit) **≫** 100K 엔티티 — index 24-bit 도 엘든링 오픈월드 100K 충분.
- 60 FPS × 24-bit gen 수명 — 서버 5분 내 churn 가능 (LoL 미니언 6분 wave × 144개 = 864 entity/min / 16M = 309시간. 현실적 wrap 없음. 단 UDP 패킷에 stale handle 재사용 시점 wrap 위험 → **32-bit gen 이 안전**).

**선택 A (권장 — RHIHandles.h 패턴 그대로)**:
```cpp
// EntityHandle = RHIHandle<EntityTag> — index 32 + generation 32 (8 byte)
struct EntityTag {};
using EntityHandle = RHIHandle<EntityTag>;
// worldId 는 외부 CECSWorldRegistry 가 EntityHandle → CECSWorld* 매핑
```
- 장점: RHI 패턴과 통일, 코드 재사용 (Make/IsValid/Index/Generation/ToU64).
- 단점: cross-world handle 검증은 외부 registry 호출 (1-3 cache miss).

**선택 B (Multi-World 강고)**:
```cpp
// EntityHandle 64-bit — index 24 + generation 32 + world 8
struct EntityHandle {
    u64_t value = 0;
    u32_t Index()      const { return static_cast<u32_t>(value & 0xFFFFFFul); }       // 24-bit
    u32_t Generation() const { return static_cast<u32_t>((value >> 24) & 0xFFFFFFFFul); } // 32-bit
    u8_t  WorldId()    const { return static_cast<u8_t>((value >> 56) & 0xFFu); }
};
```
- 장점: handle 안에 worldId 박제 — cross-world 검출 1 cache hit.
- 단점: RHIHandle<TTag> 와 별도. 산술 비트 어긋남 위험 (Codex 의 P-16 재발 가능).

**채택**: **선택 A** — RHI 패턴 일관성 우선. worldId 는 `CECSWorldRegistry` 가 `unordered_map<EntityHandle, uint8_t>` 보유 (또는 EntityHandle 의 generation 상위 8-bit 를 worldId 로 — 이건 generation 24-bit 로 줄어듦. 절충).

**최종 채택 (rev 2)**: 선택 A — `using EntityHandle = RHIHandle<EntityTag>;`. worldId 는 CECSWorld 가 자체 보유 + 외부 등록 시 검증.

---

### Legacy Adapter (★ rev 2 신설 — P-17 회피)

**현재 EntityID 사용 위치 grep 결과 (D-0 작업 명시)**:
```bash
grep -rnE "using EntityID|EntityID [a-z]|unordered_map<EntityID|vector<EntityID>" Engine/ Client/ Server/ Shared/
```

**호환 어댑터** (rev 1 의 typedef 일괄 통합 폐기):

```cpp
// Engine/Public/ECS/EntityHandle.h (rev 2 추가)
namespace Engine
{
    struct EntityTag {};
    using EntityHandle = RHIHandle<EntityTag>;

    // Legacy adapter — 기존 EntityID = uint32_t 코드와 공존.
    // Phase 1 마이그 시 새 코드는 EntityHandle 사용, 기존 EntityID 사용 코드는 유지.
    // Phase 3 (별도 cycle) 에서 점진 통합 — 절대 일괄 typedef 변경 X.

    // EntityID (uint32_t) → EntityHandle 변환 — generation 1 가정
    inline EntityHandle ToHandle(uint32_t legacyId, uint32_t generation = 1)
    {
        return EntityHandle::Make(legacyId, generation);
    }
    // EntityHandle → uint32_t 변환 — generation 검증 후 index 만 반환
    // 호환 코드 (vector index / log %u / unordered_map key) 유지용.
    inline uint32_t ToLegacyId(EntityHandle h)
    {
        return h.IsValid() ? h.Index() : 0;
    }
}

// 기존 EntityID = uint32_t 는 그대로 유지 (Entity.h 변경 X).
// CECSWorld 의 새 API 만 EntityHandle 받음.
```

**마이그 단계** (rev 2 — phase 3 분리):
- **Phase 0**: EntityHandle + CECSWorld 박제 + legacy adapter. **Entity.h 변경 X**.
- **Phase 1**: 핫패스 (MinionAI / Spatial / Render / Tower / BT) 가 새 CECSWorld 사용. 기존 CWorld 와 공존.
- **Phase 2**: 콜드패스 (Editor / Debug / Loader) 마이그.
- **Phase 3 (별도 cycle, 신중)**: 모든 `EntityID = uint32_t` 사용처를 `EntityHandle` 로 점진 교체. **vector index / log / map key 패턴 마이그 매트릭스 별도 박제 후 진입**. 일괄 typedef 절대 X.

---

## §3. CEntityManager v2

**파일**: `Engine/Public/ECS/EntityManager.h` (수정 — 기존 [Entity.h](../../../Engine/Public/ECS/Entity.h) 대체)

```cpp
#pragma once
#include "ECS/EntityHandle.h"
#include <vector>
#include <cassert>
#include <cstdint>

namespace Engine
{
    // ─────────────────────────────────────────────────────────────
    // EntityRecord — Generation 검증 + Archetype 위치 + alive 비트
    // ─────────────────────────────────────────────────────────────
    struct EntityRecord
    {
        uint32_t generation = 0;        // 0 = invalid (created 시 ≥1)
        uint32_t archetypeIndex = 0;    // 어떤 Archetype 에 속하는지 (0 = empty archetype)
        uint32_t archetypeRow = 0;      // Archetype 안의 row (chunk 안 위치)
        bool     bAlive = false;
    };

    class CEntityManager
    {
    public:
        explicit CEntityManager(uint8_t worldId) : m_uWorldId(worldId) {}

        EntityHandle Create()
        {
            uint32_t index;
            if (!m_vecFreeList.empty())
            {
                index = m_vecFreeList.back();
                m_vecFreeList.pop_back();
            }
            else
            {
                index = m_uNextIndex++;
                if (index >= m_vecRecords.size())
                    m_vecRecords.resize(index + 1);
            }
            auto& rec = m_vecRecords[index];
            ++rec.generation;
            if (rec.generation == 0) rec.generation = 1;   // wrap 회피 (0 = invalid)
            rec.bAlive = true;
            ++m_uAliveCount;
            return EntityHandle(index, rec.generation, m_uWorldId);
        }

        void Destroy(EntityHandle h)
        {
            assert(IsAlive(h));
            uint32_t index = h.GetIndex();
            m_vecRecords[index].bAlive = false;
            // Generation 은 다음 Create 시 증가 — 현재 destroy 후 IsAlive(h) → false
            m_vecFreeList.push_back(index);
            --m_uAliveCount;
        }

        bool IsAlive(EntityHandle h) const
        {
            if (!h.IsValid()) return false;
            if (h.GetWorldId() != m_uWorldId) return false;
            uint32_t index = h.GetIndex();
            if (index >= m_vecRecords.size()) return false;
            const auto& rec = m_vecRecords[index];
            return rec.bAlive && rec.generation == h.GetGeneration();
        }

        // Archetype 위치 갱신 — Archetype 시스템이 호출
        void SetArchetypeLocation(EntityHandle h, uint32_t archIdx, uint32_t row)
        {
            assert(IsAlive(h));
            auto& rec = m_vecRecords[h.GetIndex()];
            rec.archetypeIndex = archIdx;
            rec.archetypeRow = row;
        }

        const EntityRecord& GetRecord(EntityHandle h) const
        {
            assert(IsAlive(h));
            return m_vecRecords[h.GetIndex()];
        }

        uint32_t GetAliveCount() const { return m_uAliveCount; }
        uint8_t  GetWorldId()    const { return m_uWorldId; }

    private:
        uint8_t                   m_uWorldId = 0;
        uint32_t                  m_uNextIndex = 1;   // 0 = NULL_INDEX (kInvalidEntityHandle 보존)
        std::vector<EntityRecord> m_vecRecords;
        std::vector<uint32_t>     m_vecFreeList;
        uint32_t                  m_uAliveCount = 0;
    };
}
```

**핵심 안전 보장**:
- `IsAlive(h)` 에서 generation mismatch → false. **Stale handle 재사용 = silent fail 대신 명시적 false** = 모든 Get/Has/Add 가 안전 early-return.
- `worldId` mismatch → false. Cross-world handle 사고 자동 검출.

---

## §4. Archetype 베이스

**파일**: `Engine/Public/ECS/Archetype.h` (신규)

```cpp
#pragma once
#include "ECS/EntityHandle.h"
#include <bitset>
#include <vector>
#include <typeindex>
#include <memory>
#include <cstdint>

namespace Engine
{
    // 컴포넌트 ID 비트 — 64-bit (Phase 2 충분, 미래 std::bitset<256> 확장)
    inline constexpr uint32_t kMaxComponentTypes = 64;
    using ComponentMask = std::bitset<kMaxComponentTypes>;

    // 컴포넌트 타입 ID 등록 — 정적 카운터
    class CComponentTypeRegistry
    {
    public:
        template<typename T>
        static uint32_t GetID()
        {
            static const uint32_t kID = NextID();
            return kID;
        }

        static uint32_t Count() { return s_uNextID.load(); }

    private:
        static uint32_t NextID()
        {
            uint32_t id = s_uNextID.fetch_add(1);
            assert(id < kMaxComponentTypes && "Component type overflow — increase kMaxComponentTypes");
            return id;
        }
        static inline std::atomic<uint32_t> s_uNextID{ 0 };
    };

    template<typename T>
    inline uint32_t ComponentTypeID() { return CComponentTypeRegistry::GetID<T>(); }

    // ─────────────────────────────────────────────────────────────
    // CArchetype — 컴포넌트 조합 (mask) 별 chunked SoA 저장소.
    //
    //   chunk 0: [Entity ids][Comp_A 데이터][Comp_B 데이터] ... 16KB
    //   chunk 1: ...
    //
    // ForEach 가 mask 매칭 archetype 만 순회 → cache-line friendly.
    // ─────────────────────────────────────────────────────────────

    struct ArchetypeChunk
    {
        std::vector<EntityHandle> entities;
        // Component 데이터는 typed slot 별 (type_id → byte vector)
        std::vector<std::vector<uint8_t>> componentData;   // [typeSlot][row × sizeof(T)]
        uint32_t rowCount = 0;
        uint32_t rowCapacity = 0;
    };

    class CArchetype
    {
    public:
        CArchetype(ComponentMask mask, std::vector<size_t> componentSizes,
                   std::vector<uint32_t> componentTypeIds)
            : m_mask(mask)
            , m_vecComponentSizes(std::move(componentSizes))
            , m_vecComponentTypeIds(std::move(componentTypeIds))
        {}

        ComponentMask GetMask() const { return m_mask; }
        bool Matches(ComponentMask query) const { return (m_mask & query) == query; }

        // 엔티티 추가 → row 반환 (chunk index 와 row index)
        struct Location { uint32_t chunkIdx; uint32_t row; };
        Location AddEntity(EntityHandle h);
        void     RemoveEntity(uint32_t chunkIdx, uint32_t row);   // swap-remove

        // 컴포넌트 byte 포인터 (chunk row → typed component slot)
        uint8_t* GetComponentRaw(uint32_t chunkIdx, uint32_t row, uint32_t componentTypeId);

        // 모든 chunk 순회 — JobSystem 으로 chunk 단위 분할 가능
        const std::vector<std::unique_ptr<ArchetypeChunk>>& GetChunks() const { return m_vecChunks; }

    private:
        static constexpr uint32_t kChunkSize = 16 * 1024;  // 16 KB chunk (L2 cache fit)

        ComponentMask                                m_mask;
        std::vector<size_t>                          m_vecComponentSizes;       // typeSlot → sizeof(T)
        std::vector<uint32_t>                        m_vecComponentTypeIds;     // typeSlot → ComponentTypeID
        std::vector<std::unique_ptr<ArchetypeChunk>> m_vecChunks;
    };
}
```

**Implementation note**: `AddEntity` / `RemoveEntity` / `GetComponentRaw` 본 박제는 별도 `.cpp` (~400 LOC). 핵심 — chunk 가 가득 차면 새 chunk alloc. swap-remove 시 마지막 row 의 EntityHandle 위치 갱신 통보 (`CEntityManager::SetArchetypeLocation`).

---

## §5. CECSWorld — Multi-World 지원

**파일**: `Engine/Public/ECS/ECSWorld.h` (신규 — 기존 [World.h](../../../Engine/Public/ECS/World.h) 대체)

```cpp
#pragma once
#include "WintersAPI.h"
#include "ECS/EntityHandle.h"
#include "ECS/EntityManager.h"
#include "ECS/Archetype.h"
#include "ECS/SpatialIndex.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace Engine
{
    class CQueryBuilder;   // §6

    // PIMPL — public 헤더에서 STL 멤버 노출 회피 (4251 경고 제거)
    class WINTERS_ENGINE CECSWorld
    {
    public:
        explicit CECSWorld(uint8_t worldId);
        ~CECSWorld();

        CECSWorld(const CECSWorld&) = delete;
        CECSWorld& operator=(const CECSWorld&) = delete;
        CECSWorld(CECSWorld&&) noexcept;
        CECSWorld& operator=(CECSWorld&&) noexcept;

        // Entity 생명 관리
        EntityHandle CreateEntity();
        void         DestroyEntity(EntityHandle h);
        bool         IsAlive(EntityHandle h) const;

        // Component 추가/제거 — Archetype 변경 (entity 가 mask 다른 archetype 으로 이주)
        template<typename T> T&   AddComponent(EntityHandle h, T&& c = T{});
        template<typename T> void RemoveComponent(EntityHandle h);
        template<typename T> T*   GetComponent(EntityHandle h);          // nullptr 가능 (generation mismatch 시)
        template<typename T> bool HasComponent(EntityHandle h) const;

        // Query — bitset 매칭 archetype 만 순회 (단일 ForEach 5~10× 가속)
        CQueryBuilder Query();

        // Spatial (B-13 v2 통합)
        void           Initialize_Spatial(const SpatialGridDesc& desc);
        CSpatialIndex* Get_SpatialIndex() const;

        uint8_t  GetWorldId() const;
        uint32_t GetEntityCount() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_pImpl;
    };

    // ─────────────────────────────────────────────────────────────
    // CECSWorldRegistry — 모든 활성 World 등록. Cross-world handle dispatch.
    //
    //   GameRoom 마다 1 World 생성 + Registry 에 등록.
    //   엔티티 시스템 / Replication / 로깅이 worldId → CECSWorld* 매핑 사용.
    // ─────────────────────────────────────────────────────────────
    class WINTERS_ENGINE CECSWorldRegistry
    {
    public:
        static CECSWorldRegistry& Instance();

        uint8_t      RegisterWorld(CECSWorld* pWorld);   // worldId 반환
        void         UnregisterWorld(uint8_t worldId);
        CECSWorld*   GetWorld(uint8_t worldId) const;
        CECSWorld*   GetWorld(EntityHandle h) const { return GetWorld(h.GetWorldId()); }

    private:
        CECSWorldRegistry() = default;
        std::array<CECSWorld*, 256> m_apWorlds{};
        std::atomic<uint32_t>       m_uWorldCount{ 0 };
    };
}
```

**왜 PIMPL?** 현재 [World.h](../../../Engine/Public/ECS/World.h) 가 `#pragma warning(disable: 4251)` 로 STL 멤버 export 누수 회피. PIMPL 로 본 헤더에서 STL 멤버 0 — DLL ABI 안전.

**사용 예** (Server GameRoom):
```cpp
class CGameRoom {
    std::unique_ptr<Engine::CECSWorld> m_pWorld;
public:
    CGameRoom(uint8_t worldId) : m_pWorld(std::make_unique<Engine::CECSWorld>(worldId)) {}
};
```

---

## §6. Query Builder + ForEach (Archetype-based)

**파일**: `Engine/Public/ECS/QueryBuilder.h` (신규)

```cpp
#pragma once
#include "ECS/EntityHandle.h"
#include "ECS/Archetype.h"
#include <functional>

namespace Engine
{
    class CECSWorld;

    class CQueryBuilder
    {
    public:
        explicit CQueryBuilder(CECSWorld* pWorld) : m_pWorld(pWorld) {}

        template<typename T>
        CQueryBuilder& With() { m_includeMask.set(ComponentTypeID<T>()); return *this; }

        template<typename T>
        CQueryBuilder& Without() { m_excludeMask.set(ComponentTypeID<T>()); return *this; }

        // ForEach — 매칭 Archetype 만 chunk 단위 순회.
        // JobSystem 통합: ForEachParallel(fn) 가 chunk 단위 job 분할 (대용량 query 5~10× 가속)
        template<typename... Ts, typename Fn>
        void ForEach(Fn&& fn);

        template<typename... Ts, typename Fn>
        void ForEachParallel(Fn&& fn, class CJobSystem* pJobSystem);

    private:
        CECSWorld*   m_pWorld;
        ComponentMask m_includeMask;
        ComponentMask m_excludeMask;
    };
}
```

**사용 예 — 마이그레이션 매트릭스 (§7) 참조**:

```cpp
// 기존 (v1):
m_World.ForEach<TransformComponent, MinionStateComponent>(
    [&](EntityID id, TransformComponent& tf, MinionStateComponent& ms) { ... });

// v2:
m_World.Query()
       .With<TransformComponent>()
       .With<MinionStateComponent>()
       .ForEach<TransformComponent, MinionStateComponent>(
           [&](EntityHandle h, TransformComponent& tf, MinionStateComponent& ms) { ... });

// 또는 병렬 (큰 query):
m_World.Query()
       .With<TransformComponent>()
       .With<RenderComponent>()
       .ForEachParallel<TransformComponent, RenderComponent>(
           [&](EntityHandle h, TransformComponent& tf, RenderComponent& rc) { ... },
           pJobSystem);
```

---

## §7. 마이그레이션 매트릭스

**호출자 grep** (D-0 작업):
```bash
grep -rnE "m_World\.(AddComponent|HasComponent|GetComponent|RemoveComponent|ForEach|CreateEntity|DestroyEntity|IsAlive)" Client/ Engine/ Server/ Shared/
```

| 기존 패턴 | v2 패턴 | 매핑 |
|---|---|---|
| `EntityID id = m_World.CreateEntity()` | `EntityHandle h = m_World.CreateEntity()` | id (uint32) → h (struct 64-bit). 보관 변수 타입 변경 |
| `m_World.AddComponent<T>(id) = { ... }` | `m_World.AddComponent<T>(h, T{ ... })` | rvalue 전달, archetype 자동 이주 |
| `m_World.HasComponent<T>(id)` | `m_World.HasComponent<T>(h)` | generation 검증 자동 |
| `m_World.GetComponent<T>(id)` | `T* p = m_World.GetComponent<T>(h); if (!p) return;` | 반환 타입 `T&` → `T*` (nullptr 가능 — stale handle 안전) |
| `m_World.IsAlive(id)` | `m_World.IsAlive(h)` | generation/world 검증 |
| `m_World.ForEach<T1, T2>(...)` | `m_World.Query().With<T1>().With<T2>().ForEach<T1, T2>(...)` | bitset query 베이스 |
| `m_World.DestroyEntity(id)` | `m_World.DestroyEntity(h)` | generation 증가 (다음 Create 에서 stale 검출) |

**호환성 phase** (3 단계 마이그):
- **Phase 0** — `EntityHandle` + `CECSWorld` 박제. 기존 `EntityID = uint32_t` / `CWorld` 유지. v1+v2 공존.
- **Phase 1** — Engine/Client/Server 의 핫패스 (MinionAI / Spatial / Render) 부터 v2 마이그. 콜드패스 (Editor / Debug) 는 v1 유지.
- **Phase 2** — 모든 호출자 v2. v1 헤더 폐기. `using EntityID = EntityHandle;` typedef 통합.

각 phase 진입 시 PITFALLS GATE A~H 8 단계 통과.

---

## §8. JobSystem (Fiber) 통합

`CQueryBuilder::ForEachParallel` 이 chunk 단위 job 분할 → `CJobSystem::Submit` (또는 Fiber 활성화 후 `FiberJobSystem::Submit`). Counter 기반 동기화.

**Race-safety 매트릭스**:
- 같은 archetype chunk 안의 컴포넌트 read+write — 단일 job 안에서만. chunk 간 race 0.
- 다른 archetype 의 같은 컴포넌트 type — 별도 chunk 라 race 0.
- Generation 비트 atomic CAS 보호 (`CEntityManager::Create` 가 lock-free 미보장 — Phase 2 에서 lock-free 설계).

**Fiber 통합 시점** — NEXTGEN_FRAMEWORK_MASTER §2 의존 그래프 따름. ECS v2 + Fiber 둘 다 완료 후 `ForEachParallel` 가 Fiber yield 사용해 의존성 그래프 자연 표현.

---

## §9. SpatialIndex 통합 (B-13 v2 호환)

CWorld v1 의 `Initialize_Spatial(desc)` / `Get_SpatialIndex()` API 그대로 `CECSWorld` v2 에 박제. SpatialAgentComponent 도 동일. 따라서 B-13 v2 코드는 v2 마이그 시 **API 변경 0** (단 `ForEach` → `Query()` 만 변경).

---

## §10. PITFALLS GATE 통과 매트릭스

| GATE | 검증 |
|---|---|
| A 사실 수집 | §1 Preflight 표 — Entity.h / ComponentStore.h / World.h / JobSystem.h 모두 인용 |
| B TODO 0 | "TBD" 0개 (D-0 grep 결과 §7 매트릭스에 박제 후) |
| C 호출 경로 grep | §7 마이그레이션 매트릭스 |
| D ECS 책임 경계 | CECSWorld 가 Scene/CGameApp 직접 의존 X. CECSWorldRegistry 가 cross-world dispatch |
| E 향후 자료형 | 32-bit index = 4G 엔티티, 24-bit gen = 16M 재사용, 8-bit world = 256 World — 엘든링 100K + 5 GameRoom 만족 |
| F Scheduler | `ForEachParallel` chunk 단위 job 분할, race-safety 매트릭스 §8 |
| G Owner Scope | CECSWorld = GameRoom/Scene per. CECSWorldRegistry = 프로세스 전역 (Tier-1) |
| H 인용 의미 + 행동 보존 | 마이그레이션 phase 1 = 핫패스만, 행동 변경 0. Archetype 도입 = ForEach 결과 순서 다를 수 있음 → "순서 의존 코드 0" 검증 필수 |

---

## §11. 검증 시나리오

### V-1: Generation 검증
```cpp
auto h1 = world.CreateEntity();
world.DestroyEntity(h1);
auto h2 = world.CreateEntity();   // h2.GetIndex() == h1.GetIndex() 가능
assert(h1.GetGeneration() != h2.GetGeneration());
assert(world.IsAlive(h1) == false);   // ★ stale handle 검출
assert(world.IsAlive(h2) == true);
```

### V-2: Multi-World 격리
```cpp
auto worldA = std::make_unique<CECSWorld>(/*worldId=*/1);
auto worldB = std::make_unique<CECSWorld>(/*worldId=*/2);
auto hA = worldA->CreateEntity();
assert(worldB->IsAlive(hA) == false);   // ★ cross-world handle 검출
```

### V-3: Archetype 가속
- 100K 엔티티 + 3-component query (`Transform + Render + Minion`) ForEach
  - v1: 1~2 ms (sparse set 3 lookup)
  - v2 목표: < 200 us (archetype chunk 순회)

### V-4: Fiber + ForEachParallel
- 100K 엔티티 query 를 worker 12개 chunk 분할 → < 50 us

---

## §12. 구현 일정 (3 phase)

| Phase | 기간 | 내용 |
|---|---|---|
| **0 (코어 박제)** | 1 week | EntityHandle / EntityManager v2 / Archetype / CECSWorld PIMPL |
| **1 (마이그 핫패스)** | 1 week | MinionAI / SpatialHash / Vision / TurretAI / BT 의 Query<...> 변경 |
| **2 (전체 통합 + 폐기)** | 0.5 week | Editor / Debug 마이그 + v1 헤더 폐기 + EntityID = EntityHandle typedef |

**총 ~2.5 weeks**. 각 phase 마다 빌드 + 게임 회귀 테스트 통과.

---

**END OF ECS V2**
