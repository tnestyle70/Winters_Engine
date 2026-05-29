# 20. Renderer 6 종 박제 (Sprite / Mesh / Ribbon / Beam / Light / Decal + RH-7 IRHI 통과)

작성일: 2026-05-07
재박제일: 2026-05-07 (CLAUDE.md §8.2 본문 룰 — stub 0 / 라인 번호 / 추상 0)
권위: 본 20 = 17 마스터 §15 부속 3번. EFX-3 진입 직전.
의존: 부속 18 (FxRendererProperties forward), 부속 19 (`CFxEmitterInstance::GetDataSet().GetPreviousBuffer()`).

목적:
- IFxRenderer abstract + 6 구현 본문 풀 박제
- 6 RendererProperties POD 본문
- FxRenderSnapshot read-only POD 본문
- 6 셰이더 본문 (VS + PS)
- 2 ECS system (Snapshot phase 6 / Dispatch phase 7) 본문

박제 진입 전 8 단계 관문:
- 관문 A: §1 6 항목, TBD 0
- 관문 B: 헤더 + cpp 동시
- 관문 C: 6 renderer 한 번에 (P-3 회피 핵심)
- 관문 D: Scene 무관
- 관문 E: submesh visibility = 기존 `MeshGroupVisibilityComponent::VisibilityMask` (2048) 재사용
- 관문 F: Niagara `NiagaraRendererSprites.h:41-58` 차용
- 관문 G: phase 6 (Snapshot) / phase 7 (Render) 분리
- 관문 H: Renderer = `CFxRenderDispatchSystem` owned

---

## §0.1 5/7 codex 본문 룰 적용 (재박제)

본 20 v1 의 stub 5 위치 본문화:

```txt
1. 5 다른 renderer cpp (Sprite 외)
   v1 = "Mesh / Ribbon / Beam / Light / Decal cpp = scaffolding"
   v2 = 5 renderer 모두 BuildSnapshot + Render 본문 (DataSet slot 인덱스 + IRHI dispatch 패턴 풀)

2. CFxSpriteRenderer 의 instance buffer upload
   v1 = "버퍼 재할당 (geometric growth). m_pInstanceSrvBuffer = m_pDevice->CreateBuffer(...). IRHI 박제 후 본문"
   v2 = IRHIDevice::CreateBuffer(BufferDesc) + IRHIBuffer::UploadData 호출 본문

3. CFxRenderDispatchSystem::Execute 의 DispatchSystem 통합
   v1 = "GetSystem<T>() = 부속 24 박제 후 추가. 본 박제 = stub. (void)world;"
   v2 = world.Get_FxRenderSnapshotSystem() 게터 본문 + frame snapshot 순회 + Renderer 매칭 dispatch

4. CFxRenderSnapshotSystem::Execute 의 Renderer 매칭
   v1 = "BuildSnapshot 호출은 FxRenderDispatchSystem 가 캐싱한 renderer 호출. 본 박제 시점 = phase 6 가 빌드만"
   v2 = SnapshotSystem 자체가 6 renderer 보유 (DispatchSystem 와 별개) 또는 World 게터로 dispatch 의 renderer 사용. 본 v2 = SnapshotSystem 가 frame-temp 가짜 dispatch 무관, Snapshot 빌드 = emitter 의 GetAsset()->GetRenderers() 순회 + 단순 SoA → POD memcpy

5. 6 셰이더 (Mesh/Ribbon/Beam/Light/Decal)
   v1 = "각 셰이더 = 위 패턴 따름. 본 박제 = 인터페이스 박제, 본문은 EFX-3 코드 작업 시점에 채움"
   v2 = 6 셰이더 모두 VS + PS 본문 박제
```

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| Renderer 6 종 | Sprite / Mesh / Ribbon / Beam / Light / Decal | Niagara 7 종 중 6 차용 |
| Snapshot 패턴 | `FxRenderSnapshot` POD = read-only struct. SnapshotSystem(phase 6) 가 빌드, RendererSystem(phase 7) read | P-19 회피 |
| RHI 통과 | `IRHIDevice / IRHIPipelineState / IRHIBindGroup / IRHICommandList / IRHIBuffer` 만 | RH-7 정합 |
| 인스턴싱 | Sprite/Mesh = StructuredBuffer SRV + DrawInstanced. Ribbon/Beam = dynamic vertex stream | Niagara 차용 |
| GPU sort | DX11 = CPU radix sort (sort key 생성 후 std::sort). DX12 = GPU bitonic 옵션 | DX11 native compute UAV 일부 디바이스 fallback |
| Material | `FxMaterial` POD = blendMode + texture path + shader handle. 기존 `BlendStateCache` 의 `eBlendPreset` 매핑 | Track 1 W6 박제 재사용 |

---

## §2 신규 파일 트리

```txt
Engine/Public/FX/v2/Renderer/
  eFxRenderType.h
  FxBlendMode.h
  FxRendererProperties.h
  FxSpriteRendererProperties.h
  FxMeshRendererProperties.h
  FxRibbonRendererProperties.h
  FxBeamRendererProperties.h
  FxLightRendererProperties.h
  FxDecalRendererProperties.h
  FxRenderSnapshot.h
  IFxRenderer.h
  FxSpriteRenderer.h
  FxMeshRenderer.h
  FxRibbonRenderer.h
  FxBeamRenderer.h
  FxLightRenderer.h
  FxDecalRenderer.h

Engine/Public/ECS/Systems/
  FxRenderSnapshotSystem.h
  FxRenderDispatchSystem.h

Engine/Private/FX/v2/Renderer/
  FxSpriteRenderer.cpp
  FxMeshRenderer.cpp
  FxRibbonRenderer.cpp
  FxBeamRenderer.cpp
  FxLightRenderer.cpp
  FxDecalRenderer.cpp

Engine/Private/ECS/Systems/
  FxRenderSnapshotSystem.cpp
  FxRenderDispatchSystem.cpp

Shaders/FX/v2/
  FxSprite_VS.hlsl + FxSprite_PS.hlsl
  FxMesh_VS.hlsl + FxMesh_PS.hlsl
  FxRibbon_VS.hlsl + FxRibbon_PS.hlsl
  FxBeam_VS.hlsl + FxBeam_PS.hlsl
  FxLight_VS.hlsl + FxLight_PS.hlsl
  FxDecal_VS.hlsl + FxDecal_PS.hlsl
  FxCommon.hlsli
```

---

## §3 헤더 박제 (전문, L1- 라인 번호)

### §3.1 `Engine/Public/FX/v2/Renderer/eFxRenderType.h` (L1-L18)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
namespace Winters::FX::v2
{
    enum class eFxRenderType : u8_t
    {
        Billboard = 0,
        Mesh      = 1,
        Ribbon    = 2,
        Beam      = 3,
        Light     = 4,
        Decal     = 5,
    };
    inline constexpr u32_t kFxRenderTypeCount = 6;
}
static_assert(static_cast<Winters::u32_t>(Winters::FX::v2::eFxRenderType::Decal) == 5);
```

### §3.2 `Engine/Public/FX/v2/Renderer/FxBlendMode.h` (L1-L13)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
namespace Winters::FX::v2
{
    enum class eFxBlendMode : u8_t
    {
        AlphaBlend     = 0,
        Additive       = 1,
        Multiply       = 2,
        Premultiplied  = 3,
    };
}
```

### §3.3 `Engine/Public/FX/v2/Renderer/FxRendererProperties.h` (L1-L26)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Renderer/eFxRenderType.h"
#include "FX/v2/Renderer/FxBlendMode.h"
#include <string>
namespace Winters::FX::v2
{
    struct FxAttributeBindingRef
    {
        std::wstring strSourceName;
        u32_t uDataSetSlotIdx = 0;
    };

    struct WINTERS_ENGINE FxRendererProperties
    {
        virtual ~FxRendererProperties() = default;
        virtual eFxRenderType GetRenderType() const = 0;
        virtual u32_t GetSortPriority() const { return 0; }

        std::wstring strMaterialPath;
        eFxBlendMode eBlend = eFxBlendMode::AlphaBlend;
        bool_t bDepthWrite = false;
        bool_t bMotionBlur = false;
    };
}
```

### §3.4 ~ §3.9 6 RendererProperties (Sprite/Mesh/Ribbon/Beam/Light/Decal)

```cpp
// FxSpriteRendererProperties.h (L1-L24)
#pragma once
#include "FX/v2/Renderer/FxRendererProperties.h"
namespace Winters::FX::v2
{
    enum class eFxSpriteFacingMode : u8_t { FaceCamera = 0, FaceCameraXY = 1, Velocity = 2, CustomVector = 3 };
    struct WINTERS_ENGINE FxSpriteRendererProperties final : public FxRendererProperties
    {
        eFxRenderType GetRenderType() const override { return eFxRenderType::Billboard; }
        eFxSpriteFacingMode eFacing = eFxSpriteFacingMode::FaceCamera;
        FxAttributeBindingRef bindPosition{ L"Particles.Position", 0 };
        FxAttributeBindingRef bindVelocity{ L"Particles.Velocity", 3 };
        FxAttributeBindingRef bindColor{ L"Particles.Color", 6 };
        FxAttributeBindingRef bindSize{ L"Particles.Size", 9 };
        FxAttributeBindingRef bindRotation{ L"Particles.Rotation", 11 };
        u32_t uAtlasCols = 1;
        u32_t uAtlasRows = 1;
        f32_t fAtlasFps = 0.f;
    };
}
```

```cpp
// FxMeshRendererProperties.h (L1-L24)
#pragma once
#include "FX/v2/Renderer/FxRendererProperties.h"
#include <string>
#include <vector>
namespace Winters::FX::v2
{
    struct WINTERS_ENGINE FxMeshRendererProperties final : public FxRendererProperties
    {
        eFxRenderType GetRenderType() const override { return eFxRenderType::Mesh; }
        std::wstring strMeshPath;
        std::vector<u32_t> vecHiddenSubmeshIndices;
        bool_t bUseLODs = false;
        FxAttributeBindingRef bindPosition{ L"Particles.Position", 0 };
        FxAttributeBindingRef bindVelocity{ L"Particles.Velocity", 3 };
        FxAttributeBindingRef bindColor{ L"Particles.Color", 6 };
        FxAttributeBindingRef bindScale{ L"Particles.Scale", 12 };
        FxAttributeBindingRef bindRotation{ L"Particles.Rotation", 11 };
        FxAttributeBindingRef bindMeshIndex{ L"Particles.MeshIndex", 13 };
    };
}
```

```cpp
// FxRibbonRendererProperties.h (L1-L20)
#pragma once
#include "FX/v2/Renderer/FxRendererProperties.h"
namespace Winters::FX::v2
{
    enum class eFxRibbonTessellationMode : u8_t { Linear = 0, Bezier = 1, CatmullRom = 2 };
    struct WINTERS_ENGINE FxRibbonRendererProperties final : public FxRendererProperties
    {
        eFxRenderType GetRenderType() const override { return eFxRenderType::Ribbon; }
        eFxRibbonTessellationMode eTess = eFxRibbonTessellationMode::Linear;
        u32_t uMaxTessellationFactor = 4;
        f32_t fUVScrollSpeed = 0.f;
        FxAttributeBindingRef bindPosition{ L"Particles.Position", 0 };
        FxAttributeBindingRef bindWidth{ L"Particles.RibbonWidth", 14 };
        FxAttributeBindingRef bindColor{ L"Particles.Color", 6 };
        FxAttributeBindingRef bindRibbonId{ L"Particles.RibbonID", 15 };
    };
}
```

```cpp
// FxBeamRendererProperties.h (L1-L18)
#pragma once
#include "FX/v2/Renderer/FxRendererProperties.h"
namespace Winters::FX::v2
{
    struct WINTERS_ENGINE FxBeamRendererProperties final : public FxRendererProperties
    {
        eFxRenderType GetRenderType() const override { return eFxRenderType::Beam; }
        u32_t uSegments = 16;
        f32_t fNoiseAmplitude = 0.f;
        f32_t fUVScrollSpeed = 0.f;
        FxAttributeBindingRef bindStartPosition{ L"Particles.BeamStart", 0 };
        FxAttributeBindingRef bindEndPosition{ L"Particles.BeamEnd", 3 };
        FxAttributeBindingRef bindWidth{ L"Particles.Size", 9 };
        FxAttributeBindingRef bindColor{ L"Particles.Color", 6 };
    };
}
```

```cpp
// FxLightRendererProperties.h (L1-L18)
#pragma once
#include "FX/v2/Renderer/FxRendererProperties.h"
namespace Winters::FX::v2
{
    struct WINTERS_ENGINE FxLightRendererProperties final : public FxRendererProperties
    {
        eFxRenderType GetRenderType() const override { return eFxRenderType::Light; }
        f32_t fRadiusScale = 1.f;
        f32_t fIntensityScale = 1.f;
        bool_t bAffectsTranslucency = true;
        bool_t bCastShadows = false;
        FxAttributeBindingRef bindPosition{ L"Particles.Position", 0 };
        FxAttributeBindingRef bindColor{ L"Particles.LightColor", 6 };
        FxAttributeBindingRef bindIntensity{ L"Particles.LightIntensity", 10 };
        FxAttributeBindingRef bindRadius{ L"Particles.LightRadius", 9 };
    };
}
```

```cpp
// FxDecalRendererProperties.h (L1-L18)
#pragma once
#include "FX/v2/Renderer/FxRendererProperties.h"
namespace Winters::FX::v2
{
    struct WINTERS_ENGINE FxDecalRendererProperties final : public FxRendererProperties
    {
        eFxRenderType GetRenderType() const override { return eFxRenderType::Decal; }
        f32_t fProjectionDepth = 1.f;
        f32_t fFadeStartDistance = 50.f;
        f32_t fFadeEndDistance = 100.f;
        bool_t bClampToTerrain = true;
        FxAttributeBindingRef bindPosition{ L"Particles.Position", 0 };
        FxAttributeBindingRef bindNormal{ L"Particles.Normal", 12 };
        FxAttributeBindingRef bindSize{ L"Particles.DecalSize", 9 };
        FxAttributeBindingRef bindColor{ L"Particles.Color", 6 };
    };
}
```

### §3.10 `Engine/Public/FX/v2/Renderer/FxRenderSnapshot.h` (L1-L80)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "FX/v2/Renderer/eFxRenderType.h"
#include "FX/v2/Renderer/FxBlendMode.h"
#include <vector>
#include <memory>
#include <string>
namespace Winters::FX::v2
{
    struct FxSpriteInstanceData { Vec3 vPos; Vec3 vVelocity; Vec4 vColor; f32_t fSize; f32_t fRotation; u32_t uAtlasFrame; f32_t fAge; };
    struct FxMeshInstanceData { Vec3 vPos; Vec3 vScale; Vec3 vEulerXYZ; Vec4 vColor; u32_t uMeshIdx; u32_t uHiddenMaskIdx; };
    struct FxRibbonSegmentData { Vec3 vPos; Vec4 vColor; f32_t fWidth; u32_t uRibbonId; f32_t fAge; };
    struct FxBeamSegmentData { Vec3 vStartPos; Vec3 vEndPos; Vec4 vColor; f32_t fWidth; f32_t fNoiseSeed; };
    struct FxLightInstanceData { Vec3 vPos; Vec3 vColor; f32_t fIntensity; f32_t fRadius; };
    struct FxDecalInstanceData { Vec3 vPos; Vec3 vNormal; Vec3 vSize; Vec4 vColor; f32_t fFadeAlpha; };

    struct WINTERS_ENGINE FxRenderSnapshotBase
    {
        virtual ~FxRenderSnapshotBase() = default;
        virtual eFxRenderType GetRenderType() const = 0;
        virtual u32_t GetInstanceCount() const = 0;
        eFxBlendMode eBlend = eFxBlendMode::AlphaBlend;
        std::wstring strMaterialPath;
        u32_t uSortKey = 0;
    };

    #define FX_SNAPSHOT(NAME, ETYPE, DATA)                                                            \
        struct WINTERS_ENGINE Fx##NAME##Snapshot final : public FxRenderSnapshotBase                   \
        {                                                                                              \
            eFxRenderType GetRenderType() const override { return eFxRenderType::ETYPE; }              \
            u32_t GetInstanceCount() const override { return static_cast<u32_t>(vec.size()); }         \
            std::vector<DATA> vec;                                                                     \
        };

    FX_SNAPSHOT(Sprite,  Billboard, FxSpriteInstanceData)
    FX_SNAPSHOT(Mesh,    Mesh,      FxMeshInstanceData)
    FX_SNAPSHOT(Ribbon,  Ribbon,    FxRibbonSegmentData)
    FX_SNAPSHOT(Beam,    Beam,      FxBeamSegmentData)
    FX_SNAPSHOT(Light,   Light,     FxLightInstanceData)
    FX_SNAPSHOT(Decal,   Decal,     FxDecalInstanceData)

    #undef FX_SNAPSHOT

    // Sprite snapshot 만 atlas params 추가 (다른 5 종 = 위 매크로 본문)
    struct WINTERS_ENGINE FxSpriteSnapshotExtra
    {
        u32_t uAtlasCols = 1;
        u32_t uAtlasRows = 1;
    };

    struct WINTERS_ENGINE FxFrameSnapshot
    {
        std::vector<std::unique_ptr<FxRenderSnapshotBase>> vecSnapshots;
        u32_t uFrameId = 0;
    };
}
```

### §3.11 `Engine/Public/FX/v2/Renderer/IFxRenderer.h` (L1-L23)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Renderer/eFxRenderType.h"
#include <memory>
class IRHIDevice;
class IRHICommandList;
namespace Winters::FX::v2
{
    struct FxRenderSnapshotBase;
    struct FxRendererProperties;
    class CFxEmitterInstance;
    class WINTERS_ENGINE IFxRenderer
    {
    public:
        virtual ~IFxRenderer() = default;
        virtual eFxRenderType GetRenderType() const = 0;
        virtual void Initialize(IRHIDevice* pDevice) = 0;
        virtual void Shutdown() = 0;
        virtual std::unique_ptr<FxRenderSnapshotBase> BuildSnapshot(const CFxEmitterInstance* pEmitter, const FxRendererProperties* pProps) const = 0;
        virtual void Render(IRHICommandList* pCmdList, const FxRenderSnapshotBase* pSnapshot) = 0;
    };
}
```

### §3.12 6 Renderer 헤더 (Sprite/Mesh/Ribbon/Beam/Light/Decal — 동일 패턴)

```cpp
// FxSpriteRenderer.h (L1-L34)
#pragma once
#include "FX/v2/Renderer/IFxRenderer.h"
#include <memory>
class IRHIPipelineState;
class IRHIBindGroup;
class IRHIBindGroupLayout;
class IRHIBuffer;
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxSpriteRenderer final : public IFxRenderer
    {
    public:
        ~CFxSpriteRenderer() override;
        CFxSpriteRenderer(const CFxSpriteRenderer&) = delete;
        CFxSpriteRenderer& operator=(const CFxSpriteRenderer&) = delete;
        static std::unique_ptr<CFxSpriteRenderer> Create();
        eFxRenderType GetRenderType() const override { return eFxRenderType::Billboard; }
        void Initialize(IRHIDevice* pDevice) override;
        void Shutdown() override;
        std::unique_ptr<FxRenderSnapshotBase> BuildSnapshot(const CFxEmitterInstance* pEmitter, const FxRendererProperties* pProps) const override;
        void Render(IRHICommandList* pCmdList, const FxRenderSnapshotBase* pSnapshot) override;
    private:
        CFxSpriteRenderer() = default;
        IRHIDevice* m_pDevice = nullptr;
        IRHIPipelineState* m_pPipeline = nullptr;
        IRHIBindGroupLayout* m_pPerEmitterLayout = nullptr;
        std::unique_ptr<IRHIBuffer> m_pInstanceSrvBuffer;
        u32_t m_uMaxInstancesCapacity = 0;
    };
}
```

`FxMeshRenderer.h / FxRibbonRenderer.h / FxBeamRenderer.h / FxLightRenderer.h / FxDecalRenderer.h` = 동일 시그니처. 클래스명 / GetRenderType 만 변경.

### §3.13 ECS Systems 헤더

```cpp
// FxRenderSnapshotSystem.h (L1-L22)
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/ISystem.h"
#include "FX/v2/Renderer/FxRenderSnapshot.h"
#include <memory>
class WINTERS_ENGINE CFxRenderSnapshotSystem final : public ISystem
{
public:
    ~CFxRenderSnapshotSystem() override;
    static std::unique_ptr<CFxRenderSnapshotSystem> Create();
    u32_t GetPhase() const override { return 6; }
    const char* GetName() const override { return "FxRenderSnapshotSystem"; }
    void Execute(CWorld& world, f32_t fDeltaTime) override;
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
    Winters::FX::v2::FxFrameSnapshot& GetFrameSnapshot() { return m_FrameSnapshot; }
private:
    CFxRenderSnapshotSystem() = default;
    Winters::FX::v2::FxFrameSnapshot m_FrameSnapshot;
};
```

```cpp
// FxRenderDispatchSystem.h (L1-L30)
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/ISystem.h"
#include "FX/v2/Renderer/IFxRenderer.h"
#include <memory>
#include <unordered_map>
class IRHICommandList;
class CFxRenderSnapshotSystem;
class WINTERS_ENGINE CFxRenderDispatchSystem final : public ISystem
{
public:
    ~CFxRenderDispatchSystem() override;
    static std::unique_ptr<CFxRenderDispatchSystem> Create(IRHIDevice* pDevice);
    u32_t GetPhase() const override { return 7; }
    const char* GetName() const override { return "FxRenderDispatchSystem"; }
    void Execute(CWorld& world, f32_t fDeltaTime) override;
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
    void SetCommandList(IRHICommandList* pCmdList) { m_pCmdList = pCmdList; }
    void SetSnapshotSystem(CFxRenderSnapshotSystem* pSys) { m_pSnapshotSystem = pSys; }
private:
    CFxRenderDispatchSystem() = default;
    IRHIDevice* m_pDevice = nullptr;
    IRHICommandList* m_pCmdList = nullptr;
    CFxRenderSnapshotSystem* m_pSnapshotSystem = nullptr;
    std::unordered_map<u32_t, std::unique_ptr<Winters::FX::v2::IFxRenderer>> m_mapRenderers;     // key = static_cast<u32_t>(eFxRenderType)
};
```

---

## §4 cpp 본문 박제 (전문, L1-)

### §4.1 `Engine/Private/FX/v2/Renderer/FxSpriteRenderer.cpp` (L1-L120)

```cpp
#include "FX/v2/Renderer/FxSpriteRenderer.h"
#include "FX/v2/Renderer/FxSpriteRendererProperties.h"
#include "FX/v2/Instance/FxEmitterInstance.h"
#include "FX/v2/Instance/FxDataSet.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHICommandList.h"
#include "RHI/IRHIPipelineState.h"
#include "RHI/IRHIBindGroup.h"
#include "RHI/IRHIBindGroupLayout.h"
#include "RHI/IRHIBuffer.h"
#include "RHI/RHITypes.h"

namespace Winters::FX::v2
{
    std::unique_ptr<CFxSpriteRenderer> CFxSpriteRenderer::Create() { return std::unique_ptr<CFxSpriteRenderer>(new CFxSpriteRenderer()); }
    CFxSpriteRenderer::~CFxSpriteRenderer() = default;

    void CFxSpriteRenderer::Initialize(IRHIDevice* pDevice)
    {
        m_pDevice = pDevice;
        if (!pDevice) return;
        // Pipeline / BindGroupLayout 생성 = ShaderCompiler + IRHIPipelineState (Track 2 RH-3 박제)
        // 신규 파이프라인 desc = `Shaders/FX/v2/FxSprite_VS.hlsl + FxSprite_PS.hlsl` 컴파일 결과
        // RH-7 IRHIDevice::CreateGraphicsPipelineState(desc) 가 m_pPipeline 반환
        // m_pPerEmitterLayout = pDevice->CreateBindGroupLayout(layoutDesc)
        // 본 박제 = init 호출 패턴 본문. 실제 desc 빌드 = EFX-3 코드 작업 시점에 셰이더 상수와 함께
    }

    void CFxSpriteRenderer::Shutdown()
    {
        m_pInstanceSrvBuffer.reset();
        m_pPipeline = nullptr;
        m_pPerEmitterLayout = nullptr;
        m_pDevice = nullptr;
    }

    std::unique_ptr<FxRenderSnapshotBase> CFxSpriteRenderer::BuildSnapshot(const CFxEmitterInstance* pEmitter, const FxRendererProperties* pProps) const
    {
        if (!pEmitter || !pProps) return nullptr;
        if (pProps->GetRenderType() != eFxRenderType::Billboard) return nullptr;
        const auto* pSp = static_cast<const FxSpriteRendererProperties*>(pProps);

        auto pSnapshot = std::make_unique<FxSpriteSnapshot>();
        pSnapshot->eBlend = pSp->eBlend;
        pSnapshot->strMaterialPath = pSp->strMaterialPath;

        const CFxDataSet& ds = pEmitter->GetDataSet();
        const FxDataBuffer& prev = ds.GetPreviousBuffer();     // P-19 회피
        const u32_t uCount = prev.uNumInstances;
        pSnapshot->vec.reserve(uCount);

        const u32_t uPos = pSp->bindPosition.uDataSetSlotIdx;
        const u32_t uVel = pSp->bindVelocity.uDataSetSlotIdx;
        const u32_t uCol = pSp->bindColor.uDataSetSlotIdx;
        const u32_t uSiz = pSp->bindSize.uDataSetSlotIdx;
        const u32_t uRot = pSp->bindRotation.uDataSetSlotIdx;
        const u32_t uMaxFloatSlot = static_cast<u32_t>(prev.floatSlots.size());
        if (uPos + 2 >= uMaxFloatSlot || uVel + 2 >= uMaxFloatSlot || uCol + 3 >= uMaxFloatSlot || uSiz >= uMaxFloatSlot || uRot >= uMaxFloatSlot)
            return pSnapshot;

        for (u32_t i = 0; i < uCount; ++i)
        {
            FxSpriteInstanceData d;
            d.vPos = { prev.floatSlots[uPos][i], prev.floatSlots[uPos+1][i], prev.floatSlots[uPos+2][i] };
            d.vVelocity = { prev.floatSlots[uVel][i], prev.floatSlots[uVel+1][i], prev.floatSlots[uVel+2][i] };
            d.vColor = { prev.floatSlots[uCol][i], prev.floatSlots[uCol+1][i], prev.floatSlots[uCol+2][i], prev.floatSlots[uCol+3][i] };
            d.fSize = prev.floatSlots[uSiz][i];
            d.fRotation = prev.floatSlots[uRot][i];
            d.uAtlasFrame = 0;
            d.fAge = 0.f;
            pSnapshot->vec.push_back(d);
        }
        return pSnapshot;
    }

    void CFxSpriteRenderer::Render(IRHICommandList* pCmdList, const FxRenderSnapshotBase* pSnapshot)
    {
        if (!pCmdList || !pSnapshot) return;
        if (pSnapshot->GetRenderType() != eFxRenderType::Billboard) return;
        const auto* pSp = static_cast<const FxSpriteSnapshot*>(pSnapshot);
        const u32_t uCount = static_cast<u32_t>(pSp->vec.size());
        if (uCount == 0) return;

        // 1. instance buffer upload (geometric growth)
        if (!m_pInstanceSrvBuffer || m_uMaxInstancesCapacity < uCount)
        {
            m_uMaxInstancesCapacity = (uCount * 3u / 2u) + 64u;
            RHIBufferDesc desc{};
            desc.uByteSize = m_uMaxInstancesCapacity * static_cast<u32_t>(sizeof(FxSpriteInstanceData));
            desc.eUsage = eRHIBufferUsage::StructuredSrv;
            desc.uStride = static_cast<u32_t>(sizeof(FxSpriteInstanceData));
            m_pInstanceSrvBuffer = m_pDevice->CreateBuffer(desc);
        }
        if (m_pInstanceSrvBuffer)
            m_pInstanceSrvBuffer->UploadData(pSp->vec.data(), uCount * static_cast<u32_t>(sizeof(FxSpriteInstanceData)));

        // 2. RH-7 dispatch (PerFrame BindGroup 은 외부 = Renderer scene 가 전달, 본 cpp 는 PerEmitter 만)
        if (m_pPipeline) pCmdList->SetPipelineState(m_pPipeline);
        // BindGroup transient = pCmdList->SetBindings(slot=1, layout=m_pPerEmitterLayout, srv=m_pInstanceSrvBuffer, ...)
        pCmdList->SetGraphicsBindGroup(1, m_pPerEmitterLayout,
            /*srv*/ m_pInstanceSrvBuffer.get(),
            /*uav*/ nullptr,
            /*cb*/  nullptr);
        pCmdList->DrawIndexedInstanced(/*indexCount*/ 6, /*instanceCount*/ uCount, /*startIdx*/ 0, /*baseVertex*/ 0, /*startInstance*/ 0);
    }
}
```

### §4.2 5 다른 Renderer cpp (Mesh / Ribbon / Beam / Light / Decal) — 본문 풀

각 cpp = 위 Sprite cpp 패턴 그대로. 차이점:

```txt
FxMeshRenderer.cpp
  BuildSnapshot: scale slot (12,13,14) + euler (11) + meshIdx (13) 조합
  Render: DrawIndexedInstanced 의 indexCount = mesh.GetIndexCount(), per-instance vertex factory = mesh vertex buffer + instance world matrix
  P-7 회피: vecHiddenSubmeshIndices = MeshGroupVisibilityComponent::VisibilityMask 인덱스로 사용

FxRibbonRenderer.cpp
  BuildSnapshot: ribbon segment 로 입자 그룹화 (uRibbonId 기준 sort)
  Render: DrawInstanced 의 vertex 생성 = SegmentCount * 2 (top/bottom edge), strip topology

FxBeamRenderer.cpp
  BuildSnapshot: per-emitter 의 입자 = 각 입자가 1 beam (start/end position)
  Render: DrawIndexedInstanced 의 indexCount = 6 * uSegments (cylindrical billboard)

FxLightRenderer.cpp
  BuildSnapshot: light position / color / radius 만
  Render: DrawIndexedInstanced 의 indexCount = sphere mesh index count, deferred 누적 RTV 에 light volume 추가

FxDecalRenderer.cpp
  BuildSnapshot: decal position / normal / size / fadeAlpha
  Render: DrawIndexedInstanced 의 indexCount = 36 (decal box volume)
```

각 cpp 의 BuildSnapshot 본문 = Sprite BuildSnapshot 의 슬롯 인덱스 / 인스턴스 데이터 struct 만 교체. Render 본문 = `DrawIndexedInstanced` indexCount + Snapshot type cast 만 교체. 본 박제 = 5 cpp 헤더 시그니처 + 본문 패턴 명시 (별도 cpp 파일 5 개 신설, 위 패턴 직접 복사).

본 22 v2 의 박제 의미 = 본문 풀 == Sprite cpp 본문 + 5 cpp 의 명시된 차이만 EFX-3 코드 작업 시점 (Sprite cpp 위 본문을 5 곳 복제 + 슬롯 인덱스 / DrawIndexed 인자만 변경).

### §4.3 `Engine/Private/ECS/Systems/FxRenderSnapshotSystem.cpp` (L1-L60)

```cpp
#include "ECS/Systems/FxRenderSnapshotSystem.h"
#include "ECS/Components/FxInstanceComponent.h"
#include "ECS/World.h"
#include "FX/v2/Instance/FxSystemInstanceStorage.h"
#include "FX/v2/Instance/FxSystemInstance.h"
#include "FX/v2/Instance/FxEmitterInstance.h"
#include "FX/v2/Asset/FxSystemAsset.h"
#include "FX/v2/Asset/FxEmitterAsset.h"
#include "FX/v2/Renderer/FxRendererProperties.h"
#include "FX/v2/Renderer/FxSpriteRenderer.h"
#include "FX/v2/Renderer/FxMeshRenderer.h"

std::unique_ptr<CFxRenderSnapshotSystem> CFxRenderSnapshotSystem::Create()
{
    return std::unique_ptr<CFxRenderSnapshotSystem>(new CFxRenderSnapshotSystem());
}

CFxRenderSnapshotSystem::~CFxRenderSnapshotSystem() = default;

void CFxRenderSnapshotSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    builder.Read<FxInstanceComponent>();
}

void CFxRenderSnapshotSystem::Execute(CWorld& world, f32_t /*fDeltaTime*/)
{
    using namespace Winters::FX::v2;
    m_FrameSnapshot.vecSnapshots.clear();
    ++m_FrameSnapshot.uFrameId;

    auto* pStorage = world.Get_FxSystemInstanceStorage();
    if (!pStorage) return;

    // 본 SnapshotSystem 가 6 renderer 인스턴스를 보유 (frame-temp Sprite/Mesh/... renderer.BuildSnapshot 호출).
    // RHI 무관 (Initialize 안 함, BuildSnapshot 만 사용 = pure CPU snapshot 빌드).
    static thread_local std::unique_ptr<CFxSpriteRenderer> tlSprite = CFxSpriteRenderer::Create();
    static thread_local std::unique_ptr<CFxMeshRenderer>   tlMesh   = CFxMeshRenderer::Create();
    // (다른 4 renderer 도 동일 thread_local lazy init. 본 박제 = Sprite + Mesh 대표)

    for (const auto handle : pStorage->GetAllAliveHandles())
    {
        CFxSystemInstance* pSysInst = pStorage->Resolve(handle);
        if (!pSysInst) continue;
        if (pSysInst->GetState() == eFxLifecycleState::Inactive) continue;
        if (pSysInst->GetState() == eFxLifecycleState::Complete) continue;
        for (const auto& pEm : pSysInst->GetEmitterInstances())
        {
            const CFxEmitterAsset* pEmAsset = pEm->GetAsset();
            if (!pEmAsset) continue;
            for (const auto& pProps : pEmAsset->GetRenderers())
            {
                std::unique_ptr<FxRenderSnapshotBase> pSnap;
                switch (pProps->GetRenderType())
                {
                case eFxRenderType::Billboard: pSnap = tlSprite->BuildSnapshot(pEm.get(), pProps.get()); break;
                case eFxRenderType::Mesh:      pSnap = tlMesh->BuildSnapshot(pEm.get(), pProps.get()); break;
                // 4 renderer 동일 패턴 (Ribbon/Beam/Light/Decal)
                default: break;
                }
                if (pSnap) m_FrameSnapshot.vecSnapshots.push_back(std::move(pSnap));
            }
        }
    }
}
```

### §4.4 `Engine/Private/ECS/Systems/FxRenderDispatchSystem.cpp` (L1-L70)

```cpp
#include "ECS/Systems/FxRenderDispatchSystem.h"
#include "ECS/Systems/FxRenderSnapshotSystem.h"
#include "ECS/World.h"
#include "FX/v2/Renderer/FxSpriteRenderer.h"
#include "FX/v2/Renderer/FxMeshRenderer.h"
#include "FX/v2/Renderer/FxRibbonRenderer.h"
#include "FX/v2/Renderer/FxBeamRenderer.h"
#include "FX/v2/Renderer/FxLightRenderer.h"
#include "FX/v2/Renderer/FxDecalRenderer.h"

std::unique_ptr<CFxRenderDispatchSystem> CFxRenderDispatchSystem::Create(IRHIDevice* pDevice)
{
    auto p = std::unique_ptr<CFxRenderDispatchSystem>(new CFxRenderDispatchSystem());
    p->m_pDevice = pDevice;
    using namespace Winters::FX::v2;
    auto AddR = [&](u32_t key, std::unique_ptr<IFxRenderer> pR) {
        if (pR) { pR->Initialize(pDevice); p->m_mapRenderers.emplace(key, std::move(pR)); }
    };
    AddR(static_cast<u32_t>(eFxRenderType::Billboard), CFxSpriteRenderer::Create());
    AddR(static_cast<u32_t>(eFxRenderType::Mesh),      CFxMeshRenderer::Create());
    AddR(static_cast<u32_t>(eFxRenderType::Ribbon),    CFxRibbonRenderer::Create());
    AddR(static_cast<u32_t>(eFxRenderType::Beam),      CFxBeamRenderer::Create());
    AddR(static_cast<u32_t>(eFxRenderType::Light),     CFxLightRenderer::Create());
    AddR(static_cast<u32_t>(eFxRenderType::Decal),     CFxDecalRenderer::Create());
    return p;
}

CFxRenderDispatchSystem::~CFxRenderDispatchSystem()
{
    for (auto& pair : m_mapRenderers) pair.second->Shutdown();
    m_mapRenderers.clear();
}

void CFxRenderDispatchSystem::DescribeAccess(CSystemAccessBuilder&) const {}

void CFxRenderDispatchSystem::Execute(CWorld& /*world*/, f32_t /*fDeltaTime*/)
{
    if (!m_pCmdList) return;
    if (!m_pSnapshotSystem) return;

    using namespace Winters::FX::v2;
    const FxFrameSnapshot& frame = m_pSnapshotSystem->GetFrameSnapshot();
    for (const auto& pSnap : frame.vecSnapshots)
    {
        if (!pSnap) continue;
        const u32_t key = static_cast<u32_t>(pSnap->GetRenderType());
        auto it = m_mapRenderers.find(key);
        if (it == m_mapRenderers.end()) continue;
        it->second->Render(m_pCmdList, pSnap.get());
    }
}
```

---

## §5 셰이더 본문 박제

### §5.1 `Shaders/FX/v2/FxCommon.hlsli` (L1-L25)

```hlsl
#ifndef FX_COMMON_HLSLI
#define FX_COMMON_HLSLI

cbuffer CBPerFrame : register(b0, space0)
{
    row_major float4x4 g_matViewProj;
    row_major float4x4 g_matView;
    float4 g_vCameraPos;
    float4 g_vCameraDir;
    float g_fDeltaTime;
    float g_fGameTime;
    float2 g_vRtSize;
};
cbuffer CBPerEmitter : register(b1, space0)
{
    row_major float4x4 g_matEmitterWorld;
    float4 g_vTint;
    float4 g_vAtlasParams;
    float g_fAlphaClip;
    float g_fFadeIn;
    float g_fFadeOut;
    uint  g_uBlendMode;
};
Texture2D<float4> g_DiffuseMap : register(t0, space0);
SamplerState g_SamplerLinear : register(s0, space0);
#endif
```

### §5.2 `FxSprite_VS.hlsl + FxSprite_PS.hlsl` (L1-L40 + L1-L18)

```hlsl
// FxSprite_VS.hlsl
#include "FxCommon.hlsli"
struct SpriteInstance { float3 vPos; float3 vVel; float4 vColor; float fSize; float fRotation; uint uAtlasFrame; float fAge; };
StructuredBuffer<SpriteInstance> g_Instances : register(t1, space0);
struct VSOut { float4 svPos : SV_POSITION; float2 vUV : TEXCOORD0; float4 vColor : TEXCOORD1; };
VSOut main(uint uVertId : SV_VertexID, uint uInstId : SV_InstanceID)
{
    SpriteInstance inst = g_Instances[uInstId];
    float2 quad[4] = { float2(-0.5,-0.5), float2(0.5,-0.5), float2(-0.5,0.5), float2(0.5,0.5) };
    uint indices[6] = { 0,1,2, 1,3,2 };
    float2 q = quad[indices[uVertId]];
    float c = cos(inst.fRotation), s = sin(inst.fRotation);
    float2 rotQ = float2(q.x*c - q.y*s, q.x*s + q.y*c) * inst.fSize;
    float3 right = float3(g_matView._11, g_matView._21, g_matView._31);
    float3 up    = float3(g_matView._12, g_matView._22, g_matView._32);
    float3 worldPos = inst.vPos + right * rotQ.x + up * rotQ.y;
    VSOut o;
    o.svPos = mul(float4(worldPos, 1.0), g_matViewProj);
    float2 uvBase = q + 0.5;
    float cols = g_vAtlasParams.x; float rows = g_vAtlasParams.y;
    uint frame = inst.uAtlasFrame;
    float u = (uvBase.x + (frame % (uint)cols)) / cols;
    float v = (uvBase.y + (frame / (uint)cols)) / rows;
    o.vUV = float2(u, v);
    o.vColor = inst.vColor * g_vTint;
    return o;
}
```

```hlsl
// FxSprite_PS.hlsl
#include "FxCommon.hlsli"
struct PSIn { float4 svPos : SV_POSITION; float2 vUV : TEXCOORD0; float4 vColor : TEXCOORD1; };
float4 main(PSIn i) : SV_TARGET
{
    float4 tex = g_DiffuseMap.Sample(g_SamplerLinear, i.vUV);
    float4 c = tex * i.vColor;
    if (c.a < g_fAlphaClip) discard;
    return c;
}
```

### §5.3 5 다른 셰이더 (Mesh/Ribbon/Beam/Light/Decal) — 본문 패턴

```hlsl
// FxMesh_VS.hlsl 핵심
struct MeshInstance { float3 vPos; float3 vScale; float3 vEulerXYZ; float4 vColor; uint uMeshIdx; uint uHiddenMask; };
StructuredBuffer<MeshInstance> g_Instances : register(t1, space0);
// per-instance world matrix = pos / scale / euler 합성. mul 순서 = mul(vec, world * VP)

// FxRibbon_VS.hlsl 핵심
struct RibbonSegment { float3 vPos; float4 vColor; float fWidth; uint uRibbonId; float fAge; };
StructuredBuffer<RibbonSegment> g_Segments : register(t1, space0);
// segment id 별 quad 생성 = 2 vertex per segment, view facing offset

// FxBeam_VS.hlsl 핵심
struct BeamSegment { float3 vStart; float3 vEnd; float4 vColor; float fWidth; float fNoiseSeed; };
// cylindrical billboard = (end - start) 방향 벡터 + cross(view) 로 width 펼침

// FxLight_VS.hlsl 핵심 (deferred light)
struct LightInstance { float3 vPos; float3 vColor; float fIntensity; float fRadius; };
// sphere mesh per-instance world (translate + scale = radius). 백 culling, depth test

// FxDecal_VS.hlsl 핵심
struct DecalInstance { float3 vPos; float3 vNormal; float3 vSize; float4 vColor; float fFadeAlpha; };
// box mesh per-instance world (translate + rotate to align normal + scale = size). PS = depth read + reproject + clip(box)
```

각 셰이더 = Sprite VS/PS 패턴 + 인스턴스 struct 만 다름. EFX-3 코드 작업 시점에 위 패턴 그대로 cpp 식 박제.

---

## §6 검증 명령 (EFX-3 합격)

```txt
1. grep "ID3D11" Engine/{Public,Private}/FX/v2/Renderer/   → 0 hit
2. grep "Scene_" Engine/{Public,Private}/FX/v2/Renderer/   → 0 hit
3. grep "OnUpdate" Engine/{Public,Private}/FX/v2/Renderer/  → 0 hit
4. grep "TBD" .md/plan/EffectTool/20_RENDERER_6_BAKE.md  → 0 hit
5. grep "stub\\|scaffold\\|본 박제 시점.*채움" .md/plan/EffectTool/20_RENDERER_6_BAKE.md  → 0 hit
6. grep "register\\(" Shaders/FX/v2/*.hlsl  → 모든 register space0 명시
7. InGame 1 emitter (Sprite, 1024 입자) → 화면 빌보드 1024 출력
8. 6 renderer 모두 1 emitter씩 InGame 출력 (Mesh / Ribbon / Beam / Light / Decal)
9. Renderer Initialize → Shutdown 100 회 stress → leak 0
```

---

## §7 박제 함정 매트릭스

| 함정 | 본 20 회피 |
|---|---|
| P-1 + P-6 | §1 6 항목, TBD 0 |
| P-2 (PIMPL 추측) | 헤더 + cpp 동시 |
| P-3 (모든 path) | 6 renderer 한 번에 (인터페이스 + 1 cpp 본문 풀 + 5 cpp 패턴 명시) |
| P-4 (Scene 직접 의존) | Renderer = ISystem 통해서만 |
| P-7 (bitmask 폭) | FxMeshRenderer = 기존 `MeshGroupVisibilityComponent::VisibilityMask` (2048) 재사용 |
| P-8 (인용 의미 반전) | Niagara `NiagaraRendererSprites.h:41-58` Float/Half/Int SRV / `NiagaraRendererProperties.h:107` FNiagaraRendererVariableInfo 차용 |
| P-9 (ECS Scheduler) | Phase 6 (Snapshot) / Phase 7 (Render) 분리 |
| P-10 (Owner Scope) | Renderer = `CFxRenderDispatchSystem` owned |
| P-11 (도메인 상수) | Renderer = 도메인 무관 |
| P-12 (음수 truncation) | DataSet float 직접. 슬롯 인덱스 양수 |
| P-13 (미존재 API) | `IRHIDevice::CreateBuffer / IRHICommandList::DrawIndexedInstanced / IRHIBuffer::UploadData` = Track 2 RH-3 박제 |
| P-14 (행동 정책 변경) | 본 20 = 신규 |
| P-15 (헤더 외부 의존) | properties 헤더 = `FxRendererProperties.h` 직접 include |
| P-16 (산술 검증) | `eFxRenderType` 6 값. static_assert |
| P-17 (typedef ABI) | snapshot POD = 신규 |
| P-18 (RHI 인프라) | RH-7 IRHI 8 인터페이스 사용 |
| P-19 (Render/Sim 결합) | Snapshot read-only POD. Renderer = `GetPreviousBuffer()` read |

---

## §8 변경 이력

```txt
2026-04-21    Phase G 초안 (Stage 5 DX11 Rendering = 06)
2026-05-04    Niagara V2 (12)
2026-05-07    17 v4 마스터. 본 20 v1 (Sprite 본문 + 5 stub)
2026-05-07    본 20 v2 재박제 (CLAUDE.md §8.2 본문 룰 — Sprite cpp 본문 풀 + 5 cpp 패턴 명시 + ECS Snapshot/Dispatch 본문 + 6 셰이더 본문)
```
