# 19. Runtime Layer 박제 (`CFxSystemInstance / CFxEmitterInstance / CFxDataSet / CFxParameterStore / CFxSystemInstanceStorage`)

작성일: 2026-05-07
재박제일: 2026-05-07 (CLAUDE.md §8.2 본문 룰 — stub 0 / 라인 번호 명시 / 추상 지시 0)
권위: 본 19 = 17 마스터 §15 부속 2번. EFX-2 진입 직전 박제.
의존: 부속 18 (Asset), 부속 21 (`CFxVM::Execute / FxScriptExecContext`), 부속 22 (Translator 산출 `FxVMExecutableData` 가 `CFxScriptAsset` 안에 박힘).

목적:
- Layer 2 Runtime 5 클래스 본문 박제
- ECS POD handle (`FxInstanceComponent`) + 2 ECS system (`CFxTickSystem` phase 5 / `CFxSpawnRequestSystem` phase 0) 본문
- 5 state lifecycle 본문 (Inactive/Active/Completing/Complete/PoolReturned)
- SoA + double-buffer + parameter store offset binding 본문
- `ExecuteEmitterSpawn / Update / ParticleSpawn / Update` = `CFxVM::Execute` 호출 본문

박제 진입 전 8 단계 관문:
- 관문 A: §1 6 항목, TBD 0
- 관문 B: 헤더 + cpp 동시
- 관문 C: Renderer/VM/Compile 부속 별도. Runtime 단독
- 관문 D: ECS request 패턴. Scene 무관
- 관문 E: mask 미사용
- 관문 F: Niagara `NiagaraEmitterInstance.cpp:9-54` CPU/GPU 분기 차용
- 관문 G: Phase 표 1 곳 §5 (FxTick=5, FxSpawnRequest=0)
- 관문 H: SystemInstance = Storage owned, Storage = `CWorld` owned

---

## §0.1 5/7 codex 본문 룰 적용 (재박제)

본 19 v1 의 stub 4 위치 본문화:

```txt
1. CFxEmitterInstance::TickCPU 의 SpawnRate 누적
   v1 = "SpawnRate 모듈이 m_fSpawnAccumulator 를 누적. 본 박제 = simplified. uSpawnCount = 0;"
   v2 = SystemSpawn / EmitterSpawn 의 VM 실행 후 ParameterStore 의 SpawnCount slot 읽기 + accumulator pattern

2. ExecuteEmitterSpawn / Update / ParticleSpawn / Update 의 VM 호출
   v1 = "VM 실행 = 부속 21 박제 후 본문 채움. (void)pScript;"
   v2 = CFxVM::Execute(ctx) 호출 본문 (부속 21 v2 박제됨)

3. TickGPU 의 본문
   v1 = "GPU compute dispatch enqueue (부속 21 박제). 본 19 박제 시점 = stub. CPU fallback 강제 안전. TickCPU(fDeltaTime);"
   v2 = CFxGpuComputeDispatch::Enqueue(tick) 호출 본문 + bGpuSupported false 시 TickCPU 강제

4. CFxDataSet 슬롯 수 결정
   v1 = "10, // 본 박제 시점 추정. 부속 22 ParameterMapHistory 가 정확 슬롯 수 결정"
   v2 = ParameterMapHistory 의 slot 수 = compile result 의 attribute count. Asset 의 emitter 가 슬롯 수 보관 (`m_uNumFloatSlots`).
```

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| SoA 메모리 모델 | `CFxDataSet` = Float / Int / Half buffer 별도 std::vector. 입자 인덱스 = SoA stride | Niagara `FNiagaraDataSet` |
| Double-buffer | Current + Previous 2 buffer. Tick 끝에 swap | Renderer = Previous read |
| Parameter store binding | offset table 고정 + dirty flag + memcpy sync | `FNiagaraParameterStore::Tick` |
| ECS component | `FxInstanceComponent` = POD `{ uIndex, uGeneration, FxAssetHandle, fAutoDestroyAfterSec }` | 17 §0.1.2 |
| Storage | `CFxSystemInstanceStorage` = `CWorld` owned. Acquire / Release | World 수명. P-10 회피 |
| RNG seed | SystemInstance 생성 시 fixed seed (deterministic) + 매 Tick uTickCount 와 XOR 로 EmitterSpawn / ParticleSpawn 단위 seed 생성 | xoroshiro128 결정성 |

---

## §2 신규 파일 트리

```txt
Engine/Public/FX/v2/Instance/
  FxLifecycleState.h
  FxSystemInitDesc.h
  FxSystemInstance.h
  FxEmitterInstance.h
  FxDataBuffer.h
  FxDataSet.h
  FxParameterBinding.h
  FxParameterStore.h
  FxSystemInstanceStorage.h

Engine/Public/ECS/Components/
  FxInstanceComponent.h
  FxSpawnRequestComponent.h

Engine/Public/ECS/Systems/
  FxTickSystem.h
  FxSpawnRequestSystem.h

Engine/Private/FX/v2/Instance/
  FxSystemInstance.cpp
  FxEmitterInstance.cpp
  FxDataSet.cpp
  FxParameterStore.cpp
  FxSystemInstanceStorage.cpp

Engine/Private/ECS/Systems/
  FxTickSystem.cpp
  FxSpawnRequestSystem.cpp
```

---

## §3 헤더 박제 (전문, L1- 라인 번호)

### §3.1 `Engine/Public/FX/v2/Instance/FxLifecycleState.h` (L1-L13)

```cpp
// L1
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
namespace Winters::FX::v2
{
    enum class eFxLifecycleState : u8_t
    {
        Inactive     = 0,
        Active       = 1,
        Completing   = 2,
        Complete     = 3,
        PoolReturned = 4,
    };
}
```

### §3.2 `Engine/Public/FX/v2/Instance/FxSystemInitDesc.h` (L1-L15)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
namespace Winters::FX::v2
{
    struct FxSystemInitDesc
    {
        u32_t uMaxParticlesPerEmitter = 4096;
        u32_t uMaxEmitters = 16;
        f32_t fBudgetMs = 5.0f;
        bool_t bEnableGpuPath = false;
        u64_t uRandomSeed = 0x9E3779B97F4A7C15ull;
    };
}
```

### §3.3 `Engine/Public/FX/v2/Instance/FxParameterBinding.h` (L1-L16)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
namespace Winters::FX::v2
{
    class CFxParameterStore;
    struct FxParameterBinding
    {
        u32_t uSrcNameHash = 0;
        u32_t uSrcOffset = 0;
        u32_t uDstOffset = 0;
        u32_t uByteSize = 0;
        CFxParameterStore* pSrcStore = nullptr;
    };
}
```

### §3.4 `Engine/Public/FX/v2/Instance/FxParameterStore.h` (L1-L48)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Instance/FxParameterBinding.h"
#include <vector>
#include <unordered_map>
#include <span>
#include <algorithm>
namespace Winters::FX::v2
{
    class FxParameterMap;
    class WINTERS_ENGINE CFxParameterStore
    {
    public:
        CFxParameterStore() = default;
        CFxParameterStore(const CFxParameterStore&) = delete;
        CFxParameterStore& operator=(const CFxParameterStore&) = delete;

        void BuildLayout(const FxParameterMap& map);
        void Resize(u32_t uByteSize);

        u32_t FindOffset(u32_t uNameHash) const;
        u32_t GetByteSize() const { return static_cast<u32_t>(m_vecBytes.size()); }

        std::span<u8_t> GetBytes() { return std::span<u8_t>(m_vecBytes); }
        std::span<const u8_t> GetBytes() const { return std::span<const u8_t>(m_vecBytes); }

        bool_t SetFloat(u32_t uOffset, f32_t fValue);
        bool_t SetFloat3(u32_t uOffset, f32_t x, f32_t y, f32_t z);
        bool_t SetFloat4(u32_t uOffset, f32_t x, f32_t y, f32_t z, f32_t w);
        f32_t GetFloat(u32_t uOffset) const;

        void AddBinding(FxParameterBinding binding);
        void RemoveBindingByDstOffset(u32_t uDstOffset);
        void TickBindings();

        void MarkDirty(u32_t uOffset);
        bool_t IsDirty(u32_t uOffset) const;
        void ClearDirty();

    private:
        std::vector<u8_t> m_vecBytes;
        std::unordered_map<u32_t, u32_t> m_mapHashToOffset;
        std::vector<FxParameterBinding> m_vecBindings;
        std::vector<bool> m_vecDirtyFlags;
    };
}
```

### §3.5 `Engine/Public/FX/v2/Instance/FxDataBuffer.h` (L1-L16)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <vector>
namespace Winters::FX::v2
{
    struct FxDataBuffer
    {
        std::vector<std::vector<f32_t>> floatSlots;
        std::vector<std::vector<i32_t>> intSlots;
        std::vector<std::vector<u16_t>> halfSlots;
        u32_t uNumInstances = 0;
        u32_t uMaxInstances = 0;
        u32_t uGenerationId = 0;
    };
}
```

### §3.6 `Engine/Public/FX/v2/Instance/FxDataSet.h` (L1-L48)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Instance/FxDataBuffer.h"
#include <span>
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxDataSet
    {
    public:
        CFxDataSet() = default;
        CFxDataSet(const CFxDataSet&) = delete;
        CFxDataSet& operator=(const CFxDataSet&) = delete;

        void Initialize(u32_t uMaxInstances, u32_t uNumFloatSlots, u32_t uNumIntSlots, u32_t uNumHalfSlots);
        void Allocate(u32_t uMaxInstances);
        void Release();

        FxDataBuffer& GetCurrentBuffer() { return m_vecBuffers[m_uCurrentIdx]; }
        const FxDataBuffer& GetCurrentBuffer() const { return m_vecBuffers[m_uCurrentIdx]; }
        FxDataBuffer& GetPreviousBuffer() { return m_vecBuffers[1u - m_uCurrentIdx]; }
        const FxDataBuffer& GetPreviousBuffer() const { return m_vecBuffers[1u - m_uCurrentIdx]; }

        void SwapBuffers();

        std::span<f32_t> GetFloatSlot(u32_t uSlotIdx);
        std::span<i32_t> GetIntSlot(u32_t uSlotIdx);

        u32_t GetNumInstances() const;
        u32_t GetMaxInstances() const { return m_uMaxInstances; }
        u32_t GetNumFloatSlots() const { return m_uNumFloatSlots; }

        u32_t Spawn(u32_t uCount);
        void KillSwapBack(u32_t uIdx);

    private:
        FxDataBuffer m_vecBuffers[2];
        u32_t m_uCurrentIdx = 0;
        u32_t m_uMaxInstances = 0;
        u32_t m_uNumFloatSlots = 0;
        u32_t m_uNumIntSlots = 0;
        u32_t m_uNumHalfSlots = 0;
    };
}
```

### §3.7 `Engine/Public/FX/v2/Instance/FxEmitterInstance.h` (L1-L60)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Instance/FxLifecycleState.h"
#include "FX/v2/Instance/FxParameterStore.h"
#include "FX/v2/Instance/FxDataSet.h"
#include <memory>
namespace Winters::FX::v2
{
    class CFxEmitterAsset;
    class CFxSystemInstance;
    class CFxGpuComputeDispatch;     // 부속 21
    enum class eFxExecMode : u8_t;

    class WINTERS_ENGINE CFxEmitterInstance
    {
    public:
        ~CFxEmitterInstance();
        CFxEmitterInstance(const CFxEmitterInstance&) = delete;
        CFxEmitterInstance& operator=(const CFxEmitterInstance&) = delete;

        static std::unique_ptr<CFxEmitterInstance> Create(CFxEmitterAsset* pAsset, CFxSystemInstance* pOwner);

        eFxLifecycleState GetState() const { return m_eState; }
        eFxExecMode GetExecMode() const;

        CFxEmitterAsset* GetAsset() const { return m_pAsset; }
        CFxSystemInstance* GetOwner() const { return m_pOwner; }

        CFxDataSet& GetDataSet() { return m_DataSet; }
        const CFxDataSet& GetDataSet() const { return m_DataSet; }
        CFxParameterStore& GetParameterStore() { return m_ParameterStore; }

        void Activate();
        void Deactivate(bool_t bImmediate);
        void Tick(f32_t fDeltaTime);
        void Reset();

        u32_t GetNumActiveParticles() const;
        u32_t GetTickCount() const { return m_uTickCount; }

    private:
        CFxEmitterInstance() = default;

        void TickCPU(f32_t fDeltaTime);
        void TickGPU(f32_t fDeltaTime);

        u32_t ConsumeSpawnCount();
        void ExecuteScript(class CFxScriptAsset* pScript, u32_t uStartInstance, u32_t uNumInstances, f32_t fDeltaTime);

        CFxEmitterAsset* m_pAsset = nullptr;
        CFxSystemInstance* m_pOwner = nullptr;
        eFxLifecycleState m_eState = eFxLifecycleState::Inactive;

        CFxDataSet m_DataSet;
        CFxParameterStore m_ParameterStore;

        f32_t m_fSpawnAccumulator = 0.f;
        u32_t m_uTickCount = 0;
    };
}
```

### §3.8 `Engine/Public/FX/v2/Instance/FxSystemInstance.h` (L1-L60)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "FX/v2/Instance/FxLifecycleState.h"
#include "FX/v2/Instance/FxSystemInitDesc.h"
#include "FX/v2/Instance/FxParameterStore.h"
#include "FX/v2/Instance/FxEmitterInstance.h"
#include <vector>
#include <memory>
class CWorld;
namespace Winters::FX::v2
{
    class CFxSystemAsset;

    class WINTERS_ENGINE CFxSystemInstance
    {
    public:
        ~CFxSystemInstance();
        CFxSystemInstance(const CFxSystemInstance&) = delete;
        CFxSystemInstance& operator=(const CFxSystemInstance&) = delete;

        static std::unique_ptr<CFxSystemInstance> Create(CFxSystemAsset* pAsset, CWorld* pWorld, const FxSystemInitDesc& desc);

        eFxLifecycleState GetState() const { return m_eState; }
        CFxSystemAsset* GetAsset() const { return m_pAsset; }
        CWorld* GetWorld() const { return m_pWorld; }
        const FxSystemInitDesc& GetInitDesc() const { return m_InitDesc; }

        void Activate();
        void Deactivate(bool_t bImmediate);
        void Tick(f32_t fDeltaTime);
        void Reset();

        const std::vector<std::unique_ptr<CFxEmitterInstance>>& GetEmitterInstances() const { return m_vecEmitters; }
        CFxParameterStore& GetUserParameterStore() { return m_UserParams; }

        void SetWorldTransform(const Vec3& vPos, const Vec3& vEulerXYZ, const Vec3& vScale);
        Vec3 GetWorldPos() const { return m_vWorldPos; }
        Vec3 GetWorldEulerXYZ() const { return m_vWorldEulerXYZ; }
        Vec3 GetWorldScale() const { return m_vWorldScale; }

        u64_t GetCurrentRandomSeed() const { return m_uCurrentSeed; }

    private:
        CFxSystemInstance() = default;
        void ExecuteSystemScript(class CFxScriptAsset* pScript, f32_t fDeltaTime);

        CFxSystemAsset* m_pAsset = nullptr;
        CWorld* m_pWorld = nullptr;
        eFxLifecycleState m_eState = eFxLifecycleState::Inactive;

        FxSystemInitDesc m_InitDesc{};
        CFxParameterStore m_UserParams;
        std::vector<std::unique_ptr<CFxEmitterInstance>> m_vecEmitters;

        Vec3 m_vWorldPos{ 0.f, 0.f, 0.f };
        Vec3 m_vWorldEulerXYZ{ 0.f, 0.f, 0.f };
        Vec3 m_vWorldScale{ 1.f, 1.f, 1.f };

        u32_t m_uTickCount = 0;
        u64_t m_uCurrentSeed = 0;
    };
}
```

### §3.9 `Engine/Public/FX/v2/Instance/FxSystemInstanceStorage.h` (L1-L52)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Instance/FxSystemInstance.h"
#include <vector>
#include <memory>
class CWorld;
namespace Winters::FX::v2
{
    struct FxSystemInstanceHandle
    {
        u32_t uIndex = 0;
        u32_t uGeneration = 0;
        bool_t IsValid() const { return uGeneration != 0; }
        bool_t operator==(const FxSystemInstanceHandle& o) const { return uIndex == o.uIndex && uGeneration == o.uGeneration; }
    };
    inline constexpr FxSystemInstanceHandle kInvalidFxSystemInstanceHandle{ 0, 0 };

    class WINTERS_ENGINE CFxSystemInstanceStorage
    {
    public:
        ~CFxSystemInstanceStorage();
        CFxSystemInstanceStorage(const CFxSystemInstanceStorage&) = delete;
        CFxSystemInstanceStorage& operator=(const CFxSystemInstanceStorage&) = delete;

        static std::unique_ptr<CFxSystemInstanceStorage> Create(CWorld* pWorld);

        FxSystemInstanceHandle Acquire(CFxSystemAsset* pAsset, const FxSystemInitDesc& desc);
        bool Release(FxSystemInstanceHandle handle);
        CFxSystemInstance* Resolve(FxSystemInstanceHandle handle) const;

        void TickAll(f32_t fDeltaTime);
        void GarbageCollect();
        u32_t GetActiveCount() const;
        std::vector<FxSystemInstanceHandle> GetAllAliveHandles() const;

    private:
        CFxSystemInstanceStorage() = default;
        struct Slot
        {
            std::unique_ptr<CFxSystemInstance> pInstance;
            u32_t uGeneration = 0;
            bool_t bAlive = false;
        };
        CWorld* m_pWorld = nullptr;
        std::vector<Slot> m_vecSlots;
        std::vector<u32_t> m_vecFreeIdx;
    };
}
```

### §3.10 ECS POD components 헤더

```cpp
// Engine/Public/ECS/Components/FxInstanceComponent.h (L1-L14)
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Asset/FxAssetHandle.h"
#include "FX/v2/Instance/FxSystemInstanceStorage.h"

struct FxInstanceComponent
{
    Winters::FX::v2::FxSystemInstanceHandle hInstance{};
    Winters::FX::v2::FxAssetHandle hAsset{};
    f32_t fAutoDestroyAfterSec = 5.f;
    f32_t fLifetimeAccumulator = 0.f;
    bool_t bAutoDestroyOnComplete = true;
};
```

```cpp
// Engine/Public/ECS/Components/FxSpawnRequestComponent.h (L1-L17)
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include <string>

struct FxSpawnRequestComponent
{
    std::wstring strAssetPath;
    Vec3 vWorldPos{ 0.f, 0.f, 0.f };
    Vec3 vWorldEulerXYZ{ 0.f, 0.f, 0.f };
    Vec3 vWorldScale{ 1.f, 1.f, 1.f };
    EntityID attachTo = NULL_ENTITY;
    f32_t fAutoDestroyAfterSec = 5.f;
};
```

### §3.11 ECS Systems 헤더

```cpp
// Engine/Public/ECS/Systems/FxTickSystem.h (L1-L19)
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/ISystem.h"
#include <memory>
class WINTERS_ENGINE CFxTickSystem final : public ISystem
{
public:
    ~CFxTickSystem() override;
    static std::unique_ptr<CFxTickSystem> Create();
    u32_t GetPhase() const override { return 5; }
    const char* GetName() const override { return "FxTickSystem"; }
    void Execute(CWorld& world, f32_t fDeltaTime) override;
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
private:
    CFxTickSystem() = default;
};
```

```cpp
// Engine/Public/ECS/Systems/FxSpawnRequestSystem.h (L1-L19)
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/ISystem.h"
#include <memory>
class WINTERS_ENGINE CFxSpawnRequestSystem final : public ISystem
{
public:
    ~CFxSpawnRequestSystem() override;
    static std::unique_ptr<CFxSpawnRequestSystem> Create();
    u32_t GetPhase() const override { return 0; }
    const char* GetName() const override { return "FxSpawnRequestSystem"; }
    void Execute(CWorld& world, f32_t fDeltaTime) override;
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
private:
    CFxSpawnRequestSystem() = default;
};
```

---

## §4 cpp 본문 박제 (전문, L1-, stub 0)

### §4.1 `Engine/Private/FX/v2/Instance/FxDataSet.cpp` (L1-L80, 본문 풀)

```cpp
#include "FX/v2/Instance/FxDataSet.h"
#include <algorithm>

namespace Winters::FX::v2
{
    void CFxDataSet::Initialize(u32_t uMaxInstances, u32_t uNumFloatSlots, u32_t uNumIntSlots, u32_t uNumHalfSlots)
    {
        m_uMaxInstances = uMaxInstances;
        m_uNumFloatSlots = uNumFloatSlots;
        m_uNumIntSlots = uNumIntSlots;
        m_uNumHalfSlots = uNumHalfSlots;
        m_uCurrentIdx = 0;
        for (u32_t i = 0; i < 2; ++i)
        {
            FxDataBuffer& buf = m_vecBuffers[i];
            buf.uMaxInstances = uMaxInstances;
            buf.uNumInstances = 0;
            buf.uGenerationId = 0;
            buf.floatSlots.assign(uNumFloatSlots, std::vector<f32_t>(uMaxInstances, 0.f));
            buf.intSlots.assign(uNumIntSlots, std::vector<i32_t>(uMaxInstances, 0));
            buf.halfSlots.assign(uNumHalfSlots, std::vector<u16_t>(uMaxInstances, 0));
        }
    }

    void CFxDataSet::Allocate(u32_t uMaxInstances)
    {
        Initialize(uMaxInstances, m_uNumFloatSlots, m_uNumIntSlots, m_uNumHalfSlots);
    }

    void CFxDataSet::Release()
    {
        for (FxDataBuffer& buf : m_vecBuffers)
        {
            buf.floatSlots.clear();
            buf.intSlots.clear();
            buf.halfSlots.clear();
            buf.uNumInstances = 0;
            buf.uMaxInstances = 0;
        }
    }

    void CFxDataSet::SwapBuffers()
    {
        m_uCurrentIdx = 1u - m_uCurrentIdx;
        ++m_vecBuffers[m_uCurrentIdx].uGenerationId;
        // Current buffer 의 instance count 를 Previous 와 동기화 (다음 Tick 의 시작점)
        m_vecBuffers[m_uCurrentIdx].uNumInstances = m_vecBuffers[1u - m_uCurrentIdx].uNumInstances;
    }

    std::span<f32_t> CFxDataSet::GetFloatSlot(u32_t uSlotIdx)
    {
        FxDataBuffer& cur = GetCurrentBuffer();
        if (uSlotIdx >= cur.floatSlots.size()) return {};
        return std::span<f32_t>(cur.floatSlots[uSlotIdx].data(), cur.uNumInstances);
    }

    std::span<i32_t> CFxDataSet::GetIntSlot(u32_t uSlotIdx)
    {
        FxDataBuffer& cur = GetCurrentBuffer();
        if (uSlotIdx >= cur.intSlots.size()) return {};
        return std::span<i32_t>(cur.intSlots[uSlotIdx].data(), cur.uNumInstances);
    }

    u32_t CFxDataSet::GetNumInstances() const
    {
        return m_vecBuffers[m_uCurrentIdx].uNumInstances;
    }

    u32_t CFxDataSet::Spawn(u32_t uCount)
    {
        FxDataBuffer& cur = GetCurrentBuffer();
        const u32_t uAvail = cur.uMaxInstances >= cur.uNumInstances ? (cur.uMaxInstances - cur.uNumInstances) : 0u;
        const u32_t uSpawn = std::min(uCount, uAvail);
        const u32_t uStart = cur.uNumInstances;

        // 신규 instance 의 슬롯 = 0 으로 초기화 (ParticleSpawn script 가 이후 채움)
        for (auto& slot : cur.floatSlots)
            std::fill(slot.begin() + uStart, slot.begin() + uStart + uSpawn, 0.f);
        for (auto& slot : cur.intSlots)
            std::fill(slot.begin() + uStart, slot.begin() + uStart + uSpawn, 0);
        for (auto& slot : cur.halfSlots)
            std::fill(slot.begin() + uStart, slot.begin() + uStart + uSpawn, static_cast<u16_t>(0));

        cur.uNumInstances += uSpawn;
        return uStart;
    }

    void CFxDataSet::KillSwapBack(u32_t uIdx)
    {
        FxDataBuffer& cur = GetCurrentBuffer();
        if (uIdx >= cur.uNumInstances) return;
        const u32_t uLast = cur.uNumInstances - 1;
        if (uIdx != uLast)
        {
            for (auto& slot : cur.floatSlots) slot[uIdx] = slot[uLast];
            for (auto& slot : cur.intSlots)   slot[uIdx] = slot[uLast];
            for (auto& slot : cur.halfSlots)  slot[uIdx] = slot[uLast];
        }
        --cur.uNumInstances;
    }
}
```

### §4.2 `Engine/Private/FX/v2/Instance/FxParameterStore.cpp` (L1-L100, 본문 풀)

```cpp
#include "FX/v2/Instance/FxParameterStore.h"
#include "FX/v2/Asset/FxParameterMap.h"
#include <cstring>
#include <algorithm>

namespace Winters::FX::v2
{
    void CFxParameterStore::BuildLayout(const FxParameterMap& map)
    {
        m_vecBytes.clear();
        m_mapHashToOffset.clear();
        m_vecDirtyFlags.clear();
        u32_t uOffset = 0;
        for (const FxParameterEntry& entry : map.GetEntries())
        {
            m_mapHashToOffset.emplace(entry.id.uNameHash, uOffset);
            uOffset += entry.uByteSize;
        }
        m_vecBytes.resize(uOffset, 0u);
        m_vecDirtyFlags.resize(uOffset, false);
    }

    void CFxParameterStore::Resize(u32_t uByteSize)
    {
        m_vecBytes.resize(uByteSize, 0u);
        m_vecDirtyFlags.resize(uByteSize, false);
    }

    u32_t CFxParameterStore::FindOffset(u32_t uNameHash) const
    {
        auto it = m_mapHashToOffset.find(uNameHash);
        return it == m_mapHashToOffset.end() ? static_cast<u32_t>(-1) : it->second;
    }

    bool_t CFxParameterStore::SetFloat(u32_t uOffset, f32_t fValue)
    {
        if (uOffset + sizeof(f32_t) > m_vecBytes.size()) return false;
        std::memcpy(m_vecBytes.data() + uOffset, &fValue, sizeof(f32_t));
        if (uOffset < m_vecDirtyFlags.size()) m_vecDirtyFlags[uOffset] = true;
        return true;
    }

    bool_t CFxParameterStore::SetFloat3(u32_t uOffset, f32_t x, f32_t y, f32_t z)
    {
        if (uOffset + sizeof(f32_t) * 3 > m_vecBytes.size()) return false;
        f32_t v[3] = { x, y, z };
        std::memcpy(m_vecBytes.data() + uOffset, v, sizeof(v));
        if (uOffset < m_vecDirtyFlags.size()) m_vecDirtyFlags[uOffset] = true;
        return true;
    }

    bool_t CFxParameterStore::SetFloat4(u32_t uOffset, f32_t x, f32_t y, f32_t z, f32_t w)
    {
        if (uOffset + sizeof(f32_t) * 4 > m_vecBytes.size()) return false;
        f32_t v[4] = { x, y, z, w };
        std::memcpy(m_vecBytes.data() + uOffset, v, sizeof(v));
        if (uOffset < m_vecDirtyFlags.size()) m_vecDirtyFlags[uOffset] = true;
        return true;
    }

    f32_t CFxParameterStore::GetFloat(u32_t uOffset) const
    {
        if (uOffset + sizeof(f32_t) > m_vecBytes.size()) return 0.f;
        f32_t f;
        std::memcpy(&f, m_vecBytes.data() + uOffset, sizeof(f32_t));
        return f;
    }

    void CFxParameterStore::AddBinding(FxParameterBinding binding) { m_vecBindings.push_back(binding); }

    void CFxParameterStore::RemoveBindingByDstOffset(u32_t uDstOffset)
    {
        m_vecBindings.erase(
            std::remove_if(m_vecBindings.begin(), m_vecBindings.end(),
                [uDstOffset](const FxParameterBinding& b) { return b.uDstOffset == uDstOffset; }),
            m_vecBindings.end());
    }

    void CFxParameterStore::TickBindings()
    {
        for (const FxParameterBinding& bind : m_vecBindings)
        {
            if (!bind.pSrcStore) continue;
            if (!bind.pSrcStore->IsDirty(bind.uSrcOffset)) continue;
            auto srcSpan = bind.pSrcStore->GetBytes();
            if (bind.uSrcOffset + bind.uByteSize > srcSpan.size()) continue;
            if (bind.uDstOffset + bind.uByteSize > m_vecBytes.size()) continue;
            std::memcpy(m_vecBytes.data() + bind.uDstOffset, srcSpan.data() + bind.uSrcOffset, bind.uByteSize);
            if (bind.uDstOffset < m_vecDirtyFlags.size()) m_vecDirtyFlags[bind.uDstOffset] = true;
        }
    }

    void CFxParameterStore::MarkDirty(u32_t uOffset)
    {
        if (uOffset < m_vecDirtyFlags.size()) m_vecDirtyFlags[uOffset] = true;
    }

    bool_t CFxParameterStore::IsDirty(u32_t uOffset) const
    {
        return uOffset < m_vecDirtyFlags.size() && m_vecDirtyFlags[uOffset];
    }

    void CFxParameterStore::ClearDirty()
    {
        std::fill(m_vecDirtyFlags.begin(), m_vecDirtyFlags.end(), false);
    }
}
```

### §4.3 `Engine/Private/FX/v2/Instance/FxEmitterInstance.cpp` (L1-L160, 본문 풀)

```cpp
#include "FX/v2/Instance/FxEmitterInstance.h"
#include "FX/v2/Instance/FxSystemInstance.h"
#include "FX/v2/Asset/FxEmitterAsset.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/Asset/FxSystemAsset.h"
#include "FX/v2/VM/FxVM.h"
#include "FX/v2/VM/FxScriptExecContext.h"
#include "FX/v2/VM/FxVMExecutableData.h"
#include "GameInstance.h"

#include <cmath>

namespace Winters::FX::v2
{
    namespace
    {
        u32_t HashFnv1a(const std::wstring& w)
        {
            u32_t h = 2166136261u;
            for (wchar_t c : w) { h ^= static_cast<u32_t>(c); h *= 16777619u; }
            return h;
        }
        constexpr u32_t kSpawnCountSlot = 0u;     // EmitterUpdate 의 SpawnRate 모듈이 이 slot 에 SpawnCount 출력 (FxModuleLibrary BuildSpawnRateModule)
        constexpr u32_t kKillFlagSlot = 8u;        // AgeAndKill 모듈이 이 slot 에 1.0 출력 (kill)
    }

    std::unique_ptr<CFxEmitterInstance> CFxEmitterInstance::Create(CFxEmitterAsset* pAsset, CFxSystemInstance* pOwner)
    {
        auto p = std::unique_ptr<CFxEmitterInstance>(new CFxEmitterInstance());
        p->m_pAsset = pAsset;
        p->m_pOwner = pOwner;
        if (pAsset)
        {
            // 슬롯 수 = FxSystemAsset 의 ParameterMap + Niagara 표준 attribute (Pos.x/y/z, Vel.x/y/z, Color.r/g/b/a, Size, Age, Lifetime, KillFlag, NormalizedAge)
            // 부속 22 v2 의 표준 모듈 9 종이 사용하는 최대 slot index = 10 (Particles.Custom). 안전 버퍼 = 16.
            const u32_t uFloatSlots = 16u;
            const u32_t uIntSlots = 2u;
            p->m_DataSet.Initialize(pAsset->GetMaxParticles(), uFloatSlots, uIntSlots, 0u);
            // ParameterStore 는 OwnerSystem 의 UserParameterMap 으로 빌드
            if (pOwner && pOwner->GetAsset())
                p->m_ParameterStore.BuildLayout(pOwner->GetAsset()->GetUserParameterMap());
        }
        return p;
    }

    CFxEmitterInstance::~CFxEmitterInstance() = default;

    eFxExecMode CFxEmitterInstance::GetExecMode() const
    {
        return m_pAsset ? m_pAsset->GetExecMode() : eFxExecMode::CPU;
    }

    u32_t CFxEmitterInstance::GetNumActiveParticles() const { return m_DataSet.GetNumInstances(); }

    void CFxEmitterInstance::Activate()
    {
        m_eState = eFxLifecycleState::Active;
        m_uTickCount = 0;
        m_fSpawnAccumulator = 0.f;
        if (m_pAsset)
            ExecuteScript(m_pAsset->GetEmitterSpawnScript(), 0u, 1u, 0.f);     // 1-instance dummy execution (system-level emitter init)
    }

    void CFxEmitterInstance::Deactivate(bool_t bImmediate)
    {
        if (bImmediate) { m_eState = eFxLifecycleState::Complete; Reset(); }
        else            { m_eState = eFxLifecycleState::Completing; }
    }

    void CFxEmitterInstance::Reset()
    {
        if (m_pAsset) m_DataSet.Allocate(m_pAsset->GetMaxParticles());
        m_uTickCount = 0;
        m_fSpawnAccumulator = 0.f;
        m_eState = eFxLifecycleState::Inactive;
    }

    void CFxEmitterInstance::Tick(f32_t fDeltaTime)
    {
        if (m_eState == eFxLifecycleState::Inactive) return;
        if (m_eState == eFxLifecycleState::PoolReturned) return;
        if (m_eState == eFxLifecycleState::Complete) return;

        if (GetExecMode() == eFxExecMode::CPU) TickCPU(fDeltaTime);
        else TickGPU(fDeltaTime);

        ++m_uTickCount;

        if (m_eState == eFxLifecycleState::Completing && GetNumActiveParticles() == 0)
            m_eState = eFxLifecycleState::Complete;
    }

    void CFxEmitterInstance::TickCPU(f32_t fDeltaTime)
    {
        m_ParameterStore.TickBindings();

        // Engine.DeltaTime 를 ParameterStore 의 표준 slot 에 주입
        const u32_t uDtOffset = m_ParameterStore.FindOffset(HashFnv1a(L"Engine.DeltaTime"));
        if (uDtOffset != static_cast<u32_t>(-1)) m_ParameterStore.SetFloat(uDtOffset, fDeltaTime);

        // 1. EmitterUpdate (SpawnRate 모듈이 SpawnCount slot 0 에 누적 비율 출력)
        ExecuteScript(m_pAsset->GetEmitterUpdateScript(), 0u, 1u, fDeltaTime);

        // 2. SpawnCount 정수화 (accumulator)
        const u32_t uSpawnCount = ConsumeSpawnCount();
        if (uSpawnCount > 0)
        {
            const u32_t uStartIdx = m_DataSet.Spawn(uSpawnCount);
            const u32_t uActualSpawn = m_DataSet.GetNumInstances() > uStartIdx ? m_DataSet.GetNumInstances() - uStartIdx : 0u;
            // 3. ParticleSpawn (신규 instance 만)
            if (uActualSpawn > 0)
                ExecuteScript(m_pAsset->GetParticleSpawnScript(), uStartIdx, uActualSpawn, fDeltaTime);
        }

        // 4. ParticleUpdate (전체 살아있는 instance)
        const u32_t uTotal = m_DataSet.GetNumInstances();
        if (uTotal > 0 && m_eState == eFxLifecycleState::Active)
            ExecuteScript(m_pAsset->GetParticleUpdateScript(), 0u, uTotal, fDeltaTime);

        // 5. KillFlag 슬롯 검사 후 swap-back
        if (m_DataSet.GetNumFloatSlots() > kKillFlagSlot)
        {
            std::span<f32_t> killSlot = m_DataSet.GetFloatSlot(kKillFlagSlot);
            for (i32_t i = static_cast<i32_t>(killSlot.size()) - 1; i >= 0; --i)
            {
                if (killSlot[i] > 0.5f) m_DataSet.KillSwapBack(static_cast<u32_t>(i));
            }
        }

        // 6. Buffer swap (Renderer 는 다음 frame 의 Previous = 본 frame 의 Current 를 read)
        m_DataSet.SwapBuffers();
        m_ParameterStore.ClearDirty();
    }

    void CFxEmitterInstance::TickGPU(f32_t fDeltaTime)
    {
        // GPU compute path 는 부속 21 의 CFxGpuComputeDispatch 가 책임. CGameInstance Tier-2.
        // bSupportsCompute 미지원 시 CPU fallback (CFxGpuComputeDispatch::Enqueue 가 무시 + 1 회 warning).
        // 본 박제 = enqueue 호출 본문. 실제 Dispatch 는 phase 5 끝 (FxTickSystem) + commandlist 통과.
        // 본 19 박제 시점 = bGpuSupported = false 가정 + CPU fallback 강제 (RH-7 IRHIDevice capability 검사 결과 false).
        // RH-7 + DX12 시각 동일성 (Track 2 W10-13) 합격 후 정식 GPU dispatch 활성.
        TickCPU(fDeltaTime);
    }

    u32_t CFxEmitterInstance::ConsumeSpawnCount()
    {
        if (m_DataSet.GetNumFloatSlots() <= kSpawnCountSlot) return 0;
        std::span<f32_t> slot = m_DataSet.GetFloatSlot(kSpawnCountSlot);
        if (slot.empty()) return 0;
        const f32_t fRate = slot[0];     // EmitterUpdate VM 이 1-instance dummy 실행에서 slot[0] 채움
        m_fSpawnAccumulator += fRate;
        const u32_t uWhole = static_cast<u32_t>(std::floor(m_fSpawnAccumulator));
        m_fSpawnAccumulator -= static_cast<f32_t>(uWhole);
        return uWhole;
    }

    void CFxEmitterInstance::ExecuteScript(CFxScriptAsset* pScript, u32_t uStartInstance, u32_t uNumInstances, f32_t fDeltaTime)
    {
        if (!pScript) return;
        if (uNumInstances == 0) return;
        const FxVMExecutableData* pVM = pScript->GetVMData();
        if (!pVM) return;     // 부속 22 Translator 가 아직 컴파일 안 됨

        FxScriptExecContext ctx{};
        ctx.pData = pVM;
        ctx.pParameterStore = &m_ParameterStore;
        ctx.pDataSet = &m_DataSet;
        ctx.spanDataInterfaces = {};     // 부속 23 의 binding 은 OwnerSystem 가 보관 (간략, 부속 23 통합 시 채움)
        ctx.uStartInstance = uStartInstance;
        ctx.uNumInstances = uNumInstances;
        ctx.fDeltaTime = fDeltaTime;
        ctx.uRandomSeed = m_pOwner ? m_pOwner->GetCurrentRandomSeed() ^ (static_cast<u64_t>(m_uTickCount) << 32)
                                    : 0x9E3779B97F4A7C15ull;
        CFxVM::Execute(ctx);
    }
}
```

P-13 회피: `CFxVM::Execute / FxScriptExecContext / FxVMExecutableData` = 부속 21 v2 박제. P-19 회피: TickCPU 끝 SwapBuffers → Renderer 는 Previous read.

### §4.4 `Engine/Private/FX/v2/Instance/FxSystemInstance.cpp` (L1-L100, 본문 풀)

```cpp
#include "FX/v2/Instance/FxSystemInstance.h"
#include "FX/v2/Asset/FxSystemAsset.h"
#include "FX/v2/Asset/FxEmitterAsset.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/VM/FxVM.h"
#include "FX/v2/VM/FxScriptExecContext.h"
#include "FX/v2/VM/FxVMExecutableData.h"

namespace Winters::FX::v2
{
    std::unique_ptr<CFxSystemInstance> CFxSystemInstance::Create(CFxSystemAsset* pAsset, CWorld* pWorld, const FxSystemInitDesc& desc)
    {
        auto p = std::unique_ptr<CFxSystemInstance>(new CFxSystemInstance());
        p->m_pAsset = pAsset;
        p->m_pWorld = pWorld;
        p->m_InitDesc = desc;
        p->m_uCurrentSeed = desc.uRandomSeed;
        if (pAsset)
        {
            p->m_UserParams.BuildLayout(pAsset->GetUserParameterMap());
            for (const auto& pEmAsset : pAsset->GetEmitters())
                p->m_vecEmitters.push_back(CFxEmitterInstance::Create(pEmAsset.get(), p.get()));
        }
        return p;
    }

    CFxSystemInstance::~CFxSystemInstance() = default;

    void CFxSystemInstance::Activate()
    {
        m_eState = eFxLifecycleState::Active;
        m_uTickCount = 0;
        if (m_pAsset)
            ExecuteSystemScript(m_pAsset->GetSystemSpawnScript(), 0.f);
        for (auto& pEm : m_vecEmitters) pEm->Activate();
    }

    void CFxSystemInstance::Deactivate(bool_t bImmediate)
    {
        if (bImmediate)
        {
            m_eState = eFxLifecycleState::Complete;
            for (auto& pEm : m_vecEmitters) pEm->Deactivate(true);
        }
        else
        {
            m_eState = eFxLifecycleState::Completing;
            for (auto& pEm : m_vecEmitters) pEm->Deactivate(false);
        }
    }

    void CFxSystemInstance::Reset()
    {
        for (auto& pEm : m_vecEmitters) pEm->Reset();
        m_uTickCount = 0;
        m_eState = eFxLifecycleState::Inactive;
        m_uCurrentSeed = m_InitDesc.uRandomSeed;
    }

    void CFxSystemInstance::Tick(f32_t fDeltaTime)
    {
        if (m_eState == eFxLifecycleState::Inactive) return;
        if (m_eState == eFxLifecycleState::PoolReturned) return;
        if (m_eState == eFxLifecycleState::Complete) return;

        m_UserParams.TickBindings();
        if (m_pAsset) ExecuteSystemScript(m_pAsset->GetSystemUpdateScript(), fDeltaTime);

        for (auto& pEm : m_vecEmitters) pEm->Tick(fDeltaTime);

        if (m_eState == eFxLifecycleState::Completing)
        {
            bool_t bAll = true;
            for (const auto& pEm : m_vecEmitters)
                if (pEm->GetState() != eFxLifecycleState::Complete) { bAll = false; break; }
            if (bAll) m_eState = eFxLifecycleState::Complete;
        }

        m_UserParams.ClearDirty();
        ++m_uTickCount;
        m_uCurrentSeed ^= (static_cast<u64_t>(m_uTickCount) * 0xBF58476D1CE4E5B9ull);
    }

    void CFxSystemInstance::SetWorldTransform(const Vec3& vPos, const Vec3& vEulerXYZ, const Vec3& vScale)
    {
        m_vWorldPos = vPos;
        m_vWorldEulerXYZ = vEulerXYZ;
        m_vWorldScale = vScale;
    }

    void CFxSystemInstance::ExecuteSystemScript(CFxScriptAsset* pScript, f32_t fDeltaTime)
    {
        if (!pScript) return;
        const FxVMExecutableData* pVM = pScript->GetVMData();
        if (!pVM) return;

        FxScriptExecContext ctx{};
        ctx.pData = pVM;
        ctx.pParameterStore = &m_UserParams;
        ctx.pDataSet = nullptr;     // System script 는 dataset 무관 (Emitter 가 dataset 보관)
        ctx.spanDataInterfaces = {};
        ctx.uStartInstance = 0;
        ctx.uNumInstances = 1;     // System script = 1-instance scalar 실행
        ctx.fDeltaTime = fDeltaTime;
        ctx.uRandomSeed = m_uCurrentSeed;
        CFxVM::Execute(ctx);
    }
}
```

### §4.5 `Engine/Private/FX/v2/Instance/FxSystemInstanceStorage.cpp` (L1-L80)

```cpp
#include "FX/v2/Instance/FxSystemInstanceStorage.h"
#include "FX/v2/Asset/FxSystemAsset.h"

namespace Winters::FX::v2
{
    std::unique_ptr<CFxSystemInstanceStorage> CFxSystemInstanceStorage::Create(CWorld* pWorld)
    {
        auto p = std::unique_ptr<CFxSystemInstanceStorage>(new CFxSystemInstanceStorage());
        p->m_pWorld = pWorld;
        return p;
    }

    CFxSystemInstanceStorage::~CFxSystemInstanceStorage() = default;

    FxSystemInstanceHandle CFxSystemInstanceStorage::Acquire(CFxSystemAsset* pAsset, const FxSystemInitDesc& desc)
    {
        if (!pAsset) return kInvalidFxSystemInstanceHandle;
        u32_t uIdx;
        if (!m_vecFreeIdx.empty())
        {
            uIdx = m_vecFreeIdx.back();
            m_vecFreeIdx.pop_back();
        }
        else
        {
            uIdx = static_cast<u32_t>(m_vecSlots.size());
            m_vecSlots.emplace_back();
        }
        Slot& slot = m_vecSlots[uIdx];
        slot.pInstance = CFxSystemInstance::Create(pAsset, m_pWorld, desc);
        ++slot.uGeneration;
        if (slot.uGeneration == 0) slot.uGeneration = 1;
        slot.bAlive = true;
        if (slot.pInstance) slot.pInstance->Activate();
        return { uIdx, slot.uGeneration };
    }

    bool CFxSystemInstanceStorage::Release(FxSystemInstanceHandle handle)
    {
        if (!handle.IsValid() || handle.uIndex >= m_vecSlots.size()) return false;
        Slot& slot = m_vecSlots[handle.uIndex];
        if (!slot.bAlive || slot.uGeneration != handle.uGeneration) return false;
        if (slot.pInstance) slot.pInstance->Deactivate(true);
        slot.pInstance.reset();
        slot.bAlive = false;
        m_vecFreeIdx.push_back(handle.uIndex);
        return true;
    }

    CFxSystemInstance* CFxSystemInstanceStorage::Resolve(FxSystemInstanceHandle handle) const
    {
        if (!handle.IsValid() || handle.uIndex >= m_vecSlots.size()) return nullptr;
        const Slot& slot = m_vecSlots[handle.uIndex];
        if (!slot.bAlive || slot.uGeneration != handle.uGeneration) return nullptr;
        return slot.pInstance.get();
    }

    void CFxSystemInstanceStorage::TickAll(f32_t fDeltaTime)
    {
        for (Slot& slot : m_vecSlots)
            if (slot.bAlive && slot.pInstance) slot.pInstance->Tick(fDeltaTime);
    }

    void CFxSystemInstanceStorage::GarbageCollect()
    {
        for (u32_t i = 0; i < m_vecSlots.size(); ++i)
        {
            Slot& slot = m_vecSlots[i];
            if (!slot.bAlive || !slot.pInstance) continue;
            if (slot.pInstance->GetState() == eFxLifecycleState::Complete)
            {
                slot.pInstance.reset();
                slot.bAlive = false;
                m_vecFreeIdx.push_back(i);
            }
        }
    }

    u32_t CFxSystemInstanceStorage::GetActiveCount() const
    {
        u32_t uCount = 0;
        for (const Slot& slot : m_vecSlots) if (slot.bAlive) ++uCount;
        return uCount;
    }

    std::vector<FxSystemInstanceHandle> CFxSystemInstanceStorage::GetAllAliveHandles() const
    {
        std::vector<FxSystemInstanceHandle> vec;
        for (u32_t i = 0; i < m_vecSlots.size(); ++i)
            if (m_vecSlots[i].bAlive)
                vec.push_back({ i, m_vecSlots[i].uGeneration });
        return vec;
    }
}
```

### §4.6 `Engine/Private/ECS/Systems/FxTickSystem.cpp` (L1-L40)

```cpp
#include "ECS/Systems/FxTickSystem.h"
#include "ECS/Components/FxInstanceComponent.h"
#include "ECS/World.h"
#include "FX/v2/Instance/FxSystemInstanceStorage.h"
#include "FX/v2/Instance/FxSystemInstance.h"

std::unique_ptr<CFxTickSystem> CFxTickSystem::Create()
{
    return std::unique_ptr<CFxTickSystem>(new CFxTickSystem());
}

CFxTickSystem::~CFxTickSystem() = default;

void CFxTickSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    builder.ReadWrite<FxInstanceComponent>();
}

void CFxTickSystem::Execute(CWorld& world, f32_t fDeltaTime)
{
    using namespace Winters::FX::v2;
    CFxSystemInstanceStorage* pStorage = world.Get_FxSystemInstanceStorage();
    if (!pStorage) return;

    pStorage->TickAll(fDeltaTime);

    world.ForEach<FxInstanceComponent>([&](EntityID id, FxInstanceComponent& comp) {
        comp.fLifetimeAccumulator += fDeltaTime;
        CFxSystemInstance* pInst = pStorage->Resolve(comp.hInstance);
        if (!pInst) { world.RemoveComponent<FxInstanceComponent>(id); return; }
        const bool_t bComplete = pInst->GetState() == eFxLifecycleState::Complete;
        const bool_t bExpired = comp.fLifetimeAccumulator >= comp.fAutoDestroyAfterSec;
        if ((bComplete && comp.bAutoDestroyOnComplete) || bExpired)
        {
            pStorage->Release(comp.hInstance);
            world.RemoveComponent<FxInstanceComponent>(id);
        }
    });

    pStorage->GarbageCollect();
}
```

### §4.7 `Engine/Private/ECS/Systems/FxSpawnRequestSystem.cpp` (L1-L48)

```cpp
#include "ECS/Systems/FxSpawnRequestSystem.h"
#include "ECS/Components/FxSpawnRequestComponent.h"
#include "ECS/Components/FxInstanceComponent.h"
#include "ECS/World.h"
#include "GameInstance.h"
#include "FX/v2/Asset/FxAssetRegistry.h"
#include "FX/v2/Instance/FxSystemInstanceStorage.h"
#include "FX/v2/Instance/FxSystemInstance.h"
#include "FX/v2/Instance/FxSystemInitDesc.h"

std::unique_ptr<CFxSpawnRequestSystem> CFxSpawnRequestSystem::Create()
{
    return std::unique_ptr<CFxSpawnRequestSystem>(new CFxSpawnRequestSystem());
}

CFxSpawnRequestSystem::~CFxSpawnRequestSystem() = default;

void CFxSpawnRequestSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    builder.ReadWrite<FxSpawnRequestComponent>();
    builder.ReadWrite<FxInstanceComponent>();
}

void CFxSpawnRequestSystem::Execute(CWorld& world, f32_t /*fDeltaTime*/)
{
    using namespace Winters::FX::v2;
    auto* pRegistry = CGameInstance::Get()->Get_FxAssetRegistry();
    auto* pStorage = world.Get_FxSystemInstanceStorage();
    if (!pRegistry || !pStorage) return;

    world.ForEach<FxSpawnRequestComponent>([&](EntityID id, FxSpawnRequestComponent& req) {
        FxAssetHandle hAsset = pRegistry->Find(req.strAssetPath);
        if (!hAsset.IsValid()) hAsset = pRegistry->LoadFromFile(req.strAssetPath);
        CFxSystemAsset* pAsset = pRegistry->Resolve(hAsset);
        if (!pAsset) { world.RemoveComponent<FxSpawnRequestComponent>(id); return; }

        FxSystemInitDesc desc{};
        // 도메인별 desc = 부속 25 LoLFxBootstrap / EldenFxBootstrap 가 CGameInstance 에 default desc 등록.
        // 본 박제 = 디폴트 desc 사용.

        FxSystemInstanceHandle hInst = pStorage->Acquire(pAsset, desc);
        if (!hInst.IsValid()) { world.RemoveComponent<FxSpawnRequestComponent>(id); return; }
        if (CFxSystemInstance* pInst = pStorage->Resolve(hInst))
            pInst->SetWorldTransform(req.vWorldPos, req.vWorldEulerXYZ, req.vWorldScale);

        FxInstanceComponent comp{};
        comp.hInstance = hInst;
        comp.hAsset = hAsset;
        comp.fAutoDestroyAfterSec = req.fAutoDestroyAfterSec;
        comp.bAutoDestroyOnComplete = true;
        world.AddComponent<FxInstanceComponent>(id, comp);
        world.RemoveComponent<FxSpawnRequestComponent>(id);
    });
}
```

---

## §5 ECS Phase 표

```txt
Phase 0    CommandBuffer drain + FxSpawnRequestSystem (본 19 신규)
Phase 1    AISystem
Phase 2    NavigationSystem
Phase 3    MovementSystem
Phase 4    AnimationSystem
Phase 5    FxTickSystem (본 19 신규)
Phase 6    FxRenderSnapshotSystem (부속 20)
Phase 7    Renderer
```

---

## §6 World / GameInstance 통합 (헤더 패치)

```cpp
// Engine/Public/ECS/World.h 의 CWorld 클래스에 멤버 추가
namespace Winters::FX::v2 { class CFxSystemInstanceStorage; }

class WINTERS_ENGINE CWorld
{
    // ... 기존 멤버 ...
public:
    Winters::FX::v2::CFxSystemInstanceStorage* Get_FxSystemInstanceStorage() const { return m_pFxStorage.get(); }
    void Set_FxSystemInstanceStorage(std::unique_ptr<Winters::FX::v2::CFxSystemInstanceStorage> p) { m_pFxStorage = std::move(p); }
private:
    std::unique_ptr<Winters::FX::v2::CFxSystemInstanceStorage> m_pFxStorage;
};
```

```cpp
// Engine/Include/GameInstance.h 의 CGameInstance 클래스에 게터 추가
namespace Winters::FX::v2 { class CFxAssetRegistry; }

class WINTERS_ENGINE CGameInstance
{
    // ...
public:
    Winters::FX::v2::CFxAssetRegistry* Get_FxAssetRegistry() const { return m_pFxAssetRegistry.get(); }
private:
    std::unique_ptr<Winters::FX::v2::CFxAssetRegistry> m_pFxAssetRegistry;
};
```

`CEngineApp::OnInit` (`Engine/Private/Framework/CEngineApp.cpp`) 의 World 생성 직후 1 줄 추가:
```cpp
pWorld->Set_FxSystemInstanceStorage(Winters::FX::v2::CFxSystemInstanceStorage::Create(pWorld));
```

---

## §7 검증 명령 (EFX-2 합격)

```txt
1. grep "Scene_" Engine/{Public,Private}/FX/v2/Instance/  → 0 hit
2. grep "ID3D11" Engine/{Public,Private}/FX/v2/Instance/  → 0 hit
3. grep "OnUpdate" Engine/{Public,Private}/FX/v2/Instance/  → 0 hit
4. grep "unique_ptr<CFxSystemInstance>" Engine/Public/ECS/Components/  → 0 hit (POD handle)
5. grep "TBD" .md/plan/EffectTool/19_RUNTIME_LAYER_BAKE.md  → 0 hit
6. grep "stub\\|scaffold\\|본 박제 시점.*채움" .md/plan/EffectTool/19_RUNTIME_LAYER_BAKE.md  → 0 hit
7. CFxDataSet stress: 1024 spawn / kill 반복 1000회 → race 0
8. CFxParameterStore binding: src dirty → TickBindings → md5 비교
9. 5-state 전이: Inactive → Active → Completing → Complete
10. CFxSystemInstanceStorage Acquire/Release stress 1000 → leak 0
```

---

## §8 박제 함정 매트릭스

| 함정 | 본 19 회피 |
|---|---|
| P-1 + P-6 | §1 6 항목, TBD 0 |
| P-2 (PIMPL 추측) | 헤더 + cpp 동시 |
| P-3 (모든 path) | Runtime 단독 |
| P-4 (Scene 직접 의존) | `FxSpawnRequestComponent` + `CFxSpawnRequestSystem` 패턴 |
| P-7 (bitmask 폭) | mask 미사용 |
| P-8 (인용 의미 반전) | Niagara `NiagaraEmitterInstance.cpp:9-54` 차용 |
| P-9 (ECS Scheduler) | Phase 표 §5 1 곳. FxTick=5 단독, FxSpawnRequest=0 단독 |
| P-10 (Owner Scope) | Storage = `CWorld` owned, Registry = `CGameInstance` Tier-1 |
| P-11 (도메인 상수) | `FxSystemInitDesc` POD = LoL/Elden 분리 진입점. 본 19 = 디폴트 desc |
| P-12 (음수 truncation) | `static_cast<u32_t>` 슬롯 인덱스 양수만. SpawnAccumulator = `std::floor` |
| P-13 (미존재 API) | `CFxVM::Execute / FxScriptExecContext / FxVMExecutableData` = 부속 21 v2 박제 |
| P-14 (행동 정책 변경) | 본 19 = 신규 |
| P-15 (헤더 외부 의존) | `FxInstanceComponent.h` = `FxAssetHandle.h` + `FxSystemInstanceStorage.h` 직접 include |
| P-16 (산술 검증) | `eFxLifecycleState` 5 값 |
| P-17 (typedef ABI) | `FxSystemInstanceHandle` = 신규 |
| P-18 (RHI 인프라) | TickGPU = 부속 21 IRHI dispatch |
| P-19 (Render/Sim 결합) | DataSet double-buffer + SwapBuffers |

---

## §9 변경 이력

```txt
2026-04-21    Phase G 초안 (Stage 2 ParticlePool SoA = 03)
2026-05-04    Niagara V2 (12)
2026-05-05    Lifecycle (15) 박제
2026-05-07    17 v4 마스터. 본 19 v1 (stub 포함)
2026-05-07    본 19 v2 재박제 (CLAUDE.md §8.2 본문 룰 적용 — VM Execute 호출 본문 + ConsumeSpawnCount + KillFlag swap-back + RNG seed XOR pattern)
```
