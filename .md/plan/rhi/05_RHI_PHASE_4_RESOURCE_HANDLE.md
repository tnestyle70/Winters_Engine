# Phase RH-4 Sub-plan: Resource Handle System

**작성일**: 2026-04-30 (Codex 2차 검토 보정 2026-04-30)
**상위 문서**: `00_RHI_MIGRATION_MASTER.md` §2
**범위**: **64-bit handle (32 index + 32 generation) 강화 + thread-safety 정책 명시 + Destroy* API 일관**
**합격**: render thread only 강제, generation wrap 안전, raw pointer 노출 0건

**한 줄**: **★ Codex 2차 보정 — 32-bit handle (24+8) 은 generation wrap 너무 빠름. 64-bit (32+32) 로 churn 안전. 또 thread-safety 정책 부재 → render thread only 강제. RHIHandles.h / RHIResourceTable.h 는 RH-1 에서 사전 도입, RH-4 는 강화.**

---

## ★ Codex 2차 검토 변경 요약

| 변경 | 이전 RH-4 (1차) | 신규 RH-4 (2차) |
|---|---|---|
| Handle 크기 | 32-bit (24 index + 8 generation) | **64-bit (32 index + 32 generation)** (P1-16) |
| Thread-safety | 미명시 | **render thread only 명시 + debug assert** (P1-17) |
| 도입 시점 | RH-4 부터 신규 | **RH-1 부터 사전 도입** (Phase 1 §4 참조), RH-4 는 강화 |

---

## 1. RHIHandles.h (★ Codex P1-16 보정 — 64-bit, RH-1 사전 도입)

★ Phase 1 §4 (`02_RHI_PHASE_1_INTERFACES.md`) 의 박제 그대로 사용 (참조). 본 phase 는 **강화** 만:

- 64-bit (32 index + 32 generation) 그대로 유지
- Tag types 그대로 유지
- 추가 작업: `RHIPipelineHandle / RHIBindGroupHandle / RHIRenderPassHandle` 등 RH-3 에서 추가된 handle 타입 통합

```cpp
// (RH-1 박제와 동일 — 본 plan 에선 변경 없음)
// 자세한 박제는 02_RHI_PHASE_1_INTERFACES.md §4 참조
```

### 1.1 Generation wrap 안전성 검증 (★ Codex P1-16)

```cpp
// 32-bit generation = 4,294,967,295 cycle.
// 매 frame 1000 자원 churn 가정 시:
//   wrap 까지 = 4.3B / 1000 / 60 fps / 60 sec / 60 min / 24 hour / 365 day
//             = 약 2.27 년 (!)
// 이전 32-bit handle (8-bit generation) 의 256 cycle = 0.07 분 (!!)
// → 64-bit 가 production-grade.
```

---

## 2. IRHIDevice 변경 — Create 메서드 반환 타입

**기존 (RH-1)**:
```cpp
virtual std::unique_ptr<IRHIBuffer> CreateBuffer(const RHIBufferDesc& desc) = 0;
```

**변경 후 (RH-4)**:
```cpp
virtual RHIBufferHandle  CreateBuffer (const RHIBufferDesc&  desc) = 0;
virtual RHITextureHandle CreateTexture(const RHITextureDesc& desc) = 0;
virtual void             DestroyBuffer (RHIBufferHandle handle) = 0;
virtual void             DestroyTexture(RHITextureHandle handle) = 0;

// 내부 자원 접근 (백엔드만 사용)
virtual IRHIBuffer*  ResolveBuffer (RHIBufferHandle handle) = 0;
virtual IRHITexture* ResolveTexture(RHITextureHandle handle) = 0;
```

**`IRHICommandList` API 도 handle 사용**:
```cpp
virtual void SetVertexBuffer(u32_t slot, RHIBufferHandle vb, u32_t offsetBytes = 0) = 0;
virtual void SetIndexBuffer(RHIBufferHandle ib, eRHIIndexFormat fmt) = 0;
virtual void SetTexture(u32_t slot, RHITextureHandle tex) = 0;
```

---

## 3. Lookup Table — Thread-safety 강화 (★ Codex P1-17)

★ Phase 1 §16 (`02_RHI_PHASE_1_INTERFACES.md`) 의 박제 기반 + thread-safety 강화. RH-4 단계에서:

- `AssertRenderThread()` 의 placeholder → 실제 thread id 검증 구현
- (선택) mutex 기반 thread-safe 모드 (모바일 / async loading 시)
- Job system 통합 (RH-4 이후 Phase 1b JobSystem 통합 시)

```cpp
// Phase 1 박제 + 강화:
template<typename T>
class CRHIResourceTable
{
    struct Entry { std::unique_ptr<T> ptr; u32_t generation; bool_t bAlive; };
    std::vector<Entry> m_entries;
    std::vector<u32_t> m_freeList;

public:
    template<typename Handle>
    Handle Insert(std::unique_ptr<T> ptr)
    {
        u32_t index;
        if (!m_freeList.empty())
        {
            index = m_freeList.back();
            m_freeList.pop_back();
            m_entries[index].ptr = std::move(ptr);
            m_entries[index].generation += 1;
            m_entries[index].bAlive = true;
        }
        else
        {
            index = static_cast<u32_t>(m_entries.size());
            m_entries.push_back({ std::move(ptr), 1, true });
        }
        return Handle::Make(index, m_entries[index].generation);
    }

    template<typename Handle>
    T* Resolve(Handle h)
    {
        u32_t i = h.Index();
        if (i >= m_entries.size()) return nullptr;
        if (!m_entries[i].bAlive) return nullptr;
        if (m_entries[i].generation != h.Generation()) return nullptr;   // use-after-free 감지
        return m_entries[i].ptr.get();
    }

    template<typename Handle>
    void Remove(Handle h)
    {
        u32_t i = h.Index();
        if (i >= m_entries.size()) return;
        if (m_entries[i].generation != h.Generation()) return;
        m_entries[i].ptr.reset();
        m_entries[i].bAlive = false;
        m_freeList.push_back(i);
    }
};

// CDX11Device 멤버:
CRHIResourceTable<IRHIBuffer>  m_BufferTable;
CRHIResourceTable<IRHITexture> m_TextureTable;
// ...
```

---

## 4. 마이그 패턴

**기존 (RH-1)**:
```cpp
auto vb = device->CreateBuffer(RHIBufferDesc{...});   // unique_ptr<IRHIBuffer>
cmd->SetVertexBuffer(0, vb.get());
// vb 가 dangling 가능성 — caller 가 lifetime 관리
```

**변경 후 (RH-4)**:
```cpp
RHIBufferHandle vb = device->CreateBuffer(RHIBufferDesc{...});   // 32bit handle
cmd->SetVertexBuffer(0, vb);   // handle 자체 복사 — 안전
// 명시적 destroy:
device->DestroyBuffer(vb);
```

---

## 5. 합격 (★ Codex 2차 보정)
- ✅ **64-bit handle** (32 index + 32 generation) — RH-1 사전 도입 검증 (P1-16)
- ✅ IRHIDevice / IRHICommandList API 가 handle 사용 (RH-1 부터)
- ✅ raw `IRHIBuffer*` / `IRHITexture*` 외부 노출 0건 (백엔드 내부만)
- ✅ Use-after-free generation check 동작 (test 케이스)
- ✅ **`CRHIResourceTable` thread-safety 강화** (P1-17) — render thread id assert + (선택) mutex 모드
- ✅ Job system 통합 검토 (Phase 1b JobSystem 합격 후)

---

## 6. 추후 박제

RH-3 합격 후 본격. 본 sub-plan 의 강화 작업은 RH-4 진입 시 박제 확장.
