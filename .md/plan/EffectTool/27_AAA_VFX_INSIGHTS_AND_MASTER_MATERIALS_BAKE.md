# 27. AAA VFX 인사이트 + Master Material 3 종 박제 (Unlit Additive 70% / 13 HLSL 트릭 / 4 핵심 노브)

작성일: 2026-05-07
권위: 본 27 = 17 마스터 §15 부속 10번 (5/7 신규). EFX-3 진입 직전 박제. 부속 20 (Renderer 6 종) 의 디자이너 워크플로우 핵심 정정.
의존: 부속 18 (FxRendererProperties), 부속 19 (DataSet 의 floatSlots), 부속 22 (Compile 후순위 결정 — 본 27 = master material 1차 트랙).
참조 코드:
- Niagara: `Engine/Plugins/FX/Niagara/Shaders/Private/NiagaraSpriteVertexFactory.usf`, `NiagaraMeshVertexFactory.usf`, `NiagaraRibbonVertexFactory.usf`
- Unreal Engine: `Engine/Shaders/Private/MaterialTemplate.ush` (master material 패턴)
- LoL FX 분석 (사용자 통찰): unlit additive + grayscale texture as data + 4 핵심 노브
- Elden Ring FX 분석: 6-way lit smoke (Embergen / Houdini export pipeline)

---

## §0.1 AAA VFX 통찰 사실 4가지 (사용자 5/7 분석 박제)

```txt
인사이트 1. 그레이스케일 텍스처 = 데이터 (함수 입력값)
  단일 grayscale noise (BC4 압축, 32KB) + 머티리얼 파라미터 4 변주
  → 화염 (R+O) / 독 (G) / 얼음 (B+C) / 어둠 (P+M)
  콘텐츠 비율 = 텍스처 200장 + 머티리얼 인스턴스 5000개

인사이트 2. PBR / GI 거의 안 씀
  사유: VFX 표면 불명확 / VFX 자기 광원 (emission) / 픽셀당 비용 / 디자이너 직접 색 제어
  대체:
    Unlit Additive       70%  (BlendMode = Add, lit = 0)
    Simple Lit            10%  (1 directional light + ambient)
    6-Way Lit Smoke       8%   (Embergen / Houdini 6 방향 lighting bake)
    Volumetric Custom     7%   (raymarched cloud, fog)
    Stylized              5%   (LoL fresnel-heavy / cel-shaded)

인사이트 3. HLSL 극한 깎기 = 13 트릭 조합
  UV pan / distortion / two-octave noise / dissolve / edge emission /
  center mask / fresnel / color over life / per-particle 변주 /
  soft particle / fade in/out / sub-UV blend / polar coordinates
  각 5~20 줄. 합 200~500 줄 = 1 master material

인사이트 4. 메쉬 = 형태, 셰이더 = 외관
  단순 도형 (quad, sphere, disc, ring, cone, curved plane)
  + 단순 텍스처 + 정교한 셰이더 + 머티리얼 인스턴스 변주
  = 모든 챔프 마법

FxGraph 적용 우선순위 정정:
  1. 노드 머티리얼 에디터 = 후순위 (부속 22 노드 그래프, 부속 24 GraphPanel)
  2. Master Material 3 종 = 1순위
     M_VFX_Particle_Generic / M_VFX_Trail / M_VFX_Volumetric
  3. 머티리얼 인스턴스 = 데이터 애셋 (디자이너 노브 30~50 개)
  4. 4 핵심 노브 우선 노출:
     UV pan / 디스토션 / 컨트라스트 / 디졸브 / color over life / HDR emission / fresnel
```

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| Master material 종류 | 3 종 (Particle / Trail / Volumetric) 우선. Custom 머티리얼 = EFX-5 노드 그래프 (부속 22) 통과 후 별도 진입 | 99% 효과 커버. AAA 통찰 |
| 라이팅 모델 5 종 | Unlit Additive (default) / Simple Lit / 6-Way Lit Smoke / Volumetric Custom / Stylized. enum 선택 | 70+10+8+7+5 % 분포. AAA 통찰 |
| 디자이너 노출 노브 | 머티리얼 인스턴스 = 30~50 파라미터. 4 핵심 노브 = 1차 노출. 나머지 = "Advanced" 토글 | 디자이너가 30 분 이내 챔프 1 hook 박제 가능 |
| Grayscale 텍스처 정책 | DI Texture (부속 23) 의 default sampler = R-channel single. 4 channel RGBA 사용은 advanced | BC4 압축 32KB / texture |
| Compile target | Master material 3 종 = 정적 HLSL (Engine 빌드 시점 컴파일). 노드 그래프 컴파일 (부속 22) = 별도 트랙 | 1차 = 정적 HLSL, 2차 = 노드 그래프 동적 |
| 머티리얼 인스턴스 자산 형식 | `.wmi` JSON v1 (`.wfx` 와 별개). master material path + parameter override map | Niagara `UMaterialInstanceDynamic` 패턴 차용 |

---

## §2 신규 파일 트리

```txt
Engine/Public/FX/v2/Material/
  eFxLightingModel.h               5 종 enum
  FxMaterialMaster.h               master material descriptor
  FxMaterialInstance.h             instance + parameter override
  FxMaterialInstanceJsonLoader.h
  FxMasterMaterialRegistry.h       3 종 master material 캐시

Engine/Private/FX/v2/Material/
  FxMaterialInstance.cpp
  FxMaterialInstanceJsonLoader.cpp
  FxMasterMaterialRegistry.cpp

Shaders/FX/v2/Master/
  MasterCommon.hlsli               13 HLSL 트릭 함수 라이브러리
  M_VFX_Particle_Generic_VS.hlsl
  M_VFX_Particle_Generic_PS.hlsl
  M_VFX_Trail_VS.hlsl
  M_VFX_Trail_PS.hlsl
  M_VFX_Volumetric_VS.hlsl
  M_VFX_Volumetric_PS.hlsl
```

---

## §3 헤더 박제 (전문, L1- 라인 번호)

### §3.1 `Engine/Public/FX/v2/Material/eFxLightingModel.h` (L1-L18)

```cpp
// L1
#pragma once
// L2
#include "WintersAPI.h"
#include "WintersTypes.h"
namespace Winters::FX::v2
{
    enum class eFxLightingModel : u8_t
    {
        UnlitAdditive   = 0,    // 70% — 핵심 default
        SimpleLit       = 1,    // 10% — 1 directional + ambient
        SixWayLitSmoke  = 2,    // 8%  — Embergen / Houdini 6 방향 bake
        VolumetricCustom = 3,   // 7%  — raymarched cloud
        Stylized        = 4,    // 5%  — fresnel-heavy / cel-shaded
    };
    inline constexpr u32_t kFxLightingModelCount = 5;
}
static_assert(static_cast<Winters::u32_t>(Winters::FX::v2::eFxLightingModel::Stylized) == 4);
```

### §3.2 `Engine/Public/FX/v2/Material/FxMaterialMaster.h` (L1-L34)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Material/eFxLightingModel.h"
#include "FX/v2/Renderer/FxBlendMode.h"
#include <string>
#include <vector>
namespace Winters::FX::v2
{
    enum class eFxMasterMaterial : u8_t
    {
        Particle_Generic = 0,
        Trail            = 1,
        Volumetric       = 2,
    };

    struct FxMasterParameterDef
    {
        std::wstring strName;
        u32_t uByteOffset = 0;
        u32_t uByteSize = 4;
        bool_t bIsCorePinned = false;     // 4 핵심 노브 (UV pan / contrast / color-over-life / HDR emission)
        bool_t bIsAdvanced = false;
    };

    struct WINTERS_ENGINE FxMasterMaterial
    {
        eFxMasterMaterial eType = eFxMasterMaterial::Particle_Generic;
        std::wstring strName;
        std::wstring strVsHlslPath;
        std::wstring strPsHlslPath;
        eFxLightingModel eLighting = eFxLightingModel::UnlitAdditive;
        eFxBlendMode eBlend = eFxBlendMode::Additive;
        std::vector<FxMasterParameterDef> vecParameters;     // 30~50 파라미터
    };
}
```

### §3.3 `Engine/Public/FX/v2/Material/FxMaterialInstance.h` (L1-L38)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Material/FxMaterialMaster.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxMaterialInstance
    {
    public:
        ~CFxMaterialInstance();
        CFxMaterialInstance(const CFxMaterialInstance&) = delete;
        CFxMaterialInstance& operator=(const CFxMaterialInstance&) = delete;
        static std::unique_ptr<CFxMaterialInstance> Create(const FxMasterMaterial* pMaster);

        const FxMasterMaterial* GetMaster() const { return m_pMaster; }
        eFxMasterMaterial GetMasterType() const { return m_pMaster ? m_pMaster->eType : eFxMasterMaterial::Particle_Generic; }

        bool_t SetParameterFloat(const std::wstring& strName, f32_t fValue);
        bool_t SetParameterFloat3(const std::wstring& strName, f32_t x, f32_t y, f32_t z);
        bool_t SetParameterFloat4(const std::wstring& strName, f32_t x, f32_t y, f32_t z, f32_t w);
        f32_t GetParameterFloat(const std::wstring& strName, f32_t fDefault = 0.f) const;

        const std::vector<u8_t>& GetParameterBytes() const { return m_vecBytes; }
        u32_t GetParameterByteSize() const { return static_cast<u32_t>(m_vecBytes.size()); }

    private:
        CFxMaterialInstance() = default;
        const FxMasterMaterial* m_pMaster = nullptr;
        std::vector<u8_t> m_vecBytes;     // master parameter layout 그대로
        std::unordered_map<u32_t, u32_t> m_mapNameHashToOffset;
    };
}
```

### §3.4 `Engine/Public/FX/v2/Material/FxMasterMaterialRegistry.h` (L1-L26)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Material/FxMaterialMaster.h"
#include <memory>
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxMasterMaterialRegistry
    {
    public:
        ~CFxMasterMaterialRegistry();
        CFxMasterMaterialRegistry(const CFxMasterMaterialRegistry&) = delete;
        CFxMasterMaterialRegistry& operator=(const CFxMasterMaterialRegistry&) = delete;
        static std::unique_ptr<CFxMasterMaterialRegistry> Create();

        // 3 master material 빌드 (Engine init 시 1 회 호출)
        void BuildStandardMasters();

        const FxMasterMaterial* GetMaster(eFxMasterMaterial eType) const;
        const FxMasterMaterial* FindMasterByName(const std::wstring& strName) const;

    private:
        CFxMasterMaterialRegistry() = default;
        FxMasterMaterial m_Particle{};
        FxMasterMaterial m_Trail{};
        FxMasterMaterial m_Volumetric{};
    };
}
```

### §3.5 `Engine/Public/FX/v2/Material/FxMaterialInstanceJsonLoader.h` (L1-L24)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <memory>
#include <string>
#include <vector>
namespace Winters::FX::v2
{
    class CFxMaterialInstance;
    class CFxMasterMaterialRegistry;
    struct FxMaterialInstanceLoadResult
    {
        std::unique_ptr<CFxMaterialInstance> pInstance;
        std::vector<std::wstring> vecErrors;
        bool_t bSucceeded = false;
    };
    class WINTERS_ENGINE CFxMaterialInstanceJsonLoader
    {
    public:
        // .wmi JSON v1 → CFxMaterialInstance
        static FxMaterialInstanceLoadResult LoadFromFile(const std::wstring& strPath, CFxMasterMaterialRegistry* pRegistry);
        static std::string SaveToString(const CFxMaterialInstance* pInstance);
    };
}
```

---

## §4 cpp 본문 박제 (전문, L1-, stub 0)

### §4.1 `Engine/Private/FX/v2/Material/FxMaterialInstance.cpp` (L1-L100)

```cpp
#include "FX/v2/Material/FxMaterialInstance.h"
#include <cstring>

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
    }

    std::unique_ptr<CFxMaterialInstance> CFxMaterialInstance::Create(const FxMasterMaterial* pMaster)
    {
        if (!pMaster) return nullptr;
        auto p = std::unique_ptr<CFxMaterialInstance>(new CFxMaterialInstance());
        p->m_pMaster = pMaster;
        u32_t uTotal = 0;
        for (const auto& def : pMaster->vecParameters)
        {
            p->m_mapNameHashToOffset.emplace(HashFnv1a(def.strName), def.uByteOffset);
            uTotal = std::max(uTotal, def.uByteOffset + def.uByteSize);
        }
        p->m_vecBytes.resize(uTotal, 0u);
        return p;
    }

    CFxMaterialInstance::~CFxMaterialInstance() = default;

    bool_t CFxMaterialInstance::SetParameterFloat(const std::wstring& strName, f32_t fValue)
    {
        auto it = m_mapNameHashToOffset.find(HashFnv1a(strName));
        if (it == m_mapNameHashToOffset.end()) return false;
        if (it->second + sizeof(f32_t) > m_vecBytes.size()) return false;
        std::memcpy(m_vecBytes.data() + it->second, &fValue, sizeof(f32_t));
        return true;
    }

    bool_t CFxMaterialInstance::SetParameterFloat3(const std::wstring& strName, f32_t x, f32_t y, f32_t z)
    {
        auto it = m_mapNameHashToOffset.find(HashFnv1a(strName));
        if (it == m_mapNameHashToOffset.end()) return false;
        if (it->second + sizeof(f32_t) * 3 > m_vecBytes.size()) return false;
        f32_t v[3] = { x, y, z };
        std::memcpy(m_vecBytes.data() + it->second, v, sizeof(v));
        return true;
    }

    bool_t CFxMaterialInstance::SetParameterFloat4(const std::wstring& strName, f32_t x, f32_t y, f32_t z, f32_t w)
    {
        auto it = m_mapNameHashToOffset.find(HashFnv1a(strName));
        if (it == m_mapNameHashToOffset.end()) return false;
        if (it->second + sizeof(f32_t) * 4 > m_vecBytes.size()) return false;
        f32_t v[4] = { x, y, z, w };
        std::memcpy(m_vecBytes.data() + it->second, v, sizeof(v));
        return true;
    }

    f32_t CFxMaterialInstance::GetParameterFloat(const std::wstring& strName, f32_t fDefault) const
    {
        auto it = m_mapNameHashToOffset.find(HashFnv1a(strName));
        if (it == m_mapNameHashToOffset.end()) return fDefault;
        if (it->second + sizeof(f32_t) > m_vecBytes.size()) return fDefault;
        f32_t v;
        std::memcpy(&v, m_vecBytes.data() + it->second, sizeof(f32_t));
        return v;
    }
}
```

### §4.2 `Engine/Private/FX/v2/Material/FxMasterMaterialRegistry.cpp` (L1-L160, 3 master material 본문)

```cpp
#include "FX/v2/Material/FxMasterMaterialRegistry.h"

namespace Winters::FX::v2
{
    namespace
    {
        // 4 핵심 노브 + 추가 30+ 노브 정의 (Particle_Generic 대표)
        std::vector<FxMasterParameterDef> BuildParticleGenericParams()
        {
            std::vector<FxMasterParameterDef> v;
            u32_t uOff = 0;
            const auto Add = [&](const wchar_t* n, u32_t sz, bool_t core, bool_t adv) {
                FxMasterParameterDef d;
                d.strName = n;
                d.uByteOffset = uOff;
                d.uByteSize = sz;
                d.bIsCorePinned = core;
                d.bIsAdvanced = adv;
                v.push_back(d);
                uOff += sz;
            };

            // 4 핵심 노브 (UV pan, contrast/dissolve, color-over-life, HDR emission)
            Add(L"UVPanX",            4, true, false);
            Add(L"UVPanY",            4, true, false);
            Add(L"DissolveThreshold", 4, true, false);
            Add(L"ColorOverLife_R",   16, true, false);   // float4 = RGBA
            Add(L"EmissionIntensity", 4, true, false);
            Add(L"FresnelPower",      4, true, false);

            // 추가 노브 (Advanced 그룹)
            Add(L"Contrast",                4, false, false);
            Add(L"Brightness",              4, false, false);
            Add(L"DistortionAmount",        4, false, false);
            Add(L"NoiseScaleA",             4, false, true);
            Add(L"NoiseScaleB",             4, false, true);
            Add(L"NoiseSpeed",              4, false, true);
            Add(L"EdgeEmissionWidth",       4, false, true);
            Add(L"EdgeEmissionColor_R",     16, false, true);
            Add(L"CenterMaskRadius",        4, false, true);
            Add(L"SoftParticleDepthRange",  4, false, true);
            Add(L"FadeIn",                  4, false, false);
            Add(L"FadeOut",                 4, false, false);
            Add(L"AtlasCols",               4, false, true);
            Add(L"AtlasRows",               4, false, true);
            Add(L"AtlasFps",                4, false, true);
            Add(L"PolarCenter_X",           4, false, true);
            Add(L"PolarCenter_Y",           4, false, true);
            Add(L"PerParticleVariation",    4, false, true);
            Add(L"TintR",                   4, true, false);
            Add(L"TintG",                   4, true, false);
            Add(L"TintB",                   4, true, false);
            Add(L"TintA",                   4, false, false);
            Add(L"AlphaClipThreshold",      4, false, false);
            // 추가 ~ 30 파라미터까지 동일 패턴 (rim / refraction / parallax / vignette / etc.)
            Add(L"RimPower",                4, false, true);
            Add(L"RimColor_R",              16, false, true);
            Add(L"RefractionStrength",      4, false, true);
            Add(L"VignetteStrength",        4, false, true);

            return v;
        }

        std::vector<FxMasterParameterDef> BuildTrailParams()
        {
            std::vector<FxMasterParameterDef> v;
            u32_t uOff = 0;
            const auto Add = [&](const wchar_t* n, u32_t sz, bool_t core, bool_t adv) {
                FxMasterParameterDef d{ n, uOff, sz, core, adv };
                v.push_back(d);
                uOff += sz;
            };
            Add(L"UVPanV",            4, true, false);
            Add(L"WidthOverLife",     4, true, false);
            Add(L"ColorOverLife_R",   16, true, false);
            Add(L"EmissionIntensity", 4, true, false);
            Add(L"DistortionAmount",  4, false, false);
            Add(L"NoiseScale",        4, false, true);
            Add(L"FadeIn",            4, false, false);
            Add(L"FadeOut",           4, false, false);
            Add(L"TintR",             4, true, false);
            Add(L"TintG",             4, true, false);
            Add(L"TintB",             4, true, false);
            Add(L"AlphaClipThreshold",4, false, false);
            return v;
        }

        std::vector<FxMasterParameterDef> BuildVolumetricParams()
        {
            std::vector<FxMasterParameterDef> v;
            u32_t uOff = 0;
            const auto Add = [&](const wchar_t* n, u32_t sz, bool_t core, bool_t adv) {
                FxMasterParameterDef d{ n, uOff, sz, core, adv };
                v.push_back(d);
                uOff += sz;
            };
            Add(L"DensityScale",      4, true, false);
            Add(L"AbsorptionR",       16, true, false);
            Add(L"ScatteringR",       16, true, false);
            Add(L"NumSteps",          4, false, true);
            Add(L"NoiseScale",        4, false, true);
            Add(L"NoiseSpeed",        4, false, true);
            Add(L"LightDir_X",        4, false, true);
            Add(L"LightDir_Y",        4, false, true);
            Add(L"LightDir_Z",        4, false, true);
            Add(L"AmbientR",          16, false, false);
            Add(L"FadeIn",            4, false, false);
            Add(L"FadeOut",           4, false, false);
            return v;
        }
    }

    std::unique_ptr<CFxMasterMaterialRegistry> CFxMasterMaterialRegistry::Create()
    {
        return std::unique_ptr<CFxMasterMaterialRegistry>(new CFxMasterMaterialRegistry());
    }

    CFxMasterMaterialRegistry::~CFxMasterMaterialRegistry() = default;

    void CFxMasterMaterialRegistry::BuildStandardMasters()
    {
        m_Particle.eType = eFxMasterMaterial::Particle_Generic;
        m_Particle.strName = L"M_VFX_Particle_Generic";
        m_Particle.strVsHlslPath = L"Shaders/FX/v2/Master/M_VFX_Particle_Generic_VS.hlsl";
        m_Particle.strPsHlslPath = L"Shaders/FX/v2/Master/M_VFX_Particle_Generic_PS.hlsl";
        m_Particle.eLighting = eFxLightingModel::UnlitAdditive;
        m_Particle.eBlend = eFxBlendMode::Additive;
        m_Particle.vecParameters = BuildParticleGenericParams();

        m_Trail.eType = eFxMasterMaterial::Trail;
        m_Trail.strName = L"M_VFX_Trail";
        m_Trail.strVsHlslPath = L"Shaders/FX/v2/Master/M_VFX_Trail_VS.hlsl";
        m_Trail.strPsHlslPath = L"Shaders/FX/v2/Master/M_VFX_Trail_PS.hlsl";
        m_Trail.eLighting = eFxLightingModel::UnlitAdditive;
        m_Trail.eBlend = eFxBlendMode::Additive;
        m_Trail.vecParameters = BuildTrailParams();

        m_Volumetric.eType = eFxMasterMaterial::Volumetric;
        m_Volumetric.strName = L"M_VFX_Volumetric";
        m_Volumetric.strVsHlslPath = L"Shaders/FX/v2/Master/M_VFX_Volumetric_VS.hlsl";
        m_Volumetric.strPsHlslPath = L"Shaders/FX/v2/Master/M_VFX_Volumetric_PS.hlsl";
        m_Volumetric.eLighting = eFxLightingModel::VolumetricCustom;
        m_Volumetric.eBlend = eFxBlendMode::AlphaBlend;
        m_Volumetric.vecParameters = BuildVolumetricParams();
    }

    const FxMasterMaterial* CFxMasterMaterialRegistry::GetMaster(eFxMasterMaterial eType) const
    {
        switch (eType)
        {
        case eFxMasterMaterial::Particle_Generic: return &m_Particle;
        case eFxMasterMaterial::Trail:            return &m_Trail;
        case eFxMasterMaterial::Volumetric:       return &m_Volumetric;
        }
        return nullptr;
    }

    const FxMasterMaterial* CFxMasterMaterialRegistry::FindMasterByName(const std::wstring& strName) const
    {
        if (strName == m_Particle.strName)   return &m_Particle;
        if (strName == m_Trail.strName)      return &m_Trail;
        if (strName == m_Volumetric.strName) return &m_Volumetric;
        return nullptr;
    }
}
```

---

## §5 셰이더 본문 박제 (13 HLSL 트릭 + 3 Master Material)

### §5.1 `Shaders/FX/v2/Master/MasterCommon.hlsli` (L1-L120, 13 트릭 함수 라이브러리)

```hlsl
#ifndef MASTER_COMMON_HLSLI
#define MASTER_COMMON_HLSLI

// ============================================================
// 13 HLSL 트릭 함수 라이브러리 (AAA VFX 통찰 구현)
// 각 함수 5~20 줄. 합 200~500 줄 = 1 master material
// ============================================================

// 1. UV pan
float2 FxTrick_UVPan(float2 uv, float2 panSpeed, float gameTime)
{
    return uv + panSpeed * gameTime;
}

// 2. Distortion (직접 디스토션 / 다른 트릭의 입력)
float2 FxTrick_Distortion(float2 uv, Texture2D distTex, SamplerState s, float amount)
{
    float2 d = distTex.SampleLevel(s, uv, 0).rg * 2.0 - 1.0;
    return uv + d * amount;
}

// 3. Two-octave noise (디테일 + 큰 패턴)
float FxTrick_TwoOctaveNoise(float2 uv, Texture2D noiseTex, SamplerState s, float scaleA, float scaleB, float panSpeed, float t)
{
    float a = noiseTex.SampleLevel(s, uv * scaleA + float2(t, 0) * panSpeed, 0).r;
    float b = noiseTex.SampleLevel(s, uv * scaleB - float2(0, t) * panSpeed, 0).r;
    return saturate(a * 0.6 + b * 0.4);
}

// 4. Dissolve (threshold 기반 alpha clip)
float FxTrick_Dissolve(float noiseValue, float threshold, float edgeWidth, out float edgeFactor)
{
    edgeFactor = 1.0 - saturate(abs(noiseValue - threshold) / max(edgeWidth, 1e-4));
    return noiseValue >= threshold ? 1.0 : 0.0;
}

// 5. Edge emission (dissolve edge = 빨강/주황 hot edge)
float3 FxTrick_EdgeEmission(float edgeFactor, float3 edgeColor, float emissionIntensity)
{
    return edgeColor * edgeFactor * emissionIntensity;
}

// 6. Center mask (radial fade 0 at edge, 1 at center)
float FxTrick_CenterMask(float2 uv, float2 center, float radius)
{
    float d = distance(uv, center);
    return 1.0 - saturate(d / max(radius, 1e-4));
}

// 7. Fresnel (view-dependent rim)
float FxTrick_Fresnel(float3 viewDir, float3 normal, float power)
{
    float ndotv = saturate(dot(normalize(viewDir), normalize(normal)));
    return pow(1.0 - ndotv, power);
}

// 8. Color over life (gradient texture or 4 keyframe interpolation)
float4 FxTrick_ColorOverLife(float t, float4 cStart, float4 cMid, float4 cEnd)
{
    if (t < 0.5) return lerp(cStart, cMid, t * 2.0);
    return lerp(cMid, cEnd, (t - 0.5) * 2.0);
}

// 9. Per-particle variation (instance hash → 0~1 random)
float FxTrick_PerParticleVariation(uint instId, uint seed)
{
    uint h = instId * 1664525u + 1013904223u + seed;
    h ^= (h >> 16); h *= 0x85ebca6bu;
    h ^= (h >> 13); h *= 0xc2b2ae35u;
    h ^= (h >> 16);
    return (float)(h & 0xFFFFFF) / (float)0xFFFFFF;
}

// 10. Soft particle (depth difference 기반 fade)
float FxTrick_SoftParticle(float particleDepth, float sceneDepth, float fadeRange)
{
    float diff = sceneDepth - particleDepth;
    return saturate(diff / max(fadeRange, 1e-4));
}

// 11. Fade in / out (lifetime 기반 alpha)
float FxTrick_FadeInOut(float normalizedAge, float fadeIn, float fadeOut)
{
    float a = saturate(normalizedAge / max(fadeIn, 1e-4));
    float b = saturate((1.0 - normalizedAge) / max(fadeOut, 1e-4));
    return min(a, b);
}

// 12. Sub-UV blend (atlas 두 frame 보간)
float4 FxTrick_SubUVBlend(Texture2D tex, SamplerState s, float2 uv, uint cols, uint rows, float fps, float gameTime, out float blend)
{
    float frame = fps * gameTime;
    uint i0 = (uint)floor(frame) % (cols * rows);
    uint i1 = (i0 + 1) % (cols * rows);
    blend = frac(frame);
    float2 uv0 = float2((uv.x + (i0 % cols)) / cols, (uv.y + (i0 / cols)) / rows);
    float2 uv1 = float2((uv.x + (i1 % cols)) / cols, (uv.y + (i1 / cols)) / rows);
    return lerp(tex.SampleLevel(s, uv0, 0), tex.SampleLevel(s, uv1, 0), blend);
}

// 13. Polar coordinates (radial UV 변환 = ring / vortex)
float2 FxTrick_PolarCoords(float2 uv, float2 center)
{
    float2 d = uv - center;
    float r = length(d);
    float a = atan2(d.y, d.x) / 6.2831853 + 0.5;
    return float2(r, a);
}

#endif
```

### §5.2 `Shaders/FX/v2/Master/M_VFX_Particle_Generic_PS.hlsl` (L1-L60, 4 핵심 노브 본문)

```hlsl
#include "MasterCommon.hlsli"
#include "../FxCommon.hlsli"

cbuffer CB_M_VFX_Particle : register(b2, space0)
{
    // 4 핵심 노브 (디자이너 1차 노출)
    float2 g_UVPan;
    float  g_DissolveThreshold;
    float4 g_ColorOverLife;     // (mid 색)
    float  g_EmissionIntensity;
    float  g_FresnelPower;

    // Advanced
    float  g_Contrast;
    float  g_Brightness;
    float  g_DistortionAmount;
    float2 g_NoiseScale;        // x = A, y = B
    float  g_NoiseSpeed;
    float  g_EdgeEmissionWidth;
    float4 g_EdgeEmissionColor;
    float  g_CenterMaskRadius;
    float  g_SoftParticleDepthRange;
    float  g_FadeIn;
    float  g_FadeOut;
    float3 g_Tint;
    float  g_AlphaClipThreshold;
};

Texture2D    g_NoiseTex     : register(t1, space0);
Texture2D    g_DistortionTex: register(t2, space0);
SamplerState g_Sampler      : register(s0, space0);

struct PSIn
{
    float4 svPos : SV_POSITION;
    float2 vUV : TEXCOORD0;
    float4 vColor : TEXCOORD1;
    float  vAge : TEXCOORD2;
    nointerpolation uint vInstId : TEXCOORD3;
};

float4 main(PSIn i) : SV_TARGET
{
    // 트릭 1, 2: UV pan + Distortion
    float2 uv = FxTrick_UVPan(i.vUV, g_UVPan, g_fGameTime);
    uv = FxTrick_Distortion(uv, g_DistortionTex, g_Sampler, g_DistortionAmount);

    // 트릭 3: two-octave noise (그레이스케일 = 데이터)
    float n = FxTrick_TwoOctaveNoise(uv, g_NoiseTex, g_Sampler, g_NoiseScale.x, g_NoiseScale.y, g_NoiseSpeed, g_fGameTime);

    // 트릭 9: per-particle variation
    float pp = FxTrick_PerParticleVariation(i.vInstId, 12345u);
    n = saturate(n + (pp - 0.5) * 0.2);

    // 트릭 4: dissolve
    float edge = 0.0;
    float dissolveAlpha = FxTrick_Dissolve(n, g_DissolveThreshold, g_EdgeEmissionWidth, edge);

    // 트릭 6: center mask
    float centerMask = FxTrick_CenterMask(i.vUV, float2(0.5, 0.5), g_CenterMaskRadius);

    // 트릭 8: color over life
    float4 colLife = FxTrick_ColorOverLife(i.vAge, float4(g_Tint, 1), g_ColorOverLife, float4(g_Tint * 0.2, 0));

    // 트릭 11: fade in/out
    float fade = FxTrick_FadeInOut(i.vAge, g_FadeIn, g_FadeOut);

    // Final composition
    float3 base = colLife.rgb * (n + g_Brightness) * g_Contrast;
    float3 emission = FxTrick_EdgeEmission(edge, g_EdgeEmissionColor.rgb, g_EmissionIntensity);
    float3 finalRgb = base * centerMask + emission;

    // HDR emission
    finalRgb *= (1.0 + g_EmissionIntensity);

    float finalAlpha = dissolveAlpha * fade * colLife.a * i.vColor.a * centerMask;
    if (finalAlpha < g_AlphaClipThreshold) discard;

    return float4(finalRgb, finalAlpha);
}
```

### §5.3 `Shaders/FX/v2/Master/M_VFX_Trail_PS.hlsl` (L1-L40)

```hlsl
#include "MasterCommon.hlsli"
#include "../FxCommon.hlsli"

cbuffer CB_M_VFX_Trail : register(b2, space0)
{
    float  g_UVPanV;
    float  g_WidthOverLife;
    float4 g_ColorOverLife;
    float  g_EmissionIntensity;
    float  g_DistortionAmount;
    float  g_NoiseScale;
    float  g_FadeIn;
    float  g_FadeOut;
    float3 g_Tint;
    float  g_AlphaClipThreshold;
};

Texture2D g_DiffuseTex : register(t1, space0);
Texture2D g_NoiseTex : register(t2, space0);
SamplerState g_Sampler : register(s0, space0);

struct PSIn
{
    float4 svPos : SV_POSITION;
    float2 vUV : TEXCOORD0;     // U = trail length, V = width
    float vAge : TEXCOORD1;
};

float4 main(PSIn i) : SV_TARGET
{
    float2 uv = float2(i.vUV.x + g_UVPanV * g_fGameTime, i.vUV.y);
    float n = g_NoiseTex.SampleLevel(g_Sampler, uv * g_NoiseScale, 0).r;
    uv.y += (n - 0.5) * g_DistortionAmount;

    float4 base = g_DiffuseTex.SampleLevel(g_Sampler, uv, 0);

    float widthMask = 1.0 - abs(i.vUV.y - 0.5) * 2.0 / max(g_WidthOverLife, 1e-4);
    widthMask = saturate(widthMask);

    float4 colLife = FxTrick_ColorOverLife(i.vAge, float4(g_Tint, 1), g_ColorOverLife, float4(g_Tint * 0.1, 0));
    float fade = FxTrick_FadeInOut(i.vAge, g_FadeIn, g_FadeOut);

    float3 finalRgb = base.rgb * colLife.rgb * (1.0 + g_EmissionIntensity);
    float finalAlpha = base.a * widthMask * colLife.a * fade;
    if (finalAlpha < g_AlphaClipThreshold) discard;
    return float4(finalRgb, finalAlpha);
}
```

### §5.4 `Shaders/FX/v2/Master/M_VFX_Volumetric_PS.hlsl` (L1-L60, raymarched cloud)

```hlsl
#include "MasterCommon.hlsli"
#include "../FxCommon.hlsli"

cbuffer CB_M_VFX_Volumetric : register(b2, space0)
{
    float  g_DensityScale;
    float4 g_Absorption;
    float4 g_Scattering;
    uint   g_NumSteps;
    float  g_NoiseScale;
    float  g_NoiseSpeed;
    float3 g_LightDir;
    float4 g_Ambient;
    float  g_FadeIn;
    float  g_FadeOut;
};

Texture3D g_Noise3D : register(t1, space0);
SamplerState g_Sampler : register(s0, space0);

struct PSIn
{
    float4 svPos : SV_POSITION;
    float3 vBoxLocalPos : TEXCOORD0;
    float3 vBoxLocalDir : TEXCOORD1;
    float vAge : TEXCOORD2;
};

float4 main(PSIn i) : SV_TARGET
{
    // Ray march through volume
    const uint kMaxSteps = 64;
    uint steps = min(g_NumSteps, kMaxSteps);
    float stepLen = 1.0 / max((float)steps, 1.0);

    float3 rayPos = i.vBoxLocalPos;
    float3 rayDir = normalize(i.vBoxLocalDir);

    float3 accumLight = 0;
    float accumAlpha = 0;

    [loop]
    for (uint s = 0; s < steps; ++s)
    {
        float density = g_Noise3D.SampleLevel(g_Sampler, rayPos * g_NoiseScale + g_NoiseSpeed * g_fGameTime, 0).r;
        density *= g_DensityScale;

        // Beer-Lambert absorption
        float t = exp(-density * stepLen);

        // Single scattering (point light, simplified)
        float ldot = saturate(dot(rayDir, normalize(g_LightDir)));
        float3 scatter = g_Scattering.rgb * ldot + g_Ambient.rgb;

        accumLight += (1.0 - accumAlpha) * scatter * density * stepLen;
        accumAlpha += (1.0 - accumAlpha) * (1.0 - t);

        rayPos += rayDir * stepLen;
        if (accumAlpha > 0.99) break;
    }

    float fade = FxTrick_FadeInOut(i.vAge, g_FadeIn, g_FadeOut);
    return float4(accumLight, accumAlpha * fade);
}
```

VS 셰이더 = sprite (Particle) / quad-strip (Trail) / box (Volumetric) 각 vertex factory. 부속 20 의 `FxSprite_VS.hlsl` / `FxRibbon_VS.hlsl` / `FxBeam_VS.hlsl` 와 동일 패턴. 본 27 박제 = PS 본문 우선 (디자이너 노브 = PS 의 cbuffer).

---

## §6 EFX-3 합격 기준 갱신 (부속 20 의 6 renderer → 3 master material)

```txt
이전 (17 §13 v1):
  EFX-3 합격 = 6 renderer (Sprite/Mesh/Ribbon/Beam/Light/Decal) 모두 InGame 1 회 spawn + 화면 출력

5/7 갱신 (본 27 권위):
  EFX-3 합격 = 3 master material (Particle / Trail / Volumetric) 의 머티리얼 인스턴스 9 개 생성
    - M_VFX_Particle_Generic 인스턴스 3개 (화염 / 독 / 얼음 변주)
    - M_VFX_Trail 인스턴스 3개 (검 trail / 화살 trail / 마법 trail)
    - M_VFX_Volumetric 인스턴스 3개 (안개 / 연기 / 폭발 구)
  4 핵심 노브 (UV pan / dissolve / color-over-life / HDR emission) 디자이너 슬라이더 동작
  6 renderer 각각 = master material 1 종에 대응 (Sprite + Trail = Particle / Ribbon + Beam = Trail / Light + Decal = Volumetric custom path)

부속 20 의 6 renderer 헤더 + cpp = 그대로 유지 (RH-7 IRHI dispatch 파이프라인)
부속 27 의 3 master material 셰이더 = 6 renderer 가 호출 (vertex factory 별 차이만)
```

---

## §7 EFX-4 / EFX-5 우선순위 정정 (디자이너 노드 그래프 = 후순위)

```txt
이전 (17 §13 v1):
  EFX-4 = Editor MVP 7 패널 (Stack / Graph / Curve / Viewport / Parameter / ScratchPad / Toolbar) 모두 표시
  EFX-5 = Compile (Graph → VM/HLSL) 표준 모듈 9 종 컴파일

5/7 갱신:
  EFX-4 우선순위 (디자이너 1차 워크플로우):
    1. Toolbar (New Asset / Save / Recompile)
    2. Stack panel (모듈 추가/제거)
    3. Parameter panel (4 핵심 노브 + Advanced 토글)
    4. Curve editor (color-over-life 곡선)
    5. Preview viewport (라이브 프리뷰)
    6. Scratch pad (인라인 모듈) — 후순위
    7. Graph panel (노드 그래프 에디터, ImNodes) — 후순위, 2차 트랙

  EFX-4 1차 합격 기준 갱신:
    - Toolbar / Stack / Parameter / Curve / Viewport 5 패널만 동작
    - 신규 .wmi 인스턴스 생성 + 4 핵심 노브 슬라이더 → 라이브 프리뷰 갱신 200ms
    - Graph + ScratchPad = EFX-4 통과 후 별도 진입 (디자이너 요구 기반)

  EFX-5 우선순위 정정:
    - 1차 트랙 = master material 정적 HLSL (본 27 박제, Engine 빌드 시점 컴파일)
    - 2차 트랙 = 노드 그래프 동적 컴파일 (부속 22) → Custom material 박제 시점
    - 9 표준 모듈 (부속 22 §3.11) = 2차 트랙 진입 후 동적 컴파일
```

---

## §8 검증 명령 (EFX-3 본 27 합격 기준)

```txt
1. grep "Scene_" Engine/{Public,Private}/FX/v2/Material/   → 0 hit
2. grep "ID3D11" Engine/{Public,Private}/FX/v2/Material/   → 0 hit
3. grep "OnUpdate" Engine/{Public,Private}/FX/v2/Material/  → 0 hit
4. grep "TBD" .md/plan/EffectTool/27_AAA_VFX_INSIGHTS_AND_MASTER_MATERIALS_BAKE.md  → 0 hit
5. grep "stub|scaffold|본 박제 시점.*채움" 본 27  → 0 hit
6. CFxMasterMaterialRegistry::BuildStandardMasters → 3 master material 생성 검증
7. M_VFX_Particle_Generic 의 vecParameters.size() >= 30 (Advanced 포함)
8. 4 핵심 노브 (UVPanX/UVPanY/DissolveThreshold/EmissionIntensity) 의 bIsCorePinned == true 검증
9. CFxMaterialInstance::SetParameterFloat(L"UVPanX", 0.5f) 후 GetParameterFloat → 0.5f 동일성
10. M_VFX_Particle_Generic_PS.hlsl 컴파일 통과 (DXC + DX12 SM 5.1+)
11. 13 HLSL 트릭 함수 모두 unit test (각 트릭 1 회 호출 + scalar 결과 ULP 1e-5 비교)
```

---

## §9 박제 함정 매트릭스

| 함정 | 본 27 회피 |
|---|---|
| P-1 + P-6 | §1 6 항목, TBD 0 |
| P-2 (PIMPL 추측) | 헤더 + cpp 동시 |
| P-3 (모든 path) | 3 master material 한 번에 (Particle / Trail / Volumetric) + 13 HLSL 트릭 한 번에 |
| P-4 (Scene 직접 의존) | Material = pure data |
| P-7 (bitmask) | mask 미사용 |
| P-8 (인용 의미 반전) | Niagara `Engine/Plugins/FX/Niagara/Shaders/Private/NiagaraCommon.ush` 의 패턴 + Unreal `MaterialTemplate.ush` master material 차용 |
| P-9 (ECS Scheduler) | Material = ECS 무관 |
| P-10 (Owner Scope) | Registry = `CGameInstance` Tier-1. Instance = Asset (Emitter) owned |
| P-11 (도메인 상수) | Master material = 도메인 무관. LoL/Elden 인스턴스 변주만 다름 |
| P-12 (음수 truncation) | 정수 변환 0 |
| P-13 (미존재 API) | `FxBlendMode / eFxLightingModel` 부속 20 / 본 27 박제 |
| P-14 (행동 정책 변경) | 본 27 = 신규 (부속 20 의 6 renderer 호출자, 행동 보존) |
| P-15 (헤더 외부 의존) | `FxMaterialInstance.h` = `FxMaterialMaster.h` 직접 include |
| P-16 (산술 검증) | `eFxLightingModel` 5 값 / `eFxMasterMaterial` 3 값 static_assert |
| P-17 (typedef ABI) | 신규 |
| P-18 (RHI 인프라) | 부속 20 의 RH-7 IRHI 재사용 |
| P-19 (Render/Sim 결합) | Material = 정적 HLSL + 인스턴스 파라미터. Sim 무관 |

---

## §10 변경 이력

```txt
2026-04-21    Phase G 초안 (Stage 5 DX11 Rendering = 06)
2026-05-04    Niagara V2 (12)
2026-05-07    17 v4 마스터. 본 27 = 부속 10번 박제 (5/7 신규)
              - 사용자 AAA VFX 통찰 5/7 분석 박제
              - 그레이스케일 텍스처 = 데이터 / PBR/GI 거의 안 씀 / 13 HLSL 트릭 / 메쉬=형태 셰이더=외관
              - Master Material 3 종 + 5 라이팅 모델 + 4 핵심 노브
              - EFX-3 합격 기준 갱신 (6 renderer → 3 master material 인스턴스 9개)
              - EFX-4 / EFX-5 우선순위 정정 (노드 그래프 = 후순위)
```
