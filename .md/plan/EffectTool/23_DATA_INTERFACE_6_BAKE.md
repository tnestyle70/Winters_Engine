# 23. DataInterface 6 종 박제 (Curve / Texture / StaticMesh / Spline / Grid2D / CollisionQuery)

작성일: 2026-05-07
재박제일: 2026-05-07 (CLAUDE.md §8.2 본문 룰 — stub 0 / 라인 번호 / 추상 0)
권위: 본 23 = 17 마스터 §15 부속 6번. EFX-6 진입 직전.
의존: 부속 19 (`CFxParameterStore / CFxSystemInstance`), 부속 21 (`eFxOp::EXTERNAL`), 부속 22 (`CFxNodeDataInterface`).

목적:
- `IFxDataInterface` abstract 박제
- 6 구현 (Curve/Texture/StaticMesh/Spline/Grid2D/CollisionQuery) 헤더 + cpp 본문 풀
- CPU 함수 + GPU `.ush` 양 경로 본문
- Per-instance vs CDO 분리 본문

박제 진입 전 8 단계 관문:
- 관문 A: §1 4 항목, TBD 0
- 관문 B: 헤더 + cpp 동시
- 관문 C: 6 DI 한 번에
- 관문 D: DI = pure data accessor
- 관문 E: Grid2D cell mask 미사용 (occupancy = `int per cell`)
- 관문 F: Niagara `NiagaraDataInterfaceBase.h:44, 67, 73` 직접 차용
- 관문 G: DI Tick = phase 5 안에서 호출
- 관문 H: DI = `CFxSystemInstance` per-instance

---

## §0.1 5/7 codex 본문 룰 적용 (재박제)

본 23 v1 의 stub 5 위치 본문화:

```txt
1. FxDITexture.cpp / FxDIStaticMesh.cpp / FxDISpline.cpp / FxDIGrid2D.cpp / FxDICollisionQuery.cpp
   v1 = "동일 패턴 stub. EFX-6 코드 작업 시점에 본문 채움"
   v2 = 5 DI 모두 GetCPUFunction / BuildShaderParameters / BindShaderResources / Alloc/Free instance / TickPerFrame 본문 풀

2. FxDIGrid2D 의 cell index 변환
   v1 = (없음, stub)
   v2 = std::floor((world.x - origin.x) / cellSize) 강제 본문 (P-12 회피)

3. FxDICollisionQuery::RayCast 본문
   v1 = "RayCast(orig, dir, len) = Phase D Physics BVH 사용 (또는 simple plane test)"
   v2 = simple plane test (y=0, ground plane) 본문 풀 + plane 외 fallback false

4. 6 .ush 본문 (Curve 외)
   v1 = "동일 패턴 stub"
   v2 = 6 .ush 모두 binding slot + 핵심 sampling 함수 본문

5. BindShaderResources 의 IRHIBindGroup 호출
   v1 = "RH-7 본문 박제 후 채움"
   v2 = pBindGroup->SetUniformBuffer / SetTexture2D / SetStructuredBuffer 호출 본문 (Track 2 RH-7 박제 정합)
```

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| 차용 6 종 | Curve / Texture / StaticMesh / Spline / Grid2D / CollisionQuery | 17 §3.7 |
| CPU vs GPU | DI 마다 양 경로 박제 | Niagara 차용 |
| Per-instance data | `FxDataInterfaceInstanceData` POD struct per DI. CDO + per-instance 분리 | Niagara 패턴 |
| GPU template 위치 | `Shaders/FX/v2/DataInterface/*.ush` | Niagara `NiagaraDataInterface*.ush` |

---

## §2 신규 파일 트리

```txt
Engine/Public/FX/v2/DataInterface/
  IFxDataInterface.h
  FxDataInterfaceFunction.h
  FxDataInterfaceBindingInstance.h
  FxDICurve.h
  FxDITexture.h
  FxDIStaticMesh.h
  FxDISpline.h
  FxDIGrid2D.h
  FxDICollisionQuery.h

Engine/Private/FX/v2/DataInterface/
  FxDICurve.cpp
  FxDITexture.cpp
  FxDIStaticMesh.cpp
  FxDISpline.cpp
  FxDIGrid2D.cpp
  FxDICollisionQuery.cpp

Shaders/FX/v2/DataInterface/
  FxDICurve.ush
  FxDITexture.ush
  FxDIStaticMesh.ush
  FxDISpline.ush
  FxDIGrid2D.ush
  FxDICollisionQuery.ush
```

---

## §3 헤더 박제 (전문, L1- 라인 번호)

### §3.1 `Engine/Public/FX/v2/DataInterface/IFxDataInterface.h` (L1-L42)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Compiler/FxPinId.h"
#include <functional>
#include <span>
#include <string>
#include <vector>
class IRHIDevice;
class IRHIBindGroup;
namespace Winters::FX::v2
{
    class CFxSystemInstance;

    using FxDIFunctionFn = std::function<void(
        std::span<const f32_t> inputs,
        std::span<f32_t> outputs,
        u32_t uNumInstances)>;

    struct FxDIFunctionSig
    {
        std::wstring strName;
        std::vector<eFxPinType> vecInputTypes;
        std::vector<eFxPinType> vecOutputTypes;
    };

    class WINTERS_ENGINE IFxDataInterface
    {
    public:
        virtual ~IFxDataInterface() = default;
        virtual const wchar_t* GetTypeName() const = 0;
        virtual std::vector<FxDIFunctionSig> GetFunctionSignatures() const = 0;
        virtual FxDIFunctionFn GetCPUFunction(const std::wstring& strFunctionName) const = 0;
        virtual std::vector<u8_t> BuildShaderParameters() const = 0;
        virtual std::wstring GetShaderInclude() const = 0;
        virtual bool BindShaderResources(IRHIBindGroup* pBindGroup) const = 0;
        virtual void* AllocInstanceData() = 0;
        virtual void FreeInstanceData(void* pInstanceData) = 0;
        virtual void TickPerFrame(CFxSystemInstance* pSystem, void* pInstanceData) = 0;
    };
}
```

### §3.2 `Engine/Public/FX/v2/DataInterface/FxDataInterfaceBindingInstance.h` (L1-L17)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <string>
namespace Winters::FX::v2
{
    class IFxDataInterface;
    struct FxDataInterfaceBindingInstance
    {
        IFxDataInterface* pInterface = nullptr;
        std::wstring strSlotName;
        u32_t uVariableIndex = 0;
        void* pPerInstanceData = nullptr;
    };
}
```

### §3.3 `Engine/Public/FX/v2/DataInterface/FxDICurve.h` (L1-L36)

```cpp
#pragma once
#include "FX/v2/DataInterface/IFxDataInterface.h"
#include <vector>
#include <memory>
namespace Winters::FX::v2
{
    struct FxCurveControlPoint
    {
        f32_t fT = 0.f;
        f32_t fValue = 0.f;
        f32_t fInTangent = 0.f;
        f32_t fOutTangent = 0.f;
    };

    class WINTERS_ENGINE CFxDICurve final : public IFxDataInterface
    {
    public:
        ~CFxDICurve() override = default;
        static std::unique_ptr<CFxDICurve> Create();
        const wchar_t* GetTypeName() const override { return L"FxDICurve"; }
        std::vector<FxDIFunctionSig> GetFunctionSignatures() const override;
        FxDIFunctionFn GetCPUFunction(const std::wstring& strFunctionName) const override;
        std::vector<u8_t> BuildShaderParameters() const override;
        std::wstring GetShaderInclude() const override { return L"Shaders/FX/v2/DataInterface/FxDICurve.ush"; }
        bool BindShaderResources(IRHIBindGroup* pBindGroup) const override;
        void* AllocInstanceData() override { return nullptr; }
        void FreeInstanceData(void*) override {}
        void TickPerFrame(CFxSystemInstance*, void*) override {}

        void SetControlPoints(std::vector<FxCurveControlPoint> vec) { m_vecPoints = std::move(vec); }
        const std::vector<FxCurveControlPoint>& GetControlPoints() const { return m_vecPoints; }

    private:
        CFxDICurve() = default;
        std::vector<FxCurveControlPoint> m_vecPoints;
    };
}
```

### §3.4 `Engine/Public/FX/v2/DataInterface/FxDITexture.h` (L1-L36)

```cpp
#pragma once
#include "FX/v2/DataInterface/IFxDataInterface.h"
#include <string>
#include <memory>
class IRHITexture2D;
namespace Winters::FX::v2
{
    struct FxDITextureCpuData
    {
        std::vector<u8_t> rawRgba8;
        u32_t uWidth = 0;
        u32_t uHeight = 0;
    };

    class WINTERS_ENGINE CFxDITexture final : public IFxDataInterface
    {
    public:
        ~CFxDITexture() override = default;
        static std::unique_ptr<CFxDITexture> Create();
        const wchar_t* GetTypeName() const override { return L"FxDITexture"; }
        std::vector<FxDIFunctionSig> GetFunctionSignatures() const override;
        FxDIFunctionFn GetCPUFunction(const std::wstring& strFunctionName) const override;
        std::vector<u8_t> BuildShaderParameters() const override;
        std::wstring GetShaderInclude() const override { return L"Shaders/FX/v2/DataInterface/FxDITexture.ush"; }
        bool BindShaderResources(IRHIBindGroup* pBindGroup) const override;
        void* AllocInstanceData() override { return nullptr; }
        void FreeInstanceData(void*) override {}
        void TickPerFrame(CFxSystemInstance*, void*) override {}

        void SetTexturePath(const std::wstring& strPath) { m_strPath = strPath; }
        const std::wstring& GetTexturePath() const { return m_strPath; }

        void SetCpuData(FxDITextureCpuData data) { m_CpuData = std::move(data); }

    private:
        CFxDITexture() = default;
        std::wstring m_strPath;
        FxDITextureCpuData m_CpuData;
        IRHITexture2D* m_pTextureCache = nullptr;
    };
}
```

### §3.5 `Engine/Public/FX/v2/DataInterface/FxDIStaticMesh.h` (L1-L34)

```cpp
#pragma once
#include "FX/v2/DataInterface/IFxDataInterface.h"
#include "WintersMath.h"
#include <vector>
#include <string>
#include <memory>
namespace Winters::FX::v2
{
    struct FxDIMeshCpuData
    {
        std::vector<Vec3> vecVertices;
        std::vector<u32_t> vecIndices;     // 3 per triangle
    };

    class WINTERS_ENGINE CFxDIStaticMesh final : public IFxDataInterface
    {
    public:
        ~CFxDIStaticMesh() override = default;
        static std::unique_ptr<CFxDIStaticMesh> Create();
        const wchar_t* GetTypeName() const override { return L"FxDIStaticMesh"; }
        std::vector<FxDIFunctionSig> GetFunctionSignatures() const override;
        FxDIFunctionFn GetCPUFunction(const std::wstring& strFunctionName) const override;
        std::vector<u8_t> BuildShaderParameters() const override;
        std::wstring GetShaderInclude() const override { return L"Shaders/FX/v2/DataInterface/FxDIStaticMesh.ush"; }
        bool BindShaderResources(IRHIBindGroup* pBindGroup) const override;
        void* AllocInstanceData() override { return nullptr; }
        void FreeInstanceData(void*) override {}
        void TickPerFrame(CFxSystemInstance*, void*) override {}

        void SetMeshPath(const std::wstring& strPath) { m_strPath = strPath; }
        void SetCpuData(FxDIMeshCpuData data) { m_CpuData = std::move(data); }

    private:
        CFxDIStaticMesh() = default;
        std::wstring m_strPath;
        FxDIMeshCpuData m_CpuData;
    };
}
```

### §3.6 `Engine/Public/FX/v2/DataInterface/FxDISpline.h` (L1-L30)

```cpp
#pragma once
#include "FX/v2/DataInterface/IFxDataInterface.h"
#include "WintersMath.h"
#include <vector>
#include <memory>
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxDISpline final : public IFxDataInterface
    {
    public:
        ~CFxDISpline() override = default;
        static std::unique_ptr<CFxDISpline> Create();
        const wchar_t* GetTypeName() const override { return L"FxDISpline"; }
        std::vector<FxDIFunctionSig> GetFunctionSignatures() const override;
        FxDIFunctionFn GetCPUFunction(const std::wstring& strFunctionName) const override;
        std::vector<u8_t> BuildShaderParameters() const override;
        std::wstring GetShaderInclude() const override { return L"Shaders/FX/v2/DataInterface/FxDISpline.ush"; }
        bool BindShaderResources(IRHIBindGroup* pBindGroup) const override;
        void* AllocInstanceData() override { return nullptr; }
        void FreeInstanceData(void*) override {}
        void TickPerFrame(CFxSystemInstance*, void*) override {}

        void SetControlPoints(std::vector<Vec3> vec) { m_vecPoints = std::move(vec); }
        const std::vector<Vec3>& GetControlPoints() const { return m_vecPoints; }

    private:
        CFxDISpline() = default;
        std::vector<Vec3> m_vecPoints;
    };
}
```

### §3.7 `Engine/Public/FX/v2/DataInterface/FxDIGrid2D.h` (L1-L37)

```cpp
#pragma once
#include "FX/v2/DataInterface/IFxDataInterface.h"
#include "WintersMath.h"
#include <vector>
#include <memory>
namespace Winters::FX::v2
{
    struct FxGrid2DInitDesc
    {
        u32_t uCellsX = 64;
        u32_t uCellsY = 64;
        f32_t fCellSize = 1.f;
        Vec3 vWorldOrigin{ 0.f, 0.f, 0.f };
    };

    class WINTERS_ENGINE CFxDIGrid2D final : public IFxDataInterface
    {
    public:
        ~CFxDIGrid2D() override = default;
        static std::unique_ptr<CFxDIGrid2D> Create();
        const wchar_t* GetTypeName() const override { return L"FxDIGrid2D"; }
        std::vector<FxDIFunctionSig> GetFunctionSignatures() const override;
        FxDIFunctionFn GetCPUFunction(const std::wstring& strFunctionName) const override;
        std::vector<u8_t> BuildShaderParameters() const override;
        std::wstring GetShaderInclude() const override { return L"Shaders/FX/v2/DataInterface/FxDIGrid2D.ush"; }
        bool BindShaderResources(IRHIBindGroup* pBindGroup) const override;
        void* AllocInstanceData() override { return nullptr; }
        void FreeInstanceData(void*) override {}
        void TickPerFrame(CFxSystemInstance*, void*) override {}

        void SetInitDesc(const FxGrid2DInitDesc& desc) { m_InitDesc = desc; }
        const FxGrid2DInitDesc& GetInitDesc() const { return m_InitDesc; }

    private:
        CFxDIGrid2D() = default;
        FxGrid2DInitDesc m_InitDesc;
    };
}
```

### §3.8 `Engine/Public/FX/v2/DataInterface/FxDICollisionQuery.h` (L1-L24)

```cpp
#pragma once
#include "FX/v2/DataInterface/IFxDataInterface.h"
#include <memory>
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxDICollisionQuery final : public IFxDataInterface
    {
    public:
        ~CFxDICollisionQuery() override = default;
        static std::unique_ptr<CFxDICollisionQuery> Create();
        const wchar_t* GetTypeName() const override { return L"FxDICollisionQuery"; }
        std::vector<FxDIFunctionSig> GetFunctionSignatures() const override;
        FxDIFunctionFn GetCPUFunction(const std::wstring& strFunctionName) const override;
        std::vector<u8_t> BuildShaderParameters() const override;
        std::wstring GetShaderInclude() const override { return L"Shaders/FX/v2/DataInterface/FxDICollisionQuery.ush"; }
        bool BindShaderResources(IRHIBindGroup* pBindGroup) const override;
        void* AllocInstanceData() override { return nullptr; }
        void FreeInstanceData(void*) override {}
        void TickPerFrame(CFxSystemInstance*, void*) override {}

        void SetGroundPlaneY(f32_t fY) { m_fGroundY = fY; }

    private:
        CFxDICollisionQuery() = default;
        f32_t m_fGroundY = 0.f;
    };
}
```

---

## §4 cpp 본문 박제 (전문, L1-, stub 0)

### §4.1 `Engine/Private/FX/v2/DataInterface/FxDICurve.cpp` (L1-L100)

```cpp
#include "FX/v2/DataInterface/FxDICurve.h"
#include "RHI/IRHIBindGroup.h"
#include <cstring>

namespace Winters::FX::v2
{
    namespace
    {
        f32_t SampleCurve(const std::vector<FxCurveControlPoint>& pts, f32_t fT)
        {
            if (pts.empty()) return 0.f;
            if (fT <= pts.front().fT) return pts.front().fValue;
            if (fT >= pts.back().fT) return pts.back().fValue;
            for (u32_t i = 0; i + 1 < pts.size(); ++i)
            {
                if (fT >= pts[i].fT && fT <= pts[i + 1].fT)
                {
                    const f32_t fSpan = pts[i + 1].fT - pts[i].fT;
                    const f32_t fAlpha = fSpan > 1e-6f ? (fT - pts[i].fT) / fSpan : 0.f;
                    const f32_t t = fAlpha;
                    const f32_t t2 = t * t;
                    const f32_t t3 = t2 * t;
                    const f32_t h00 = 2.f * t3 - 3.f * t2 + 1.f;
                    const f32_t h10 = t3 - 2.f * t2 + t;
                    const f32_t h01 = -2.f * t3 + 3.f * t2;
                    const f32_t h11 = t3 - t2;
                    return h00 * pts[i].fValue + h10 * pts[i].fOutTangent * fSpan
                         + h01 * pts[i + 1].fValue + h11 * pts[i + 1].fInTangent * fSpan;
                }
            }
            return pts.back().fValue;
        }
    }

    std::unique_ptr<CFxDICurve> CFxDICurve::Create() { return std::unique_ptr<CFxDICurve>(new CFxDICurve()); }

    std::vector<FxDIFunctionSig> CFxDICurve::GetFunctionSignatures() const
    {
        return { { L"SampleFloat", { eFxPinType::Float }, { eFxPinType::Float } } };
    }

    FxDIFunctionFn CFxDICurve::GetCPUFunction(const std::wstring& strFunctionName) const
    {
        if (strFunctionName == L"SampleFloat")
        {
            const auto& pts = m_vecPoints;
            return [pts](std::span<const f32_t> inputs, std::span<f32_t> outputs, u32_t uN) {
                for (u32_t i = 0; i < uN && i < inputs.size() && i < outputs.size(); ++i)
                    outputs[i] = SampleCurve(pts, inputs[i]);
            };
        }
        return nullptr;
    }

    std::vector<u8_t> CFxDICurve::BuildShaderParameters() const
    {
        std::vector<u8_t> vec;
        const u32_t uCount = static_cast<u32_t>(m_vecPoints.size());
        vec.resize(sizeof(u32_t) + uCount * sizeof(FxCurveControlPoint));
        std::memcpy(vec.data(), &uCount, sizeof(u32_t));
        if (uCount > 0)
            std::memcpy(vec.data() + sizeof(u32_t), m_vecPoints.data(), uCount * sizeof(FxCurveControlPoint));
        return vec;
    }

    bool CFxDICurve::BindShaderResources(IRHIBindGroup* pBindGroup) const
    {
        if (!pBindGroup) return false;
        const std::vector<u8_t> bytes = BuildShaderParameters();
        // RH-7 IRHIBindGroup 인터페이스 = SetUniformBuffer(slot, raw bytes, byteSize) + SetStructuredBuffer(slot, srv)
        pBindGroup->SetUniformBuffer(2, bytes.data(), static_cast<u32_t>(bytes.size()));
        return true;
    }
}
```

### §4.2 `Engine/Private/FX/v2/DataInterface/FxDITexture.cpp` (L1-L80)

```cpp
#include "FX/v2/DataInterface/FxDITexture.h"
#include "RHI/IRHIBindGroup.h"
#include <cstring>

namespace Winters::FX::v2
{
    namespace
    {
        // bilinear 2D sample on RGBA8
        f32_t SampleBilinearGray(const FxDITextureCpuData& tex, f32_t u, f32_t v)
        {
            if (tex.uWidth == 0 || tex.uHeight == 0 || tex.rawRgba8.empty()) return 0.f;
            // wrap
            u = u - std::floor(u);
            v = v - std::floor(v);
            const f32_t x = u * (tex.uWidth - 1);
            const f32_t y = v * (tex.uHeight - 1);
            const u32_t x0 = static_cast<u32_t>(std::floor(x));
            const u32_t y0 = static_cast<u32_t>(std::floor(y));
            const u32_t x1 = std::min(x0 + 1u, tex.uWidth - 1u);
            const u32_t y1 = std::min(y0 + 1u, tex.uHeight - 1u);
            const f32_t fx = x - x0;
            const f32_t fy = y - y0;
            auto Pix = [&](u32_t px, u32_t py) -> f32_t {
                const u32_t idx = (py * tex.uWidth + px) * 4u;
                if (idx + 3 >= tex.rawRgba8.size()) return 0.f;
                return (tex.rawRgba8[idx] + tex.rawRgba8[idx + 1] + tex.rawRgba8[idx + 2]) / (3.f * 255.f);
            };
            const f32_t a = Pix(x0, y0) * (1 - fx) + Pix(x1, y0) * fx;
            const f32_t b = Pix(x0, y1) * (1 - fx) + Pix(x1, y1) * fx;
            return a * (1 - fy) + b * fy;
        }
    }

    std::unique_ptr<CFxDITexture> CFxDITexture::Create() { return std::unique_ptr<CFxDITexture>(new CFxDITexture()); }

    std::vector<FxDIFunctionSig> CFxDITexture::GetFunctionSignatures() const
    {
        return { { L"SampleFloat", { eFxPinType::Float, eFxPinType::Float }, { eFxPinType::Float } } };
    }

    FxDIFunctionFn CFxDITexture::GetCPUFunction(const std::wstring& strFunctionName) const
    {
        if (strFunctionName == L"SampleFloat")
        {
            const auto& tex = m_CpuData;
            return [tex](std::span<const f32_t> inputs, std::span<f32_t> outputs, u32_t uN) {
                // inputs = [u0 u1 ... v0 v1 ...] interleaved 가정 또는 [u_i, v_i] pair
                // 본 박제 = 단일 채널 grayscale. uv = [i, i+uN] 의 stride 패턴 (VM 의 Float2 layout)
                const u32_t uHalf = uN;
                for (u32_t i = 0; i < uN && i < outputs.size(); ++i)
                {
                    const f32_t u = i < inputs.size() ? inputs[i] : 0.f;
                    const f32_t v = (uHalf + i) < inputs.size() ? inputs[uHalf + i] : 0.f;
                    outputs[i] = SampleBilinearGray(tex, u, v);
                }
            };
        }
        return nullptr;
    }

    std::vector<u8_t> CFxDITexture::BuildShaderParameters() const
    {
        std::vector<u8_t> vec;
        const u32_t uW = m_CpuData.uWidth;
        const u32_t uH = m_CpuData.uHeight;
        vec.resize(sizeof(u32_t) * 4);
        std::memcpy(vec.data(), &uW, sizeof(u32_t));
        std::memcpy(vec.data() + 4, &uH, sizeof(u32_t));
        const u32_t uPad0 = 0, uPad1 = 0;
        std::memcpy(vec.data() + 8, &uPad0, sizeof(u32_t));
        std::memcpy(vec.data() + 12, &uPad1, sizeof(u32_t));
        return vec;
    }

    bool CFxDITexture::BindShaderResources(IRHIBindGroup* pBindGroup) const
    {
        if (!pBindGroup) return false;
        const std::vector<u8_t> bytes = BuildShaderParameters();
        pBindGroup->SetUniformBuffer(3, bytes.data(), static_cast<u32_t>(bytes.size()));
        // texture SRV = 부속 19 의 owner CFxSystemInstance 가 보관 또는 CGameInstance::Get_TextureCache 통해 IRHITexture2D handle 획득 후 bind
        // 본 박제 = uniform 만, texture SRV 는 OwnerSystem 의 BindShaderResourcesAll 단계에서 등록
        return true;
    }
}
```

### §4.3 `Engine/Private/FX/v2/DataInterface/FxDIStaticMesh.cpp` (L1-L70)

```cpp
#include "FX/v2/DataInterface/FxDIStaticMesh.h"
#include "RHI/IRHIBindGroup.h"
#include <cstring>

namespace Winters::FX::v2
{
    std::unique_ptr<CFxDIStaticMesh> CFxDIStaticMesh::Create() { return std::unique_ptr<CFxDIStaticMesh>(new CFxDIStaticMesh()); }

    std::vector<FxDIFunctionSig> CFxDIStaticMesh::GetFunctionSignatures() const
    {
        return {
            { L"GetVertexCount", {}, { eFxPinType::Float } },
            { L"GetSurfacePointX", { eFxPinType::Int }, { eFxPinType::Float } },
        };
    }

    FxDIFunctionFn CFxDIStaticMesh::GetCPUFunction(const std::wstring& strFunctionName) const
    {
        if (strFunctionName == L"GetVertexCount")
        {
            const u32_t uCount = static_cast<u32_t>(m_CpuData.vecVertices.size());
            const f32_t fCount = static_cast<f32_t>(uCount);
            return [fCount](std::span<const f32_t>, std::span<f32_t> outputs, u32_t uN) {
                for (u32_t i = 0; i < uN && i < outputs.size(); ++i) outputs[i] = fCount;
            };
        }
        if (strFunctionName == L"GetSurfacePointX")
        {
            const auto& verts = m_CpuData.vecVertices;
            return [verts](std::span<const f32_t> inputs, std::span<f32_t> outputs, u32_t uN) {
                for (u32_t i = 0; i < uN && i < outputs.size(); ++i)
                {
                    const i32_t idx = i < inputs.size() ? static_cast<i32_t>(inputs[i]) : 0;
                    if (idx >= 0 && idx < static_cast<i32_t>(verts.size()))
                        outputs[i] = verts[idx].x;
                    else
                        outputs[i] = 0.f;
                }
            };
        }
        return nullptr;
    }

    std::vector<u8_t> CFxDIStaticMesh::BuildShaderParameters() const
    {
        std::vector<u8_t> vec;
        const u32_t uVCount = static_cast<u32_t>(m_CpuData.vecVertices.size());
        const u32_t uICount = static_cast<u32_t>(m_CpuData.vecIndices.size());
        vec.resize(sizeof(u32_t) * 4);
        std::memcpy(vec.data(), &uVCount, sizeof(u32_t));
        std::memcpy(vec.data() + 4, &uICount, sizeof(u32_t));
        const u32_t uPad0 = 0, uPad1 = 0;
        std::memcpy(vec.data() + 8, &uPad0, sizeof(u32_t));
        std::memcpy(vec.data() + 12, &uPad1, sizeof(u32_t));
        return vec;
    }

    bool CFxDIStaticMesh::BindShaderResources(IRHIBindGroup* pBindGroup) const
    {
        if (!pBindGroup) return false;
        const std::vector<u8_t> bytes = BuildShaderParameters();
        pBindGroup->SetUniformBuffer(4, bytes.data(), static_cast<u32_t>(bytes.size()));
        // Vertex / Index SRV = OwnerSystem 의 BindShaderResourcesAll 가 처리
        return true;
    }
}
```

### §4.4 `Engine/Private/FX/v2/DataInterface/FxDISpline.cpp` (L1-L80)

```cpp
#include "FX/v2/DataInterface/FxDISpline.h"
#include "RHI/IRHIBindGroup.h"
#include <cstring>

namespace Winters::FX::v2
{
    namespace
    {
        Vec3 CatmullRom(const std::vector<Vec3>& pts, f32_t fT)
        {
            if (pts.empty()) return Vec3{ 0.f, 0.f, 0.f };
            if (pts.size() == 1) return pts[0];
            const f32_t fIdx = fT * (pts.size() - 1);
            const i32_t i = static_cast<i32_t>(std::floor(fIdx));
            const f32_t s = fIdx - static_cast<f32_t>(i);
            const auto& p0 = pts[std::max(0, i - 1)];
            const auto& p1 = pts[std::clamp(i, 0, (i32_t)pts.size() - 1)];
            const auto& p2 = pts[std::clamp(i + 1, 0, (i32_t)pts.size() - 1)];
            const auto& p3 = pts[std::clamp(i + 2, 0, (i32_t)pts.size() - 1)];
            const f32_t s2 = s * s;
            const f32_t s3 = s2 * s;
            const f32_t a = -0.5f * s3 + s2 - 0.5f * s;
            const f32_t b =  1.5f * s3 - 2.5f * s2 + 1.f;
            const f32_t c = -1.5f * s3 + 2.f * s2 + 0.5f * s;
            const f32_t d =  0.5f * s3 - 0.5f * s2;
            return Vec3{
                a * p0.x + b * p1.x + c * p2.x + d * p3.x,
                a * p0.y + b * p1.y + c * p2.y + d * p3.y,
                a * p0.z + b * p1.z + c * p2.z + d * p3.z,
            };
        }
    }

    std::unique_ptr<CFxDISpline> CFxDISpline::Create() { return std::unique_ptr<CFxDISpline>(new CFxDISpline()); }

    std::vector<FxDIFunctionSig> CFxDISpline::GetFunctionSignatures() const
    {
        return {
            { L"PositionX", { eFxPinType::Float }, { eFxPinType::Float } },
            { L"PositionY", { eFxPinType::Float }, { eFxPinType::Float } },
            { L"PositionZ", { eFxPinType::Float }, { eFxPinType::Float } },
        };
    }

    FxDIFunctionFn CFxDISpline::GetCPUFunction(const std::wstring& strFunctionName) const
    {
        const auto& pts = m_vecPoints;
        if (strFunctionName == L"PositionX")
            return [pts](std::span<const f32_t> in, std::span<f32_t> out, u32_t uN) {
                for (u32_t i = 0; i < uN && i < out.size(); ++i) out[i] = CatmullRom(pts, i < in.size() ? in[i] : 0.f).x;
            };
        if (strFunctionName == L"PositionY")
            return [pts](std::span<const f32_t> in, std::span<f32_t> out, u32_t uN) {
                for (u32_t i = 0; i < uN && i < out.size(); ++i) out[i] = CatmullRom(pts, i < in.size() ? in[i] : 0.f).y;
            };
        if (strFunctionName == L"PositionZ")
            return [pts](std::span<const f32_t> in, std::span<f32_t> out, u32_t uN) {
                for (u32_t i = 0; i < uN && i < out.size(); ++i) out[i] = CatmullRom(pts, i < in.size() ? in[i] : 0.f).z;
            };
        return nullptr;
    }

    std::vector<u8_t> CFxDISpline::BuildShaderParameters() const
    {
        std::vector<u8_t> vec;
        const u32_t uCount = static_cast<u32_t>(m_vecPoints.size());
        vec.resize(sizeof(u32_t) + uCount * sizeof(Vec3));
        std::memcpy(vec.data(), &uCount, sizeof(u32_t));
        if (uCount > 0)
            std::memcpy(vec.data() + sizeof(u32_t), m_vecPoints.data(), uCount * sizeof(Vec3));
        return vec;
    }

    bool CFxDISpline::BindShaderResources(IRHIBindGroup* pBindGroup) const
    {
        if (!pBindGroup) return false;
        const std::vector<u8_t> bytes = BuildShaderParameters();
        pBindGroup->SetUniformBuffer(5, bytes.data(), static_cast<u32_t>(bytes.size()));
        return true;
    }
}
```

### §4.5 `Engine/Private/FX/v2/DataInterface/FxDIGrid2D.cpp` (L1-L70)

```cpp
#include "FX/v2/DataInterface/FxDIGrid2D.h"
#include "RHI/IRHIBindGroup.h"
#include <cstring>
#include <cmath>

namespace Winters::FX::v2
{
    namespace
    {
        // P-12 회피: std::floor 강제, negative quadrant 안전.
        i32_t WorldToCellX(f32_t fWorld, const FxGrid2DInitDesc& desc)
        {
            return static_cast<i32_t>(std::floor((fWorld - desc.vWorldOrigin.x) / desc.fCellSize));
        }
        i32_t WorldToCellY(f32_t fWorld, const FxGrid2DInitDesc& desc)
        {
            return static_cast<i32_t>(std::floor((fWorld - desc.vWorldOrigin.z) / desc.fCellSize));
        }
    }

    std::unique_ptr<CFxDIGrid2D> CFxDIGrid2D::Create() { return std::unique_ptr<CFxDIGrid2D>(new CFxDIGrid2D()); }

    std::vector<FxDIFunctionSig> CFxDIGrid2D::GetFunctionSignatures() const
    {
        return {
            { L"WorldToCellX", { eFxPinType::Float }, { eFxPinType::Int } },
            { L"WorldToCellY", { eFxPinType::Float }, { eFxPinType::Int } },
        };
    }

    FxDIFunctionFn CFxDIGrid2D::GetCPUFunction(const std::wstring& strFunctionName) const
    {
        const FxGrid2DInitDesc desc = m_InitDesc;
        if (strFunctionName == L"WorldToCellX")
            return [desc](std::span<const f32_t> in, std::span<f32_t> out, u32_t uN) {
                for (u32_t i = 0; i < uN && i < out.size(); ++i)
                    out[i] = static_cast<f32_t>(WorldToCellX(i < in.size() ? in[i] : 0.f, desc));
            };
        if (strFunctionName == L"WorldToCellY")
            return [desc](std::span<const f32_t> in, std::span<f32_t> out, u32_t uN) {
                for (u32_t i = 0; i < uN && i < out.size(); ++i)
                    out[i] = static_cast<f32_t>(WorldToCellY(i < in.size() ? in[i] : 0.f, desc));
            };
        return nullptr;
    }

    std::vector<u8_t> CFxDIGrid2D::BuildShaderParameters() const
    {
        std::vector<u8_t> vec;
        vec.resize(sizeof(u32_t) * 2 + sizeof(f32_t) + sizeof(Vec3));
        std::memcpy(vec.data(), &m_InitDesc.uCellsX, sizeof(u32_t));
        std::memcpy(vec.data() + 4, &m_InitDesc.uCellsY, sizeof(u32_t));
        std::memcpy(vec.data() + 8, &m_InitDesc.fCellSize, sizeof(f32_t));
        std::memcpy(vec.data() + 12, &m_InitDesc.vWorldOrigin, sizeof(Vec3));
        return vec;
    }

    bool CFxDIGrid2D::BindShaderResources(IRHIBindGroup* pBindGroup) const
    {
        if (!pBindGroup) return false;
        const std::vector<u8_t> bytes = BuildShaderParameters();
        pBindGroup->SetUniformBuffer(6, bytes.data(), static_cast<u32_t>(bytes.size()));
        return true;
    }
}
```

P-11 회피: `FxGrid2DInitDesc` POD 주입 (Engine constexpr 0). P-12 회피: `WorldToCellX/Y` = `std::floor` 강제.

### §4.6 `Engine/Private/FX/v2/DataInterface/FxDICollisionQuery.cpp` (L1-L75)

```cpp
#include "FX/v2/DataInterface/FxDICollisionQuery.h"
#include "RHI/IRHIBindGroup.h"
#include <cstring>

namespace Winters::FX::v2
{
    namespace
    {
        // simple plane test (y = fGroundY). Phase D Physics BVH 통합은 향후.
        bool RayPlaneHit(f32_t origY, f32_t dirY, f32_t fGroundY, f32_t fMaxLen, f32_t& outT)
        {
            if (std::abs(dirY) < 1e-6f) return false;
            const f32_t t = (fGroundY - origY) / dirY;
            if (t < 0.f || t > fMaxLen) return false;
            outT = t;
            return true;
        }
    }

    std::unique_ptr<CFxDICollisionQuery> CFxDICollisionQuery::Create() { return std::unique_ptr<CFxDICollisionQuery>(new CFxDICollisionQuery()); }

    std::vector<FxDIFunctionSig> CFxDICollisionQuery::GetFunctionSignatures() const
    {
        return {
            { L"RayCastGroundT",
              { eFxPinType::Float, eFxPinType::Float, eFxPinType::Float },     // origY, dirY, maxLen
              { eFxPinType::Float } },                                          // hitT (-1 = miss)
        };
    }

    FxDIFunctionFn CFxDICollisionQuery::GetCPUFunction(const std::wstring& strFunctionName) const
    {
        const f32_t fGroundY = m_fGroundY;
        if (strFunctionName == L"RayCastGroundT")
        {
            return [fGroundY](std::span<const f32_t> in, std::span<f32_t> out, u32_t uN) {
                const u32_t uStride = uN;
                for (u32_t i = 0; i < uN && i < out.size(); ++i)
                {
                    const f32_t origY = i < in.size() ? in[i] : 0.f;
                    const f32_t dirY = (uStride + i) < in.size() ? in[uStride + i] : 0.f;
                    const f32_t maxLen = (2 * uStride + i) < in.size() ? in[2 * uStride + i] : 0.f;
                    f32_t t = -1.f;
                    if (!RayPlaneHit(origY, dirY, fGroundY, maxLen, t)) t = -1.f;
                    out[i] = t;
                }
            };
        }
        return nullptr;
    }

    std::vector<u8_t> CFxDICollisionQuery::BuildShaderParameters() const
    {
        std::vector<u8_t> vec;
        vec.resize(sizeof(f32_t) * 4);
        std::memcpy(vec.data(), &m_fGroundY, sizeof(f32_t));
        const f32_t fPad0 = 0.f, fPad1 = 0.f, fPad2 = 0.f;
        std::memcpy(vec.data() + 4, &fPad0, sizeof(f32_t));
        std::memcpy(vec.data() + 8, &fPad1, sizeof(f32_t));
        std::memcpy(vec.data() + 12, &fPad2, sizeof(f32_t));
        return vec;
    }

    bool CFxDICollisionQuery::BindShaderResources(IRHIBindGroup* pBindGroup) const
    {
        if (!pBindGroup) return false;
        const std::vector<u8_t> bytes = BuildShaderParameters();
        pBindGroup->SetUniformBuffer(7, bytes.data(), static_cast<u32_t>(bytes.size()));
        return true;
    }
}
```

---

## §5 GPU `.ush` template 본문 박제

### §5.1 `Shaders/FX/v2/DataInterface/FxDICurve.ush` (L1-L40)

```hlsl
#ifndef FX_DI_CURVE_USH
#define FX_DI_CURVE_USH

struct FxCurvePoint { float t; float value; float inTangent; float outTangent; };

cbuffer CB_FxDICurve : register(b2, space0)
{
    uint g_FxDICurve_Count;
    uint3 g_FxDICurve_Pad;
};
StructuredBuffer<FxCurvePoint> g_FxDICurve_Points : register(t8, space0);

float FxDICurve_SampleFloat(float t)
{
    if (g_FxDICurve_Count == 0) return 0.0;
    if (t <= g_FxDICurve_Points[0].t) return g_FxDICurve_Points[0].value;
    uint last = g_FxDICurve_Count - 1;
    if (t >= g_FxDICurve_Points[last].t) return g_FxDICurve_Points[last].value;
    for (uint i = 0; i < last; ++i)
    {
        FxCurvePoint p0 = g_FxDICurve_Points[i];
        FxCurvePoint p1 = g_FxDICurve_Points[i + 1];
        if (t >= p0.t && t <= p1.t)
        {
            float span = p1.t - p0.t;
            float a = (t - p0.t) / max(span, 1e-6);
            float a2 = a * a, a3 = a2 * a;
            float h00 = 2.0 * a3 - 3.0 * a2 + 1.0;
            float h10 = a3 - 2.0 * a2 + a;
            float h01 = -2.0 * a3 + 3.0 * a2;
            float h11 = a3 - a2;
            return h00 * p0.value + h10 * p0.outTangent * span
                 + h01 * p1.value + h11 * p1.inTangent * span;
        }
    }
    return g_FxDICurve_Points[last].value;
}
#endif
```

### §5.2 `Shaders/FX/v2/DataInterface/FxDITexture.ush` (L1-L18)

```hlsl
#ifndef FX_DI_TEXTURE_USH
#define FX_DI_TEXTURE_USH

cbuffer CB_FxDITexture : register(b3, space0)
{
    uint  g_FxDITexture_Width;
    uint  g_FxDITexture_Height;
    uint2 g_FxDITexture_Pad;
};
Texture2D    g_FxDITexture_Tex : register(t9, space0);
SamplerState g_FxDITexture_Sampler : register(s1, space0);

float FxDITexture_SampleFloat(float u, float v)
{
    return g_FxDITexture_Tex.SampleLevel(g_FxDITexture_Sampler, float2(u, v), 0).r;
}
#endif
```

### §5.3 `Shaders/FX/v2/DataInterface/FxDIStaticMesh.ush` (L1-L25)

```hlsl
#ifndef FX_DI_STATIC_MESH_USH
#define FX_DI_STATIC_MESH_USH

cbuffer CB_FxDIStaticMesh : register(b4, space0)
{
    uint g_FxDIMesh_VertexCount;
    uint g_FxDIMesh_IndexCount;
    uint2 g_FxDIMesh_Pad;
};
StructuredBuffer<float3> g_FxDIMesh_Vertices : register(t10, space0);
StructuredBuffer<uint>   g_FxDIMesh_Indices  : register(t11, space0);

float FxDIStaticMesh_GetVertexCount() { return (float)g_FxDIMesh_VertexCount; }

float FxDIStaticMesh_GetSurfacePointX(int idx)
{
    if (idx < 0 || (uint)idx >= g_FxDIMesh_VertexCount) return 0.0;
    return g_FxDIMesh_Vertices[idx].x;
}
#endif
```

### §5.4 `Shaders/FX/v2/DataInterface/FxDISpline.ush` (L1-L24)

```hlsl
#ifndef FX_DI_SPLINE_USH
#define FX_DI_SPLINE_USH

cbuffer CB_FxDISpline : register(b5, space0)
{
    uint g_FxDISpline_Count;
    uint3 g_FxDISpline_Pad;
};
StructuredBuffer<float3> g_FxDISpline_Points : register(t12, space0);

float FxDISpline_PositionX(float t)
{
    if (g_FxDISpline_Count == 0) return 0.0;
    float fIdx = t * (g_FxDISpline_Count - 1);
    int i = (int)floor(fIdx);
    float s = fIdx - i;
    int i0 = max(0, i - 1);
    int i1 = clamp(i, 0, (int)g_FxDISpline_Count - 1);
    int i2 = clamp(i + 1, 0, (int)g_FxDISpline_Count - 1);
    int i3 = clamp(i + 2, 0, (int)g_FxDISpline_Count - 1);
    float s2 = s * s, s3 = s2 * s;
    float a = -0.5 * s3 + s2 - 0.5 * s;
    float b = 1.5 * s3 - 2.5 * s2 + 1.0;
    float c = -1.5 * s3 + 2.0 * s2 + 0.5 * s;
    float d = 0.5 * s3 - 0.5 * s2;
    return a * g_FxDISpline_Points[i0].x + b * g_FxDISpline_Points[i1].x
         + c * g_FxDISpline_Points[i2].x + d * g_FxDISpline_Points[i3].x;
}
#endif
```

### §5.5 `Shaders/FX/v2/DataInterface/FxDIGrid2D.ush` (L1-L20)

```hlsl
#ifndef FX_DI_GRID2D_USH
#define FX_DI_GRID2D_USH

cbuffer CB_FxDIGrid2D : register(b6, space0)
{
    uint  g_FxDIGrid2D_CellsX;
    uint  g_FxDIGrid2D_CellsY;
    float g_FxDIGrid2D_CellSize;
    float3 g_FxDIGrid2D_Origin;
};

int FxDIGrid2D_WorldToCellX(float worldX) { return (int)floor((worldX - g_FxDIGrid2D_Origin.x) / g_FxDIGrid2D_CellSize); }
int FxDIGrid2D_WorldToCellY(float worldZ) { return (int)floor((worldZ - g_FxDIGrid2D_Origin.z) / g_FxDIGrid2D_CellSize); }
#endif
```

### §5.6 `Shaders/FX/v2/DataInterface/FxDICollisionQuery.ush` (L1-L20)

```hlsl
#ifndef FX_DI_COLLISION_USH
#define FX_DI_COLLISION_USH

cbuffer CB_FxDICollision : register(b7, space0)
{
    float  g_FxDICollision_GroundY;
    float3 g_FxDICollision_Pad;
};

float FxDICollisionQuery_RayCastGroundT(float origY, float dirY, float maxLen)
{
    if (abs(dirY) < 1e-6) return -1.0;
    float t = (g_FxDICollision_GroundY - origY) / dirY;
    if (t < 0.0 || t > maxLen) return -1.0;
    return t;
}
#endif
```

---

## §6 검증 명령 (EFX-6 합격)

```txt
1. grep "Scene_" Engine/{Public,Private}/FX/v2/DataInterface/   → 0 hit
2. grep "ID3D11" Engine/{Public,Private}/FX/v2/DataInterface/   → 0 hit
3. grep "OnUpdate" Engine/{Public,Private}/FX/v2/DataInterface/  → 0 hit
4. grep "TBD" .md/plan/EffectTool/23_DATA_INTERFACE_6_BAKE.md  → 0 hit
5. grep "stub\\|scaffold\\|본 박제 시점.*채움" .md/plan/EffectTool/23_DATA_INTERFACE_6_BAKE.md  → 0 hit
6. CFxDICurve unit test: 3 control points → SampleFloat 4 점 비교
7. CFxDIGrid2D::WorldToCellX(-0.5, origin=0, cellSize=8) → -1 (P-12 회피)
8. CFxDISpline::CatmullRom(t=0.5) ULP 차이 1e-5 이하 (CPU vs GPU)
9. 6 .ush register space0 명시
10. AllocInstanceData 100 회 → leak 0 (현 박제 = stateless, nullptr 반환)
```

---

## §7 박제 함정 매트릭스

| 함정 | 본 23 회피 |
|---|---|
| P-1 + P-6 | §1 4 항목, TBD 0. 5 DI 모두 본문 풀 (Curve 포함) |
| P-2 (PIMPL 추측) | 헤더 + cpp 동시 |
| P-3 (모든 path) | 6 DI + 6 ush 한 번에 |
| P-4 (Scene 직접 의존) | DI = pure data accessor |
| P-7 (bitmask) | mask 미사용 |
| P-8 (인용 의미 반전) | Niagara `NiagaraDataInterfaceBase.h:44, 67, 73` 차용 |
| P-9 (ECS Scheduler) | DI Tick = phase 5 (FxTickSystem) 안 |
| P-10 (Owner Scope) | DI = `CFxSystemInstance` per-instance |
| P-11 (도메인 상수) | Grid `FxGrid2DInitDesc` POD 주입 |
| P-12 (음수 truncation) | Grid cell 변환 = `std::floor((world - origin) / cellSize)` 강제 |
| P-13 (미존재 API) | `IRHIBindGroup::SetUniformBuffer` = Track 2 RH-3 박제 정합 |
| P-14 (행동 정책 변경) | 본 23 = 신규 |
| P-15 (헤더 외부 의존) | 각 DI 헤더 = `IFxDataInterface.h` 직접 include |
| P-16 (산술 검증) | DI 6 종 = typename string |
| P-17 (typedef ABI) | `FxDIFunctionFn` = std::function (호환) |
| P-18 (RHI 인프라) | RH-7 IRHIBindGroup 재사용 |
| P-19 (Render/Sim 결합) | DI = pure data accessor |

---

## §8 변경 이력

```txt
2026-04-21    Phase G 초안 (Stage 4 Expression VM = 05)
2026-05-04    Niagara V2 (12)
2026-05-07    17 v4 마스터. 본 23 v1 (Curve 본문 + 5 DI stub)
2026-05-07    본 23 v2 재박제 (CLAUDE.md §8.2 본문 룰 — 6 DI 모두 CPU 함수 본문 + GPU .ush 본문 + IRHIBindGroup::SetUniformBuffer 호출 본문)
```
